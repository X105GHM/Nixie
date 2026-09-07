#include "OTA/OTA.hpp"

#ifdef IPADDR_NONE
#undef IPADDR_NONE
#endif
#ifdef INADDR_NONE
#undef INADDR_NONE
#endif
#include "esp_spiffs.h"
#include <cstring>
#include <memory>
#include <new>

extern TaskHandle_t displayTaskHandle;

namespace
{
constexpr size_t MAX_MANIFEST_SIZE = 4096;
}

OTAManager& OTAManager::instance() noexcept
{
    static OTAManager inst;
    return inst;
}

OTAManager::OTAManager() noexcept
{
    statusMutex_ = xSemaphoreCreateMutex();
    resetStatus();
}

static std::string normalizeVersion(std::string v)
{
    constexpr const char* prefix = "Nixie_V.";
    if (v.rfind(prefix, 0) == 0)
    {
        v.erase(0, std::strlen(prefix));
    }
    return v;
}

bool OTAManager::checkForUpdateAvailable(const std::string& baseUrl) noexcept
{
    const std::string manifestUrl = baseUrl + "/manifest.json";

    Logger::log(LoggerType::OTA, "Checking update availability from %s", manifestUrl.c_str());
    setStageMessage_(Stage::Manifest, "Checking manifest");

    auto verOpt = fetchManifestVersion(manifestUrl);
    if (!verOpt)
    {
        Logger::log(LoggerType::OTA, "Could not load manifest for availability check");
        Globals::updateAvailable = false;
        return false;
    }

    const std::string manifestVersionRaw = *verOpt;
    const std::string runningVersionRaw = Globals::getTextConfig().softwareVersion;

    const std::string manifestVersion = normalizeVersion(manifestVersionRaw);
    const std::string runningVersion  = normalizeVersion(runningVersionRaw);

    setVersions_(runningVersionRaw.c_str(), manifestVersionRaw.c_str());

    const bool updateAvailable = (manifestVersion != runningVersion);
    Globals::updateAvailable = updateAvailable;

    Logger::log(LoggerType::OTA, "Manifest version raw: %s, running version raw: %s", manifestVersionRaw.c_str(), runningVersionRaw.c_str());

    Logger::log(LoggerType::OTA, "Manifest version normalized: %s, running version normalized: %s, updateAvailable=%s", manifestVersion.c_str(), runningVersion.c_str(), updateAvailable ? "true" : "false");

    return updateAvailable;
}

bool OTAManager::startAsync(const std::string& baseUrl) noexcept
{
    if (!statusMutex_)
    {
        Logger::log(LoggerType::OTA, "OTA status mutex is unavailable");
        return false;
    }

    if (isRunning())
    {
        Logger::log(LoggerType::OTA, "OTA already running");
        return false;
    }

    pendingBaseUrl_ = baseUrl;
    resetStatus();

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.running = true;
        status_.stage = Stage::Manifest;
        status_.message = "OTA started";
        status_.lastResult = ESP_OK;
        xSemaphoreGive(statusMutex_);
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        otaTaskEntry_,
        "ota_task",
        18432,
        this,
        3,
        &otaTaskHandle_,
        1
    );

    if (ok != pdPASS)
    {
        otaTaskHandle_ = nullptr;
        setError_(ESP_FAIL, "Could not create OTA task");
        return false;
    }

    return true;
}

bool OTAManager::isRunning() const noexcept
{
    Status s = getStatusCopy_();
    return s.running;
}

void OTAManager::resetStatus() noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_ = Status{};
        xSemaphoreGive(statusMutex_);
    }
}

OTAManager::Status OTAManager::getStatusCopy_() const noexcept
{
    Status copy;

    if (!statusMutex_) return copy;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        copy = status_;
        xSemaphoreGive(statusMutex_);
    }

    return copy;
}

const char* OTAManager::stageToString_(Stage s) noexcept
{
    switch (s)
    {
        case Stage::Idle:     return "idle";
        case Stage::Manifest: return "manifest";
        case Stage::Firmware: return "firmware";
        case Stage::SPIFFS:   return "spiffs";
        case Stage::Done:     return "done";
        case Stage::Error:    return "error";
        default:              return "unknown";
    }
}

