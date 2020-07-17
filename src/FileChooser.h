#pragma once

#include <Arduino.h>
#include <SD.h>
#include <functional>
#include <etl/vector.h>

#include <U8g2lib.h>

#define FC_DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define FC_DEBUGFI(...)  { log_printf(__VA_ARGS__); }
#define FC_DEBUGS(s)  { Serial.println(s); }

enum class Button {
    ENC_UP, ENC_DOWN, BT1, BT2, BT3
};

class FileChooser {
public:

    void begin() {
        //const char* t = cDir.name();
        //FC_DEBUGF("loadDirContents: cdir is %s\n", t);
        loadDirContents(SD.open("/"), 0 );
    }

    void buttonPressed(Button bt) {
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
                    FC_DEBUGF("cdir is %s, file is %s\n", cDir.name(), file.c_str() );
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
                    FC_DEBUGF("moving up from %s\n", newPath.c_str() );
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
        FC_DEBUGFI("leaving bt, selLine is %d\n", selLine);
        repaintNeeded = true;
    }

    

    void loop() {

    }

    void draw(U8G2 & u8g2) {
        if(!repaintNeeded) return;

        u8g2.clearBuffer();

        u8g2.setDrawColor(1);
        const char* t = cDir.name();
        u8g2.drawStr(1, 0, t ); 
        u8g2.drawLine(0, 9, u8g2.getWidth(), 9);

        const int nLines = min(MAX_FILES, maxLines-topLine);
        for(int i=0; i<nLines; i++) {
            if(i+topLine == selLine) {
                u8g2.setDrawColor( 1 );
                u8g2.drawBox(0, 10-1+i*10, u8g2.getWidth(), 10);
                u8g2.setDrawColor( 0 );
            } else u8g2.setDrawColor( 1 );
            
            u8g2.drawStr(1, 10 + i*10, files[i].c_str() ); 
        }

        u8g2.sendBuffer();
        repaintNeeded = false;
    }

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
                FC_DEBUGF("loadDirContents: file %s\n", files[i-startingIndex].c_str() );
            }
            i++;
            file.close();
        }

        maxLines = i;
        FC_DEBUGF("loadDirContents: file count %d\n", maxLines );
        repaintNeeded = true;
    }

};