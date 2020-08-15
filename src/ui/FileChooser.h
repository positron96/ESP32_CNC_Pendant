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
        loadDirContents(SD.open("/") );

        //menuItems.push_back("xClose");
        //menuItems.push_back("yOpen");
        //menuItems.push_back("^Up");
    }
    

    //void loop() override {    }

    void setCallback(const std::function<void(bool, String)> &cb) {
        returnCallback = cb;
    }

private:
    std::function<void(bool, String)> returnCallback;
    
    int selLine;
    int topLine;
    File cDir;
    static const size_t MAX_FILES = 50;
    static const size_t VISIBLE_FILES = 11;
    //String files[MAX_FILES];
    etl::vector<String, MAX_FILES> files;

    void loadDirContents(File newDir, int startingIndex=0) ;

    bool isGCode(const String &s);

protected:

    void drawContents() override;

    void onButtonPressed(Button bt, int8_t arg) override;

};