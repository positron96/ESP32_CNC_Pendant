#include "Screen.h"

bool Screen::buttonPressed[3] = {false};
int Screen::potVal[2] = {0};
int Screen::encVal = 0;


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
            float p = job->getPercentage();
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

    

    void Screen::draw() {
        if(!dirty) return;
        u8g2.clearBuffer();
        drawStatusBar();
        drawContents();

        char str[15]; sprintf(str, "%lu", millis() ); u8g2.drawStr(20,100, str);

        u8g2.sendBuffer();
        dirty = false;
    }

    void Screen::processInput() {
        processEnc();
        processButtons();
        processPot();
    } 