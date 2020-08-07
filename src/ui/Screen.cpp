#include "Screen.h"

bool Screen::buttonPressed[3] = {false};
int Screen::potVal[2] = {0};
int Screen::encVal = 0;


    void Screen::processEnc() {
        static int lastEnc;
        if(encVal != lastEnc) {
            int8_t dx = (encVal - lastEnc);
            onButtonPressed(dx>0 ? Button::ENC_DOWN : Button::ENC_UP, dx);
        }
        lastEnc = encVal;
    }

    void Screen::processButtons() {
        static bool lastButtPressed[3];
        static const Button buttons[] = {Button::BT1, Button::BT2, Button::BT3};
        for(int i=0; i<3; i++) {
            if(lastButtPressed[i] != buttonPressed[i]) {
                S_DEBUGF("button%d changed: %d\n", i, buttonPressed[i] );
                if(buttonPressed[i]) {
                    if(i==0) if(selMenuItem>0) { selMenuItem--; setDirty(); }
                    if(i==2) if(selMenuItem<menuItems.size()-1 ) { selMenuItem++; setDirty(); }
                    if(i==1) onMenuItemSelected(selMenuItem);
                    //onButtonPressed(buttons[i]);
                }
                lastButtPressed[i] = buttonPressed[i];
            }
        }
    }

    void Screen::processPot() {
        static int lastPotVal[2] = {0,0};
        
        for(int i=0; i<2; i++) {
            if(lastPotVal[i] != potVal[i]) {
                onPotValueChanged(i, potVal[i]);
                lastPotVal[i] = potVal[i];
            }
        }
    }

    void Screen::drawStatusBar() {

        u8g2.setFont(u8g2_font_5x8_tr);

        char c;
        // device status
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) c='?';
        else {
            c = dev->getDescrption().charAt(0);
            if(dev->isConnected() ) c=toupper(c); else c=tolower(c);
            if(dev->isInPanic() ) c='!';            
        }
        u8g2.drawGlyph(0,0, c);

        // job status
        Job *job = Job::getJob();
        char str[20];
        if(job->isRunning() ) {
            float p = job->getCompletion()*100;
            if(p<10) snprintf(str, 20, " %.1f%%", p );
            else snprintf(str, 20, " %d%%", (int)p );
            if(job->isPaused() ) str[0] = '|';
        } else strncpy(str, " ---%", 20);
        int w = u8g2.getStrWidth(str);
        u8g2.drawStr(u8g2.getWidth()-w, 0, str);
        //S_DEBUGF("drawing '%s' len %d\n", str, strlen(str) );
        

        // web
        WebServer *ws = WebServer::getWebServer();
        if(ws!=nullptr) {
            char c;
            if(ws->isDownloading() ) c='W';
            else if(ws->isRunning() ) c = 'w'; else c='.';
            u8g2.drawGlyph(5,0, c);
        }


        // line
        u8g2.drawHLine(0, STATUS_BAR_HEIGHT-1, u8g2.getWidth() );
    }


    void Screen::drawMenu() {
        int len = menuItems.size();
        int onscreenLen = len<5 ? len : 5;
        const int w=10;
        int y = u8g2.getHeight()-w;        
        u8g2.setFontMode(2);
        for(int i=0; i<onscreenLen; i++) {
            if(selMenuItem == i) {
                u8g2.drawBox(i*w, y, w, w);    
            } else {            
                u8g2.drawFrame(i*w, y, w, w);
            }
            uint16_t c = menuItems[i].charAt(0);
            u8g2.drawGlyph(i*w+2, y+1, c);
        }
    }
    

    void Screen::draw() {
        if(!dirty) return;
        u8g2.clearBuffer();
        drawContents();
        drawStatusBar();
        drawMenu();

        char str[15]; sprintf(str, "%lu", millis() ); u8g2.drawStr(20,100, str);

        u8g2.sendBuffer();
        dirty = false;
    }

    void Screen::processInput() {
        processEnc();
        processButtons();
        processPot();
    } 

