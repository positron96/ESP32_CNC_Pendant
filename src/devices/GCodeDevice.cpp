#include "GCodeDevice.h"

#define XOFF  0x13
#define XON   0x11

GCodeDevice *GCodeDevice::device;

GCodeDevice *GCodeDevice::getDevice() {
    return device;
}
void GCodeDevice::setDevice(GCodeDevice *dev) {
    device = dev;
}

/*
static union DevicesUnion {
    GrblDevice grbl;
    MarlinDevice marlin;
    DevicesUnion() {  }
    ~DevicesUnion() {  }
} devices;*/
#define MAX(a,b)  ( (a)>(b) ? (a) : (b) )
static char deviceBuffer[MAX(sizeof(MarlinDevice), sizeof(GrblDevice))];


void DeviceDetector::sendProbe(uint8_t i, Stream &serial) {
    switch(i) {
        case 0: 
            serial.println();
            serial.println("$I");
            break;
        case 1:
            serial.println();
            serial.println("M115");
            break;
    }
}


GCodeDevice* DeviceDetector::checkProbe(uint8_t i, String v, Stream &serial) {
    if(i==0) {
        if(v.indexOf("[VER:")!=-1 ) {
            GD_DEBUGS("Detected GRBL device");
            //devices.grbl = GrblDevice(&serial);
            return new (deviceBuffer) GrblDevice(&serial);
            //return (GCodeDevice*)deviceBuffer;
        }
    }
    if(i==1) {
        if(v.indexOf("MACHINE_TYPE") != -1) {
            GD_DEBUGS("Detected Marlin device");
            //devices.marlin = MarlinDevice(&serial);
            return new (deviceBuffer) MarlinDevice(&serial);
            //return (GCodeDevice*)deviceBuffer;
        }
    }

    return nullptr;

    //return false;
}






void GrblDevice::sendCommands() {

    if(panic) return;

    const int LEN = 100;
    char msg[LEN+1];

    size_t l = xMessageBufferReceive(buf0, msg, LEN, 0);
    if(l==0) l = xMessageBufferReceive(buf1, msg, LEN, 0);
    if(l==0) return;

    msg[l] = 0;
    if(sentQueue.getFreeSlots()>=1 && sentQueue.getRemoteFreeSpace()>l+2) {
        sentQueue.push( String(msg) );
        sentQueue.markSent();
        printerSerial->write(msg, l);  
        printerSerial->print('\n');
        armRxTimeout();
        //GD_DEBUGF("Sent '%s', free space %d\n", msg, sentQueue.getRemoteFreeSpace() );
    } else {
        GD_DEBUGF("Not sent, free space: %d\n", sentQueue.getRemoteFreeSpace());
    }

};

void GrblDevice::receiveResponses() {

    static int lineStartPos = 0;
    static String resp;

    while (printerSerial->available()) {
        char ch = (char)printerSerial->read();
        if (ch != '\n')
            resp += ch;
        else {
            //bool incompleteResponse = false;
            String responseDetail = "";

            //DEBUGF("Got response %s\n", serialResponse.c_str() );

            if (resp.startsWith("ok", lineStartPos)) {
                sentQueue.markAcknowledged();
                responseDetail = "ok";
            } else 
            if (resp.startsWith("error") ) {
                sentQueue.markAcknowledged();
                responseDetail = "error";
                DeviceError err = { 1 };
                notify_observers(err); panic=true;
            } else
            if ( resp.startsWith("<") ) {
                parseGrblStatus(resp);
            }
            //GD_DEBUGF("free space: %4d rx: %s\n", commandQueue.getRemoteFreeSpace(), resp.c_str() );
            updateRxTimeout(sentQueue.hasUnacknowledged() );
            resp = "";
        }
    }

};


void GrblDevice::parseGrblStatus(String v) {
    //<Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
    GD_DEBUGF("parsing %s\n", v.c_str() );
    int pos;
    pos = v.indexOf('|');
    if(pos==-1) return;
    String stat = v.substring(1,pos);
    v = v.substring(pos+1);
    if(v.startsWith("MPos:")) {
        int p2 = v.indexOf(',');
        x = v.substring(5, p2).toFloat();
        v = v.substring(p2+1);
        p2 = v.indexOf(',');
        y = v.substring(0, p2).toFloat();
        v = v.substring(p2+1);
        p2 = v.indexOf('|');
        z = v.substring(0, p2).toFloat();
    }
}







