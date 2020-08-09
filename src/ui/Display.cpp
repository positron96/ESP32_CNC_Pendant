#include "Display.h"

#include "Screen.h"

#define D_DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define D_DEBUGFI(...)  { log_printf(__VA_ARGS__); }
#define D_DEBUGS(s)  { Serial.println(s); }

Display * Display::inst = nullptr;


bool Display::buttonPressed[3] = {false};
int Display::potVal[2] = {0};
int Display::encVal = 0;


    Display* Display::getDisplay() { return inst; }


    void Display::loop() {
        processInput();
        if(cScreen!=nullptr) cScreen->loop();
        draw();
    }

    void Display::processEnc() {
        static int lastEnc;
        if(encVal != lastEnc) {
            int8_t dx = (encVal - lastEnc);
            if(cScreen!=nullptr) cScreen->onButtonPressed(dx>0 ? Button::ENC_DOWN : Button::ENC_UP, dx);
        }
        lastEnc = encVal;
    }

    void Display::processButtons() {
        static bool lastButtPressed[3];
        static const Button buttons[] = {Button::BT1, Button::BT2, Button::BT3};
        if (cScreen == nullptr) return;
        for(int i=0; i<3; i++) {
            if(lastButtPressed[i] != buttonPressed[i]) {
                S_DEBUGF("button%d changed: %d\n", i, buttonPressed[i] );
                if(buttonPressed[i]) {
                    int menuLen = cScreen->menuItems.size();
                    if(menuLen!=0) {
                        if(i==0) { selMenuItem = selMenuItem>0 ? selMenuItem-1 : menuLen-1;  setDirty(); }
                        if(i==2) { selMenuItem = (selMenuItem+1) % menuLen; setDirty(); }
                        if(i==1) {
                            MenuItem& item = cScreen->menuItems[selMenuItem];
                            if(!item.togglalbe) { item.onCmd(item); }
                            else {
                                if(item.on) { item.offCmd(item); item.on=false; } else { item.onCmd(item); item.on=true; }
                            }
                        }
                        //cScreen->onMenuItemSelected(cScreen->menuItems[selMenuItem]);                    
                    } else {
                        cScreen->onButtonPressed(buttons[i], 1);
                    }
                }
                lastButtPressed[i] = buttonPressed[i];
            }
        }
    }

    void Display::processPot() {
        static int lastPotVal[2] = {0,0};
        
        for(int i=0; i<2; i++) {
            if(lastPotVal[i] != potVal[i]) {
                if(cScreen!=nullptr) cScreen->onPotValueChanged(i, potVal[i]);
                lastPotVal[i] = potVal[i];
            }
        }
    }

    void Display::drawStatusBar() {

        u8g2.setFont(u8g2_font_5x8_tr);

        char c;
        // device status
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) c='?';
        else {
            c = dev->getType().charAt(0);
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


    void Display::drawMenu() {
        if(cScreen==nullptr) return;
        u8g2.setFont(u8g2_font_5x8_tr);
        
        int len = cScreen->menuItems.size();

        int onscreenLen = len<6 ? len : 6;
        const int w=10;
        int y = u8g2.getHeight()-w;        
        u8g2.setFontMode(2);
        for(int i=0; i<onscreenLen; i++) {
            if(selMenuItem == i) {
                u8g2.drawBox(i*w, y, w, w);    
            } else {            
                u8g2.drawFrame(i*w, y, w, w);
            }
            MenuItem &item = cScreen->menuItems[i];
            uint16_t c = item.glyph;
            if(item.font!=nullptr) u8g2.setFont(item.font);
            u8g2.drawGlyph(i*w+2, y+1, c);
        }
    }
    

    void Display::draw() {
        if(!dirty) return;
        u8g2.clearBuffer();
        if(cScreen!=nullptr) cScreen->drawContents();
        drawStatusBar();
        drawMenu();

        char str[15]; sprintf(str, "%lu", millis() ); u8g2.drawStr(20,110, str);

        u8g2.sendBuffer();
        dirty = false;
    }

    void Display::processInput() {
        processEnc();
        processButtons();
        processPot();
    } 
