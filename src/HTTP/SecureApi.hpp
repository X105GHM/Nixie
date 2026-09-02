#pragma once

#include <initializer_list>
#include <string>

#include "HTTP/NativeHttpServer.hpp"

struct cJSON;

class SecureApi final
{
public:
    explicit SecureApi(NativeHttpServer& server) noexcept : server_(server) {}

    void registerRoutes();

private:
    static constexpr size_t MAX_JSON_BODY_SIZE = 1024;

    void handleCommand();
    void handleWifiUpsert();
    void handleWifiRemove();
    void handleWifiErase();
    void handleHistory();
    void handleResetLog();

    cJSON* parseJsonRequest();
    bool validateCsrfRequest();
    bool invoke(
        const char* path,
        httpd_method_t registeredMethod,
        std::initializer_list<std::pair<std::string, std::string>> arguments = {});
    void sendError(int status, const char* code, const char* message);

    static bool hasOnlyFields(const cJSON* object, std::initializer_list<const char*> fields);
    static bool getRequiredString(
        const cJSON* object, const char* name, std::string& value, size_t minimum, size_t maximum);
    static bool getRequiredBool(const cJSON* object, const char* name, bool& value);
    static bool getOptionalBool(const cJSON* object, const char* name, bool& present, bool& value);
    static bool getRequiredInteger(
        const cJSON* object, const char* name, int64_t minimum, int64_t maximum, int64_t& value);
    static bool getOptionalInteger(
        const cJSON* object, const char* name, int64_t minimum, int64_t maximum,
        bool& present, int64_t& value);
    static bool isTime(const std::string& value, bool secondsRequired) noexcept;
    static bool isZip(const std::string& value) noexcept;
    static bool isSsid(const std::string& value) noexcept;
    static bool isWifiPassword(const std::string& value) noexcept;

    NativeHttpServer& server_;
};
