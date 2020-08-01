#include <Arduino.h>

#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>

#include "devices/GCodeDevice.h"
#include "Job.h"
#include "ui/FileChooser.h"
#include "ui/DRO.h"
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


U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R1, PIN_CE_LCD, PIN_RST_LCD); 
U8G2 &Screen::u8g2{u8g2};


WebServer server;

GCodeDevice *dev = nullptr;

Job *job;

enum class Mode {
    DRO, FILECHOOSER
};

FileChooser fileChooser;
DRO dro;
Screen *cScreen;
Mode cMode = Mode::DRO;


void encISR();
void bt1ISR();
void bt2ISR();
void bt3ISR();

bool detectPrinterAttempt(uint32_t speed, uint8_t type);
void detectPrinter();

void deviceLoop(void* pvParams);
TaskHandle_t deviceTask;


void setup() {

    pinMode(PIN_BT1, INPUT_PULLUP);
    pinMode(PIN_BT2, INPUT_PULLUP);
    pinMode(PIN_BT3, INPUT_PULLUP);

    pinMode(PIN_ENC1, INPUT_PULLUP);
    pinMode(PIN_ENC2, INPUT_PULLUP);

    attachInterrupt(PIN_ENC1, encISR, CHANGE);
    attachInterrupt(PIN_BT1, bt1ISR, CHANGE);
    attachInterrupt(PIN_BT2, bt2ISR, CHANGE);
    attachInterrupt(PIN_BT3, bt3ISR, CHANGE);

    //PrinterSerial.begin(250000);

    Serial.begin(115200);

    u8g2.begin();
    u8g2.setBusClock(600000);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setFontPosTop();
    u8g2.setFontMode(1);

    digitalWrite(PIN_RST_LCD, LOW);
    delay(100);
    digitalWrite(PIN_RST_LCD, HIGH);


    Serial.print("Initializing SD card...");

    if (!SD.begin(PIN_CE_SD)) {
        Serial.println("initialization failed!");
        while (1);
    }
    Serial.println("initialization done.");

    xTaskCreatePinnedToCore(deviceLoop, "DeviceTask", 
        4096, nullptr, 1, &deviceTask, 1); // cpu1 
    
    job = Job::getJob();

    cScreen = &dro;

    dro.begin();    
    fileChooser.begin();
    fileChooser.setCallback( [&](bool res, String path){
        if(res) { 
            job->setFile(path);            
            job->resume();
            
            cMode = Mode::DRO;
            dev->enableStatusUpdates();
        }
    } );

    server.begin();
    
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
    dev->begin();
    job->add_observer( dro );
    job->add_observer( fileChooser );
    
    while(1) {
        dev->loop();
    }
    vTaskDelete( NULL );
}

void readPots() {
    Screen::potVal[0] = analogRead(PIN_POT1);
    Screen::potVal[1] = analogRead(PIN_POT2);
   
}

void loop() {
    readPots();    

    job->loop();

    if(cScreen != nullptr) {
        cScreen->processInput();
        cScreen->draw();
    }

    if(dev==nullptr) return;

    static uint32_t nextSent;
    static size_t lv1,lv2;
    if(nextSent < millis()) {
        size_t v1 = dev->getQueueLength(), v2 = dev->getSentQueueLength();
        if(v1!=lv1 || v2!=lv2)
            DEBUGF("queue: %d, sent: %d\n", v1, v2 );
        lv1=v1; lv2=v2;
        nextSent = millis()+100;
    }

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
    static int last1=0;

    int v1 = digitalRead(PIN_ENC1);
    int v2 = digitalRead(PIN_ENC2);
    if(v1==HIGH && last1==LOW) {
        if(v2==HIGH) Screen::encVal++; else Screen::encVal--;
    }
    if(v1==LOW && last1==HIGH) {
        if(v2==LOW) Screen::encVal++; else Screen::encVal--;
    }
    last1 = v1;
}


IRAM_ATTR void btChanged(uint8_t button, uint8_t val) {
  Screen::buttonPressed[button] = val==LOW;
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
