#include "Screen.h"

bool Screen::buttonPressed[3] = {false};
int Screen::potVal[2] = {0};
int Screen::encVal = 0;


    void Screen::drawStatusBar() {

        u8g2.setFont(u8g2_font_5x8_tr);

        // device status
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) u8g2.drawGlyph(0,0, '?');
        else {
            char s = dev->getDescrption().charAt(0);
            if(dev->isConnected() ) s=toupper(s); else s=tolower(s);
            if(dev->isInPanic() ) s='!';            
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
        int w = u8g2.getStrWidth(str);
        u8g2.drawStr(u8g2.getWidth()-w, 0, str);
        

        // download
        WebServer *ws = WebServer::getWebServer();
        if(ws!=nullptr) {
            if(ws->isDownloading() ) u8g2.drawGlyph(10, 0, 'W');
            else u8g2.drawGlyph(10,0, 'w');
        }


        // line
        u8g2.drawHLine(0, STATUS_BAR_HEIGHT, u8g2.getWidth() );
    }

    

    void Screen::draw() {
        if(!dirty) return;
        u8g2.clearBuffer();
        drawStatusBar();
        drawContents();

        char str[15]; sprintf(str, "%lu", millis() ); u8g2.drawStr(50,50, str);

        u8g2.sendBuffer();
        dirty = false;
    }

    void Screen::processInput() {
        processEnc();
        processButtons();
        processPot();
    }