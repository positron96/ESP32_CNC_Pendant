#include "Job.h"

Job * Job::job;

void Job::setJob(Job* _job) { job = _job; }

Job * Job::getJob() { return job; }



void Job::loop() {
    if(!running || paused) return;

    GCodeDevice * dev = GCodeDevice::getDevice();

    if(dev->canSchedule()) {
        int rd;
        char cline[256];
        uint32_t len=0;
        while( gcodeFile.available() ) {
            rd = gcodeFile.read();
            if(rd=='\n' || rd=='\r') break;
            cline[len++] = rd;
        }        
        if(gcodeFile.available()==0) running = false;
        percentage = 1.0 - gcodeFile.available()*1.0/fileSize;

        cline[len]=0;

        if(len==0) return;

        String line(cline);
        DEBUGF("popped line '%s', len %d\n", line.c_str(), len );

        int pos = line.indexOf(';');
        if(pos!=-1) line = line.substring(0, pos);

        if(line.length()==0) return;

        dev->scheduleCommand(line);

    }
}