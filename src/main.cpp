#include <Arduino.h>

#include <SPI.h>
#include <SD.h>
#include <U8g2lib.h>

#include <CommandQueue.h>

HardwareSerial PrinterSerial(2);

#define DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define DEBUGFI(...)  { log_printf(__VA_ARGS__); }
#define DEBUGS(s)  { Serial.println(s); }

#define PIN_POT1  33

#define PIN_BT1  14
#define PIN_BT2  12
#define PIN_BT3  13

#define PIN_ENC1 26
#define PIN_ENC2 27

#define PIN_CE_SD  5
#define PIN_CE_LCD  4
#define PIN_RST_LCD 22

#define KEEPALIVE_INTERVAL 2500         // Marlin defaults to 2 seconds, get a little of margin

U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R1, PIN_CE_LCD, PIN_RST_LCD); 

CommandQueue<16, 100> commandQueue;
CommandQueue<3, 0> immediateQueue;

File root;
File gcodeFile;
uint32_t gcodeFileSize;
bool gcodeFilePrinting = false;
float gcodeFilePercentage = 0;
float droX, droY, droZ;

enum class JogAxis {
    X,Y,Z
};
String axisStr(const JogAxis &a) {
    switch(a) {
        case JogAxis::X : return "X";
        case JogAxis::Y : return "Y";
        case JogAxis::Z : return "Z";
    }
    DEBUGF("Unknown axis\n");
    return "";
}
enum class JogDist {
    _001, _01, _1
};
String distStr(const JogDist &a) {
    switch(a) {
        case JogDist::_001: return "0.01";
        case JogDist::_01: return "0.1";
        case JogDist::_1: return "1";
    }
    DEBUGF("Unknown dist\n");
    return "";
}

int encVal = 0;

JogAxis cAxis;
JogDist cDist;


void encISR();
void bt1ISR();
void bt2ISR();
void bt3ISR();



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

    PrinterSerial.begin(115200);

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

    root = SD.open("/");
    gcodeFile = SD.open("/meat.nc");
    gcodeFileSize = gcodeFile.size();
}

void parseGrblStatus(String v) {
    //<Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
    DEBUGF("parsing %s\n", v.c_str() );
    int pos;
    pos = v.indexOf('|');
    if(pos==-1) return;
    String stat = v.substring(1,pos);
    v = v.substring(pos+1);
    if(v.startsWith("MPos:")) {
        int p2 = v.indexOf(',');
        droX = v.substring(5, p2).toFloat();
        v = v.substring(p2+1);
        p2 = v.indexOf(',');
        droY = v.substring(0, p2).toFloat();
        v = v.substring(p2+1);
        p2 = v.indexOf('|');
        droZ = v.substring(0, p2).toFloat();
    }

}

void scheduleGcode() {
    if(!gcodeFilePrinting) return;

    if(commandQueue.getFreeSlots() > 3) { // } && commandQueue.getRemoteFreeSpace()>0) {
        int rd;
        char cline[256];
        uint32_t len=0;
        while( gcodeFile.available() ) {
            rd = gcodeFile.read();
            if(rd=='\n' || rd=='\r') break;
            cline[len++] = rd;
        }        
        if(gcodeFile.available()==0) gcodeFilePrinting = false;
        gcodeFilePercentage = 1.0 - gcodeFile.available()*1.0/gcodeFileSize;

        cline[len]=0;

        if(len==0) return;

        String line(cline);
        DEBUGF("popped line '%s', len %d\n", line.c_str(), len );

        int pos = line.indexOf(';');
        if(pos!=-1) line = line.substring(0, pos);

        if(line.length()==0) return;

        commandQueue.push(line);

    }

}

uint32_t serialReceiveTimeoutTimer;

inline void restartSerialTimeout() {
  serialReceiveTimeoutTimer = millis() + KEEPALIVE_INTERVAL;
}

void sendCommands() {
    String command;
    if(immediateQueue.hasUnsent() ) {
        command = immediateQueue.markSent();
        PrinterSerial.print(command);
        PrinterSerial.print("\n");
        restartSerialTimeout();
        DEBUGF("Sent '%s', immediate\n", command.c_str());
        return;
    }
    command = commandQueue.peekSend();
    if (command != "") {
        String s = commandQueue.markSent();
        if(s=="") {
            DEBUGF("Not sent, free space: %d\n", commandQueue.getRemoteFreeSpace());
        } else {
            DEBUGF("Sent '%s', free spacee %d\n", command.c_str(), commandQueue.getRemoteFreeSpace());
            PrinterSerial.print(command);  
            PrinterSerial.print("\n");
            restartSerialTimeout();
        }
    }
}

String lastReceivedResponse;

