#pragma once

#include <Arduino.h>
#include <etl/observer.h>
#include "CommandQueue.h"

#define GD_DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define GD_DEBUGS(s)  { Serial.println(s); }

#define KEEPALIVE_INTERVAL 2500    // Marlin defaults to 2 seconds, get a little of margin

const int MAX_MOUSE_OBSERVERS = 3;

struct DeviceError{  int errCode;  };

typedef etl::observer<const DeviceError&> DeviceObserver;

class GCodeDevice : public etl::observable<DeviceObserver, MAX_MOUSE_OBSERVERS> {
public:

    static GCodeDevice *getDevice();
    static void setDevice(GCodeDevice *dev);

    GCodeDevice(Stream & s): printerSerial(s), connected(true)  {}
    virtual ~GCodeDevice() {}

    virtual bool scheduleCommand(String cmd) = 0;
    virtual bool schedulePriorityCommand(String cmd) { return scheduleCommand(cmd); } ;
    virtual bool canSchedule() = 0;

    virtual bool jog(uint8_t axis, float dist, int feed=100)=0;

    virtual void loop() {
        sendCommands();
        receiveResponses();
        checkTimeout();
    }
    virtual void sendCommands() = 0;
    virtual void receiveResponses() = 0;

    float getX() { return x; }
    float getY() { return y; }
    float getZ() { return z; }

    bool isConnected() { return connected; }

    virtual void enableStatusUpdates(bool v) = 0;

    virtual void reset()=0;

    bool isInPanic() { return panic; }

protected:
    Stream & printerSerial;

    uint32_t serialRxTimeout;
    bool connected;

    void resetWatchdog() {
        serialRxTimeout = millis() + KEEPALIVE_INTERVAL;
    };

    void checkTimeout() {
        if (millis() > serialRxTimeout) connected = false;
    }

    float x,y,z;
    bool panic = false;

private:
    static GCodeDevice *device;

};



class GrblDevice : public GCodeDevice {
public:

    GrblDevice(Stream & s): GCodeDevice(s) {};

    virtual ~GrblDevice() {}

    virtual bool scheduleCommand(String cmd) {
        if(panic) return false;
        return commandQueue.push(cmd);
    };

    virtual bool schedulePriorityCommand(String cmd) { 
        if(panic) return false;
        return immediateQueue.push(cmd);
    } ;

    virtual bool canSchedule() {
        return commandQueue.getFreeSlots();
    }

    virtual bool jog(uint8_t axis, float dist, int feed) override {
        constexpr const char AXIS[] = {'X', 'Y', 'Z'};
        char msg[81]; snprintf(msg, 81, "$J=G91 F%d %c%04f", feed, AXIS[axis], dist);
        return schedulePriorityCommand(msg);
    }

    virtual void reset() {
        panic = false;
        commandQueue.clear();
        immediateQueue.clear();
        schedulePriorityCommand("$X");
    }

    virtual void sendCommands() {
        if(panic) return;
        String command;
        if(immediateQueue.hasUnsent() ) {
            command = immediateQueue.markSent();
            printerSerial.print(command);
            printerSerial.print("\n");
            resetWatchdog();
            GD_DEBUGF("Sent '%s', immediate\n", command.c_str());
            return;
        }
        command = commandQueue.peekSend();
        if (command != "") {
            String s = commandQueue.markSent();
            if(s=="") {
                GD_DEBUGF("Not sent, free space: %d\n", commandQueue.getRemoteFreeSpace());
            } else {
                GD_DEBUGF("Sent '%s', free space %d\n", command.c_str(), commandQueue.getRemoteFreeSpace());
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
                //bool incompleteResponse = false;
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
                    notify_observers(err); panic=true;
                } else
                if ( resp.startsWith("<") ) {
                    parseGrblStatus(resp);
                }
                GD_DEBUGF("free space: %4d rx: %s\n", commandQueue.getRemoteFreeSpace(), resp.c_str() );
                resp = "";
            }
        }

    };

    virtual void enableStatusUpdates(bool v) {
        if(v) nextPosRequestTime = millis();
        else nextPosRequestTime = 0;
    }

    virtual void loop() override {
        GCodeDevice::loop();
        if(nextPosRequestTime!=0 && millis() > nextPosRequestTime) {
            schedulePriorityCommand("?");
            nextPosRequestTime = millis() + 1000;
        }
    }
    
private:
    CommandQueue<16, 100> commandQueue;
    CommandQueue<3, 0> immediateQueue;

    uint32_t nextPosRequestTime;
    
    String lastReceivedResponse;

    void parseGrblStatus(String v) {
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



};