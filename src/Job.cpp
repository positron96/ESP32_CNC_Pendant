#include "Job.h"

Job Job::job;

void Job::setJob(Job* _job) { job = *_job; }

Job * Job::getJob() { return &job; }

#define J_DEBUGF(...) // { Serial.printf(__VA_ARGS__); }
#define J_DEBUGS(s) // { Serial.println(s); }

void Job::readNextLine() {
    if(gcodeFile.available()==0) { 
        running = false; 
        return; 
    }
    while( gcodeFile.available()>0 ) {
        int rd = gcodeFile.read();
        filePos++;
        if(rd=='\n' || rd=='\r') {
            if(curLinePos!=0) break; // if it's an empty string of LF after last CR, just continue reading
        } else {
            if(curLinePos<MAX_LINE) curLine[curLinePos++] = rd;
            else { running = false; break; }
        }
    }
    curLine[curLinePos]=0;
}

void Job::loop() {
    if(!running || paused) return;

    GCodeDevice * dev = GCodeDevice::getDevice();
    if(dev==nullptr) return;

    if(curLinePos==0) readNextLine();
    if(!running) return;
    
    
    if(dev->canSchedule(curLinePos)) {

        J_DEBUGF("popped line '%s', len %d\n", curLine, curLinePos );

        char* pos = strchr(curLine, ';');
        if(pos!=NULL) *pos = 0; //line = line.substring(0, pos);

        dev->scheduleCommand(curLine, curLinePos);

        curLinePos = 0;

    }

}