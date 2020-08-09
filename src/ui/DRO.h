#pragma once

#include <ArduinoJson.h> 
#include <etl/map.h>

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

    DRO() {}
    
    void begin() override {
        /*
        menuItems.push_back("oFile...");
        menuItems.push_back("xReset");
        menuItems.push_back("uUpdate");
        */
    };

    void loop() override {
        Screen::loop();
        if(millis()>nextRefresh) {
            nextRefresh = millis() + 500;
            GCodeDevice *dev = GCodeDevice::getDevice();
            if (dev!=nullptr) {
                dev->requestStatusUpdate();
                setDirty();
            }
        }
    }

/*
    void config(JsonObjectConst cfg) {
        for (JsonPairConst kv : cfg) {
            etl::map<String, String, 10> devMenu{};
            S_DEBUGF("Device menu %s\n", kv.key().c_str() );
            for (JsonPairConst cmd : kv.value().as<JsonObject>() ) {
                devMenu[ String(cmd.key().c_str()) ] = cmd.value().as<String>();
                S_DEBUGF(" %s=%s\n", cmd.key().c_str(), cmd.value().as<char*>()); 
                allMenuItems[ String( kv.key().c_str() )] = devMenu;
                if( devMenu.available()==0) break;
            }
            if( allMenuItems.available()==0) break;
        }

    }

    void setDevice(GCodeDevice *dev) {
        S_DEBUGF("setDevice %s\n", dev->getType() );
        String t = dev->getType();
        if( allMenuItems.find(t) == allMenuItems.end() ) {
            S_DEBUGF("Could not find type %s\n", t.c_str() );
            return;
        }
        devMenu = &allMenuItems[t];
        for(auto r: *devMenu) {
            S_DEBUGF("Adding menu %s\n", r.first.c_str() );
            menuItems.push_back(r.first);
            if(menuItems.available()==0) break;
        }
    } */

private:
    //etl::imap<String,String> * devMenu;

    //etl::map<String, etl::map<String,String, 10>, 3> allMenuItems;

protected:

    JogAxis cAxis;
    JogDist cDist;
    uint32_t nextRefresh;
    uint32_t lastJog;

    void drawAxis(char axis, float v, int y) {
        const int LEN=20;
        char str[LEN];
        
        snprintf(str, LEN, "%c% 8.3f", axis, v );
        Display::u8g2.drawStr(1, y, str );
        //u8g2.drawGlyph();
    }

    void drawContents() override {
        const int LEN = 20;
        char str[LEN];

        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) return;

        U8G2 &u8g2 = Display::u8g2;

        u8g2.setFont( u8g2_font_7x13B_tr );
        int y = Display::STATUS_BAR_HEIGHT+3, h=u8g2.getAscent()-u8g2.getDescent()+3;

        //u8g2.drawGlyph(0, y+h*(int)cAxis, '>' ); 

        u8g2.setDrawColor(1);
        u8g2.drawBox(0, y+h*(int)cAxis-1, 8, h);

        u8g2.setDrawColor(2);

        drawAxis('X', dev->getX(), y); y+=h;
        drawAxis('Y', dev->getY(), y); y+=h;
        drawAxis('Z', dev->getZ(), y); y+=h;        

        y+=5;
        u8g2.setFont( u8g2_font_nokiafc22_tr   );
        float m = distVal(cDist);
        snprintf(str, LEN, m<1 ? "x%.1f" : "x%.0f", m );
        u8g2.drawStr(0, y, str);
                
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

/*
    void onMenuItemSelected(MenuItem & item) override {
        
        setDirty(true);

        if(item==0) { 
            S_DEBUGF("Should open file selector here\n");
            // go to file manager
            return;
        }
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) return;
        if(item==1) { 
            dev->reset();
            return;
        }
        if(item==2) { 
            dev->requestStatusUpdate();
            return;
        }
        
        const char* cmd = devMenu->at( menuItems[item] ).c_str();

        const char *p1 = cmd, *p2 = strchr(cmd, '\n');
        while(p2!=nullptr) {
            dev->scheduleCommand(p1, p2-p1);
            p1 = p2+1;
            p2 = strchr(p1, '\n');
        }
        p2 = cmd + strlen(cmd);
        dev->scheduleCommand(p1, p2-p1);

    };
    */

    void onButtonPressed(Button bt, int8_t arg) override {
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) {
            S_DEBUGF("device is null\n");
            return;
        }
        switch(bt) {
            case Button::ENC_UP:
            case Button::ENC_DOWN: {
                float f=0;
                float d = distVal(cDist)*arg;
                if( lastJog!=0) { f = d / (millis()-lastJog) * 1000*60; };
                if(f<500) f=500;
                //S_DEBUGF("jog af %d, dt=%d ms, delta=%d\n", (int)f, millis()-lastJog, arg);
                bool r = dev->jog( (int)cAxis, d, (int)f );
                lastJog = millis();
                if(!r) S_DEBUGF("Could not schedule jog\n");
                setDirty();
                break;
            }
            default: break;
        }
    };


};