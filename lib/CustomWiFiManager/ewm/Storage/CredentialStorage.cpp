#include "ewm/Storage/CredentialStorage.hpp"
#include "ewm/Utils/Crc32.hpp"
#include "nvs.h"

namespace ewm
{
    CredentialStorage::CredentialStorage(const char* primaryNs, const char* backupNs)
        : nsPrimary_(primaryNs), nsBackup_(backupNs) {}

    bool CredentialStorage::saveToNamespace(const char* ns, const StorageHeader& hdr, const CredentialArray& data)
    {
        nvs_handle_t handle{};
        esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
        if (err != ESP_OK)
        {
            return false;
        }

        err = nvs_set_blob(handle, "hdr", &hdr, sizeof(hdr));
        if (err == ESP_OK)
        {
            err = nvs_set_blob(handle, "data", data.data(), sizeof(Credential) * data.size());
        }
        if (err == ESP_OK)
        {
            err = nvs_commit(handle);
        }

        nvs_close(handle);
        return err == ESP_OK;
    }

    bool CredentialStorage::loadFromNamespace(const char* ns, StorageHeader& hdr, CredentialArray& data)
    {
        nvs_handle_t handle{};
        esp_err_t err = nvs_open(ns, NVS_READONLY, &handle);
        if (err != ESP_OK)
        {
            return false;
        }

        size_t gotH = 0;
        size_t gotD = 0;
        const esp_err_t headerSizeResult = nvs_get_blob(handle, "hdr", nullptr, &gotH);
        const esp_err_t dataSizeResult = nvs_get_blob(handle, "data", nullptr, &gotD);
        if (gotH != sizeof(StorageHeader) || gotD != sizeof(Credential) * data.size())
        {
            nvs_close(handle);
            return false;
        }

        if (headerSizeResult != ESP_OK || dataSizeResult != ESP_OK ||
            nvs_get_blob(handle, "hdr", &hdr, &gotH) != ESP_OK ||
            nvs_get_blob(handle, "data", data.data(), &gotD) != ESP_OK)
        {
            nvs_close(handle);
            return false;
        }
        nvs_close(handle);

        if (hdr.magic != MAGIC || hdr.version != 0x0001) return false;
        if (hdr.count > data.size()) return false;

        StorageHeader tmp = hdr;
        tmp.crc32 = 0;

        uint32_t crc = ewm::utils::crc32(reinterpret_cast<const uint8_t*>(&tmp), sizeof(tmp)) ^
                       ewm::utils::crc32(reinterpret_cast<const uint8_t*>(data.data()), sizeof(Credential) * data.size());

        return (crc == hdr.crc32);
    }

    bool CredentialStorage::saveBoth(const StorageHeader& hdr, const CredentialArray& data)
    {
        return saveToNamespace(nsPrimary_, hdr, data) && saveToNamespace(nsBackup_, hdr, data);
    }

    bool CredentialStorage::loadBest(StorageHeader& hdrOut, CredentialArray& dataOut)
    {
        StorageHeader hp{}, hb{};
        CredentialArray dp{}, db{};

        bool okp = loadFromNamespace(nsPrimary_, hp, dp);
        bool okb = loadFromNamespace(nsBackup_,  hb, db);

        if (!okp && !okb) return false;

        if (okp && (!okb || hp.crc32 == hb.crc32))
        {
            hdrOut = hp; dataOut = dp;
        }
        else if (okb && !okp)
        {
            hdrOut = hb; dataOut = db;
        }
        else
        {
            auto score = [](const StorageHeader& h, const CredentialArray& d)
            {
                uint32_t s = h.count;
                for (size_t i = 0; i < h.count && i < d.size(); ++i) s += d[i].last_ok;
                return s;
            };
            if (score(hp, dp) >= score(hb, db)) { hdrOut = hp; dataOut = dp; }
            else { hdrOut = hb; dataOut = db; }
        }

        // Reparatur: schreibt geladenen Stand wieder in beide (wenn eine Kopie mal kaputt war)
        const bool needsRepair = !okp || !okb || hp.crc32 != hb.crc32;
        if (needsRepair) saveBoth(hdrOut, dataOut);

        return true;
    }
}
