#include "Memory.hpp"
#include "Logger/Logger.hpp"
#include "Globals/Globals.hpp"

namespace Memory
{
    static PersistentStorage storage;

    // Persistenz initialisieren (falls extern benötigt)
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
        nvs_set_str(handle, KEY_STRING, stringValue_.c_str());
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

    void saveGlobals() noexcept
    {
        int flags =
            (Globals::tickerEnabled           ? 1 << 0 : 0) |
            (Globals::timeLimitEnabled        ? 1 << 1 : 0) |
            (Globals::SilentModeEnabled       ? 1 << 2 : 0) |
            (Globals::manualBrightnessEnabled ? 1 << 3 : 0) |
            (Globals::WeatherUpdateEnabled    ? 1 << 4 : 0);
        storage.setIntValue(flags);

        storage.setFloatValue(static_cast<float>(Globals::logConfig));

        std::string packed = Globals::zipCode + "|" +
                             Globals::HardwareVersion + "|" +
                             std::to_string(static_cast<int>(Globals::currentFirmwareTarget));
        storage.setStringValue(packed);

        if (auto err = storage.save(); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL,
                        "saveGlobals failed: %s",
                        esp_err_to_name(err));
        }
    }

    void loadGlobals() noexcept
    {
        if (auto err = storage.load(); err != ESP_OK)
        {
            Logger::log(LoggerType::GENERAL,
                        "loadGlobals failed: %s",
                        esp_err_to_name(err));
            return;
        }

        int flags = storage.getIntValue();
        Globals::tickerEnabled           = flags & (1 << 0);
        Globals::timeLimitEnabled        = flags & (1 << 1);
        Globals::SilentModeEnabled       = flags & (1 << 2);
        Globals::manualBrightnessEnabled = flags & (1 << 3);
        Globals::WeatherUpdateEnabled    = flags & (1 << 4);

        Globals::logConfig = static_cast<uint32_t>(storage.getFloatValue());

        std::string packed = storage.getStringValue();
        size_t p1 = packed.find('|');
        size_t p2 = packed.find('|', p1 + 1);
        if (p1 != std::string::npos && p2 != std::string::npos)
        {
            Globals::zipCode              = packed.substr(0, p1);
            Globals::HardwareVersion      = packed.substr(p1 + 1, p2 - p1 - 1);
            Globals::currentFirmwareTarget =
                static_cast<Globals::FirmwareTarget>(std::stoi(packed.substr(p2 + 1)));
        }
    }
}
