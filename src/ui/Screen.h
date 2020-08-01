#pragma once

#include <Arduino.h>

#include <U8g2lib.h>

#include "../devices/GCodeDevice.h"
#include "../InetServer.h"
#include "../Job.h"


enum class Button {
    ENC_UP, ENC_DOWN, BT1, BT2, BT3
};


class Screen : public JobObserver, public DeviceObserver {
public:
    static U8G2 &u8g2;
    static bool buttonPressed[3];
    static int encVal;
    static int potVal[2];
    static const int STATUS_BAR_HEIGHT = 8;

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

    virtual void drawContents() = 0;

    virtual void onButtonPressed(Button bt) {};

    virtual void onPotValueChanged(int pot, int val) {};

private:

    void processEnc() {
        static int lastEnc;
        if(encVal != lastEnc) {
            int8_t dx = (encVal - lastEnc);
            onButtonPressed(dx>0 ? Button::ENC_DOWN : Button::ENC_UP);
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
                    onButtonPressed(buttons[i]);
                }
                lastButtPressed[i] = buttonPressed[i];
            }
        }
    }

    void processPot() {
        static int lastPotVal[2] = {0,0};
        for(int i=0; i<2; i++) {
            if(lastPotVal[i] != potVal[i]) {
                onPotValueChanged(i, potVal[i]);
                lastPotVal[i] = potVal[i];
            }
        }
    }

    void drawMessageBox() {
        //u8g2.userInterfaceMessage
    }
    
};