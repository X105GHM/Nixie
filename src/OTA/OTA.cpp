#include "OTA/OTA.hpp"

static const char *TAG = "OTAManager";

OTAManager::OTAManager() noexcept {}

esp_err_t OTAManager::performUpdate(const std::string &firmwareUrl) noexcept
{
    esp_http_client_config_t config{};
    config.url = firmwareUrl.c_str();
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
        return ESP_FAIL;
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
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ESP_OK;
}
