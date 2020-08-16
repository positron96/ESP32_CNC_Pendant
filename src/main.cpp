#include <Arduino.h>

#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>

#include "devices/GCodeDevice.h"
#include "Job.h"
#include "ui/FileChooser.h"
#include "ui/DRO.h"
#include "ui/GrblDRO.h"
#include "InetServer.h"

HardwareSerial PrinterSerial(2);

#ifdef DEBUGF
  #undef DEBUGF
#endif
#define DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define DEBUGFI(...)  { log_printf(__VA_ARGS__); }
#define DEBUGS(s)  { Serial.println(s); }

#define PIN_POT1  33
#define PIN_POT2  32

#define PIN_BT1  14
#define PIN_BT2  12
#define PIN_BT3  13

#define PIN_ENC1 26
#define PIN_ENC2 27

#define PIN_CE_SD  5
#define PIN_CE_LCD  4
#define PIN_RST_LCD 22


U8G2_ST7920_128X64_F_HW_SPI u8g2_{U8G2_R3, PIN_CE_LCD, PIN_RST_LCD}; 
U8G2 &Display::u8g2 = u8g2_;


WebServer server;

GCodeDevice *dev = nullptr;

Job *job;

enum class Mode {
    DRO, FILECHOOSER
};

Display display;
FileChooser fileChooser;
uint8_t droBuffer[ sizeof(GrblDRO) ];
DRO *dro;
Mode cMode = Mode::DRO;


void encISR();
void bt1ISR();
void bt2ISR();
void bt3ISR();

bool detectPrinterAttempt(uint32_t speed, uint8_t type);
void detectPrinter();

void deviceLoop(void* );
TaskHandle_t deviceTask;

void wifiLoop(void * );
TaskHandle_t wifiTask;


void setup() {

    Serial.begin(115200);

    pinMode(PIN_BT1, INPUT_PULLUP);
    pinMode(PIN_BT2, INPUT_PULLUP);
    pinMode(PIN_BT3, INPUT_PULLUP);

    pinMode(PIN_ENC1, INPUT_PULLUP);
    pinMode(PIN_ENC2, INPUT_PULLUP);

    attachInterrupt(PIN_ENC1, encISR, CHANGE);
    attachInterrupt(PIN_BT1, bt1ISR, CHANGE);
    attachInterrupt(PIN_BT2, bt2ISR, CHANGE);
    attachInterrupt(PIN_BT3, bt3ISR, CHANGE);

    u8g2_.begin();
    u8g2_.setBusClock(600000);
    u8g2_.setFont(u8g2_font_5x8_tr);
    u8g2_.setFontPosTop();
    u8g2_.setFontMode(1);

    digitalWrite(PIN_RST_LCD, LOW);
    delay(100);
    digitalWrite(PIN_RST_LCD, HIGH);


    Serial.print("Initializing SD card...");

    if (!SD.begin(PIN_CE_SD)) {
        Serial.println("initialization failed!");
        while (1);
    }
    Serial.println("initialization done.");

    DynamicJsonDocument cfg(512);
    File file = SD.open("/config.json");
    DeserializationError error = deserializeJson(cfg, file);
    if (error)  Serial.println(F("Failed to read file, using default configuration"));
 
    server.config( cfg["web"].as<JsonObjectConst>() );
    server.add_observer(display);


    xTaskCreatePinnedToCore(deviceLoop, "DeviceTask", 
        4096, nullptr, 1, &deviceTask, 1); // cpu1 

    xTaskCreatePinnedToCore(wifiLoop, "WifiTask", 
        4096, nullptr, 1, &wifiTask, 1); // cpu1 
    
    
    job = Job::getJob();
    job->add_observer( display );

    //dro.config(cfg["menu"].as<JsonObjectConst>() );

    fileChooser.begin();
    fileChooser.setCallback( [&](bool res, String path){
        if(res) {
            DEBUGF("Starting job %s\n", path.c_str() );
            job->setFile(path);            
            job->start();
            
            Display::getDisplay()->setScreen(dro); // select file
        } else {
            Display::getDisplay()->setScreen(dro); // cancel
        }
    } );

    file.close();

    
}


void deviceLoop(void* pvParams) {
    PrinterSerial.begin(115200);
    PrinterSerial.setTimeout(1000);
    dev = DeviceDetector::detectPrinterAttempt(PrinterSerial, 115200, 1); 
    if(dev==nullptr ) {
        dev = DeviceDetector::detectPrinter(PrinterSerial);
    }
    
    //GCodeDevice::setDevice(dev);
    dev->add_observer( *job );
    dev->add_observer(display);
    //dev->add_observer(dro);  // dro.setDevice(dev);
    //dev->add_observer(fileChooser);
    dev->addReceivedLineHandler( [](const char* d, size_t l) {server.resendDeviceResponse(d,l);} );
    dev->begin();

    if(dev->getType() == "grbl") {
        dro = new (droBuffer) GrblDRO();
    } else dro = new (droBuffer) DRO();
    dro->begin( );

    display.setScreen(dro);
   
    while(1) {
        dev->loop();
    }
    vTaskDelete( NULL );
}

void wifiLoop(void* args) {
    server.begin();
    vTaskDelete( NULL );
}

void readPots() {
    Display::potVal[0] = analogRead(PIN_POT1);
    Display::potVal[1] = analogRead(PIN_POT2);
}

void loop() {
    readPots();    

    job->loop();

    display.loop();

    if(dev==nullptr) return;

    static String s;
    while(Serial.available()!=0) {
        char c = Serial.read();
        if(c=='\n' || c=='\r') { 
            if(s.length()>0) DEBUGF("send %s, result: %d\n", s.c_str(), dev->schedulePriorityCommand(s) ); 
            s=""; 
            continue; 
        }
        s += c;
    }

}


IRAM_ATTR void encISR() {
    static int lastV1=0;
    static unsigned int lastISRTime = millis();
    if(millis()<lastISRTime+2) return;

    int v1 = digitalRead(PIN_ENC1);
    int v2 = digitalRead(PIN_ENC2);
    if(v1==HIGH && lastV1==LOW) {
        if(v2==HIGH) Display::encVal++; else Display::encVal--;
    }
    if(v1==LOW && lastV1==HIGH) {
        if(v2==LOW) Display::encVal++; else Display::encVal--;
    }
    //log_printf("%d, %d\n", v1,v2);
    //log_printf("%d, %d,  %3d\n", v1,v2,Display::encVal);
    lastV1 = v1;
}


IRAM_ATTR void btChanged(uint8_t button, uint8_t val) {
    Display::buttonPressed[button] = val==LOW;
}

IRAM_ATTR void bt1ISR() {
    static long lastChangeTime = millis();
    if(millis() < lastChangeTime+10) return;
    lastChangeTime = millis();
    btChanged(0, digitalRead(PIN_BT1) );
}

IRAM_ATTR void bt2ISR() {
    static long lastChangeTime = millis();
    if(millis() < lastChangeTime+10) return;
    lastChangeTime = millis();
    btChanged(1, digitalRead(PIN_BT2) );
}

IRAM_ATTR void bt3ISR() {
    static long lastChangeTime = millis();
    if(millis() < lastChangeTime+10) return;
    lastChangeTime = millis();
    btChanged(2, digitalRead(PIN_BT3) );
}
