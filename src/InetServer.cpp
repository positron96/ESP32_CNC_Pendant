#include "InetServer.h"

#include <ESPmDNS.h>
#include <SD.h>
#include <WiFi.h>
#include <AsyncJson.h>

#include "Job.h"

#define API_VERSION     "0.1"
#define SKETCH_VERSION  "0.0.1"


WebServer* WebServer::inst = nullptr;


void WebServer::begin() {

    /*
    File cfg = SD.open("/config.txt");
    String essid = cfg.readStringUntil('\n'); essid.trim();
    String pass = cfg.readStringUntil('\n'); pass.trim();
    cfg.close();
    WiFi.begin(essid.c_str(), pass.c_str() );
    */

    WiFi.begin("***REMOVED***", "***REMOVED***");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    MDNS.begin("cncpendant");
    MDNS.setInstanceName("cncpendantinstance");

    // OctoPrint API
    // Unfortunately, Slic3r doesn't seem to recognize it
    MDNS.addService("octoprint", "tcp", port);
    MDNS.addServiceTxt("octoprint", "tcp", "path", "/");
    MDNS.addServiceTxt("octoprint", "tcp", "api", API_VERSION);
    MDNS.addServiceTxt("octoprint", "tcp", "version", SKETCH_VERSION);

    MDNS.addService("http", "tcp", port);
    MDNS.addServiceTxt("http", "tcp", "path", "/");
    MDNS.addServiceTxt("http", "tcp", "api", API_VERSION);
    MDNS.addServiceTxt("http", "tcp", "version", SKETCH_VERSION);

    registerOptoPrintApi();
    registerWebBrowser();

    server.begin();
    running = true;

}

void WebServer::stop() {
    server.end();
    running = false;
}

inline String stringify(bool value) {
  return value ? "true" : "false";
}

String getStateText(Job * job = nullptr, MarlinDevice * dev = nullptr) {
    if(job==nullptr) job=Job::getJob();
    if(dev==nullptr) dev=static_cast<MarlinDevice*>( GCodeDevice::getDevice() );
    if(dev==nullptr) return "Discovering";
    if(dev->isInPanic()) return "Error";
    if(!dev->isConnected() ) return "Offline";
    if(job->isPaused()) return "Paused";
    if(job->isRunning()) return "Printing";
    if(job->isCancelled()) {if(dev->getSentQueueLength()==0) return "Cancelled"; else return "Cancelling";}
    return "Operational";
}

