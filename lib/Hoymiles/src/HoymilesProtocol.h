// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

// Hoymiles sub-GHz protocol constants shared across radio backends
// (CMT2300A, SX1262, etc.)

#include <stdint.h>

// Frequency hopping channel parameters
// One step size is 2.5 kHz, offset of 100 steps gives 250 kHz channel spacing
#define HOYMILES_FH_STEP_SIZE 2500
#define HOYMILES_FH_OFFSET 100

enum FrequencyBand_t {
    BAND_860,
    BAND_900,
    FrequencyBand_Max,
};

inline constexpr uint32_t getHoymilesBaseFrequency(FrequencyBand_t band)
{
    switch (band) {
    case FrequencyBand_t::BAND_900:
        return 900000000U;
    case FrequencyBand_t::BAND_860:
    default:
        return 860000000U;
    }
}
