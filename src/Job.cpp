#include "Job.h"

Job Job::job;

void Job::setJob(Job* _job) { job = *_job; }

Job * Job::getJob() { return &job; }

#define J_DEBUGF(...) // { Serial.printf(__VA_ARGS__); }
#define J_DEBUGS(s) // { Serial.println(s); }


void Job::loop() {
    if(!running || paused) return;

    GCodeDevice * dev = GCodeDevice::getDevice();
    if(dev==nullptr) return;

    if(dev->canSchedule(100)) {
        int rd;
        char cline[256];
        uint32_t len=0;
        while( gcodeFile.available() ) {
            rd = gcodeFile.read();
            if(rd=='\n' || rd=='\r') break;
            cline[len++] = rd;
        }        
        if(gcodeFile.available()==0) running = false;
        filePos = fileSize - gcodeFile.available();

        cline[len]=0;

        if(len==0) return;

        String line(cline);
        J_DEBUGF("popped line '%s', len %d\n", line.c_str(), len );

        int pos = line.indexOf(';');
        if(pos!=-1) line = line.substring(0, pos);

        if(line.length()==0) return;

        dev->scheduleCommand(line);

    }
}