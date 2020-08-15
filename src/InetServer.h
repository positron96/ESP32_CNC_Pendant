#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>  
#include <AsyncTCP.h>
#include <ArduinoJson.h>   // for implementing a subset of the OctoPrint API

#include <etl/observer.h>
#include <etl/set.h>

struct WebServerStatusEvent { int statusField; };

typedef etl::observer<const WebServerStatusEvent&> WebServerObserver;

class WebServer : public etl::observable<WebServerObserver, 3> {
public:
    WebServer(uint16_t port=80): server(port), telnetServer(23), port(port) {
        inst = this;
    }

    ~WebServer() { clear_observers(); }

    void config(JsonObjectConst cfg = JsonObjectConst() );

    void begin();

    void stop();

    static WebServer * getWebServer() { return inst; }

    bool isDownloading() { return downloading; }

    bool isRunning() { return running; }

    void resendDeviceResponse(const char*, size_t);

private:

    static WebServer * inst;

    AsyncWebServer server;
    AsyncServer telnetServer;
    String essid, password;
    uint16_t port;
    String hostname;

    String uploadedFilePath;
    size_t uploadedFileSize;
    //String localUrlBase;
    bool downloading;
    bool running;

    etl::set<AsyncClient*, 5> telnetClients;
    
    void registerOptoPrintApi() ;
    
    /** Retuns path in a form of /dir1/dir2 (leaading slash, no trailing slash). */
    static String extractPath(String sdir, int prefixLen);

    void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

    void registerWebBrowser() ;

    int apiJobHandler(JsonObject &root);

};