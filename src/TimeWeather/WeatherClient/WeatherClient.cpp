#include "WeatherClient.hpp"

WeatherClient::WeatherClient(const std::string &apiKey)
    : apiKey_(apiKey)
{
}

float WeatherClient::getTemperatureByZip(const std::string &zip) const
{
    std::string url = "http://api.openweathermap.org/data/2.5/weather?zip=" +
                      zip + ",de&units=metric&appid=" + apiKey_;

    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    int content_length = esp_http_client_get_content_length(client);
    int read_len = 0;
    char *buffer = nullptr;

    if (content_length > 0)
    {
        buffer = static_cast<char *>(std::malloc(static_cast<size_t>(content_length) + 1));
        if (!buffer)
        {
            esp_http_client_cleanup(client);
            return std::numeric_limits<float>::quiet_NaN();
        }
        read_len = esp_http_client_read_response(client, buffer, content_length);
    }
    else
    {
        int max_len = 2048;
        buffer = static_cast<char *>(std::malloc(static_cast<size_t>(max_len)));
        if (!buffer)
        {
            esp_http_client_cleanup(client);
            return std::numeric_limits<float>::quiet_NaN();
        }
        read_len = esp_http_client_read(client, buffer, max_len - 1);
    }

    float result = std::numeric_limits<float>::quiet_NaN();
    if (read_len > 0)
    {
        buffer[read_len] = '\0';
        cJSON *root = cJSON_Parse(buffer);
        if (root)
        {
            cJSON *mainObj = cJSON_GetObjectItem(root, "main");
            if (mainObj)
            {
                cJSON *tempObj = cJSON_GetObjectItem(mainObj, "temp");
                if (tempObj && (tempObj->type == cJSON_Number))
                {
                    result = static_cast<float>(tempObj->valuedouble);
                }
            }
            cJSON_Delete(root);
        }
    }

    std::free(buffer);
    esp_http_client_cleanup(client);
    return result;
}
