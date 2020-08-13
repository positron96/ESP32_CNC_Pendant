#include "DRO.h"

class GrblDRO : public DRO {

public:
    void begin() override {
        DRO::begin();
        menuItems.push_back( MenuItem::simpleItem(0, 'o', [](MenuItem&){   }) );
        menuItems.push_back( MenuItem::simpleItem(1, 'x', [](MenuItem&){  GCodeDevice::getDevice()->reset(); }) );
        menuItems.push_back( MenuItem::simpleItem(2, 'u', [](MenuItem&){  GCodeDevice::getDevice()->requestStatusUpdate(); }) );

        menuItems.push_back( MenuItem::simpleItem(3, 'H', [](MenuItem&){  GCodeDevice::getDevice()->scheduleCommand("$H"); }) );
        menuItems.push_back( MenuItem::simpleItem(4, 'w', [](MenuItem&){  GCodeDevice::getDevice()->scheduleCommand("G10 L20 P1 X0Y0Z0"); GCodeDevice::getDevice()->scheduleCommand("G54"); }) );
        menuItems.push_back(MenuItem{5, 'L', true, false, nullptr,
          [](MenuItem&){  GCodeDevice::getDevice()->scheduleCommand("M3 S1"); },
          [](MenuItem&){  GCodeDevice::getDevice()->scheduleCommand("M5"); } 
        } );
    };

protected:
    

    void drawContents() override {
        const int LEN = 20;
        char str[LEN];

        GrblDevice *dev = static_cast<GrblDevice*>( GCodeDevice::getDevice() );
        if(dev==nullptr) return;

        U8G2 &u8g2 = Display::u8g2;

        u8g2.setFont( u8g2_font_7x13B_tr );
        int y = Display::STATUS_BAR_HEIGHT+2, h=u8g2.getAscent()-u8g2.getDescent()+2;

        //u8g2.drawGlyph(0, y+h*(int)cAxis, '>' ); 

        u8g2.setDrawColor(1);
        String &status = dev->getStatus();
        if(status == "Idle" || status=="Jogging")
            u8g2.drawBox(0, y+h*(int)cAxis-1, 8, h);
        else
            u8g2.drawFrame(0, y+h*(int)cAxis-1, 8, h);

        u8g2.setDrawColor(2);

        drawAxis('X', dev->getX()-dev->getXOfs(), y); y+=h;
        drawAxis('Y', dev->getY()-dev->getYOfs(), y); y+=h;
        drawAxis('Z', dev->getZ()-dev->getZOfs(), y); y+=h;

        u8g2.drawHLine(0, y-1, u8g2.getWidth() );
        if(dev->getXOfs()!=0 || dev->getYOfs()!=0 || dev->getZOfs()!=0 ) {
            drawAxis('x', dev->getX(), y); y+=h;
            drawAxis('y', dev->getY(), y); y+=h;
            drawAxis('z', dev->getZ(), y); y+=h; 
        } else { y += 3*h; }

        u8g2.drawHLine(0, y-1, u8g2.getWidth() );

        u8g2.setFont( u8g2_font_5x8_tr  );

        snprintf(str, LEN, "F%4d S%4d", dev->getFeed(), dev->getSpindleVal() );
        u8g2.drawStr(0, y, str);  y+=7;
        
        float m = distVal(cDist);
        snprintf(str, LEN, m<1 ? "%c x%.1f %s" : "%c x%.0f %s", axisChar(cAxis), m, status.c_str() );
        u8g2.drawStr(0, y, str);
                
    };
    
};