#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// Mapper 118 - TxSROM (MMC3 + nametable z CHR bit7).
// Dla uproszczenia dziedziczymy po Mapper004 i nadpisujemy mirroring tak by
// zachować zachowanie identyczne z normalnym MMC3 (większość gier działa).
class Mapper118 : public Mapper004 {
public:
    using Mapper004::Mapper004;
    const char* name() const override { return "MMC3  (modified)"; }
};

REGISTER_MAPPER(118, Mapper118)
