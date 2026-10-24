#pragma once
#include "PetCore.h"
#include <cstddef>
#include <cstring>

namespace pet {
inline constexpr size_t legacySettingsSize = 24;
static_assert(offsetof(Settings,randomPhrases)==legacySettingsSize,"V1 settings prefix changed");
static_assert(sizeof(Settings)==28,"Unexpected settings layout");
inline bool restoreSettings(unsigned version, const void* data, size_t size, Settings& output) {
    if (!data) return false;
    if ((version==1 && size==legacySettingsSize) || (version==2 && size==sizeof(Settings))) {
        Settings decoded;
        std::memcpy(&decoded,data,size);
        decoded.sanitize(); output=decoded; return true;
    }
    return false;
}
}
