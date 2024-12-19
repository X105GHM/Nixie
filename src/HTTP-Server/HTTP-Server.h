#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <WebServer.h>
#include <WiFiManager.h>

class HTTP_Server {
public:
    HTTP_Server();
    void begin();
    void handleClient();
    int32_t getDigits() const;
    bool isDisplayEnabled() const;
    bool isBuzzerActive() const;

private:
    WebServer server;
    String priceInput;
    int32_t digits;
    bool displayEnabled;
    bool buzzerActive;

    void handleRoot();
    void handleSetPrice();
    void handleToggleDisplay();
    void handleBuzzer();
};

#endif
