#include "OTA/OTA.hpp"

OTAManager::OTAManager() noexcept {}

std::optional<std::string> OTAManager::fetchManifestVersion(const std::string &manifestUrl) noexcept
{
    Logger::log(LoggerType::OTA, "Loading manifest: %s", manifestUrl.c_str());
    esp_http_client_config_t config{};
    config.url = manifestUrl.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = isrg_root_x1;
    config.skip_cert_common_name_check = false;
    config.use_global_ca_store = false;
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        Logger::log(LoggerType::OTA, F("HTTP client could not be initialized"));
        return std::nullopt;
    }
    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, F("HTTP client could not open connection"));
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0)
    {
        Logger::log(LoggerType::OTA, F("No headers received or Content-Length is 0"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    std::string body;
    char *buffer = static_cast<char *>(malloc(256));
    if (!buffer)
    {
        Logger::log(LoggerType::OTA, F("Manifest buffer allocation failed"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    int total = 0;
    int len;

    while ((len = esp_http_client_read(client, buffer, 256)) > 0)
    {
        body.append(buffer, len);
        total += len;
    }

    if (len < 0)
    {
        Logger::log(LoggerType::OTA, F("Manifest read failed"));
        free(buffer);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    free(buffer);

    Logger::log(LoggerType::OTA, "Manifest (%d Bytes):\n%s", total, body.c_str());
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    const std::string key = "\"version\"";
    size_t start = body.find(key);
    if (start == std::string::npos)
        return std::nullopt;
    start = body.find(':', start);
    start = body.find('"', start);
    size_t end = body.find('"', start + 1);
    if (start == std::string::npos || end == std::string::npos)
        return std::nullopt;

    return body.substr(start + 1, end - start - 1);
}

esp_err_t OTAManager::performFirmwareUpdate(const std::string &firmwareUrl) noexcept
{
    Logger::log(LoggerType::OTA, "Starting firmware OTA from %s", firmwareUrl.c_str());

    esp_http_client_config_t config{};
    config.url = firmwareUrl.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = isrg_root_x1;
    config.skip_cert_common_name_check = false;
    config.use_global_ca_store = false;
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        Logger::log(LoggerType::OTA, F("esp_http_client_init failed"));
        return ESP_FAIL;
    }
    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, F("Error opening firmware URL"));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    if (esp_http_client_fetch_headers(client) <= 0)
    {
        Logger::log(LoggerType::OTA, F("Error fetching HTTP headers"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition)
    {
        Logger::log(LoggerType::OTA, F("No OTA partition found"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    esp_ota_handle_t update_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "esp_ota_begin failed: %s", esp_err_to_name(err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const size_t bufferSize = 4096;
    uint8_t *buffer = static_cast<uint8_t *>(malloc(bufferSize));
    if (!buffer)
    {
        Logger::log(LoggerType::OTA, F("Could not allocate memory for OTA buffer"));
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
            Logger::log(LoggerType::OTA, F("esp_http_client_read failed"));
            free(buffer);
            esp_ota_end(update_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        else if (data_read == 0)
        {
            break;
        }
        esp_err_t write_err = esp_ota_write(update_handle, buffer, data_read);
        if (write_err != ESP_OK)
        {
            Logger::log(LoggerType::OTA, "OTA write failed: %s", esp_err_to_name(write_err));
            free(buffer);
            esp_ota_end(update_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return write_err;
        }
    }

    free(buffer);
    esp_err_t end_err = esp_ota_end(update_handle);
    if (end_err != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "OTA End failed: %s", esp_err_to_name(end_err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return end_err;
    }

    esp_app_desc_t new_desc{};
    esp_err_t desc_err = esp_ota_get_partition_description(update_partition, &new_desc);
    if (desc_err != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "Could not read new partition description: %s", esp_err_to_name(desc_err));
    }
    else
    {
        Logger::log(LoggerType::OTA, "New firmware version in slot: %s", new_desc.version);
    }

    const esp_partition_t *running_partition = esp_ota_get_running_partition();
    esp_app_desc_t running_desc;
    if (esp_ota_get_partition_description(running_partition, &running_desc) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, F("Could not read running partition"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
/*
    if (std::strcmp(new_desc.version, running_desc.version) == 0)
    {
        Logger::log(LoggerType::OTA, F("Update version is identical to the running one"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
*/
    esp_err_t boot_err = esp_ota_set_boot_partition(update_partition);
    if (boot_err != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(boot_err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    Logger::log(LoggerType::OTA, F("Firmware OTA successful. Restart required."));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}

esp_err_t OTAManager::checkAndUpdate(const std::string &baseUrl) noexcept
{
    std::string manifestUrl = baseUrl + "/manifest.json";
    std::string firmwareUrl = baseUrl + "/firmware.bin";
    std::string spiffsUrl = baseUrl + "/spiffs.bin";

    auto verOpt = fetchManifestVersion(manifestUrl);
    if (!verOpt)
    {
        Logger::log(LoggerType::OTA, "Could not load manifest: %s", manifestUrl.c_str());
        return ESP_FAIL;
    }
    const auto *running = esp_ota_get_running_partition();
    esp_app_desc_t desc;
    if (esp_ota_get_partition_description(running, &desc) == ESP_OK && *verOpt != desc.version)
    {
        Logger::log(LoggerType::OTA, "Firmware update detected: %s", verOpt->c_str());
        if (performFirmwareUpdate(firmwareUrl) != ESP_OK)
            return ESP_FAIL;
    }
    else
    {
        Logger::log(LoggerType::OTA, F("Firmware is up to date"));
    }

    Logger::log(LoggerType::OTA, "Starting SPIFFS download from %s", spiffsUrl.c_str());
    return performSPIFFSUpdate(spiffsUrl);
}

esp_err_t OTAManager::performSPIFFSUpdate(const std::string &spiffsUrl) noexcept
{
    Logger::log(LoggerType::OTA, "Starting SPIFFS OTA from %s", spiffsUrl.c_str());

    esp_http_client_config_t config{};
    config.url = spiffsUrl.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.cert_pem = isrg_root_x1;
    config.skip_cert_common_name_check = false;
    config.use_global_ca_store = false;
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        Logger::log(LoggerType::OTA, F("esp_http_client_init failed"));
        return ESP_FAIL;
    }

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, F("Error opening SPIFFS URL"));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    Logger::log(LoggerType::OTA, "SPIFFS HTTP status: %d, Content-Length: %d", status, content_length);
    if (status != 200 || content_length <= 0)
    {
        Logger::log(LoggerType::OTA, "Invalid HTTP response: %d / %d", status, content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_DATA_SPIFFS,nullptr);

    if (!part)
    {
        Logger::log(LoggerType::OTA, F("SPIFFS partition not found"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    Logger::log(LoggerType::OTA, "SPIFFS partition @ 0x%08x, size %u bytes", part->address, part->size);

    if (esp_partition_erase_range(part, 0, part->size) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, F("SPIFFS partition erase failed"));
    }

    const size_t bufSize = 4096;
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[bufSize]);
    size_t offset = 0;
    int read_len = 0;

    size_t nextLog = 64 * 1024; // alle 64 KB

    while ((read_len = esp_http_client_read(client, reinterpret_cast<char *>(buffer.get()), bufSize)) > 0)
    {
        if (esp_partition_write(part, offset, buffer.get(), read_len) != ESP_OK)
        {
            Logger::log(LoggerType::OTA, F("SPIFFS write failed"));
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        offset += read_len;

        if (offset >= nextLog)
        {
            if (content_length > 0)
            {
                int percent = static_cast<int>((offset * 100) / content_length);
                Logger::log(LoggerType::OTA, "SPIFFS progress: %u/%d bytes (%d%%)", static_cast<unsigned>(offset), content_length, percent);
            }
            else
            {
                Logger::log(LoggerType::OTA, "SPIFFS progress: %u bytes", static_cast<unsigned>(offset));
            }
            nextLog += 64 * 1024;
        }
    }

    if (read_len < 0)
    {
        Logger::log(LoggerType::OTA, F("SPIFFS read failed"));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    Logger::log(LoggerType::OTA, "SPIFFS download complete, total written: %u bytes", offset);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return ESP_OK;
}