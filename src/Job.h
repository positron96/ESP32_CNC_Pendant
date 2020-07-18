#pragma once

#include <Arduino.h>
#include <SD.h>

#include "devices/GCodeDevice.h"

class Job : public DeviceObserver {

public:

    static Job* getJob();
    static void setJob(Job* job);

    ~Job() { if(gcodeFile) gcodeFile.close(); }

    void loop();

    void setFile(String file) { 
        if(gcodeFile) gcodeFile.close();

        gcodeFile = SD.open(file);
        if(gcodeFile) fileSize = gcodeFile.size();
        filePos = 0;
        running = false; 
    }

    void notification(const DeviceError& err)  {
        Serial.println("ERR!!!");
        running = false;
    }

    void start() { startTime = millis();  running = true;  }
    void cancel() { running = false; if(gcodeFile) gcodeFile.close();  }
    bool isRunning() {  return running; }

    void pause() { paused = false; }
    void resume() { paused = true; }
    void setPaused(bool v) { paused = v; }
    bool isPaused() { return paused; }

    float getPercentage() { if(isValid()) return 1.0 * filePos/fileSize; else return 0; }
    size_t getFilePos() { if(isValid()) return filePos; else return 0;}
    size_t getFileSize() { if(isValid()) return fileSize; else return 0;}
    bool isValid() { return (bool)gcodeFile; }
    String getFilename() { if(isValid()) return gcodeFile.name(); else return ""; }
    uint32_t getPrintDuration() { return millis()-startTime; }

private:

    File gcodeFile;
    uint32_t fileSize;
    uint32_t filePos;
    uint32_t startTime;

    //float percentage = 0;
    bool running;
    bool paused;


    static Job job;

};