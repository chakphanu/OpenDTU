// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "types.h"
#include <cstddef>
#include <cstdint>

class HoymilesAirFrame {
public:
    enum class FecPolynomial : uint8_t {
        X3_X1_1 = 0,
        X3_X2_1 = 1,
    };

    struct DecodeResult {
        bool success = false;
        bool corrected = false;
        bool airCrcValid = false;
        size_t logicalLength = 0;
        size_t decodedBytes = 0;
    };

    static constexpr size_t AirLengthBytes = 1;
    static constexpr size_t AirCrcBytes = 2;
    static constexpr size_t MaxLogicalBytes = MAX_RF_PAYLOAD_SIZE;
    static constexpr size_t MaxDecodedBytes = AirLengthBytes + MaxLogicalBytes + AirCrcBytes;
    static constexpr size_t MaxCodedBytes = ((MaxDecodedBytes * 14U) + 7U) / 8U;

    static bool encodeCmtPacket(const uint8_t* logical, size_t logicalLen, uint8_t* coded, size_t codedCapacity, size_t& codedLen,
        FecPolynomial poly = FecPolynomial::X3_X2_1);
    static DecodeResult decodeCmtPacket(const uint8_t* coded, size_t codedLen, uint8_t* logical, size_t logicalCapacity,
        FecPolynomial poly = FecPolynomial::X3_X2_1);

private:
    static uint16_t crc16IbmMsb(const uint8_t* data, size_t len, uint16_t initial = 0x0000);
    static uint8_t encodeNibble(uint8_t nibble, FecPolynomial poly);
    static bool decodeNibble(uint8_t codeword, uint8_t& nibble, bool& corrected, FecPolynomial poly);
};
