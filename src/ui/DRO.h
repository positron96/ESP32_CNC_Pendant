#pragma once

#include "Screen.h"

#include "../devices/GCodeDevice.h"
#include "../Job.h"



enum class JogAxis {
    X,Y,Z
};
enum class JogDist {
    _01, _1, _10
};


char axisChar(const JogAxis &a) {
    switch(a) {
        case JogAxis::X : return 'X';
        case JogAxis::Y : return 'Y';
        case JogAxis::Z : return 'Z';
    }
    log_printf("Unknown axis\n");
    return 0;
}


float distVal(const JogDist &a) {
    switch(a) {
        case JogDist::_01: return 0.1;
        case JogDist::_1: return 1;
        case JogDist::_10: return 10;
    }
    log_printf("Unknown multiplier\n");
    return 1;
}



class DRO: public Screen {
public:
    
    //virtual void begin() {};

    /*void loop() override {
        if(millis()>nextRefresh) {
            nextRefresh = millis() + 500;
            setDirty();
        }
    };*/

private:

    JogAxis cAxis;
    JogDist cDist;
    uint32_t nextRefresh;
    uint32_t lastJog;

protected:

    void drawAxis(char axis, float v, int y) {
        const int LEN=20;
        char str[LEN];
        
        snprintf(str, LEN, "%c% 8.3f", axis, v );
        u8g2.drawStr(0, y, str );
        //u8g2.drawGlyph();
    }

    void drawContents() override {
        const int LEN = 20;
        char str[LEN];

        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) return;

        u8g2.setFont( u8g2_font_7x13B_tr );
        int y = STATUS_BAR_HEIGHT+3, h=u8g2.getAscent()-u8g2.getDescent()+3;

        //u8g2.drawGlyph(0, y+h*(int)cAxis, '>' ); 

        u8g2.setDrawColor(1);
        u8g2.drawBox(0, y+h*(int)cAxis-1, 6, h);

        u8g2.setDrawColor(2);

        drawAxis('X', dev->getX(), y); y+=h;
        drawAxis('Y', dev->getY(), y); y+=h;
        drawAxis('Z', dev->getZ(), y); y+=h;        

        y+=5;
        u8g2.setFont( u8g2_font_nokiafc22_tr   );
        float m = distVal(cDist);
        snprintf(str, LEN, m<1 ? "x%.1f" : "x%.0f", m );
        u8g2.drawStr(0, y, str);
        

        //Job *job = Job::getJob();
        //u8g2.drawStr(0, 40, job->isPaused() ? "P" : "");
        
        //snprintf(str, 100, "%d%%", int(job->getPercentage()*100) );
        //u8g2.drawStr(10, 40, str);
        
    };

    void onPotValueChanged(int pot, int v) override {
        //  center lines : 2660    3480    4095
        // borders:            3000    3700
        // v1:    0     250     500
        //          125    375
        const static int MX = 1200;
        const static int b1 = MX/4;
        const static int b2 = b1*3;   
        const static int d=60;
        bool ch=false;
        if(pot==0) {
            if( cAxis==JogAxis::X && v>b1+d) {cAxis=JogAxis::Y; ch=true;}
            if( cAxis==JogAxis::Y && v>b2+d) {cAxis=JogAxis::Z; ch=true;}
            if( cAxis==JogAxis::Z && v<b2-d) {cAxis=JogAxis::Y; ch=true;}
            if( cAxis==JogAxis::Y && v<b1-d) {cAxis=JogAxis::X; ch=true;}
            if(ch) lastJog=0;
        } else
        if(pot==1) {

            // centers:      950    1620     2420
            // borders:         1300    2000            
            if( cDist==JogDist::_01 && v>b1+d) {cDist=JogDist::_1; ch=true;}
            if( cDist==JogDist::_1  && v>b2+d) {cDist=JogDist::_10; ch=true;}
            if( cDist==JogDist::_10 && v<b2-d) {cDist=JogDist::_1; ch=true;}
            if( cDist==JogDist::_1  && v<b1-d) {cDist=JogDist::_01; ch=true;}
        }
        if(ch) {
            //S_DEBUGF("changed pot: axis:%d dist:%d, pot%d=%d\n", (int)cAxis, (int)cDist, pot, v);
            setDirty();
        } 
    }

    virtual void onButtonPressed(Button bt) override {
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) {
            S_DEBUGF("device is null\n");
            return;
        }
        if(bt == Button::ENC_UP || bt==Button::ENC_DOWN) {
            float f=0;
            float d = distVal(cDist);
            if( lastJog!=0) { f = d / (millis()-lastJog) * 1000*60; };
            if(f<500) f=500;
            S_DEBUGF("jog af %d\n", (int)f);
            bool r = dev->jog( (int)cAxis, (bt==Button::ENC_DOWN ? 1 : -1) * d, (int)f );
            lastJog = millis();
            //schedulePriorityCommand("$J=G91 F100 "+axisStr(cAxis)+(dx>0?"":"-")+distStr(cDist) );
            if(!r) S_DEBUGF("Could not schedule jog\n");
            setDirty();
        }
    };


};