void WebServer::registerOptoPrintApi() {
    
    server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest * request) {
        // https://docs.octoprint.org/en/master/api/general.html#post--api-login
        // https://github.com/fieldOfView/Cura-OctoPrintPlugin/issues/155#issuecomment-596109663
        Serial.printf("/api/login");
        request->send(200, "application/json", "{}");  
    });

    server.on("/api/version", HTTP_GET, [](AsyncWebServerRequest * request) {
        Serial.printf("/api/version");
        // http://docs.octoprint.org/en/master/api/version.html
        request->send(200, "application/json", "{\r\n"
                "  \"api\": \"" API_VERSION "\",\r\n"
                "  \"server\": \"" SKETCH_VERSION "\"\r\n"
                "}");  
    });

    server.on("/api/connection", HTTP_GET, [](AsyncWebServerRequest * request) {
        Serial.printf("/api/connection");
        // http://docs.octoprint.org/en/master/api/connection.html#get-connection-settings
        String baudsString; baudsString.reserve(50);
        for(int i=0; i<DeviceDetector::N_SERIAL_BAUDS; i++) {
            if(i!=0) baudsString += ", ";
            baudsString += String(DeviceDetector::serialBauds[i]);
        }
        request->send(200, "application/json", "{\r\n"
                "  \"current\": {\r\n"
                "    \"state\": \"" + getStateText() + "\",\r\n"
                "    \"port\": \"Serial\",\r\n"
                "    \"baudrate\": " + DeviceDetector::serialBaud + ",\r\n"
                "    \"printerProfile\": \"Default\"\r\n"
                "  },\r\n"
                "  \"options\": {\r\n"
                "    \"ports\": \"Serial\",\r\n"
                "    \"baudrates\": ["+baudsString+"],\r\n"
                "    \"printerProfiles\": \"Default\",\r\n"
                "    \"portPreference\": \"Serial\",\r\n"
                "    \"baudratePreference\": 115200,\r\n"
                "    \"printerProfilePreference\": \"Default\",\r\n"
                "    \"autoconnect\": true\r\n"
                "  }\r\n"
                "}");
    });


    // File Operations
    // Pending: http://docs.octoprint.org/en/master/api/files.html#retrieve-all-files
    server.on("/api/files", HTTP_GET, [](AsyncWebServerRequest * request) {
        Serial.printf("/api/files");
        request->send(200, "application/json", "{\r\n"
                "  \"files\": ["
                "    { \"name\": \"whistle_.gco\", \"path\":\"whistle_.gco\" }\r\n"
                "  ]\r\n"
                "}");
    });

    static const int filesPrefixLen = strlen("/api/files/local");

    // only handle fle uploads
    server.on("/api/files/local", HTTP_POST, [this](AsyncWebServerRequest * request) {
        Serial.printf("POST %s\n", request->url().c_str() );

        //if( request->hasHeader("Content-Type") ) Serial.println(request->getHeader("Content-Type")->value() );

        if(request->hasParam("select", true) && request->getParam("select", true)->value()=="true") {
            Job *job = Job::getJob();
            job->setFile(uploadedFilePath);
        }
        if(request->hasParam("print", true) && request->getParam("print", true)->value()=="true") { 
            Job *job = Job::getJob();
            job->start();
        } // print now


        // OctoPrint sends 201 here; https://github.com/fieldOfView/Cura-OctoPrintPlugin/issues/155#issuecomment-596110996
        AsyncWebServerResponse *response = request->beginResponse(201, "application/json", "{\r\n"
                "  \"files\": {\r\n"
                "    \"local\": {\r\n"
                "      \"name\": \"" + uploadedFilePath + "\",\r\n"
                "      \"origin\": \"local\"\r\n"
                "    }\r\n"
                "  },\r\n"
                "  \"done\": true\r\n"
                "}");
        response->addHeader("Location", "http://"+request->host()+"/api/files/local"+uploadedFilePath);
        request->send(response);

    }, [this](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        //Serial.printf("FILE %s file %s\n", req->url().c_str(), filename.c_str() );
        if(index==0) {
            int pos = filename.lastIndexOf("/");
            filename = pos == -1 ? "/" + filename : filename.substring(pos);
        }
        handleUpload(req, filename, index, data, len, final);
    } ).setFilter([](AsyncWebServerRequest *req){ return req->url().length()==filesPrefixLen; } );


    // handle /api/files/local/filename.cgode in a separate handler for clarity
    AsyncCallbackJsonWebHandler* fileOp = new AsyncCallbackJsonWebHandler("/api/files/local", 
            [this](AsyncWebServerRequest *req, JsonVariant &json) {
            JsonObject doc = json.as<JsonObject>();
            String file = extractPath(req->url(), filesPrefixLen);
            Serial.printf("JSON %s, file is %s\n", req->url().c_str(), file.c_str() );
            if( doc["command"] == "select" ) {
                Job *job = Job::getJob();
                job->setFile(file);
                if(doc["print"] == true) job->start();
                // fail with 409 if no printer
                req->send(204, "text/plain", "");
            } else {
                req->send(200, "text/plain", "Unsupported op");
            }
            
        }
    );    
    server.addHandler(fileOp).setFilter( [](AsyncWebServerRequest *req){ return req->url().length()>filesPrefixLen; } ); 
    

    server.on("/api/job", HTTP_GET, [this](AsyncWebServerRequest * request) {
        // http://docs.octoprint.org/en/master/api/job.html#retrieve-information-about-the-current-job
        //Serial.println("GET /api/job");
        
        Job *job = Job::getJob();
        if(job==nullptr) {//} || !job->isValid()) {
            request->send(500, "text/plain", "");
        }
        int32_t printTime=0, printTimeLeft = INT32_MAX;
        if (job->isRunning() ) {
            printTime = job->getPrintDuration() / 1000;
            float p = job->getPercentage();
            printTimeLeft = (p > 0) ? printTime / p * (1-p) : INT32_MAX;
        }
        
        request->send(200, "application/json", "{\r\n"
                "  \"job\": {\r\n"
                "    \"file\": {\r\n"
                "      \"name\": \"" + job->getFilename() + "\",\r\n"
                "      \"origin\": \"local\",\r\n"
                "      \"size\": " + String(job->getFileSize() ) + "\r\n"
                "    },\r\n"
                "    \"estimatedPrintTime\": \"" + String(printTime + printTimeLeft) + "\" \r\n"
                //"    \"filament\": {\r\n"
                //"      \"length\": \"" + filementLength + "\",\r\n"
                //"      \"volume\": \"" + filementVolume + "\"\r\n"
                //"    }\r\n"
                "  },\r\n"
                "  \"progress\": {\r\n"
                "    \"completion\": " + String(job->getPercentage()*100 ) + ",\r\n"
                "    \"filepos\": " + String(job->getFilePos()) + ",\r\n"
                //"    \"filepos\": 0,\r\n"
                "    \"printTime\": " + String(printTime) + ",\r\n"
                "    \"printTimeLeft\": " + String(printTimeLeft) + ",\r\n"
                "    \"printTimeLeftOrigin\": \"linear\"\r\n"
                "  },\r\n"
                "  \"state\": \"" + getStateText(job) + "\"\r\n"
                "}");
    });

    AsyncCallbackJsonWebHandler* jobHandler = new AsyncCallbackJsonWebHandler("/api/job", 
        [this](AsyncWebServerRequest *req, JsonVariant &json) {
            Serial.printf("JSON %s\n", req->url().c_str() );
            JsonObject doc = json.as<JsonObject>();        
            int responseCode = apiJobHandler(doc);
            req->send(responseCode, "text/plain", "");
        }
    );
    server.addHandler(jobHandler);

    // this handles API key check from Cura
    server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest * request) {
        // https://github.com/probonopd/WirelessPrinting/issues/30
        // https://github.com/probonopd/WirelessPrinting/issues/18#issuecomment-321927016
        request->send(200, "application/json", "{}");
    });


    server.on("/api/printer", HTTP_GET, [this](AsyncWebServerRequest * request) {
        //Serial.print("GET "); Serial.println(request->url() );
        // https://docs.octoprint.org/en/master/api/printer.html#retrieve-the-current-printer-state
        //String readyState = stringify(printerConnected);
        Job * job = Job::getJob();
        MarlinDevice * dev = static_cast<MarlinDevice*>(GCodeDevice::getDevice());
        bool connected = dev==nullptr ? false : dev->isConnected();
        bool queueEmpty = dev==nullptr ? true : dev->getSentQueueLength()==0;
        bool error = dev==nullptr ? false : dev->isInPanic();
        String readyState = stringify(connected);

        String message = "{\r\n"
                "  \"state\": {\r\n"
                "    \"text\": \"" + getStateText(job,dev) + "\",\r\n"
                "    \"sentQueueLength: " + (dev!=nullptr?dev->getSentQueueLength():0) + ",\r\n"
                "    \"queueLength: " + (dev!=nullptr?dev->getQueueLength():0) + ",\r\n"
                "    \"flags\": {\r\n"
                "      \"operational\": " + readyState + ",\r\n"
                "      \"paused\": " + stringify(job->isPaused()) + ",\r\n"
                "      \"printing\": " + stringify(job->isRunning()) + ",\r\n"
                "      \"pausing\": " + stringify(job->isPaused() && !queueEmpty ) + ",\r\n"
                "      \"cancelling\": " + stringify(job->isCancelled() && !queueEmpty ) + ",\r\n"
                "      \"sdReady\": false,\r\n"
                "      \"error\": " + stringify(error) + ",\r\n"
                "      \"ready\": " + readyState + ",\r\n"
                "      \"closedOrError\": " + stringify(!connected | error) + "\r\n"
                "    }\r\n"
                "  },\r\n";
                
        if(dev!=nullptr) {
            message +=
                "  \"temperature\": {\r\n";
            for (int t = 0; t < dev->getExtruderCount(); ++t) message += 
                "    \"tool" + String(t) + "\": {\r\n"
                "      \"actual\": " + String(dev->getExtruderTemp(t).actual) + ",\r\n"
                "      \"target\": " + String(dev->getExtruderTemp(t).target) + ",\r\n"
                "      \"offset\": 0\r\n"
                "    },\r\n";
        
            message +=  
                "    \"bed\": {\r\n"
                "      \"actual\": " + String(dev->getBedTemp().actual) + ",\r\n"
                "      \"target\": " + String(dev->getBedTemp().target) + ",\r\n"
                "      \"offset\": 0\r\n"
                "    }\r\n"
                "  },\r\n";
        }
        message +=    
                "  \"sd\": { \"ready\": false }\r\n"
                "}";
            
        request->send(200, "application/json", message);
    });

    // http://docs.octoprint.org/en/master/api/printer.html#send-an-arbitrary-command-to-the-printer
    AsyncCallbackJsonWebHandler* printerCommandHandler = new AsyncCallbackJsonWebHandler(
        "/api/printer/command", 
        [this](AsyncWebServerRequest *req, JsonVariant &json) {
            JsonObject doc = json.as<JsonObject>();        
            Serial.printf("JSON %s\n", req->url().c_str() );
            GCodeDevice * dev = GCodeDevice::getDevice();
            if(dev==nullptr) { req->send(400, "text/plain", "No printer"); return; }
            JsonArray commands = doc["commands"].as<JsonArray>();
            for (JsonVariant command : commands)
                dev->scheduleCommand(String(command.as<String>()));
            req->send(204, "text/plain", "");
        }
    );    
    server.addHandler(printerCommandHandler);

}