#define TEMP_COMMAND      "M105"
#define AUTOTEMP_COMMAND  "M155 S"

size_t MarlinDevice::getSentQueueLength() {
    return sentQueue.bytes();
}

void MarlinDevice::sendCommands() {
    if(panic) return;
    bool loadedNewCmd=false;

    if(xoffEnabled && xoff) return;
    static size_t nline=0;

    if(curUnsentCmdLen==0) {
        char tmp[MAX_GCODE_LINE+1];
        curUnsentCmdLen = xMessageBufferReceive(buf0, tmp, MAX_GCODE_LINE, 0);
        if(curUnsentCmdLen==0) curUnsentCmdLen = xMessageBufferReceive(buf1, tmp, MAX_GCODE_LINE, 0);
        //curUnsentCmdLen = xMessageBufferReceive(buf0, curUnsentCmd, MAX_GCODE_LINE, 0);
        //if(curUnsentCmdLen==0) curUnsentCmdLen = xMessageBufferReceive(buf1, curUnsentCmd, MAX_GCODE_LINE, 0);
        loadedNewCmd=true;
        if(curUnsentCmdLen!=0) {
            tmp[curUnsentCmdLen] = 0;
            snprintf(curUnsentCmd, MAX_GCODE_LINE, "%s ;%d", tmp, nline++);
            curUnsentCmdLen = strlen(curUnsentCmd);
        }
        //snprintf(curUnsentCmd, MAX_GCODE_LINE, "%s", tmp);
    }
    if(curUnsentCmdLen==0) return;
    
    curUnsentCmd[curUnsentCmdLen] = 0;

    if( sentQueue.canPush(curUnsentCmdLen) ) {
        sentQueue.push( curUnsentCmd, curUnsentCmdLen );
        printerSerial->write(curUnsentCmd, curUnsentCmdLen);  
        printerSerial->print('\n');
        armRxTimeout();
        curUnsentCmdLen = 0;
        GD_DEBUGF("<  (f%3d) '%s'\n", sentQueue.getFreeLines(), curUnsentCmd );
    } else {
        //if(loadedNewCmd) GD_DEBUGF("<  Not sent, free lines: %d, free space: %d\n", sentQueue.getFreeLines() , sentQueue.getFreeBytes()  );
    }
};

bool startsWith(const char *str, const char *pre) {
    return strncmp(pre, str, strlen(pre)) == 0;
}

