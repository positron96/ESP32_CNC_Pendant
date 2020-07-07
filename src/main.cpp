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

File root;
File gcodeFile;
uint32_t gcodeFileSize;

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


void encISR() {
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



void setup() {

    pinMode(PIN_BT1, INPUT_PULLUP);
    pinMode(PIN_BT2, INPUT_PULLUP);
    pinMode(PIN_BT3, INPUT_PULLUP);

    pinMode(PIN_ENC1, INPUT_PULLUP);
    pinMode(PIN_ENC2, INPUT_PULLUP);

    attachInterrupt(PIN_ENC1, encISR, CHANGE);

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

void scheduleGcode() {
    //++lastPrintedLine;
    if(cAxis!=JogAxis::Z) return;

    static uint32_t filePos = 0;
    static bool printing = true;

    if(!printing) return;

    if(commandQueue.getFreeSlots() > 3) { // } && commandQueue.getRemoteFreeSpace()>0) {
        int rd;
        char cline[256];
        uint32_t len=0;
        while(( rd=gcodeFile.read()) > 0) {
            if(rd=='\n' || rd=='\r') break;
            cline[len++] = rd;
            filePos++;
        }        
        if(rd==-1) printing = false;

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
    String command = commandQueue.peekSend();
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
                commandQueue.markAcknowledged();
                responseDetail = "ok";
            } else 
            if (resp.startsWith("error") ) {
                commandQueue.markAcknowledged();
                responseDetail = "error";
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

void draw() {
    u8g2.clearBuffer();
    //char str[100];
    //snprintf(str, 100, "%d", i++);
    u8g2.drawStr(10, 10, axisStr(cAxis).c_str() ); 
    u8g2.drawStr(10, 20, distStr(cDist).c_str() ); 
    u8g2.sendBuffer();
}

void loop() {
    processPot();
    processEnc();

    scheduleGcode();

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


