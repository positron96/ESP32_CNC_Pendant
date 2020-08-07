#pragma once

#include "Screen.h"

#include <SD.h>
#include <functional>
#include <etl/vector.h>


class FileChooser: public Screen {
public:

    void begin() override {
        //const char* t = cDir.name();
        //FC_DEBUGF("loadDirContents: cdir is %s\n", t);
        loadDirContents(SD.open("/"), 0 );

        menuItems.push_back("xClose");
        menuItems.push_back("yOpen");
        menuItems.push_back("^Up");
    }
    

    //void loop() override {    }

    void setCallback(const std::function<void(bool, String)> &cb) {
        returnCallback = cb;
    }

private:
    std::function<void(bool, String)> returnCallback;
    bool repaintNeeded;
    int selLine;
    int maxLines;
    int topLine;
    File cDir;
    static const int MAX_FILES = 10;
    String files[MAX_FILES];

    void loadDirContents(File newDir, int startingIndex) {
        if(!newDir) return;
        //FC_DEBUGF("loadDirContents dir %s\n", newDir.name() );
        if( !cDir || strcmp(cDir.name(), newDir.name())!=0 ) {
            cDir = newDir;
            //FC_DEBUGF("loadDirContents opening new dir %s\n", cDir.name() );
        }
        topLine = startingIndex;
        File file;
        int i = 0;
        cDir.rewindDirectory();
        while ( file = cDir.openNextFile() ) {
            if(i>=startingIndex && i<startingIndex+MAX_FILES) {
                String name = file.name();
                int p = name.lastIndexOf('/');
                if(p>=0) name = name.substring(p+1);
                if(file.isDirectory() ) name += "/";
                files[i-startingIndex] = name;
                //if(i==MAX_FILES) break;
                S_DEBUGF("loadDirContents: file %s\n", files[i-startingIndex].c_str() );
            }
            i++;
            file.close();
        }

        maxLines = i;
        S_DEBUGF("loadDirContents: file count %d\n", maxLines );
        setDirty();
    }
protected:


    void drawContents() {

        //u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_5x8_tr);

        const char* t = cDir.name();
        int y = STATUS_BAR_HEIGHT;
        u8g2.drawStr(1, y, t ); 
        u8g2.drawHLine(0, y+9, u8g2.getWidth() );

        const int nLines = min(MAX_FILES, maxLines-topLine);
        for(int i=0; i<nLines; i++) {
            y = STATUS_BAR_HEIGHT+10 + i*10;
            if(i+topLine == selLine) {
                u8g2.setDrawColor( 1 );
                u8g2.drawBox(0, y-1, u8g2.getWidth(), 10);
                u8g2.setDrawColor( 0 );
            } else u8g2.setDrawColor( 1 );
            
            u8g2.drawStr(1, i, files[i].c_str() ); 
        }

    }


    void onButtonPressed(Button bt) override {
        switch(bt) {
            case Button::ENC_UP:
                selLine--;
                if(selLine<0) selLine = 0;
                break;
            case Button::ENC_DOWN:
                selLine++;
                if(selLine>=maxLines) selLine = maxLines-1;
                break;
            case Button::BT1: {
                String file = files[selLine-topLine];
                bool isDir = file.charAt(file.length()-1) == '/';
                if(isDir) {
                    file = file.substring(0, file.length()-1 );
                }
                String cDirName = cDir.name(); 
                if(cDirName.charAt(cDirName.length()-1) != '/' ) cDirName+="/";
                String newPath = cDirName+file;
                if(isDir) {
                    S_DEBUGF("cdir is %s, file is %s\n", cDir.name(), file.c_str() );
                    selLine = 0;
                    loadDirContents(SD.open(newPath), 0);
                } else {
                    if(returnCallback) returnCallback(true, newPath);
                }
                break;
            }
            case Button::BT2: {
                String newPath = cDir.name();
                if(newPath=="/") {
                    if(returnCallback) returnCallback(false, "");
                } else {
                    S_DEBUGF("moving up from %s\n", newPath.c_str() );
                    int p = newPath.lastIndexOf("/");
                    if(p==0) newPath="/"; else newPath = newPath.substring(0, p);
                    selLine = 0;
                    loadDirContents(SD.open(newPath), 0);
                }
                break;
            }
            default: 
                break;
        }
        S_DEBUGF("leaving bt, selLine is %d\n", selLine);
        setDirty();
    }

};