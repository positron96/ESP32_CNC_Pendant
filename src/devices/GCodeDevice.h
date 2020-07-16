#pragma once

#include <Arduino.h>
#include <etl/observer.h>
#include "CommandQueue.h"

#define DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define DEBUGFI(...)  { log_printf(__VA_ARGS__); }
#define DEBUGS(s)  { Serial.println(s); }

#define KEEPALIVE_INTERVAL 2500    // Marlin defaults to 2 seconds, get a little of margin

const int MAX_MOUSE_OBSERVERS = 3;

struct DeviceError{  int errCode;  };

typedef etl::observer<const DeviceError&> DeviceObserver;

class GCodeDevice : public etl::observable<DeviceObserver, MAX_MOUSE_OBSERVERS> {
public:

    static GCodeDevice *getDevice();
    static void setDevice(GCodeDevice *dev);

    GCodeDevice(Stream & s): printerSerial(s) {}
    virtual ~GCodeDevice() {}

    virtual bool scheduleCommand(String cmd) = 0;
    virtual bool schedulePriorityCommand(String cmd) { scheduleCommand(cmd); } ;
    virtual bool canSchedule() = 0;

    virtual void sendCommands() = 0;
    virtual void receiveResponses() = 0;

    float getX() { return x; }
    float getY() { return y; }
    float getZ() { return z; }

protected:
    Stream & printerSerial;

    uint32_t serialReceiveTimeoutTimer;

    void resetWatchdog() {
        serialReceiveTimeoutTimer = millis() + KEEPALIVE_INTERVAL;
    };

    float x,y,z;

private:
    static GCodeDevice *device;

};



class GrblDevice : public GCodeDevice {
public:

    GrblDevice(Stream & s): GCodeDevice(s) {};

    virtual ~GrblDevice() {}

    virtual bool scheduleCommand(String cmd) {
        if(emgr) return false;
        return commandQueue.push(cmd);
    };

    virtual bool schedulePriorityCommand(String cmd) { 
        if(emgr) return false;
        return immediateQueue.push(cmd);
    } ;

    virtual bool canSchedule() {
        return commandQueue.getFreeSlots();
    }

    virtual void sendCommands() {
        if(emgr) return;
        String command;
        if(immediateQueue.hasUnsent() ) {
            command = immediateQueue.markSent();
            printerSerial.print(command);
            printerSerial.print("\n");
            resetWatchdog();
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
                printerSerial.print(command);  
                printerSerial.print("\n");
                resetWatchdog();
            }
        }
    };

    virtual void receiveResponses() {

        static int lineStartPos = 0;
        static String resp;

        while (printerSerial.available()) {
            char ch = (char)printerSerial.read();
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
                    DeviceError err = { 1 };
                    notify_observers(err); emgr=true;
                } else
                if ( resp.startsWith("<") ) {
                    parseGrblStatus(resp);
                }
                DEBUGF("free space: %4d rx: %s\n", commandQueue.getRemoteFreeSpace(), resp.c_str() );
                resp = "";
            }
        }

    };
    
private:
    CommandQueue<16, 100> commandQueue;
    CommandQueue<3, 0> immediateQueue;

    bool emgr = false;
    
    String lastReceivedResponse;

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
            x = v.substring(5, p2).toFloat();
            v = v.substring(p2+1);
            p2 = v.indexOf(',');
            y = v.substring(0, p2).toFloat();
            v = v.substring(p2+1);
            p2 = v.indexOf('|');
            z = v.substring(0, p2).toFloat();
        }
    }



};