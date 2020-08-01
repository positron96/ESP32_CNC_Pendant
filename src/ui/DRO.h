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

    //virtual void loop() {};

private:

    JogAxis cAxis;
    JogDist cDist;

protected:

    void drawContents() override {
        const int LEN = 100;
        char str[LEN];

        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) return;

        u8g2.setFont( u8g2_font_6x12_tr );
        int y = STATUS_BAR_HEIGHT, h=u8g2.getAscent()-u8g2.getDescent();

        u8g2.drawGlyph(1, y+h*(int)cAxis, '>' ); 

        snprintf(str, LEN, "X: %.3f", dev->getX() );   u8g2.drawStr(10, y, str ); y+=h;
        snprintf(str, LEN, "Y: %.3f", dev->getY() );   u8g2.drawStr(10, y, str ); y+=h;
        snprintf(str, LEN, "Z: %.3f", dev->getZ() );   u8g2.drawStr(10, y, str ); y+=h;

        snprintf(str, LEN, "x%3f", distVal(cDist)  );  u8g2.drawStr(10, y, str);
        

        //Job *job = Job::getJob();
        //u8g2.drawStr(0, 40, job->isPaused() ? "P" : "");
        
        //snprintf(str, 100, "%d%%", int(job->getPercentage()*100) );
        //u8g2.drawStr(10, 40, str);
        
    };

    void onPotValueChanged(int pot, int v) override {
        //  center lines : 2660    3480    4095
        // borders:            3000    3700
        if(pot==0) {
            if( cAxis==JogAxis::X && v>3000+100) cAxis=JogAxis::Y;
            if( cAxis==JogAxis::Y && v>3700+100) cAxis=JogAxis::Z;
            if( cAxis==JogAxis::Z && v<3700-100) cAxis=JogAxis::Y;
            if( cAxis==JogAxis::Y && v<3000-100) cAxis=JogAxis::X;
        } else
        if(pot==1) {

            // centers:      950    1620     2420
            // borders:         1300    2000            
            if( cDist==JogDist::_01 && v>1300+100) cDist=JogDist::_1;
            if( cDist==JogDist::_1  && v>2000+100) cDist=JogDist::_10;
            if( cDist==JogDist::_10 && v<2000-100) cDist=JogDist::_1;
            if( cDist==JogDist::_1  && v<1300-100) cDist=JogDist::_01;
        }
        
    }

    virtual void onButtonPressed(Button bt) override {
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) {
            DEBUGF("device is null\n");
            return;
        }
        bool r = dev->jog( (int)cAxis, distVal(cDist) );
        //schedulePriorityCommand("$J=G91 F100 "+axisStr(cAxis)+(dx>0?"":"-")+distStr(cDist) );
        if(!r) DEBUGF("Could not schedule jog\n");
    };


};