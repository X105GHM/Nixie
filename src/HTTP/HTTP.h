#ifndef HTTP_HANDLER_H
#define HTTP_HANDLER_H

extern SemaphoreHandle_t gongSemaphore;
extern bool displayEnabled;
extern float HSS_V;
extern String melodyID;

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
