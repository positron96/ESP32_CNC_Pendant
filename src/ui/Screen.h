#pragma once

#include <Arduino.h>

#include <U8g2lib.h>

#include "../devices/GCodeDevice.h"
#include "../InetServer.h"
#include "../Job.h"


enum class Button {
    ENC_UP, ENC_DOWN, BT1, BT2, BT3
};


class Screen {
public:
    static U8G2 &u8g2;
    static bool buttonPressed[3];
    static int encVal;
    static int potVal[2];
    static const int STATUS_BAR_HEIGHT = 8;


    virtual void begin() {}

    virtual void loop() {}

    void draw() {
        u8g2.clearBuffer();
        drawStatusBar();
        drawContents();
        u8g2.sendBuffer();
    }

    void processInput() {
        processEnc();
        processButtons();
        processPot();
    }

    static void drawStatusBar() {

        u8g2.setFont(u8g2_font_5x8_tr);

        // device status
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) u8g2.drawGlyph(0,0, '?');
        else {
            const char s = dev->getDescrption().charAt(0);
            //u8g2.setF
            u8g2.drawGlyph(0,0, s );
        }

        // job
        Job *job = Job::getJob();
        char str[20];
        if(job->isRunning() ) {
            snprintf(str, 20, ">%.1f%%", job->getPercentage() );
            if(job->isPaused() ) {
                str[0] = ' ';
            }
        }
        u8g2.drawStr(50, 0, str);
        

        // download
        WebServer *ws = WebServer::getWebServer();
        if(ws!=nullptr) {
            if(ws->isDownloading() ) u8g2.drawGlyph(10, 0, 'W');
            else u8g2.drawGlyph(10,0, 'w');
        }


        // line
        u8g2.drawHLine(0, STATUS_BAR_HEIGHT, u8g2.getWidth() );
    }

protected:

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

    
};