#pragma once

#include <Arduino.h>

#include <U8g2lib.h>

#include <etl/vector.h>

#include "../devices/GCodeDevice.h"
#include "../InetServer.h"
#include "../Job.h"

#define S_DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define S_DEBUGFI(...)  { log_printf(__VA_ARGS__); }
#define S_DEBUGS(s)  { Serial.println(s); }

enum class Button {
    ENC_UP, ENC_DOWN, BT1, BT2, BT3
};


class Screen : public JobObserver, public DeviceObserver {
public:
    static U8G2 &u8g2;
    static bool buttonPressed[3];
    static int encVal;
    static int potVal[2];
    static const int STATUS_BAR_HEIGHT = 9;

    void setDirty(bool fdirty=true) { dirty=fdirty; }

    void notification(JobStatusEvent e) override {
        setDirty();
    }
    void notification(const DeviceStatusEvent &e) override {
        setDirty();
    }

    virtual void begin() { dirty=true; }

    virtual void loop() {}

    void draw();

    void processInput();

    static void drawStatusBar();

protected:

    bool dirty;

    int selMenuItem=0;

    etl::vector<String, 10> menuItems;

    virtual void drawContents() = 0;

    virtual void onButtonPressed(Button bt) {};

    virtual void onPotValueChanged(int pot, int val) {};

    //virtual etl::ivector<String> & getMenuItems() = 0;
    virtual void onMenuItemSelected(uint8_t item) {};

private:

    void processEnc();

    void processButtons();

    void processPot();   

    void drawMenu() ;

};