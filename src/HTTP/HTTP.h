#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

#include <WebServer.h>
#include "freertos/FreeRTOS.h"
#include "Time/Time.h"

class HTTPHandler {
private:
    WebServer server;
    void handleReset();
    void handleInfo();

public:
    HTTPHandler(int port = 80);
    void begin();
    void handleClient();
};

#endif // HTTP_HANDLER_H