void MarlinDevice::receiveResponses() {

    //static int lineStartPos = 0;
    //static String resp;
    static const size_t MAX_LINE = 200; // M115 is far longer than 100
    static char resp[MAX_LINE+1];
    static size_t respLen;
    char responseDetail[MAX_LINE];

    while (printerSerial->available()) {
        char ch = (char)printerSerial->read();
        switch(ch) {
            case '\n':
            case '\r': break;
            case XOFF: xoff=true; break;
            case XON: xoff=false; break;
            default: if(respLen<MAX_LINE) resp[respLen++] = ch;
        }
        if(ch=='\n') {
            resp[respLen]=0;

            char tmp = 0;
            char* curCmd;
            size_t curCmdLen = sentQueue.peek(curCmd);
            if(curCmdLen==0) curCmd = &tmp;

            //GD_DEBUGF(" > '%s'; current cmd %s\n", resp, curCmd );

            if ( startsWith(resp, "ok") ) {

                if (startsWith(curCmd, TEMP_COMMAND))
                    parseTemperatures(String(resp) );
                else if (fwAutoreportTempCap && startsWith(curCmd, AUTOTEMP_COMMAND))
                    autoreportTempEnabled = (curCmd[6] != '0');
                else if(startsWith(curCmd, "G0") || startsWith(curCmd, "G1")) {
                    parseG0G1(curCmd); // artificial position from G0/G1 command
                }
                
                snprintf(responseDetail, MAX_LINE, "ACK");
                //sentQueue.markAcknowledged();     // Go on with next command
                sentQueue.pop();
                
                //curCmdLen = 0; // need to fetch another sent command from queue

                connected = true;
            } else {
                if (connected) {
                    if(startsWith(curCmd,"M115") ) {
                        parseM115(String(resp) ); sprintf(responseDetail, "status");
                    } else if (parseTemperatures(String(resp) ) )
                        sprintf(responseDetail, "autotemp");
                    else if (parsePosition(String(resp) ) )
                        sprintf(responseDetail, "position");
                    /*else if (startsWith(resp, "Resend: ")) {
                        nline = (int)extractFloat(resp, "Resend:");
                        sprintf(responseDetail, "sync line=%d", nline);
                    }*/
                    else if (startsWith(resp, "echo: cold extrusion prevented")) {
                        // To do: Pause sending gcode, or do something similar
                        sprintf(responseDetail, "cold extrusion");
                        DeviceError err = { 1 };
                        notify_observers(err); 
                    }
                    else if (startsWith(resp, "Error:") ) {
                        sprintf(responseDetail, "ERROR");
                        DeviceError err = { 1 };
                        notify_observers(err); 
                    }
                    else {
                        //incompleteResponse = true;
                        sprintf(responseDetail, "incomplete");
                    }
                } else {
                    //incompleteResponse = true;
                    sprintf(responseDetail, "discovering");
                }
            }

            GD_DEBUGF(" > (f%3d) '%s' current cmd '%s', desc:'%s'\n", sentQueue.getFreeLines(), resp, curCmd, responseDetail );
            //GD_DEBUGF("   free slots: %d type:'%s' \n", sentQueue.getFreeLines(),  responseDetail  );
            updateRxTimeout( sentQueue.size()>0 );
            respLen = 0;
        }
    }

};



// Parse temperatures from printer responses like
// ok T:32.8 /0.0 B:31.8 /0.0 T0:32.8 /0.0 @:0 B@:0
bool MarlinDevice::parseTemperatures(const String &response) {
    bool ret;

    if (fwExtruders == 1)
        ret = parseTemp(response, "T", &toolTemperatures[0]);
    else {
        ret = false;
        for (int t = 0; t < fwExtruders; t++)
            ret |= parseTemp(response, "T" + String(t), &toolTemperatures[t]);
    }
    ret |= parseTemp(response, "B", &bedTemperature);
    if (!ret) {
        // Parse Prusa heating temperatures
        int e = extractPrusaHeatingExtruder(response);
        ret = e >= 0 && e < MAX_SUPPORTED_EXTRUDERS && extractPrusaHeatingTemp(response, "T", toolTemperatures[e].actual);
        ret |= extractPrusaHeatingTemp(response, "B", bedTemperature.actual);
    }

    if(ret) GD_DEBUGF("Parsed temp E:%d->%d  B:%d->%d\n", 
        (int)toolTemperatures[0].actual, (int)toolTemperatures[0].target,  
        (int)bedTemperature.actual, (int)bedTemperature.target );
    return ret;
}

// parses line T:aaa.aa /ttt.tt
bool MarlinDevice::parseTemp(const String &response, const String whichTemp, Temperature *temperature) {
    int tpos = response.indexOf(whichTemp + ":");
    if (tpos != -1) { // This response contains a temperature
        int slashpos = response.indexOf(" /", tpos);
        int spacepos = response.indexOf(" ", slashpos + 1);
        // if match mask T:xxx.xx /xxx.xx
        if (slashpos != -1 && spacepos != -1) {
            String actual = response.substring(tpos + whichTemp.length() + 1, slashpos);
            String target = response.substring(slashpos + 2, spacepos);
            if (isFloat(actual) && isFloat(target)) {
                temperature->actual = actual.toFloat();
                temperature->target = target.toFloat();

                return true;
            }
        }
    }

    return false;
}

// Parse position responses from printer like
// X:-33.00 Y:-10.00 Z:5.00 E:37.95 Count X:-3300 Y:-1000 Z:2000
bool MarlinDevice::parsePosition(const String &str) {
    float t = extractFloat(str, "X:");
    if(!isnan(t) ) x = t; else return false;
    t = extractFloat(str, "Y:");
    if(!isnan(t) ) y = t; else return false;
    t = extractFloat(str, "Z:");
    if(!isnan(t) ) z = t; else return false;
    t = extractFloat(str, "E:");
    if(!isnan(t) ) ePos = t; else return false;
    GD_DEBUGF("Parsed pos: X: %f, Y: %f, Z: %f, E: %f\n", x,y,z,ePos);
    return true;
}

