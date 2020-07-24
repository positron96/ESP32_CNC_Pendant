#include <Arduino.h>

#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>

#include "devices/GCodeDevice.h"
#include "Job.h"
#include "FileChooser.h"
#include "InetServer.h"

HardwareSerial PrinterSerial(2);

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


WebServer server;

FileChooser fileChooser;

GCodeDevice *dev = nullptr;

Job *job;

enum class Mode {
    DRO, FILECHOOSER
};

Mode cMode = Mode::DRO;

enum class JogAxis {
    X,Y,Z
};
char axisChar(const JogAxis &a) {
    switch(a) {
        case JogAxis::X : return 'X';
        case JogAxis::Y : return 'Y';
        case JogAxis::Z : return 'Z';
    }
    DEBUGF("Unknown axis\n");
    return 0;
}
enum class JogDist {
    _01, _1, _10
};
float distVal(const JogDist &a) {
    switch(a) {
        case JogDist::_01: return 0.1;
        case JogDist::_1: return 1;
        case JogDist::_10: return 10;
    }
    DEBUGF("Unknown dist\n");
    return 1;
}

int encVal = 0;
bool buttonPressed[3];

JogAxis cAxis;
JogDist cDist;


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
    u8g2.setFont(u8g2_font_5x8_tf);
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

const uint32_t serialBauds[] = { 115200, 250000, 57600 };    // Marlin valid bauds (removed very low bauds; roughly ordered by popularity to speed things up)

bool detectPrinterAttempt(uint32_t speed, uint8_t type) {
    for(uint8_t retry=0; retry<2; retry++) {
        DEBUGF("attempt %d, speed %d, type %d\n", retry, speed, type);
        //PrinterSerial.end();
        //PrinterSerial.begin(speed);
        PrinterSerial.updateBaudRate(speed);
        while(PrinterSerial.available()) PrinterSerial.read();
        DeviceDetector::sendProbe(type, PrinterSerial);            
        String v = PrinterSerial.readStringUntil('\n'); v.trim();
        DEBUGF("Got response '%s'\n", v.c_str() );
        if(v) {
            dev = DeviceDetector::checkProbe(type, v, PrinterSerial);
            if(dev != nullptr) {
                GCodeDevice::setDevice(dev);
                dev->add_observer( *job );
                dev->begin();
                return true;
            }
        }
    }
    return false;
}

void detectPrinter() {
    while(true) {
        for(uint32_t speed: serialBauds) {
            for(int type=0; type<DeviceDetector::N_TYPES; type++)
                if(detectPrinterAttempt(speed, type)) return;
        }
    }    
    
}

void deviceLoop(void* pvParams) {
    PrinterSerial.begin(115200);
    PrinterSerial.setTimeout(2000);
    if(! detectPrinterAttempt(250000, 1) ) {
        detectPrinter();
    }
    while(1) {
        dev->loop();
    }
    vTaskDelete( NULL );
}

void processPot() {
    //  center lines : 2660    3480    4095
    // borders:            3000    3700

    int v = analogRead(PIN_POT1);
    if( cAxis==JogAxis::X && v>3000+100) cAxis=JogAxis::Y;
    if( cAxis==JogAxis::Y && v>3700+100) cAxis=JogAxis::Z;
    if( cAxis==JogAxis::Z && v<3700-100) cAxis=JogAxis::Y;
    if( cAxis==JogAxis::Y && v<3000-100) cAxis=JogAxis::X;

    // centers:      950    1620     2420
    // borders:         1300    2000

    v = analogRead(PIN_POT2);
    if( cDist==JogDist::_01 && v>1300+100) cDist=JogDist::_1;
    if( cDist==JogDist::_1  && v>2000+100) cDist=JogDist::_10;
    if( cDist==JogDist::_10 && v<2000-100) cDist=JogDist::_1;
    if( cDist==JogDist::_1  && v<1300-100) cDist=JogDist::_01;
    
}

void processEnc() {
    static int lastEnc;
    if(encVal != lastEnc) {
        int8_t dx = (encVal - lastEnc);

        if(cMode==Mode::FILECHOOSER) {
            fileChooser.buttonPressed(dx>0 ? Button::ENC_DOWN : Button::ENC_UP);
        } else {
            bool r = dev->jog( (int)cAxis, distVal(cDist) );
            //schedulePriorityCommand("$J=G91 F100 "+axisStr(cAxis)+(dx>0?"":"-")+distStr(cDist) );
            if(!r) DEBUGF("Could not schedule jog\n");
        }
    }
    lastEnc = encVal;
}

void processButtons() {
    static bool lastButtPressed[3];
    static const Button buttons[] = {Button::BT1, Button::BT2, Button::BT3};
    for(int i=0; i<3; i++) {
        if(lastButtPressed[i] != buttonPressed[i]) {
            //DEBUGF("button changed: %d %d\n", i, buttonPressed[i] );
            if(buttonPressed[i]) {
                if(cMode==Mode::FILECHOOSER) fileChooser.buttonPressed(buttons[i]);
                else { if(i==2) job->setPaused(!job->isPaused()); }
            }
            lastButtPressed[i] = buttonPressed[i];
        }
    }
}


void draw() {

    if(cMode==Mode::FILECHOOSER) {
        fileChooser.draw(u8g2);
        return;
    }

    if(dev==nullptr) return;

    char str[100];

    u8g2.clearBuffer();
    

    snprintf(str, 100, "X: %.3f", dev->getX() );   u8g2.drawStr(10, 0, str ); 
    snprintf(str, 100, "Y: %.3f", dev->getX() );   u8g2.drawStr(10, 10, str ); 
    snprintf(str, 100, "Z: %.3f", dev->getX() );   u8g2.drawStr(10, 20, str ); 

    snprintf(str, 100, "%c x%3f", axisChar(cAxis), distVal(cDist)  );
    u8g2.drawStr(10, 30, str ); 

    u8g2.drawStr(0, 40, job->isPaused() ? "P" : "");
    
    snprintf(str, 100, "%d%%", int(job->getPercentage()*100) );
    u8g2.drawStr(10, 40, str);
    u8g2.sendBuffer();
}

void loop() {
    processPot();
    processEnc();
    processButtons();

    job->loop();

    //draw();

    if(dev==nullptr) return;

    static uint32_t nextSent;
    if(nextSent < millis()) {
        DEBUGF("queue: %d, sent: %d\n", dev->getQueueLength(), dev->getSentQueueLength() );
        nextSent = millis()+1000;
    }

    static String s;
    while(Serial.available()!=0) {
        char c = Serial.read();
        if(c=='\n' || c=='\r') { 
            if(s.length()>0) DEBUGF("send %s, result: %d\n", s.c_str(), dev->scheduleCommand(s) ); 
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
        if(v2==HIGH) encVal++; else encVal--;
    }
    if(v1==LOW && last1==HIGH) {
        if(v2==LOW) encVal++; else encVal--;
    }
    last1 = v1;
}


IRAM_ATTR void btChanged(uint8_t button, uint8_t val) {
  buttonPressed[button] = val==LOW;
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
