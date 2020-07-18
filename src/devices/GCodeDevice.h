#pragma once

#include <Arduino.h>
#include <etl/observer.h>
#include "CommandQueue.h"

#define GD_DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define GD_DEBUGS(s)  { Serial.println(s); }
#define GD_DEBUGLN GD_DEBUGS

#define KEEPALIVE_INTERVAL 2500    // Marlin defaults to 2 seconds, get a little of margin

const int MAX_MOUSE_OBSERVERS = 3;

struct DeviceError{  int errCode;  };

typedef etl::observer<const DeviceError&> DeviceObserver;

class GCodeDevice : public etl::observable<DeviceObserver, MAX_MOUSE_OBSERVERS> {
public:

    static GCodeDevice *getDevice();
    static void setDevice(GCodeDevice *dev);

    GCodeDevice(Stream & s): printerSerial(s), connected(true)  {}
    virtual ~GCodeDevice() { }

    virtual bool scheduleCommand(String cmd) {
        if(panic) return false;
        return queue->push(cmd);
    };
    virtual bool schedulePriorityCommand(String cmd) { 
        return queue->pushPriority(cmd);
    } ;
    virtual bool canSchedule() { return queue->getFreeSlots()>0; }

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

    virtual void reset()=0;

    bool isInPanic() { return panic; }

    virtual void enableStatusUpdates(bool v) {
        if(v) nextStatusRequestTime = millis();
        else nextStatusRequestTime = 0;
    }

    String getDescrption() { return desc; }

protected:
    Stream & printerSerial;

    uint32_t serialRxTimeout;
    bool connected;
    String desc;

    void armRxTimeout() {
        //GD_DEBUGLN(enable ? "GCodeDevice::resetRxTimeout enable" : "GCodeDevice::resetRxTimeout disable");
        serialRxTimeout = millis() + KEEPALIVE_INTERVAL;
    };
    void disarmRxTimeout() {        
        serialRxTimeout=0; 
    };
    void updateRxTimeout() {
        if(isRxTimeoutEnabled() ) { if(queue->allAcknowledged()) disarmRxTimeout(); else armRxTimeout(); }
    }

    bool isRxTimeoutEnabled() { return serialRxTimeout!=0; }

    void checkTimeout() {
        if( !isRxTimeoutEnabled() ) return;
        if (millis() > serialRxTimeout) { 
            connected = false; 
            GD_DEBUGLN("GCodeDevice::checkTimeout fired"); 
            cleanupQueue();
            disarmRxTimeout(); 
        }
    }

    virtual void cleanupQueue() { queue->clear(); }

    float x,y,z;
    bool panic = false;
    uint32_t nextStatusRequestTime;
    AbstractQueue *queue;

private:
    static GCodeDevice *device;

};



class GrblDevice : public GCodeDevice {
public:

    GrblDevice(Stream & s): GCodeDevice(s) { queue=&commandQueue; desc = "Grbl"; };

    virtual ~GrblDevice() {}

    virtual bool jog(uint8_t axis, float dist, int feed) override {
        constexpr const char AXIS[] = {'X', 'Y', 'Z'};
        char msg[81]; snprintf(msg, 81, "$J=G91 F%d %c%04f", feed, AXIS[axis], dist);
        return schedulePriorityCommand(msg);
    }

    virtual void reset() {
        panic = false;
        cleanupQueue();
        schedulePriorityCommand("$X");
    }

    virtual void sendCommands();

    virtual void receiveResponses();

    virtual void loop() override {
        GCodeDevice::loop();
        if(nextPosRequestTime!=0 && millis() > nextPosRequestTime) {
            schedulePriorityCommand("?");
            nextPosRequestTime = millis() + 1000;
        }
    }
    
private:
    DoubleCommandQueue<16, 100, 3> commandQueue;

    uint32_t nextPosRequestTime;
    
    String lastReceivedResponse;

    void parseGrblStatus(String v);

};


class MarlinDevice: public GCodeDevice {

public:

    MarlinDevice(Stream & s): GCodeDevice(s) { queue=&commandQueue; desc="Marlin"; }

    virtual ~MarlinDevice() {}

    virtual bool jog(uint8_t axis, float dist, int feed) override {
        constexpr const char AXIS[] = {'X', 'Y', 'Z', 'E'};
        char msg[81]; snprintf(msg, 81, "G91 G0 F%d %c%04f G90", feed, AXIS[axis], dist);
        return schedulePriorityCommand(msg);
    }

    virtual void reset() {        
        cleanupQueue();
        //schedulePriorityCommand("$X");
    }

    virtual void sendCommands();

    virtual void receiveResponses() ;

    virtual void loop() override {
        GCodeDevice::loop();
        if(nextStatusRequestTime!=0 && millis() > nextStatusRequestTime) {
            schedulePriorityCommand("M114");
            nextStatusRequestTime = millis() + 1000;
        }
    }

private:

    static const int MAX_SUPPORTED_EXTRUDERS = 3;

    struct Temperature {
        float actual;
        float target;
    };

    CommandQueue<16, 0> commandQueue;
    int fwExtruders;
    bool fwAutoreportTempCap, fwProgressCap, fwBuildPercentCap;
    bool autoreportTempEnabled;

    Temperature toolTemperature[MAX_SUPPORTED_EXTRUDERS];
    Temperature bedTemperature;
    String lastReceivedResponse;
    float ePos; ///< extruder pos

    bool parseTemperatures(const String &response);

    // Parse temperatures from printer responses like
    // ok T:32.8 /0.0 B:31.8 /0.0 T0:32.8 /0.0 @:0 B@:0
    bool parseTemp(const String &response, const String whichTemp, Temperature *temperature);
    
    // Parse position responses from printer like
    // X:-33.00 Y:-10.00 Z:5.00 E:37.95 Count X:-3300 Y:-1000 Z:2000
    bool parsePosition(const String &str);

    bool parseM115(const String &str);


    static float extractFloat(const String &str, const String key) ;
    
    static bool isFloat(const String value);

    // Parse temperatures from prusa firmare (sent when heating)
    // ok T:32.8 E:0 B:31.8
    static bool extractPrusaHeatingTemp(const String &response, const String whichTemp, float &temperature) ;

    static int extractPrusaHeatingExtruder(const String &response) ;

    static String extractM115String(const String &response, const String field) ;

    static bool extractM115Bool(const String &response, const String field, const bool onErrorValue = false);

};