std::string OTAManager::escapeJson_(const std::string& s) noexcept
{
    std::string out;
    out.reserve(s.size() + 8);

    for (char c : s)
    {
        switch (c)
        {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<uint8_t>(c) < 0x20)
                {
                    out += ' ';
                }
                else
                {
                    out += c;
                }
                break;
        }
    }

    return out;
}

int OTAManager::calcPercent_(size_t done, size_t total) noexcept
{
    if (total == 0) return 0;
    if (done >= total) return 100;
    return static_cast<int>((done * 100U) / total);
}

std::string OTAManager::getStatusJson() const noexcept
{
    Status s = getStatusCopy_();

    std::string json;
    json.reserve(512);

    json += "{";
    json += "\"running\":";          json += (s.running ? "true" : "false"); json += ",";
    json += "\"rebootRequired\":";   json += (s.rebootRequired ? "true" : "false"); json += ",";
    json += "\"firmwareUpdated\":";  json += (s.firmwareUpdated ? "true" : "false"); json += ",";
    json += "\"spiffsUpdated\":";    json += (s.spiffsUpdated ? "true" : "false"); json += ",";
    json += "\"stage\":\"";          json += stageToString_(s.stage); json += "\",";
    json += "\"message\":\"";        json += escapeJson_(s.message); json += "\",";
    json += "\"lastResult\":\"";     json += esp_err_to_name(s.lastResult); json += "\",";
    json += "\"currentVersion\":\""; json += escapeJson_(s.currentVersion); json += "\",";
    json += "\"targetVersion\":\"";  json += escapeJson_(s.targetVersion); json += "\",";
    json += "\"appProgressPct\":";   json += std::to_string(s.appProgressPct); json += ",";
    json += "\"appBytesDone\":";     json += std::to_string(s.appBytesDone); json += ",";
    json += "\"appBytesTotal\":";    json += std::to_string(s.appBytesTotal); json += ",";
    json += "\"spiffsProgressPct\":";json += std::to_string(s.spiffsProgressPct); json += ",";
    json += "\"spiffsBytesDone\":";  json += std::to_string(s.spiffsBytesDone); json += ",";
    json += "\"spiffsBytesTotal\":"; json += std::to_string(s.spiffsBytesTotal);
    json += "}";

    return json;
}

