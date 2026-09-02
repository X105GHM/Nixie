#pragma once
#include <string>
#include <vector>
#include "ewm/Types.hpp"
#include "ewm/Storage/CredentialStorage.hpp"
#include "ewm/Utils/Crc32.hpp"

namespace ewm
{
    class CredentialStore
    {
    public:
        bool load(CredentialStorage& storage);
        bool save(CredentialStorage& storage);

        std::vector<Credential> list() const;

        bool addOrUpdate(CredentialStorage& storage, const std::string& ssid, const std::string& password, uint8_t priority);
        bool remove(CredentialStorage& storage, const std::string& ssid);
        bool eraseAll(CredentialStorage& storage);

        void reorderBySsidList(CredentialStorage& storage, const std::vector<std::string>& order);

        StorageHeader& header() { return hdr_; }
        const StorageHeader& header() const { return hdr_; }

        CredentialArray& data() { return creds_; }
        const CredentialArray& data() const { return creds_; }

    private:
        bool normalizePrioritiesIfNeeded_();

        StorageHeader hdr_{};
        CredentialArray creds_{};
    };
}
