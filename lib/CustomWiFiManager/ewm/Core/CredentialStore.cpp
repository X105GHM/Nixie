#include "ewm/Core/CredentialStore.hpp"
#include <cstring>

namespace ewm
{
    bool CredentialStore::load(CredentialStorage &storage)
    {
        StorageHeader h{};
        CredentialArray d{};
        if (!storage.loadBest(h, d))
        {
            hdr_.count = 0;
            creds_ = {};
            return false;
        }
        hdr_ = h;
        creds_ = d;
        return true;
    }

    bool CredentialStore::save(CredentialStorage &storage)
    {
        StorageHeader h = hdr_;
        if (h.count > creds_.size())
            h.count = creds_.size();

        StorageHeader tmp = h;
        tmp.crc32 = 0;

        uint32_t crc = ewm::utils::crc32(reinterpret_cast<const uint8_t *>(&tmp), sizeof(tmp)) ^ ewm::utils::crc32(reinterpret_cast<const uint8_t *>(creds_.data()), sizeof(Credential) * creds_.size());
        h.crc32 = crc;

        hdr_ = h;
        return storage.saveBoth(hdr_, creds_);
    }

    std::vector<Credential> CredentialStore::list() const
    {
        std::vector<Credential> v;
        for (size_t i = 0; i < hdr_.count && i < creds_.size(); ++i)
            v.push_back(creds_[i]);
        return v;
    }

    bool CredentialStore::addOrUpdate(CredentialStorage &storage, const String &ssid, const String &password, uint8_t priority)
    {
        if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 64)
            return false;

        for (size_t i = 0; i < hdr_.count && i < creds_.size(); ++i)
        {
            if (ssid == creds_[i].ssid)
            {
                // Passwort nur überschreiben, wenn tatsächlich eins geliefert wurde
                if (password.length() > 0)
                {
                    strncpy(creds_[i].password, password.c_str(), sizeof(creds_[i].password) - 1);
                    creds_[i].password[sizeof(creds_[i].password) - 1] = '\0';
                }

                creds_[i].priority = priority;
                return save(storage);
            }
        }

        if (hdr_.count >= creds_.size())
            return false;

        auto &c = creds_[hdr_.count++];
        strncpy(c.ssid, ssid.c_str(), sizeof(c.ssid) - 1);
        c.ssid[sizeof(c.ssid) - 1] = '\0';

        strncpy(c.password, password.c_str(), sizeof(c.password) - 1);
        c.password[sizeof(c.password) - 1] = '\0';

        c.priority = priority;
        c.last_ok = 0;

        return save(storage);
    }

    bool CredentialStore::eraseAll(CredentialStorage &storage)
    {
        hdr_.count = 0;
        creds_ = {};
        return save(storage);
    }

    bool CredentialStore::remove(CredentialStorage &storage, const String &ssid)
    {
        for (size_t i = 0; i < hdr_.count && i < creds_.size(); ++i)
        {
            if (ssid == creds_[i].ssid)
            {
                const uint8_t removedPrio = creds_[i].priority;

                for (size_t j = i + 1; j < hdr_.count; ++j)
                    creds_[j - 1] = creds_[j];

                if (hdr_.count > 0)
                    creds_[hdr_.count - 1] = Credential{};

                hdr_.count--;

                for (size_t k = 0; k < hdr_.count && k < creds_.size(); ++k)
                {
                    if (creds_[k].priority > removedPrio)
                        creds_[k].priority--;
                }
                return save(storage);
            }
        }
        return false;
    }

    void CredentialStore::reorderBySsidList(CredentialStorage &storage, const std::vector<String> &order)
    {
        uint8_t p = 0;
        for (const auto &ss : order)
        {
            for (size_t i = 0; i < hdr_.count && i < creds_.size(); ++i)
            {
                if (ss == creds_[i].ssid)
                    creds_[i].priority = p++;
            }
        }
        save(storage);
    }
}
