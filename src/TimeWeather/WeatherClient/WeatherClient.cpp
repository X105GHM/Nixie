#include "WeatherClient.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <new>

#include "Logger/Logger.hpp"
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

namespace
{
constexpr size_t MAX_RESPONSE_SIZE = 4096;

bool isValidZip(const std::string& zip) noexcept
{
    return !zip.empty() && zip.size() <= 10 && std::all_of(zip.begin(), zip.end(), [](char value) { return value >= '0' && value <= '9'; });
}

bool isValidApiKey(const std::string& apiKey) noexcept
{
    return !apiKey.empty() && apiKey.size() <= 128 && std::all_of(apiKey.begin(), apiKey.end(), [](unsigned char value)
           {
               return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
                      (value >= '0' && value <= '9') || value == '-' || value == '_';
           });
}

template<size_t Size>
class SensitiveBuffer final
{
public:
    ~SensitiveBuffer()
    {
        volatile char* output = value;
        for (size_t index = 0; index < Size; ++index) output[index] = 0;
    }

    char value[Size]{};
};
}

WeatherClient::WeatherClient(const std::string &apiKey): apiKey_(apiKey)
{}

float WeatherClient::getTemperatureByZip(const std::string &zip) const
{
    if (!isValidZip(zip))
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: invalid ZIP configuration");
        return std::numeric_limits<float>::quiet_NaN();
    }
    if (!isValidApiKey(apiKey_))
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: API key is not configured or invalid");
        return std::numeric_limits<float>::quiet_NaN();
    }

    SensitiveBuffer<256> query;
    const int queryLength = std::snprintf(query.value, sizeof(query.value), "zip=%s,de&units=metric&appid=%s", zip.c_str(), apiKey_.c_str());
    if (queryLength < 0 || static_cast<size_t>(queryLength) >= sizeof(query.value))
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: request configuration is too long");
        return std::numeric_limits<float>::quiet_NaN();
    }

    esp_http_client_config_t config{};
    config.host = "api.openweathermap.org";
    config.path = "/data/2.5/weather";
    config.query = query.value;
    config.port = 443;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.method = HTTP_METHOD_GET;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.skip_cert_common_name_check = false;
    config.disable_auto_redirect = true;
    config.timeout_ms = 10000;
    config.user_agent = "NixieClock/6";

    // ESP-IDF's INFO/DEBUG HTTP client trace may contain the complete request
    // line, including query parameters. Keep it at warning level so the API key
    // cannot be emitted by this request path.
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (!client)
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: failed to initialize HTTPS client");
        return std::numeric_limits<float>::quiet_NaN();
    }

    esp_err_t err = esp_http_client_open(client, 0);

    if (err != ESP_OK)
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: HTTPS connection failed: %s", esp_err_to_name(err));
        Logger::log(
            LoggerType::GENERAL,
            "WeatherClient: heap after TLS failure: internal free=%u, largest=%u, PSRAM free=%u bytes",
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
            static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    const int64_t headerResult = esp_http_client_fetch_headers(client);
    if (headerResult < 0)
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: failed to read HTTPS response headers");
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    int status = esp_http_client_get_status_code(client);
    if (status != 200)
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: weather service returned HTTP %d", status);
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    const int64_t contentLength = esp_http_client_get_content_length(client);
    if (contentLength >= static_cast<int64_t>(MAX_RESPONSE_SIZE))
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: response exceeds size limit");
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    char* responseMemory = static_cast<char*>(
        heap_caps_malloc(MAX_RESPONSE_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!responseMemory)
    {
        responseMemory = static_cast<char*>(
            heap_caps_malloc(MAX_RESPONSE_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    std::unique_ptr<char, decltype(&heap_caps_free)> buffer(responseMemory, &heap_caps_free);
    if (!buffer)
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: response buffer allocation failed");
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    const int readLength = esp_http_client_read_response(client, buffer.get(), MAX_RESPONSE_SIZE - 1);

    if (readLength < 0 || !esp_http_client_is_complete_data_received(client))
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: incomplete weather response");
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    buffer.get()[readLength] = '\0';
    float result = std::numeric_limits<float>::quiet_NaN();
    cJSON *root = cJSON_ParseWithLength(buffer.get(), static_cast<size_t>(readLength));

    if (!root)
    {
        Logger::log(LoggerType::GENERAL, "WeatherClient: JSON parsing failed");
    }
    else
    {
        cJSON *mainObj = cJSON_GetObjectItem(root, "main");
        if (mainObj)
        {
            cJSON *tempObj = cJSON_GetObjectItem(mainObj, "temp");
            if (cJSON_IsNumber(tempObj) && std::isfinite(tempObj->valuedouble) &&
                tempObj->valuedouble >= -100.0 && tempObj->valuedouble <= 100.0)
            {
                result = static_cast<float>(tempObj->valuedouble);
            }
            else
            {
                Logger::log(LoggerType::GENERAL, "WeatherClient: \"main\" does not contain a valid \"temp\" field");
            }
        }
        else
        {
            Logger::log(LoggerType::GENERAL, "WeatherClient: JSON does not contain a \"main\" object");
        }
        cJSON_Delete(root);
    }
    esp_http_client_cleanup(client);
    return result;
}
