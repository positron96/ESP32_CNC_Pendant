#include "GCodeDevice.h"

GCodeDevice *GCodeDevice::device;

GCodeDevice *GCodeDevice::getDevice() {
    return device;
}
void GCodeDevice::setDevice(GCodeDevice *dev) {
    device = dev;
}









void GrblDevice::sendCommands() {
    if(panic) return;
    String command = commandQueue.peekUnsent();
    if (command != "") {
        String s = commandQueue.markSent();
        if(s=="") {
            GD_DEBUGF("Not sent, free space: %d\n", commandQueue.getRemoteFreeSpace());
        } else {
            GD_DEBUGF("Sent '%s', free space %d\n", command.c_str(), commandQueue.getRemoteFreeSpace());
            printerSerial.print(command);  
            printerSerial.print("\n");
            armRxTimeout();
        }
    }
};

void GrblDevice::receiveResponses() {

    static int lineStartPos = 0;
    static String resp;

    while (printerSerial.available()) {
        char ch = (char)printerSerial.read();
        if (ch != '\n')
            resp += ch;
        else {
            //bool incompleteResponse = false;
            String responseDetail = "";

            //DEBUGF("Got response %s\n", serialResponse.c_str() );

            if (resp.startsWith("ok", lineStartPos)) {
                commandQueue.markAcknowledged();
                responseDetail = "ok";
            } else 
            if (resp.startsWith("error") ) {
                commandQueue.markAcknowledged();
                responseDetail = "error";
                DeviceError err = { 1 };
                notify_observers(err); panic=true;
            } else
            if ( resp.startsWith("<") ) {
                parseGrblStatus(resp);
            }
            GD_DEBUGF("free space: %4d rx: %s\n", commandQueue.getRemoteFreeSpace(), resp.c_str() );
            updateRxTimeout();
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



void MarlinDevice::sendCommands() {
    String command;
    command = commandQueue.peekUnsent();
    if (command != "") {
        String s = commandQueue.markSent();
        if(s=="") {
            GD_DEBUGF("Not sent, free slots: %d\n", commandQueue.getFreeSlots() );
        } else {
            GD_DEBUGF("TX (free slots %d) '%s'\n", commandQueue.getFreeSlots(), command.c_str() );
            printerSerial.print(command);  
            printerSerial.print("\n");
            armRxTimeout();
        }
    }
};

void MarlinDevice::receiveResponses() {

    static int lineStartPos = 0;
    static String resp;
    String responseDetail;
    String curCmd;

    while (printerSerial.available()) {
        char ch = (char)printerSerial.read();
        if (ch != '\n')
            resp += ch;
        else {

            curCmd = commandQueue.peekUnacknowledged();
            GD_DEBUGF("RX '%s'; current cmd %s\n", resp.c_str(), curCmd.c_str() );

            if (resp.startsWith("ok", lineStartPos)) {

                if (curCmd.startsWith(TEMP_COMMAND))
                    parseTemperatures(resp);
                else if (fwAutoreportTempCap && curCmd.startsWith(AUTOTEMP_COMMAND))
                    autoreportTempEnabled = (curCmd[6] != '0');
                
                responseDetail = "OK, ack '"+curCmd+"'";
                commandQueue.markAcknowledged();     // Go on with next command
                connected = true;
            } else if (connected) {
                if(curCmd == "M115") {
                    parseM115(resp); responseDetail = "status";
                } else if (parseTemperatures(resp))
                    responseDetail = "autotemp";
                else if (parsePosition(resp) )
                    responseDetail = "position";
                else if (resp.startsWith("echo:busy"))
                    responseDetail = "busy";
                else if (resp.startsWith("echo: cold extrusion prevented")) {
                    // To do: Pause sending gcode, or do something similar
                    responseDetail = "cold extrusion";
                    DeviceError err = { 1 };
                    notify_observers(err); 
                }
                else if (resp.startsWith("Error:")) {
                    responseDetail = "ERROR";
                    DeviceError err = { 1 };
                    notify_observers(err); 
                }
                else {
                    //incompleteResponse = true;
                    responseDetail = "wait more";
                }
            } else {
                //incompleteResponse = true;
                responseDetail = "discovering";
            }
            
            GD_DEBUGF("free slots: %d type:'%s' \n", commandQueue.getFreeSlots(),  responseDetail.c_str()  );
            updateRxTimeout();
            resp = "";
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

    /*if(ret) GD_DEBUGF("Parsed temp E:%d->%d  B:%d->%d\n", 
        (int)toolTemperatures[0].actual, (int)toolTemperatures[0].target,  
        (int)bedTemperature.actual, (int)bedTemperature.target );*/
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
    //GD_DEBUGF("Parsed pos: X: %f, Y: %f, Z: %f, E: %f\n", x,y,z,ePos);
    return true;
}

bool MarlinDevice::parseM115(const String &str) {
    desc = extractM115String(str, "FIRMWARE_NAME") + " " + extractM115String(str, "MACHINE_TYPE");
    String value = extractM115String(str, "EXTRUDER_COUNT");
    fwExtruders = value == "" ? 1 : min(value.toInt(), (long)MAX_SUPPORTED_EXTRUDERS);
    fwAutoreportTempCap = extractM115Bool(str, "Cap:AUTOREPORT_TEMP");
    fwProgressCap = extractM115Bool(str, "Cap:PROGRESS");
    fwBuildPercentCap = extractM115Bool(str, "Cap:BUILD_PERCENT");
    //GD_DEBUGF("Parsed M115: desc=%s, extruders:%d, autotemp:%d, progress:%d, buildPercent:%d\n", 
    //    desc.c_str(), fwExtruders, fwAutoreportTempCap, fwProgressCap, fwBuildPercentCap );
    return true;
}


bool MarlinDevice::isFloat(const String value) {
    for (int i = 0; i < value.length(); i++) {
        char ch = value[i];
        if (ch != ' ' && ch != '.' && ch != '-' && !isDigit(ch)) return false;
    }
    return true;
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
