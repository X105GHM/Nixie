#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H
#include <WebServer.h>

extern SemaphoreHandle_t gongSemaphore;

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
