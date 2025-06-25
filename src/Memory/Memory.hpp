#pragma once

#include <string>
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "Logger/Logger.hpp"
#include "Globals/Globals.hpp"

namespace Memory
{
    class PersistentStorage
    {
    public:
        esp_err_t init() noexcept;
        esp_err_t load() noexcept;
        esp_err_t save() noexcept;
        esp_err_t clearAll() noexcept;

        void resetGlobals() noexcept;

        void setIntValue(int val) noexcept;
        int getIntValue() const noexcept;

        void setFloatValue(float val) noexcept;
        float getFloatValue() const noexcept;

        void setStringValue(const std::string &val) noexcept;
        std::string getStringValue() const noexcept;

    private:
        static constexpr const char *NVS_NAMESPACE = "storage";
        static constexpr const char *KEY_INT = "int_val";
        static constexpr const char *KEY_FLOAT = "float_val";
        static constexpr const char *KEY_STRING = "str_val";

        int intValue_;
        float floatValue_;
        std::string stringValue_;
    };

    void loadGlobals() noexcept;

    void saveGlobals() noexcept;

    void StorageReset() noexcept;
}