#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>  
#include <ArduinoJson.h>   // for implementing a subset of the OctoPrint API


class WebServer {
public:
    WebServer(uint16_t port=80): server(port) , port(port) {

    }

    void begin();

private:
    AsyncWebServer server;
    uint16_t port;

    String uploadedFilePath;
    size_t uploadedFileSize;
    //String localUrlBase;
    
    void registerOptoPrintApi() ;
    
    /** Retuns path in a form of /dir1/dir2 (leaading slash, no trailing slash). */
    static String extractPath(String sdir);

    void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

    void registerWebBrowser() ;

    int apiJobHandler(JsonObject &root);

};