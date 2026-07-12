#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// Mapper 194 - pirackie MMC3 z 2 KB CHR-RAM (pages $00-$01). Bez osobnego
// CHR-RAM storage'u w kartridzu traktujemy jak czysty MMC3 (mapper 004).
class Mapper194 : public Mapper004 {
public:
    using Mapper004::Mapper004;
    const char* name() const override { return "pirackie MMC3 z 2 KB CHR-RAM (pages $00-$01). Bez osobnego"; }
};

REGISTER_MAPPER(194, Mapper194)
