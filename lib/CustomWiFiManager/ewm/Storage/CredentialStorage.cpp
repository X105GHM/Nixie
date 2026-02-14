#include "ewm/Storage/CredentialStorage.hpp"
#include "ewm/Utils/Crc32.hpp"

namespace ewm
{
    CredentialStorage::CredentialStorage(const char* primaryNs, const char* backupNs)
        : nsPrimary_(primaryNs), nsBackup_(backupNs) {}

    bool CredentialStorage::saveToNamespace(const char* ns, const StorageHeader& hdr, const CredentialArray& data)
    {
        Preferences p;
        if (!p.begin(ns, false)) return false;

        bool ok = true;
        ok &= (p.putBytes("hdr", &hdr, sizeof(hdr)) == sizeof(hdr));
        ok &= (p.putBytes("data", data.data(), sizeof(Credential) * data.size()) == sizeof(Credential) * data.size());
        p.end();
        return ok;
    }

    bool CredentialStorage::loadFromNamespace(const char* ns, StorageHeader& hdr, CredentialArray& data)
    {
        Preferences p;
        if (!p.begin(ns, true)) return false;

        size_t gotH = p.getBytesLength("hdr");
        size_t gotD = p.getBytesLength("data");
        if (gotH != sizeof(StorageHeader) || gotD != sizeof(Credential) * data.size())
        {
            p.end();
            return false;
        }

        p.getBytes("hdr", &hdr, sizeof(hdr));
        p.getBytes("data", data.data(), sizeof(Credential) * data.size());
        p.end();

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
        saveBoth(hdrOut, dataOut);
        return true;
    }
}