// Parse position responses from printer like
// X:-33.00 Y:-10.00 Z:5.00 E:37.95 Count X:-3300 Y:-1000 Z:2000
bool MarlinDevice::parseG0G1(const char *str) {
    float t = extractFloat(str, "X");
    if(!isnan(t) ) x = t; else return false;
    t = extractFloat(str, "Y");
    if(!isnan(t) ) y = t; else return false;
    t = extractFloat(str, "Z");
    if(!isnan(t) ) z = t; else return false;
    t = extractFloat(str, "E");
    if(!isnan(t) ) ePos = t; else return false;
    GD_DEBUGF("Parsed pos: X: %f, Y: %f, Z: %f, E: %f\n", x,y,z,ePos);
    return true;
}

bool MarlinDevice::parseM115(const String &str) {
    desc = extractM115String(str, "FIRMWARE_NAME") + " " + extractM115String(str, "MACHINE_TYPE");
    String value = extractM115String(str, "EXTRUDER_COUNT");
    fwExtruders = value == "" ? 1 : min(value.toInt(), (long)MAX_SUPPORTED_EXTRUDERS);
    fwAutoreportTempCap = extractM115Bool(str, "Cap:AUTOREPORT_TEMP");
    fwProgressCap = extractM115Bool(str, "Cap:PROGRESS");
    fwBuildPercentCap = extractM115Bool(str, "Cap:BUILD_PERCENT");
    GD_DEBUGF("Parsed M115: desc=%s, extruders:%d, autotemp:%d, progress:%d, buildPercent:%d\n", 
        desc.c_str(), fwExtruders, fwAutoreportTempCap, fwProgressCap, fwBuildPercentCap );
    return true;
}


bool MarlinDevice::isFloat(const String value) {
    for (int i = 0; i < value.length(); i++) {
        char ch = value[i];
        if (ch != ' ' && ch != '.' && ch != '-' && !isDigit(ch)) return false;
    }
    return true;
}

inline float MarlinDevice::extractFloat(const char *str, const char * key) {
    char* s = strstr(str, key);
    if(s==NULL) return NAN; 
    s += strlen(key);
    return atof(s);
}

inline float MarlinDevice::extractFloat(const String &str, const String key) {
    int s = str.indexOf(key);
    if(s==-1) return NAN; 
    s += key.length();
    int e = str.indexOf(' ', s);
    if(e==-1) e=str.length();
    return str.substring(s,e).toFloat();
}

// Parse temperatures from prusa firmare (sent when heating)
// ok T:32.8 E:0 B:31.8
bool MarlinDevice::extractPrusaHeatingTemp(const String &response, const String whichTemp, float &temperature) {
    int tpos = response.indexOf(whichTemp + ":");
    if (tpos != -1) { // This response contains a temperature
        int spacepos = response.indexOf(" ", tpos);
        if (spacepos == -1)
        spacepos = response.length();
        String actual = response.substring(tpos + whichTemp.length() + 1, spacepos);
        if (isFloat(actual)) {
            temperature = actual.toFloat();
            return true;
        }
    }

    return false;
}


inline int MarlinDevice::extractPrusaHeatingExtruder(const String &response) {
    float tmp;
    return extractPrusaHeatingTemp(response, "E", tmp) ? (int)tmp : -1;
}

String MarlinDevice::extractM115String(const String &response, const String key) {
    int spos = response.indexOf(key + ":");
    if (spos != -1) {
        spos += key.length() + 1;
        int epos = response.indexOf(':', spos);
        if (epos == -1) return response.substring(spos);
        else {
            while (epos >= spos && response[epos] != ' ' && response[epos] != '\n') --epos;
            return response.substring(spos, epos);
        }
    }

    return "";
}

inline bool MarlinDevice::extractM115Bool(const String &response, const String key, const bool onErrorValue) {
    String result = extractM115String(response, key);
    return result == "" ? onErrorValue : (result == "1" ? true : false);
}
