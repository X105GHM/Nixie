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
            std::to_string(static_cast<int>(Globals::currentTimeZone))          + "|" +
            std::to_string(Globals::brightnessNightStartHour)                   + "|" +
            std::to_string(Globals::brightnessNightEndHour)                     + "|" +
            std::to_string(Globals::brightnessDimStartHour)                     + "|" +
            std::to_string(Globals::brightnessDimEndHour)                       + "|" +
            std::to_string(Globals::brightnessNightValue)                       + "|" +
            std::to_string(Globals::brightnessDimValue)                         + "|" +
            std::to_string(Globals::brightnessDayValue);

        nvs_set_str(handle, KEY_STRING, packed.c_str());
        nvs_commit(handle);
        nvs_close(handle);
        return ESP_OK;
    }

    esp_err_t PersistentStorage::saveEventLog(const std::string &json) noexcept
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) return err;

        err = nvs_set_str(handle, KEY_LAST_EVENT, json.c_str());
        if (err == ESP_OK) 
        {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
        return err;
    }

    esp_err_t PersistentStorage::getLastEventLog(std::string &outJson) noexcept
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
        if (err != ESP_OK) return err;

        size_t len = 0;
        if ((err = nvs_get_str(handle, KEY_LAST_EVENT, nullptr, &len)) == ESP_OK) 
        {
            std::string buf(len, '\0');
            nvs_get_str(handle, KEY_LAST_EVENT, buf.data(), &len);
            outJson = buf;
        }
        nvs_close(handle);
        return err;
    }

    esp_err_t PersistentStorage::deleteEventLog() noexcept
    {
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
        if (err != ESP_OK) return err;

        err = nvs_erase_key(handle, KEY_LAST_EVENT);
        if (err == ESP_OK) 
        {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
        return err;
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
        Globals::noACPatNight            = false;

        Globals::zipCode                 = "88457";
        Globals::currentFirmwareTarget   = Globals::FirmwareTarget::NixieV6_std;
        Globals::timeLimitFrom           = "06:00:00";
        Globals::timeLimitTo             = "00:00:00";

        Globals::brightnessNightStartHour = 22;
        Globals::brightnessNightEndHour   = 6;
        Globals::brightnessDimStartHour   = 20;
        Globals::brightnessDimEndHour     = 8;

        Globals::brightnessNightValue     = 15;
        Globals::brightnessDimValue       = 75;
        Globals::brightnessDayValue       = 100;

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
            (Globals::PWM_disabled            ? 1 << 5 : 0) |
            (Globals::noACPatNight            ? 1 << 6 : 0);
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
            std::to_string(static_cast<int>(Globals::currentTimeZone))          + "|" +
            std::to_string(Globals::brightnessNightStartHour)                   + "|" +
            std::to_string(Globals::brightnessNightEndHour)                     + "|" +
            std::to_string(Globals::brightnessDimStartHour)                     + "|" +
            std::to_string(Globals::brightnessDimEndHour)                       + "|" +
            std::to_string(Globals::brightnessNightValue)                       + "|" +
            std::to_string(Globals::brightnessDimValue)                         + "|" +
            std::to_string(Globals::brightnessDayValue);

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
        Globals::noACPatNight            = flags & (1 << 6);

        Globals::logConfig = static_cast<uint32_t>(storage.getFloatValue());

        std::string packed = storage.getStringValue();

        std::vector<size_t> pos;
        size_t start = 0;
        while (true)
        {
            size_t p = packed.find('|', start);
            if (p == std::string::npos) break;
            pos.push_back(p);
           start = p + 1;
        }

        if (pos.size() >= 7)
        {
            Globals::zipCode               = packed.substr(0, pos[0]);
            Globals::HardwareVersion       = packed.substr(pos[0] + 1, pos[1] - pos[0] - 1);
            Globals::currentFirmwareTarget = static_cast<Globals::FirmwareTarget>(std::stoi(packed.substr(pos[1] + 1, pos[2] - pos[1] - 1)));
            Globals::timeLimitFrom         = packed.substr(pos[2] + 1, pos[3] - pos[2] - 1);
            Globals::timeLimitTo           = packed.substr(pos[3] + 1, pos[4] - pos[3] - 1);
            brightness                     = std::stoi(packed.substr(pos[4] + 1, pos[5] - pos[4] - 1));
            PWM_PERIOD_US                  = std::stoul(packed.substr(pos[5] + 1, pos[6] - pos[5] - 1));
            Globals::currentTimeZone       = static_cast<Globals::TimeZone>(std::stoi(packed.substr(pos[6] + 1, (pos.size() >= 8 ? pos[7] : packed.size()) - pos[6] - 1)));
        }

        // Neue Felder nur laden, wenn sie vorhanden sind.
        // Sonst bleiben die Defaultwerte aus Globals.cpp erhalten.
        if (pos.size() >= 14)
        {
            Globals::brightnessNightStartHour = static_cast<uint8_t>(std::stoi(packed.substr(pos[7] + 1,  pos[8]  - pos[7]  - 1)));
            Globals::brightnessNightEndHour   = static_cast<uint8_t>(std::stoi(packed.substr(pos[8] + 1,  pos[9]  - pos[8]  - 1)));
            Globals::brightnessDimStartHour   = static_cast<uint8_t>(std::stoi(packed.substr(pos[9] + 1,  pos[10] - pos[9]  - 1)));
            Globals::brightnessDimEndHour     = static_cast<uint8_t>(std::stoi(packed.substr(pos[10] + 1, pos[11] - pos[10] - 1)));
            Globals::brightnessNightValue     = static_cast<uint8_t>(std::stoi(packed.substr(pos[11] + 1, pos[12] - pos[11] - 1)));
            Globals::brightnessDimValue       = static_cast<uint8_t>(std::stoi(packed.substr(pos[12] + 1, pos[13] - pos[12] - 1)));
            Globals::brightnessDayValue       = static_cast<uint8_t>(std::stoi(packed.substr(pos[13] + 1)));
        }
    }

    void StorageReset() noexcept
    {
        storage.resetGlobals();
        Logger::log(LoggerType::GENERAL, "Persistent storage reset and globals reloaded");
    }

    void ReadBrownoutLog(std::string &outJson) noexcept
    {
        if (auto err = storage.getLastEventLog(outJson); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL, "ReadBrownoutLog failed: %s", esp_err_to_name(err));
        }
    }

    void BrownoutReset() noexcept
    {
        if (auto err = storage.deleteEventLog(); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL, "BrownoutReset failed: %s", esp_err_to_name(err));
        }
        else
        {
            Logger::log(LoggerType::GENERAL, "Brownout log cleared");
        }
    }
}