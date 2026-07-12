#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// Mapper 192 - pirackie MMC3 z 4 KB CHR-RAM (pages $08-$0B). Bez osobnego
// CHR-RAM storage'u w kartridzu traktujemy jak czysty MMC3 (mapper 004).
class Mapper192 : public Mapper004 {
public:
    using Mapper004::Mapper004;
    const char* name() const override { return "pirackie MMC3 z 4 KB CHR-RAM (pages $08-$0B). Bez osobnego"; }
};

REGISTER_MAPPER(192, Mapper192)
