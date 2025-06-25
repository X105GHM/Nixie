#include "WeatherClient.hpp"

WeatherClient::WeatherClient(const std::string &apiKey): apiKey_(apiKey)
{}

float WeatherClient::getTemperatureByZip(const std::string &zip) const
{
    Logger::log(LoggerType::GENERAL,"WeatherClient: API-Key Länge = %u, Inhalt = %s",static_cast<unsigned>(apiKey_.length()),apiKey_.c_str());

    std::string url = "http://api.openweathermap.org/data/2.5/weather?zip=" +zip + ",de&units=metric&appid=" + apiKey_;

    Logger::log(LoggerType::GENERAL,"WeatherClient: Anfrage an URL: %s",url.c_str());

    esp_http_client_config_t config{};
    config.url = url.c_str();
    config.method = HTTP_METHOD_GET;

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (!client)
    {
        Logger::log(LoggerType::GENERAL,"WeatherClient: HTTP-Client konnte nicht initialisiert werden");
        return std::numeric_limits<float>::quiet_NaN();
    }

    esp_err_t err = esp_http_client_open(client, 0);

    if (err != ESP_OK)
    {
        Logger::log(LoggerType::GENERAL,"WeatherClient: Verbindung konnte nicht geöffnet werden: %s",esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    esp_http_client_fetch_headers(client);

    int status = esp_http_client_get_status_code(client);
    Logger::log(LoggerType::GENERAL,"WeatherClient: HTTP-Statuscode = %d", status);

    if (status != 200)
    {
        Logger::log(LoggerType::GENERAL,
                    "WeatherClient: Unerwarteter HTTP-Status %d", status);
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    const size_t buf_size = 4096;
    char *buffer = static_cast<char *>(std::malloc(buf_size));

    if (!buffer)
    {
        Logger::log(LoggerType::GENERAL,"WeatherClient: Speicherreservierung fehlgeschlagen (%u Bytes)", buf_size);
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    int read_len = esp_http_client_read_response(client, buffer, buf_size - 1);

    if (read_len < 0)
    {
        Logger::log(LoggerType::GENERAL,"WeatherClient: Fehler beim Lesen des Bodys: %d", read_len);
        std::free(buffer);
        esp_http_client_cleanup(client);
        return std::numeric_limits<float>::quiet_NaN();
    }

    buffer[read_len] = '\0';
    Logger::log(LoggerType::GENERAL,"WeatherClient: Gelesene Bytes = %d", read_len);

    Logger::log(LoggerType::GENERAL,"WeatherClient: Rohes JSON: %s", buffer);

    float result = std::numeric_limits<float>::quiet_NaN();
    cJSON *root = cJSON_Parse(buffer);

    if (!root)
    {
        Logger::log(LoggerType::GENERAL,"WeatherClient: JSON-Parsing fehlgeschlagen");
    }
    else
    {
        cJSON *mainObj = cJSON_GetObjectItem(root, "main");
        if (mainObj)
        {
            cJSON *tempObj = cJSON_GetObjectItem(mainObj, "temp");
            if (tempObj && tempObj->type == cJSON_Number)
            {
                result = static_cast<float>(tempObj->valuedouble);
                Logger::log(LoggerType::GENERAL,"WeatherClient: Extrahierte Temperatur = %.2f °C", result);
            }
            else
            {
                Logger::log(LoggerType::GENERAL,"WeatherClient: \"main\" enthält kein gültiges \"temp\"-Feld");
            }
        }
        else
        {
            Logger::log(LoggerType::GENERAL,"WeatherClient: JSON enthält kein \"main\"-Objekt");
        }
        cJSON_Delete(root);
    }
    std::free(buffer);
    esp_http_client_cleanup(client);
    return result;
}