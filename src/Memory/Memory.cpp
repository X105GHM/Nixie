#include "Memory.hpp"
#include "Logger/Logger.hpp"
#include "Globals/Globals.hpp"

namespace Memory
{
    static PersistentStorage storage;

    esp_err_t PersistentStorage::init() noexcept
    {
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            nvs_flash_erase();
            err = nvs_flash_init();
        }
        if (err != ESP_OK) {
            Logger::log(LoggerType::GENERAL, "NVS init failed: %s", esp_err_to_name(err));
        }
        return err;
    }

    esp_err_t PersistentStorage::load() noexcept
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
        if (err != ESP_OK) return err;

        int32_t i;
        if ((err = nvs_get_i32(handle, KEY_INT, &i)) == ESP_OK) intValue_ = i;

        int32_t f;
        if ((err = nvs_get_i32(handle, KEY_FLOAT, &f)) == ESP_OK) floatValue_ = f / 10000.0f;

        size_t len = 0;
        if ((err = nvs_get_str(handle, KEY_STRING, nullptr, &len)) == ESP_OK) {
            std::string buf(len, '\0');
            nvs_get_str(handle, KEY_STRING, buf.data(), &len);
            stringValue_ = buf;
        }

        nvs_close(handle);
        return ESP_OK;
    }

    esp_err_t PersistentStorage::save() noexcept
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) return err;

        nvs_set_i32(handle, KEY_INT, intValue_);
        nvs_set_i32(handle, KEY_FLOAT, static_cast<int32_t>(floatValue_ * 10000));

        std::string packed =
            Globals::zipCode                                                    + "|" +
            Globals::HardwareVersion                                            + "|" +
            std::to_string(static_cast<int>(Globals::currentFirmwareTarget))    + "|" +
            Globals::timeLimitFrom                                              + "|" +
            Globals::timeLimitTo                                                + "|" +
            std::to_string(brightness)                                          + "|" +
            std::to_string(PWM_PERIOD_US)                                       + "|" +
            std::to_string(static_cast<int>(Globals::currentTimeZone));

        nvs_set_str(handle, KEY_STRING, packed.c_str());
        nvs_commit(handle);
        nvs_close(handle);
        return ESP_OK;
    }

    void PersistentStorage::setIntValue(int val) noexcept    { intValue_ = val; }
    int  PersistentStorage::getIntValue() const noexcept     { return intValue_; }

    void PersistentStorage::setFloatValue(float val) noexcept { floatValue_ = val; }
    float PersistentStorage::getFloatValue() const noexcept   { return floatValue_; }

    void PersistentStorage::setStringValue(const std::string &val) noexcept { stringValue_ = val; }
    std::string PersistentStorage::getStringValue() const noexcept           { return stringValue_; }

    esp_err_t PersistentStorage::clearAll() noexcept
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) return err;

        err = nvs_erase_all(handle);
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
        return err;
    }

    void PersistentStorage::resetGlobals() noexcept
    {
        if (auto err = storage.clearAll(); err != ESP_OK) 
        {
            Logger::log(LoggerType::GENERAL,"resetGlobals: erase failed: %s",esp_err_to_name(err));
        }

        Globals::tickerEnabled           = false;
        Globals::timeLimitEnabled        = false;
        Globals::SilentModeEnabled       = false;
        Globals::manualBrightnessEnabled = false;
        Globals::WeatherUpdateEnabled    = false;
        Globals::PWM_disabled            = false;

        Globals::zipCode                 = "88457";
        Globals::currentFirmwareTarget   = Globals::FirmwareTarget::NixieV6_std;
        Globals::timeLimitFrom           = "06:00:00";
        Globals::timeLimitTo             = "00:00:00";

        Globals::logConfig               = 0;

        brightness                       = 0;

        PWM_PERIOD_US                    = 10000;

        Globals::currentTimeZone         = Globals::TimeZone::CET;

        saveGlobals();
    }

    void saveGlobals() noexcept
    {
        int flags =
            (Globals::tickerEnabled           ? 1 << 0 : 0) |
            (Globals::timeLimitEnabled        ? 1 << 1 : 0) |
            (Globals::SilentModeEnabled       ? 1 << 2 : 0) |
            (Globals::manualBrightnessEnabled ? 1 << 3 : 0) |
            (Globals::WeatherUpdateEnabled    ? 1 << 4 : 0) |
            (Globals::PWM_disabled            ? 1 << 5 : 0);
        storage.setIntValue(flags);

        storage.setFloatValue(static_cast<float>(Globals::logConfig));

        std::string packed =
            Globals::zipCode                                                    + "|" +
            Globals::HardwareVersion                                            + "|" +
            std::to_string(static_cast<int>(Globals::currentFirmwareTarget))    + "|" +
            Globals::timeLimitFrom                                              + "|" +
            Globals::timeLimitTo                                                + "|" +
            std::to_string(brightness)                                          + "|" +
            std::to_string(PWM_PERIOD_US)                                       + "|" +
            std::to_string(static_cast<int>(Globals::currentTimeZone));

        storage.setStringValue(packed);

        if (auto err = storage.save(); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL,"saveGlobals failed: %s",esp_err_to_name(err));
        }
    }

    void loadGlobals() noexcept
    {
        if (auto err = storage.load(); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL,"loadGlobals failed: %s",esp_err_to_name(err));
            return;
        }

        int flags = storage.getIntValue();
        Globals::tickerEnabled           = flags & (1 << 0);
        Globals::timeLimitEnabled        = flags & (1 << 1);
        Globals::SilentModeEnabled       = flags & (1 << 2);
        Globals::manualBrightnessEnabled = flags & (1 << 3);
        Globals::WeatherUpdateEnabled    = flags & (1 << 4);
        Globals::PWM_disabled            = flags & (1 << 5);

        Globals::logConfig = static_cast<uint32_t>(storage.getFloatValue());

        std::string packed = storage.getStringValue();

        size_t p1 = packed.find('|');
        size_t p2 = packed.find('|', p1 + 1);
        size_t p3 = packed.find('|', p2 + 1);
        size_t p4 = packed.find('|', p3 + 1);
        size_t p5 = packed.find('|', p4 + 1);
        size_t p6 = packed.find('|', p5 + 1);
        size_t p7 = packed.find('|', p6 + 1);

        if (p1!=std::string::npos && p2!=std::string::npos && p3!=std::string::npos && p4!=std::string::npos && p5!=std::string::npos && p6!=std::string::npos && p7!=std::string::npos)
        {
            Globals::zipCode                = packed.substr(0,       p1);
            Globals::HardwareVersion        = packed.substr(p1+1,   p2-p1-1);
            Globals::currentFirmwareTarget  = static_cast<Globals::FirmwareTarget>(std::stoi(packed.substr(p2+1, p3-p2-1)));
            Globals::timeLimitFrom          = packed.substr(p3+1,   p4-p3-1);
            Globals::timeLimitTo            = packed.substr(p4+1,   p5-p4-1);
            brightness                      = std::stoi(packed.substr(p5+1, p6-p5-1));
            PWM_PERIOD_US                   = std::stoul(packed.substr(p6+1, p7-p6-1));  
            Globals::currentTimeZone        = static_cast<Globals::TimeZone>(std::stoi(packed.substr(p7+1)));
        }
    }

    void StorageReset() noexcept
    {
        storage.resetGlobals();
        Logger::log(LoggerType::GENERAL, "Persistent storage reset and globals reloaded");
    }
}
