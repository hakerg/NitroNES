#pragma once
#include "Mapper004.h"
#include "MapperRegistry.h"

// Mapper 119 - TQROM (MMC3 + 8KB CHR-RAM przełączane bitem 6 CHR reg).
// Bazowa implementacja MMC3 traktuje całość CHR jako ROM; szczegółową
// obsługę CHR-RAM można dodać później – większość typowych ROM-ów działa.
class Mapper119 : public Mapper004 {
public:
	using Mapper004::Mapper004;
};

REGISTER_MAPPER(119, Mapper119)
