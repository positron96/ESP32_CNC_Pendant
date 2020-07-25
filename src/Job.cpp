#include "Job.h"

Job Job::job;

void Job::setJob(Job* _job) { job = *_job; }

Job * Job::getJob() { return &job; }

#define J_DEBUGF(...)  { Serial.printf(__VA_ARGS__); }
#define J_DEBUGS(s)    { Serial.println(s); }

void Job::readNextLine() {
    if(gcodeFile.available()==0) { 
        cancel();
        return; 
    }
    while( gcodeFile.available()>0 ) {
        int rd = gcodeFile.read();
        filePos++;
        if(rd=='\n' || rd=='\r') {
            if(curLinePos!=0) break; // if it's an empty string or LF after last CR, just continue reading
        } else {
            if(curLinePos<MAX_LINE) curLine[curLinePos++] = rd;
            else { 
                cancel(); 
                break; 
            }
        }
    }
    curLine[curLinePos]=0;
}

bool Job::scheduleNextCommand(GCodeDevice *dev) {
    
    if(curLinePos==0) readNextLine();
    if(!running) return false;    // don't run next time
    //assert(curLinePos != 0);

    if(dev->canSchedule(curLinePos)) {

        J_DEBUGF("  J queueing line '%s', len %d\n", curLine, curLinePos );

        char* pos = strchr(curLine, ';');
        if(pos!=NULL) {*pos = 0; curLinePos = pos-curLine; }

        if(curLinePos==0) { J_DEBUGS("  J zero-length line"); return true; } // can seek next

        dev->scheduleCommand(curLine, curLinePos);

        curLinePos = 0;
        return true; //can try next command

    } return false; // stop trying for now
}

void Job::loop() {
    if(!running || paused) return;

    GCodeDevice * dev = GCodeDevice::getDevice();
    if(dev==nullptr) return;

    while( scheduleNextCommand(dev) ) {}

}