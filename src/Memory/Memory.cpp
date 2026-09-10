#include "Memory.hpp"
#include "Logger/Logger.hpp"
#include "Globals/Globals.hpp"

#include <utility>

namespace Memory
{
    static PersistentStorage storage;

    esp_err_t PersistentStorage::init() noexcept
    {
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            err = nvs_flash_erase();
            if (err != ESP_OK) return err;
            err = nvs_flash_init();
        }
        if (err != ESP_OK) {
            Logger::log(LoggerType::STORAGE, "NVS init failed: %s", esp_err_to_name(err));
        }
        return err;
    }

    esp_err_t PersistentStorage::load() noexcept
    {
        hasFloatValue_ = false;
        hasLogConfigValue_ = false;
        nvs_handle_t handle;
        esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
        if (err != ESP_OK) return err;

        int32_t i;
        if ((err = nvs_get_i32(handle, KEY_INT, &i)) == ESP_OK) intValue_ = i;

        int32_t f;
        if ((err = nvs_get_i32(handle, KEY_FLOAT, &f)) == ESP_OK)
        {
            floatValue_ = f / 10000.0f;
            hasFloatValue_ = true;
        }

        // New firmware stores the mask as an integer.  The legacy float key
        // above remains readable so existing devices retain their settings.
        uint32_t logConfig = 0;
        if (nvs_get_u32(handle, "log_mask", &logConfig) == ESP_OK)
        {
            logConfigValue_ = logConfig;
            hasLogConfigValue_ = true;
        }

        size_t len = 0;
        if ((err = nvs_get_str(handle, KEY_STRING, nullptr, &len)) == ESP_OK) {
            std::string buf(len, '\0');
            err = nvs_get_str(handle, KEY_STRING, buf.data(), &len);
            if (err != ESP_OK) { nvs_close(handle); return err; }
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

        err = nvs_set_i32(handle, KEY_INT, intValue_);
        if (err != ESP_OK) { nvs_close(handle); return err; }
        err = nvs_set_i32(handle, KEY_FLOAT, static_cast<int32_t>(floatValue_ * 10000));
        if (err != ESP_OK) { nvs_close(handle); return err; }
        err = nvs_set_u32(handle, "log_mask", logConfigValue_);
        if (err != ESP_OK) { nvs_close(handle); return err; }

        const auto textConfig = Globals::getTextConfig();
        std::string packed =
            textConfig.zipCode                                                  + "|" +
            textConfig.hardwareVersion                                          + "|" +
            std::to_string(static_cast<int>(Globals::currentFirmwareTarget.load())) + "|" +
            textConfig.timeLimitFrom                                            + "|" +
            textConfig.timeLimitTo                                              + "|" +
            std::to_string(brightness.load())                                   + "|" +
            std::to_string(PWM_PERIOD_US.load())                                + "|" +
            std::to_string(static_cast<int>(Globals::currentTimeZone.load()))   + "|" +
            std::to_string(Globals::brightnessNightStartHour)                   + "|" +
            std::to_string(Globals::brightnessNightEndHour)                     + "|" +
            std::to_string(Globals::brightnessDimStartHour)                     + "|" +
            std::to_string(Globals::brightnessDimEndHour)                       + "|" +
            std::to_string(Globals::brightnessNightValue)                       + "|" +
            std::to_string(Globals::brightnessDimValue)                         + "|" +
            std::to_string(Globals::brightnessDayValue);

        err = nvs_set_str(handle, KEY_STRING, packed.c_str());
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
        return err;
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
            err = nvs_get_str(handle, KEY_LAST_EVENT, buf.data(), &len);
            if (err != ESP_OK) { nvs_close(handle); return err; }
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
    bool PersistentStorage::hasFloatValue() const noexcept    { return hasFloatValue_; }

    void PersistentStorage::setLogConfigValue(uint32_t val) noexcept
    {
        logConfigValue_ = val;
        hasLogConfigValue_ = true;
    }

    uint32_t PersistentStorage::getLogConfigValue() const noexcept { return logConfigValue_; }
    bool PersistentStorage::hasLogConfigValue() const noexcept     { return hasLogConfigValue_; }

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
            Logger::log(LoggerType::STORAGE,"resetGlobals: erase failed: %s",esp_err_to_name(err));
        }

        Globals::tickerEnabled           = false;
        Globals::timeLimitEnabled        = false;
        Globals::SilentModeEnabled       = false;
        Globals::manualBrightnessEnabled = false;
        Globals::WeatherUpdateEnabled    = false;
        Globals::PWM_disabled            = false;
        Globals::noACPatNight            = false;

        auto textConfig = Globals::getTextConfig();
        textConfig.zipCode               = "88457";
        Globals::currentFirmwareTarget   = Globals::FirmwareTarget::NixieV6_std;
        textConfig.timeLimitFrom         = "06:00:00";
        textConfig.timeLimitTo           = "00:00:00";
        Globals::setTextConfig(std::move(textConfig));

        Globals::brightnessNightStartHour = 22;
        Globals::brightnessNightEndHour   = 6;
        Globals::brightnessDimStartHour   = 20;
        Globals::brightnessDimEndHour     = 8;

        Globals::brightnessNightValue     = 15;
        Globals::brightnessDimValue       = 75;
        Globals::brightnessDayValue       = 100;

        Globals::logConfig               = 0;
        Globals::applyLogConfig();

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

        const uint32_t logConfig = Globals::logConfig.load(std::memory_order_relaxed);
        storage.setLogConfigValue(logConfig);
        // Keep writing the old key for downgrade compatibility with older
        // firmware that only knows the float-based representation.
        storage.setFloatValue(static_cast<float>(logConfig));

        const auto textConfig = Globals::getTextConfig();
        std::string packed =
            textConfig.zipCode                                                  + "|" +
            textConfig.hardwareVersion                                          + "|" +
            std::to_string(static_cast<int>(Globals::currentFirmwareTarget.load())) + "|" +
            textConfig.timeLimitFrom                                            + "|" +
            textConfig.timeLimitTo                                              + "|" +
            std::to_string(brightness.load())                                   + "|" +
            std::to_string(PWM_PERIOD_US.load())                                + "|" +
            std::to_string(static_cast<int>(Globals::currentTimeZone.load()))   + "|" +
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
            Logger::log(LoggerType::STORAGE,"saveGlobals failed: %s",esp_err_to_name(err));
        }
    }

    void loadGlobals() noexcept
    {
        if (auto err = storage.load(); err != ESP_OK)
        {
            Logger::log(LoggerType::STORAGE,"loadGlobals failed: %s",esp_err_to_name(err));
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

        const bool hasLegacyLogConfig = storage.hasFloatValue();
        const uint32_t legacyLogConfig = hasLegacyLogConfig
            ? static_cast<uint32_t>(storage.getFloatValue())
            : 0U;
        if (storage.hasLogConfigValue() &&
            (!hasLegacyLogConfig || legacyLogConfig == storage.getLogConfigValue()))
        {
            Globals::logConfig = storage.getLogConfigValue() & Globals::LOG_CONFIG_MASK;
        }
        else if (hasLegacyLogConfig)
        {
            // A downgrade can update only the legacy float key.  Prefer that
            // changed value instead of resurrecting a stale extended mask.
            Globals::logConfig = legacyLogConfig & Globals::LOG_CONFIG_MASK;
        }
        else
        {
            Globals::logConfig = Globals::allLogBits();
        }

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
            auto textConfig = Globals::getTextConfig();
            textConfig.zipCode             = packed.substr(0, pos[0]);
            textConfig.hardwareVersion     = packed.substr(pos[0] + 1, pos[1] - pos[0] - 1);
            Globals::currentFirmwareTarget = static_cast<Globals::FirmwareTarget>(std::stoi(packed.substr(pos[1] + 1, pos[2] - pos[1] - 1)));
            textConfig.timeLimitFrom       = packed.substr(pos[2] + 1, pos[3] - pos[2] - 1);
            textConfig.timeLimitTo         = packed.substr(pos[3] + 1, pos[4] - pos[3] - 1);
            Globals::setTextConfig(std::move(textConfig));
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
        Logger::log(LoggerType::STORAGE, "Persistent storage reset and globals reloaded");
    }

    void ReadBrownoutLog(std::string &outJson) noexcept
    {
        if (auto err = storage.getLastEventLog(outJson); err != ESP_OK)
        {
            Logger::log(LoggerType::STORAGE, "ReadBrownoutLog failed: %s", esp_err_to_name(err));
        }
    }

    void BrownoutReset() noexcept
    {
        if (auto err = storage.deleteEventLog(); err != ESP_OK)
        {
            Logger::log(LoggerType::STORAGE, "BrownoutReset failed: %s", esp_err_to_name(err));
        }
        else
        {
            Logger::log(LoggerType::STORAGE, "Brownout log cleared");
        }
    }
}
