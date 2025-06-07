#include "Memory.hpp"

static constexpr LoggerType logType = LoggerType::GENERAL;

PersistentStorage::PersistentStorage() noexcept
    : intValue_(0),
      floatValue_(0.0f),
      stringValue_("")
{}

PersistentStorage::~PersistentStorage() noexcept {}

esp_err_t PersistentStorage::init() noexcept
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        esp_err_t eraseErr = nvs_flash_erase();
        if (eraseErr != ESP_OK)
        {
            Logger::log(logType, "NVS Erase failed: %s", esp_err_to_name(eraseErr));
            return eraseErr;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK)
    {
        Logger::log(logType, "NVS Init failed: %s", esp_err_to_name(err));
        return err;
    }
    Logger::log(logType, F("NVS initialized successfully"));
    return ESP_OK;
}

esp_err_t PersistentStorage::load() noexcept
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK)
    {
        Logger::log(logType, "NVS open (READONLY) failed: %s", esp_err_to_name(err));
        return err;
    }

    int32_t storedInt = 0;
    err = nvs_get_i32(handle, KEY_INT, &storedInt);
    if (err == ESP_OK)
    {
        intValue_ = static_cast<int>(storedInt);
        Logger::log(logType, "Loaded intValue: %d", intValue_);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        Logger::log(logType, "KEY_INT not found, default intValue: %d", intValue_);
    }
    else
    {
        Logger::log(logType, "Error reading KEY_INT: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    int32_t storedFloatScaled = 0;
    err = nvs_get_i32(handle, KEY_FLOAT, &storedFloatScaled);
    if (err == ESP_OK)
    {
        floatValue_ = static_cast<float>(storedFloatScaled) / 10000.0f;
        Logger::log(logType, "Loaded floatValue: %.4f", floatValue_);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        Logger::log(logType, "KEY_FLOAT not found, default floatValue: %.4f", floatValue_);
    }
    else
    {
        Logger::log(logType, "Error reading KEY_FLOAT: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    size_t required_size = 0;
    err = nvs_get_str(handle, KEY_STRING, nullptr, &required_size);
    if (err == ESP_OK && required_size > 0)
    {
        char* buffer = new char[required_size];
        err = nvs_get_str(handle, KEY_STRING, buffer, &required_size);
        if (err == ESP_OK)
        {
            stringValue_.assign(buffer);
            Logger::log(logType, "Loaded stringValue: %s", buffer);
        }
        else
        {
            Logger::log(logType, "Error reading KEY_STRING content: %s", esp_err_to_name(err));
            delete[] buffer;
            nvs_close(handle);
            return err;
        }
        delete[] buffer;
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        Logger::log(logType, F("KEY_STRING not found, default stringValue is empty"));
    }
    else if (err == ESP_ERR_NVS_INVALID_HANDLE)
    {
        Logger::log(logType, "Invalid handle reading KEY_STRING: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }
    else
    {
        Logger::log(logType, "Error determining size for KEY_STRING: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t PersistentStorage::save() noexcept
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        Logger::log(logType, "NVS open (READWRITE) failed: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i32(handle, KEY_INT, static_cast<int32_t>(intValue_));
    if (err != ESP_OK)
    {
        Logger::log(logType, "Error writing KEY_INT: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }
    Logger::log(logType, "Saved intValue: %d", intValue_);

    int32_t floatScaled = static_cast<int32_t>(floatValue_ * 10000.0f);
    err = nvs_set_i32(handle, KEY_FLOAT, floatScaled);
    if (err != ESP_OK)
    {
        Logger::log(logType, "Error writing KEY_FLOAT: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }
    Logger::log(logType, "Saved floatValue (scaled): %d", floatScaled);

    err = nvs_set_str(handle, KEY_STRING, stringValue_.c_str());
    if (err != ESP_OK)
    {
        Logger::log(logType, "Error writing KEY_STRING: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }
    Logger::log(logType, "Saved stringValue: %s", stringValue_.c_str());

    err = nvs_commit(handle);
    if (err != ESP_OK)
    {
        Logger::log(logType, "Error during commit: %s", esp_err_to_name(err));
        nvs_close(handle);
        return err;
    }

    nvs_close(handle);
    return ESP_OK;
}

void PersistentStorage::setIntValue(int val) noexcept
{
    intValue_ = val;
}

int PersistentStorage::getIntValue() const noexcept
{
    return intValue_;
}

void PersistentStorage::setFloatValue(float val) noexcept
{
    floatValue_ = val;
}

float PersistentStorage::getFloatValue() const noexcept
{
    return floatValue_;
}

void PersistentStorage::setStringValue(const std::string& val) noexcept
{
    stringValue_ = val;
}

std::string PersistentStorage::getStringValue() const noexcept
{
    return stringValue_;
}
