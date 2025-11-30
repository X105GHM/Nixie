#pragma once

#include <string>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "Logger/Logger.hpp"
#include "Globals/Globals.hpp"
#include "digits/Digits.hpp"

namespace Memory
{
    class PersistentStorage
    {
    public:
        esp_err_t init() noexcept;
        esp_err_t load() noexcept;
        esp_err_t save() noexcept;
        esp_err_t clearAll() noexcept;
        esp_err_t saveEventLog(const std::string &json) noexcept;
        esp_err_t getLastEventLog(std::string &outJson) noexcept;
        esp_err_t deleteEventLog() noexcept;

        void resetGlobals() noexcept;

        void setIntValue(int val) noexcept;
        int getIntValue() const noexcept;

        void setFloatValue(float val) noexcept;
        float getFloatValue() const noexcept;

        void setStringValue(const std::string &val) noexcept;
        std::string getStringValue() const noexcept;

    private:
        static constexpr const char *NVS_NAMESPACE  = "storage";
        static constexpr const char *KEY_INT        = "int_val";
        static constexpr const char *KEY_FLOAT      = "float_val";
        static constexpr const char *KEY_STRING     = "str_val";
        static constexpr char KEY_LAST_EVENT[]      = "last_event";

        int intValue_;
        float floatValue_;
        std::string stringValue_;
    };

    void loadGlobals() noexcept;

    void saveGlobals() noexcept;

    void StorageReset() noexcept;

    void ReadBrownoutLog(std::string &outJson) noexcept;

    void BrownoutReset() noexcept;

}