void OTAManager::setStageMessage_(Stage stage, const char* message) noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.stage = stage;
        status_.message = message ? message : "";
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::setError_(esp_err_t err, const char* message) noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.running = false;
        status_.stage = Stage::Error;
        status_.lastResult = err;
        status_.message = message ? message : "Error";
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::setDone_(const char* message) noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.running = false;
        status_.stage = Stage::Done;
        status_.lastResult = ESP_OK;
        status_.message = message ? message : "Done";
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::setAppProgress_(size_t done, size_t total, const char* message) noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.stage = Stage::Firmware;
        status_.appBytesDone = done;
        status_.appBytesTotal = total;
        status_.appProgressPct = calcPercent_(done, total);
        status_.message = message ? message : "Firmware update";
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::setSpiffsProgress_(size_t done, size_t total, const char* message) noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.stage = Stage::SPIFFS;
        status_.spiffsBytesDone = done;
        status_.spiffsBytesTotal = total;
        status_.spiffsProgressPct = calcPercent_(done, total);
        status_.message = message ? message : "SPIFFS update";
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::setVersions_(const char* currentVersion, const char* targetVersion) noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.currentVersion = currentVersion ? currentVersion : "";
        status_.targetVersion = targetVersion ? targetVersion : "";
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::markFirmwareUpdated_() noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.firmwareUpdated = true;
        status_.rebootRequired = true;
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::markSpiffsUpdated_() noexcept
{
    if (!statusMutex_) return;

    if (xSemaphoreTake(statusMutex_, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        status_.spiffsUpdated = true;
        xSemaphoreGive(statusMutex_);
    }
}

void OTAManager::otaTaskEntry_(void* arg) noexcept
{
    auto* self = static_cast<OTAManager*>(arg);
    if (self)
    {
        self->otaTask_();
    }
    vTaskDelete(nullptr);
}

void OTAManager::otaTask_() noexcept
{
    bool displayWasSuspended = false;

    if (displayTaskHandle != nullptr)
    {
        Logger::log(LoggerType::OTA, "Suspending DisplayDigits task for OTA");
        vTaskSuspend(displayTaskHandle);
        displayWasSuspended = true;
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    Logger::log(LoggerType::OTA, "SPIFFS for OTA unmounting...");
    const esp_err_t unmountResult = esp_vfs_spiffs_unregister(nullptr);
    if (unmountResult != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "SPIFFS unmount failed: %s", esp_err_to_name(unmountResult));
        if (displayWasSuspended && displayTaskHandle != nullptr)
        {
            vTaskResume(displayTaskHandle);
        }
        setError_(unmountResult, "SPIFFS unmount failed");
        displayEnabled = true;
        otaTaskHandle_ = nullptr;
        return;
    }

    esp_err_t result = checkAndUpdate(pendingBaseUrl_);

    Logger::log(LoggerType::OTA, "SPIFFS remounting...");
    esp_vfs_spiffs_conf_t spiffsConfig{};
    spiffsConfig.base_path = "/spiffs";
    spiffsConfig.partition_label = nullptr;
    spiffsConfig.max_files = 5;
    spiffsConfig.format_if_mount_failed = true;
    const esp_err_t mountResult = esp_vfs_spiffs_register(&spiffsConfig);
    if (mountResult != ESP_OK && mountResult != ESP_ERR_INVALID_STATE)
    {
        Logger::log(LoggerType::OTA, "esp_vfs_spiffs_register failed after OTA: %s", esp_err_to_name(mountResult));

        if (displayWasSuspended && displayTaskHandle != nullptr)
        {
            Logger::log(LoggerType::OTA, "Resuming DisplayDigits task after OTA failure");
            vTaskResume(displayTaskHandle);
        }

        setError_(ESP_FAIL, "SPIFFS remount failed");
        otaTaskHandle_ = nullptr;
        displayEnabled = true;
        return;
    }

    Logger::log(LoggerType::OTA, "SPIFFS remounted");

    if (displayWasSuspended && displayTaskHandle != nullptr)
    {
        Logger::log(LoggerType::OTA, "Resuming DisplayDigits task after OTA");
        vTaskResume(displayTaskHandle);
        displayEnabled = true;
    }

    if (result == ESP_OK)
    {
        Status s = getStatusCopy_();

        if (s.firmwareUpdated)
        {
            setDone_("OTA finished, reboot required");
        }
        else
        {
            setDone_("OTA finished");
        }
    }
    else
    {
        setError_(result, "OTA failed");
    }

    displayEnabled = true;
    otaTaskHandle_ = nullptr;
}

std::optional<std::string> OTAManager::fetchManifestVersion(const std::string &manifestUrl) noexcept
{
    setStageMessage_(Stage::Manifest, "Loading manifest");
    Logger::log(LoggerType::OTA, "Loading manifest: %s", manifestUrl.c_str());

    esp_http_client_config_t config{};
    config.url = manifestUrl.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.cert_pem = nullptr;
    config.skip_cert_common_name_check = false;
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        Logger::log(LoggerType::OTA, "HTTP client could not be initialized");
        return std::nullopt;
    }

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "HTTP client could not open connection");
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    int hdr = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (hdr < 0 || status != 200)
    {
        Logger::log(LoggerType::OTA, "Manifest HTTP error: status=%d hdr=%d", status, hdr);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    std::string body;
    body.reserve(512);

    char* buffer = static_cast<char*>(malloc(256));
    if (!buffer)
    {
        Logger::log(LoggerType::OTA, "Manifest buffer allocation failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    int total = 0;
    int len = 0;

    while ((len = esp_http_client_read(client, buffer, 256)) > 0)
    {
        if (body.size() + static_cast<size_t>(len) > MAX_MANIFEST_SIZE)
        {
            Logger::log(LoggerType::OTA, "Manifest exceeds size limit");
            free(buffer);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return std::nullopt;
        }
        body.append(buffer, len);
        total += len;
    }

    free(buffer);

    if (len < 0)
    {
        Logger::log(LoggerType::OTA, "Manifest read failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return std::nullopt;
    }

    Logger::log(LoggerType::OTA, "Manifest received (%d bytes)", total);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    const std::string key = "\"version\"";
    size_t start = body.find(key);
    if (start == std::string::npos) return std::nullopt;

    start = body.find(':', start);
    if (start == std::string::npos) return std::nullopt;

    start = body.find('"', start);
    if (start == std::string::npos) return std::nullopt;

    size_t end = body.find('"', start + 1);
    if (end == std::string::npos) return std::nullopt;

    return body.substr(start + 1, end - start - 1);
}

esp_err_t OTAManager::performFirmwareUpdate(const std::string &firmwareUrl) noexcept
{
    Logger::log(LoggerType::OTA, "Starting firmware OTA from %s", firmwareUrl.c_str());
    setStageMessage_(Stage::Firmware, "Starting firmware OTA");

    esp_http_client_config_t config{};
    config.url = firmwareUrl.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.cert_pem = nullptr;
    config.skip_cert_common_name_check = false;
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        Logger::log(LoggerType::OTA, "esp_http_client_init failed");
        return ESP_FAIL;
    }

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "Error opening firmware URL");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);

    if (status != 200 || content_length < 0)
    {
        Logger::log(LoggerType::OTA, "Error fetching firmware headers: status=%d, len=%d", status, content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    setAppProgress_(0, content_length > 0 ? static_cast<size_t>(content_length) : 0, "Firmware downloading");

    const esp_partition_t* update_partition = esp_ota_get_next_update_partition(nullptr);
    if (!update_partition)
    {
        Logger::log(LoggerType::OTA, "No OTA partition found");
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
        return err;
    }

    constexpr size_t bufferSize = 4096;
    uint8_t* buffer = static_cast<uint8_t*>(malloc(bufferSize));
    if (!buffer)
    {
        Logger::log(LoggerType::OTA, "Could not allocate memory for OTA buffer");
        esp_ota_end(update_handle);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;

    while (true)
    {
        int data_read = esp_http_client_read(client, reinterpret_cast<char*>(buffer), bufferSize);

        if (data_read < 0)
        {
            Logger::log(LoggerType::OTA, "esp_http_client_read failed");
            free(buffer);
            esp_ota_end(update_handle);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        if (data_read == 0)
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

        offset += static_cast<size_t>(data_read);
        setAppProgress_(offset, content_length > 0 ? static_cast<size_t>(content_length) : 0, "Firmware writing");
    }

    free(buffer);

    esp_err_t end_err = esp_ota_end(update_handle);
    if (end_err != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "OTA end failed: %s", esp_err_to_name(end_err));
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

    esp_err_t boot_err = esp_ota_set_boot_partition(update_partition);
    if (boot_err != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(boot_err));
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return boot_err;
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    setAppProgress_(content_length > 0 ? static_cast<size_t>(content_length) : offset,
                    content_length > 0 ? static_cast<size_t>(content_length) : offset,
                    "Firmware done");

    markFirmwareUpdated_();

    Logger::log(LoggerType::OTA, "Firmware OTA successful. Restart required.");
    return ESP_OK;
}

esp_err_t OTAManager::performSPIFFSUpdate(const std::string &spiffsUrl) noexcept
{
    Logger::log(LoggerType::OTA, "Starting SPIFFS OTA from %s", spiffsUrl.c_str());
    setStageMessage_(Stage::SPIFFS, "Starting SPIFFS OTA");

    esp_http_client_config_t config{};
    config.url = spiffsUrl.c_str();
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.cert_pem = nullptr;
    config.skip_cert_common_name_check = false;
    config.disable_auto_redirect = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        Logger::log(LoggerType::OTA, "esp_http_client_init failed");
        return ESP_FAIL;
    }

    if (esp_http_client_open(client, 0) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "Error opening SPIFFS URL");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    Logger::log(LoggerType::OTA, "SPIFFS HTTP status: %d, Content-Length: %d", status, content_length);

    if (status != 200 || content_length < 0)
    {
        Logger::log(LoggerType::OTA, "Invalid SPIFFS HTTP response: %d / %d", status, content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    const esp_partition_t* part = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_DATA_SPIFFS,
        nullptr
    );

    if (!part)
    {
        Logger::log(LoggerType::OTA, "SPIFFS partition not found");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    Logger::log(LoggerType::OTA, "SPIFFS partition @ 0x%08x, size %u bytes", part->address, part->size);

    if (esp_partition_erase_range(part, 0, part->size) != ESP_OK)
    {
        Logger::log(LoggerType::OTA, "SPIFFS partition erase failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    constexpr size_t bufSize = 4096;
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[bufSize]);

    if (!buffer)
    {
        Logger::log(LoggerType::OTA, "SPIFFS buffer allocation failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    size_t offset = 0;
    int read_len = 0;

    setSpiffsProgress_(0, content_length > 0 ? static_cast<size_t>(content_length) : 0, "SPIFFS downloading");

    while ((read_len = esp_http_client_read(client, reinterpret_cast<char*>(buffer.get()), bufSize)) > 0)
    {
        if ((offset + static_cast<size_t>(read_len)) > part->size)
        {
            Logger::log(LoggerType::OTA, "SPIFFS image too large for partition");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        if (esp_partition_write(part, offset, buffer.get(), read_len) != ESP_OK)
        {
            Logger::log(LoggerType::OTA, "SPIFFS write failed");
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }

        offset += static_cast<size_t>(read_len);
        setSpiffsProgress_(offset,
                           content_length > 0 ? static_cast<size_t>(content_length) : 0,
                           "SPIFFS writing");
    }

    if (read_len < 0)
    {
        Logger::log(LoggerType::OTA, "SPIFFS read failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    Logger::log(LoggerType::OTA, "SPIFFS download complete, total written: %u bytes", static_cast<unsigned>(offset));

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    setSpiffsProgress_(content_length > 0 ? static_cast<size_t>(content_length) : offset,
                       content_length > 0 ? static_cast<size_t>(content_length) : offset,
                       "SPIFFS done");

    markSpiffsUpdated_();

    return ESP_OK;
}

esp_err_t OTAManager::checkAndUpdate(const std::string &baseUrl) noexcept
{
    const std::string manifestUrl = baseUrl + "/manifest.json";
    const std::string firmwareUrl = baseUrl + "/firmware.bin";
    const std::string spiffsUrl   = baseUrl + "/spiffs.bin";

    setStageMessage_(Stage::Manifest, "Checking manifest");

    auto verOpt = fetchManifestVersion(manifestUrl);
    if (!verOpt)
    {
        Logger::log(LoggerType::OTA, "Could not load manifest: %s", manifestUrl.c_str());
        return ESP_FAIL;
    }

    // Use the same source and normalization as the availability check. A
    // different channel may intentionally offer an older version.
    const std::string runningVersion = Globals::getTextConfig().softwareVersion;
    setVersions_(runningVersion.c_str(), verOpt->c_str());
    const bool firmwareNeedsUpdate =
        normalizeVersion(*verOpt) != normalizeVersion(runningVersion);

    if (firmwareNeedsUpdate)
    {
        Logger::log(LoggerType::OTA, "Firmware update detected: %s", verOpt->c_str());
        esp_err_t fwErr = performFirmwareUpdate(firmwareUrl);
        if (fwErr != ESP_OK)
        {
            return fwErr;
        }
    }
    else
    {
        Logger::log(LoggerType::OTA, "Firmware is up to date");
        setAppProgress_(100, 100, "Firmware up to date");
    }

    Logger::log(LoggerType::OTA, "Starting SPIFFS download from %s", spiffsUrl.c_str());
    return performSPIFFSUpdate(spiffsUrl);
}