int WebServer::apiJobHandler(JsonObject &root) {
  const char* command = root["command"];
  Job *job = Job::getJob();

  if(job==nullptr)  { return 500; }

  if (command != NULL) {
    if (strcmp(command, "cancel") == 0) {
        if (!job->isRunning() )
            return 409;
        job->cancel();
    }
    else if (strcmp(command, "start") == 0) {
        if (job->isRunning() )
            return 409;
        if(!job->isValid() ) { job->setFile(uploadedFilePath); Serial.println("Starting empty job, selecting uploaded file"); }
        job->start();
    }
    else if (strcmp(command, "restart") == 0) {
        //if (!printPause)
        return 409;
        //restartPrint = true;
    }
    else if (strcmp(command, "pause") == 0) {
        if (!job->isRunning() )
            return 409;
        const char* action = root["action"];
        if (action == NULL || strcmp(action, "toggle") == 0)
            job->setPaused(!job->isPaused() );
        else if (strcmp(action, "pause") == 0)
            job->pause();
        else if (strcmp(action, "resume") == 0)
            job->resume();
    }
  }

  return 204;
}

/** Returns path in a form of /dir1/dir2 (leaading slash, no trailing slash). */
String WebServer::extractPath(String sdir, int prefixLen) {
    sdir = sdir.substring(prefixLen); // cut "/fs/"
    if(sdir.length()>1 && sdir.endsWith("/") ) sdir=sdir.substring(0, sdir.length()-1); // remove trailing slash
    if(sdir.charAt(0) != '/') sdir = '/'+sdir;
    return sdir;
}

