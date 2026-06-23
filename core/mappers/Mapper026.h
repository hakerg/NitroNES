#pragma once

#include "Mapper024.h"

// ============================================================================
// Konami VRC6b - Madara, Esper Dream 2.
// ----------------------------------------------------------------------------
// Identyczny z VRC6a z dokladnoscia do zamiany linii A0<->A1 dla rejestrow
// (czyli adresy $x001 i $x002 sa zamienione). Cala logika banking/IRQ/audio
// jest w VRC6Mapper (Mapper024.h) - tutaj tylko przelacznik wariantu.
// ============================================================================
class Mapper026 : public VRC6Mapper {
public:
    using VRC6Mapper::VRC6Mapper;

protected:
    bool swapA01() const override { return true; }
};

REGISTER_MAPPER(26, Mapper026)
