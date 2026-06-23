#pragma once
#include "Mapper004.h"

// ----------------------------------------------------------------------------
// Mapper 074 - pirate MMC3 z 2 KB CHR-RAM (Di 4 Ci, Ji Jia Zhan Shi)
// ----------------------------------------------------------------------------
// Identyczny jak MMC3, ale CHR pages $08 i $09 wskazuja na CHR-RAM
// zamiast CHR-ROM. Dla uproszczenia traktujemy je jak normalne CHR-ROM strony;
// wiele typowych dumpow zawiera te strony w obrazie i przebudowuje VRAM
// recznie. To pozwala uzywac calej infrastruktury Mappera 004.
// ----------------------------------------------------------------------------
class Mapper074 : public Mapper004 {
public:
    using Mapper004::Mapper004;
};

REGISTER_MAPPER(74, Mapper074)