void receiveResponses() {
    static int lineStartPos = 0;
    static String resp;

    while (PrinterSerial.available()) {
        char ch = (char)PrinterSerial.read();
        if (ch != '\n')
            resp += ch;
        else {
            bool incompleteResponse = false;
            String responseDetail = "";

            //DEBUGF("Got response %s\n", serialResponse.c_str() );

            if (resp.startsWith("ok", lineStartPos)) {
                if(!immediateQueue.allAcknowledged() ) immediateQueue.markAcknowledged(); else commandQueue.markAcknowledged();
                responseDetail = "ok";
            } else 
            if (resp.startsWith("error") ) {
                if(!immediateQueue.allAcknowledged() ) immediateQueue.markAcknowledged(); else commandQueue.markAcknowledged();
                responseDetail = "error";
            } else
            if ( resp.startsWith("<") ) {
                parseGrblStatus(resp);
            }
            DEBUGF("free space: %4d rx: %s\n", commandQueue.getRemoteFreeSpace(), resp.c_str() );
            resp = "";
        }
    }

}

void processPot() {
    //  center lines : 2660    3480    4095
    // borders:            3000    3700

    int v = analogRead(PIN_POT1);
    if( cAxis==JogAxis::X && v>3000+100) cAxis=JogAxis::Y;
    if( cAxis==JogAxis::Y && v>3700+100) cAxis=JogAxis::Z;
    if( cAxis==JogAxis::Z && v<3700-100) cAxis=JogAxis::Y;
    if( cAxis==JogAxis::Y && v<3000-100) cAxis=JogAxis::X;
    
}

void processEnc() {
    static int lastEnc;
    //static uint32_t 
    if(encVal != lastEnc) {
        int8_t dx = (encVal - lastEnc);
        bool r = commandQueue.push("$J=G91 F100 "+axisStr(cAxis)+(dx>0?"":"-")+distStr(cDist) );
        //DEBUGF("Encoder val is %d, push ret=%d\n", encVal, r);
    }
    lastEnc = encVal;
}


void draw() {
    char str[100];

    u8g2.clearBuffer();
    
    snprintf(str, 100, "X: %.3f", droX );   u8g2.drawStr(10, 0, str ); 
    snprintf(str, 100, "Y: %.3f", droY );   u8g2.drawStr(10, 10, str ); 
    snprintf(str, 100, "Z: %.3f", droZ );   u8g2.drawStr(10, 20, str ); 

    snprintf(str, 100, "%s %s", axisStr(cAxis).c_str(), distStr(cDist).c_str()  );
    u8g2.drawStr(10, 30, str ); 

    u8g2.drawStr(0, 40, gcodeFilePrinting ? "P" : "");
    
    snprintf(str, 100, "%d%%", int(gcodeFilePercentage*100) );
    u8g2.drawStr(10, 40, str);
    u8g2.sendBuffer();
}

void loop() {
    processPot();
    processEnc();

    scheduleGcode();

    static uint32_t nextPosRequestTime;
    if(millis() > nextPosRequestTime) {
        immediateQueue.push("?");
        nextPosRequestTime = millis() + 1000;
    }

    sendCommands();

    receiveResponses();

    draw();

    if(Serial.available()) {
        PrinterSerial.write( Serial.read() );
    }

    

    /*File entry = root.openNextFile();
    if(!entry) {
        root.rewindDirectory();
        entry = root.openNextFile();
    }
    if(entry) { Serial.println( entry.name() ); }
    else Serial.println("Failed dir");

    delay(1000);

    static int i=0;

    u8g2.clearBuffer();
    char str[100];
    snprintf(str, 100, "%d", i++);
    u8g2.drawStr(10, 10, str); 
    u8g2.sendBuffer();

    delay(1000);*/
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


IRAM_ATTR void btChanged(uint8_t pin) {
  uint8_t val = digitalRead(pin);
  if(val == LOW) { // pressed
    DEBUGFI("Pressed pin %d\n", pin);
    
  }
}

IRAM_ATTR void bt1ISR() {
  static long lastChangeTime = millis();
  if(millis() < lastChangeTime+10) return;
  lastChangeTime = millis();
  btChanged(PIN_BT1);
}

IRAM_ATTR void bt2ISR() {
  static long lastChangeTime = millis();
  if(millis() < lastChangeTime+10) return;
  lastChangeTime = millis();
  btChanged(PIN_BT2);
}

IRAM_ATTR void bt3ISR() {
  static long lastChangeTime = millis();
  
  if(millis() < lastChangeTime+10) return;
  lastChangeTime = millis();
  bool val = digitalRead(PIN_BT3) == LOW;
  if(val) {
      gcodeFilePrinting = !gcodeFilePrinting;
      DEBUGFI("bt3: filePrinting=%d\n", gcodeFilePrinting);
  }
  

}
