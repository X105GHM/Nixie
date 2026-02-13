#pragma once
#include <Preferences.h>
#include "ewm/Types.hpp"

namespace ewm
{
    class CredentialStorage
    {
    public:
        CredentialStorage(const char* primaryNs, const char* backupNs);

        bool loadBest(StorageHeader& hdrOut, CredentialArray& dataOut);

        bool saveBoth(const StorageHeader& hdr, const CredentialArray& data);

    private:
        bool saveToNamespace(const char* ns, const StorageHeader& hdr, const CredentialArray& data);
        bool loadFromNamespace(const char* ns, StorageHeader& hdr, CredentialArray& data);

        const char* nsPrimary_;
        const char* nsBackup_;
    };
}