void WebServer::handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    static File file;

    if (index==0) { // first chunk
        uploadedFilePath = filename;
        uploadedFileSize = 0;

        if (uploadedFilePath.length() > 255)//storageFS.getMaxPathLength())
            uploadedFilePath = "/cached.gco";   // TODO maybe a different solution

        Serial.printf("Uploading to file %s\n", filename.c_str() );

        if(SD.exists(uploadedFilePath)) SD.remove(uploadedFilePath);
        file = SD.open(uploadedFilePath, "w"); // create or truncate file
        if(!file) { request->send(400, "text/plain", "Could not open file"); return; }
        downloading = true;
    }

    if(!file) return;

    //Serial.printf("uploading pos %d if size %d to %s\n", index, len, uploadedFullname.c_str() );
    file.write(data, len);

    if (final) { // last chunk
        Serial.printf("uploaded\n");
        uploadedFileSize = index + len;
        file.close();
        downloading = false;
    }
}

void WebServer::registerWebBrowser() {
        server.onNotFound([](AsyncWebServerRequest * request) {
        //telnetSend("404 | Page '" + request->url() + "' not found");
        Serial.printf("Sending 404 to %s\n", request->url().c_str() );
        request->send(404, "text/plain", "Page not found!");
    });


    static const char fsPrefix[] = "/fs";
    static const char fsPrefixSlash[] = "/fs/";
    static const char fsPrefixLength = strlen(fsPrefixSlash);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
        Serial.printf("Requested /, redirecting to /fs\n" );
        request->redirect(fsPrefixSlash);
    });

    server.serveStatic(fsPrefix, SD, "/").setDefaultFile(""); // disable index.htm searching

    server.on(fsPrefix, HTTP_GET, [](AsyncWebServerRequest * request) {
        String sdir = extractPath(request->url(), fsPrefixLength );            
        
        Serial.println("listing dir "+sdir);
        File dir = SD.open(sdir);

        if(!dir || !dir.isDirectory()) { dir.close(); request->send(404, "text/plain", "No such file"); return; }

        String resp; resp.reserve(2048);
        resp += "<html><body>\n<h1>Listing of \""+sdir+"\"</h1>\n<form method='post' enctype='multipart/form-data'><input type='file' name='f'><input type='submit'></form>\n<ul>\n";

        if(sdir.length()>1) {
            int p=sdir.lastIndexOf('/'); 
            resp += "<li><a href=\"";
            resp += fsPrefixSlash;
            resp += sdir.substring(0,p);
            resp += "\">../</a></li>\n";
        }
        File f;
        while( f = dir.openNextFile() ) {
            String fname = f.name(); 
            int p = fname.lastIndexOf('/'); fname = fname.substring(p+1);
            if(f.isDirectory())
                resp += "<li><a href=\"/fs"+String(f.name())+"/\">"+fname+"</a></li>\n";
            else 
                resp += "<li><a href=\"/fs"+String(f.name())+"\">"+fname+"</a> "+f.size()+"B "+
                        +"[<a href=\"/api2/print?file="+String(f.name())+"\">print</a>]</li>\n";
            f.close();
        }
        dir.close();
        resp += "\n</ul>\n</body></html>";
        request->send(200, "text/html", resp);
    });
    
    server.on("/fs", HTTP_POST, [](AsyncWebServerRequest * request) {
        request->send(201, "text/html", "created");
    }, [this](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if(index==0) {
            String sdir = req->url();
            sdir = extractPath(sdir,fsPrefixLength);
            if(!sdir.endsWith("/") ) sdir += '/';
            filename = sdir + filename;
        }
        handleUpload(req, filename, index, data, len, final);
    } );

    server.on("/api2/print", HTTP_GET, [](AsyncWebServerRequest * req) {
        if(!req->hasParam("file")) {
            Serial.printf("GET %s\n", req->url().c_str() );
            req->send(400, "text/html", "no file paraameter");
            return;
        }
        String file = req->getParam("file")->value();
        Serial.printf("GET %s, file=%s\n", req->url().c_str(), file.c_str() );
        GCodeDevice *dev = GCodeDevice::getDevice();
        if(dev==nullptr) {
            req->send(409, "text/html", "no printer");
            return;
        }
        Job *job = Job::getJob();
        if(job->isValid() ) { 
            req->send(409, "text/html", "Job already set");
            return;
        }
        job->setFile(file);
        if(!job->isValid() ){ 
            req->send(400, "text/html", "File not found or invalid");
            return;
        }
        job->start();
        req->send(200, "text/html", "ok");
    } );

}
