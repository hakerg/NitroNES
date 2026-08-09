#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// Mapper 191 - pirackie MMC3 z dodatkowymi 2 KB CHR-RAM (bit7 CHR reg
// wybiera RAM). Brak osobnego storage'u CHR-RAM w naszym kartridzu, wiec
// traktujemy jak czysty MMC3 - wiekszosc grafiki dziala poprawnie z ROMu.
class Mapper191 : public Mapper004 {
public:
    using Mapper004::Mapper004;
    std::unique_ptr<Mapper> clone() const override { return std::make_unique<Mapper191>(*this); }
    const char* name() const override { return "pirackie MMC3 z dodatkowymi 2 KB CHR-RAM (bit7 CHR reg"; }
};

REGISTER_MAPPER(191, Mapper191)
