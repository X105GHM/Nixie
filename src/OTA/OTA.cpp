#include "OTA/OTA.hpp"
#include "Logger/Logger.hpp"

OTAManager::OTAManager() noexcept {}

std::optional<std::string> OTAManager::fetchManifestVersion(const std::string &manifestUrl) noexcept
{
    Logger::log(LoggerType::OTA, "Lade Manifest: %s", manifestUrl.c_str());

    esp_http_client_config_t config{};
    config.url = manifestUrl.c_str();
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client || esp_http_client_open(client, 0) != ESP_OK)
    {
        if (client) esp_http_client_cleanup(client);
        Logger::log(LoggerType::OTA, F("Fehler beim Abrufen des Manifests"));
        return std::nullopt;
    }

    std::string json;
    char buffer[256];
    while (true)
    {
        int len = esp_http_client_read(client, buffer, sizeof(buffer));
        if (len <= 0) break;
        json.append(buffer, len);
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    const std::string key = "\"version\"";
    size_t start = json.find(key);
    if (start == std::string::npos) return std::nullopt;
    start = json.find(":", start);
    start = json.find("\"", start);
    size_t end = json.find("\"", start + 1);
    if (start == std::string::npos || end == std::string::npos) return std::nullopt;

    return json.substr(start + 1, end - start - 1);
}

esp_err_t OTAManager::checkAndUpdate(const std::string &manifestUrl, const std::string &firmwareUrl) noexcept
{
    auto versionOpt = fetchManifestVersion(manifestUrl);
    if (!versionOpt)
        return ESP_FAIL;

    const std::string &availableVersion = *versionOpt;

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_desc;
    if (esp_ota_get_partition_description(running, &running_desc) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, F("Aktuelle Partition konnte nicht gelesen werden"));
        return ESP_FAIL;
    }

    if (availableVersion == running_desc.version)
    {
        Logger::log(LoggerType::OTA, F("Firmware ist aktuell"));
        return ESP_OK;
    }

    Logger::log(LoggerType::OTA, "Update erkannt: %s", availableVersion.c_str());
    return performUpdate(firmwareUrl);
}

esp_err_t OTAManager::performUpdate(const std::string &firmwareUrl) noexcept
{
    Logger::log(LoggerType::OTA, "Beginne OTA-Update von %s", firmwareUrl.c_str());

    esp_http_client_config_t config{};
    config.url = firmwareUrl.c_str();
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;
    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    if (esp_http_client_fetch_headers(client) <= 0)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_ota_handle_t update_handle = 0;
    if (esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle) != ESP_OK)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const size_t bufferSize = 4096;
    uint8_t *buffer = static_cast<uint8_t *>(std::malloc(bufferSize));
    if (!buffer)
    {
        esp_ota_end(update_handle);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    while (true)
    {
        int data_read = esp_http_client_read(client, reinterpret_cast<char *>(buffer), bufferSize);
        if (data_read < 0)
        {
            std::free(buffer);
            esp_ota_end(update_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        else if (data_read == 0)
        {
            break;
        }
        else
        {
            if (esp_ota_write(update_handle, buffer, data_read) != ESP_OK)
            {
                std::free(buffer);
                esp_ota_end(update_handle);
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }
        }
    }

    std::free(buffer);
    if (esp_ota_end(update_handle) != ESP_OK)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_app_desc_t new_desc;
    if (esp_ota_get_partition_description(update_partition, &new_desc) != ESP_OK)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_desc;
    if (esp_ota_get_partition_description(running_partition, &running_desc) != ESP_OK)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    if (std::strcmp(new_desc.version, running_desc.version) == 0)
    {
        Logger::log(LoggerType::OTA, F("Update-Version ist identisch zur laufenden"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    if (esp_ota_set_boot_partition(update_partition) != ESP_OK)
    {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    Logger::log(LoggerType::OTA, F("Update erfolgreich. Neustart erforderlich."));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}
