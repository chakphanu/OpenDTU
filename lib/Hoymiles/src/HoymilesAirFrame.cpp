// SPDX-License-Identifier: GPL-2.0-or-later
#include "HoymilesAirFrame.h"

namespace {
constexpr uint16_t PolyX3X1_1 = 0x0B; // x^3 + x + 1
constexpr uint16_t PolyX3X2_1 = 0x0D; // x^3 + x^2 + 1
constexpr uint16_t CrcPolyIbm = 0x8005;

uint16_t getFecPoly(HoymilesAirFrame::FecPolynomial poly)
{
    return poly == HoymilesAirFrame::FecPolynomial::X3_X1_1 ? PolyX3X1_1 : PolyX3X2_1;
}

uint8_t popcount7(uint8_t value)
{
    value &= 0x7F;
    uint8_t count = 0;
    while (value != 0) {
        count += value & 0x01;
        value >>= 1;
    }
    return count;
}
} // namespace

bool HoymilesAirFrame::encodeCmtPacket(const uint8_t* logical, size_t logicalLen, uint8_t* coded, size_t codedCapacity, size_t& codedLen, FecPolynomial poly)
{
    codedLen = 0;
    if (logical == nullptr || coded == nullptr || logicalLen == 0 || logicalLen > MaxLogicalBytes) {
        return false;
    }

    uint8_t decoded[MaxDecodedBytes] = { 0 };
    decoded[0] = static_cast<uint8_t>(logicalLen);
    for (size_t i = 0; i < logicalLen; ++i) {
        decoded[1 + i] = logical[i];
    }

    const uint16_t airCrc = crc16IbmMsb(decoded, logicalLen + AirLengthBytes);
    decoded[1 + logicalLen] = static_cast<uint8_t>((airCrc >> 8) & 0xFF);
    decoded[2 + logicalLen] = static_cast<uint8_t>(airCrc & 0xFF);

    const size_t decodedLen = logicalLen + AirLengthBytes + AirCrcBytes;
    const size_t requiredBits = decodedLen * 14U;
    const size_t requiredBytes = (requiredBits + 7U) / 8U;
    if (requiredBytes > codedCapacity) {
        return false;
    }

    for (size_t i = 0; i < requiredBytes; ++i) {
        coded[i] = 0x00;
    }

    size_t bitPos = 0;
    for (size_t i = 0; i < decodedLen; ++i) {
        const uint8_t nibbles[2] = {
            static_cast<uint8_t>((decoded[i] >> 4) & 0x0F),
            static_cast<uint8_t>(decoded[i] & 0x0F)
        };

        for (uint8_t nibble : nibbles) {
            const uint8_t codeword = encodeNibble(nibble, poly);
            for (int bit = 6; bit >= 0; --bit) {
                const uint8_t outBit = (codeword >> bit) & 0x01;
                const size_t outByte = bitPos / 8U;
                const uint8_t outShift = 7U - static_cast<uint8_t>(bitPos % 8U);
                coded[outByte] |= static_cast<uint8_t>(outBit << outShift);
                ++bitPos;
            }
        }
    }

    codedLen = requiredBytes;
    return true;
}

HoymilesAirFrame::DecodeResult HoymilesAirFrame::decodeCmtPacket(const uint8_t* coded, size_t codedLen, uint8_t* logical, size_t logicalCapacity, FecPolynomial poly)
{
    DecodeResult result;
    if (coded == nullptr || logical == nullptr || codedLen == 0 || logicalCapacity < MaxLogicalBytes) {
        return result;
    }

    uint8_t decoded[MaxDecodedBytes] = { 0 };
    size_t decodedLen = 0;
    int pendingNibble = -1;
    size_t expectedDecodedBytes = 0;
    bool correctedAny = false;

    size_t bitPos = 0;
    while (bitPos + 7U <= codedLen * 8U && decodedLen < MaxDecodedBytes) {
        uint8_t codeword = 0;
        for (uint8_t i = 0; i < 7; ++i) {
            const size_t inByte = (bitPos + i) / 8U;
            const uint8_t inShift = 7U - static_cast<uint8_t>((bitPos + i) % 8U);
            codeword = static_cast<uint8_t>((codeword << 1) | ((coded[inByte] >> inShift) & 0x01));
        }
        bitPos += 7U;

        uint8_t nibble = 0;
        bool corrected = false;
        if (!decodeNibble(codeword, nibble, corrected, poly)) {
            return result;
        }
        correctedAny = correctedAny || corrected;

        if (pendingNibble < 0) {
            pendingNibble = nibble;
            continue;
        }

        decoded[decodedLen++] = static_cast<uint8_t>((pendingNibble << 4) | nibble);
        pendingNibble = -1;

        if (decodedLen == 1) {
            const size_t logicalLen = decoded[0];
            if (logicalLen == 0 || logicalLen > MaxLogicalBytes) {
                return result;
            }
            expectedDecodedBytes = AirLengthBytes + logicalLen + AirCrcBytes;
        }

        if (expectedDecodedBytes != 0 && decodedLen >= expectedDecodedBytes) {
            break;
        }
    }

    if (decodedLen < AirLengthBytes + AirCrcBytes) {
        return result;
    }

    const size_t logicalLen = decoded[0];
    const size_t totalDecodedBytes = AirLengthBytes + logicalLen + AirCrcBytes;
    if (decodedLen < totalDecodedBytes) {
        return result;
    }

    const uint16_t crcExpected = (static_cast<uint16_t>(decoded[1 + logicalLen]) << 8) | decoded[2 + logicalLen];
    const uint16_t crcCalculated = crc16IbmMsb(decoded, AirLengthBytes + logicalLen);
    if (crcExpected != crcCalculated) {
        return result;
    }

    for (size_t i = 0; i < logicalLen; ++i) {
        logical[i] = decoded[1 + i];
    }

    result.success = true;
    result.corrected = correctedAny;
    result.airCrcValid = true;
    result.logicalLength = logicalLen;
    result.decodedBytes = totalDecodedBytes;
    return result;
}

uint16_t HoymilesAirFrame::crc16IbmMsb(const uint8_t* data, size_t len, uint16_t initial)
{
    uint16_t crc = initial;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000) {
                crc = static_cast<uint16_t>((crc << 1) ^ CrcPolyIbm);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

uint8_t HoymilesAirFrame::encodeNibble(uint8_t nibble, FecPolynomial poly)
{
    const uint16_t generator = getFecPoly(poly);
    uint16_t working = static_cast<uint16_t>((nibble & 0x0F) << 3);

    for (int bit = 6; bit >= 3; --bit) {
        if (working & (1U << bit)) {
            working ^= static_cast<uint16_t>(generator << (bit - 3));
        }
    }

    return static_cast<uint8_t>(((nibble & 0x0F) << 3) | (working & 0x07));
}

bool HoymilesAirFrame::decodeNibble(uint8_t codeword, uint8_t& nibble, bool& corrected, FecPolynomial poly)
{
    corrected = false;
    codeword &= 0x7F;

    for (uint8_t candidate = 0; candidate < 16; ++candidate) {
        const uint8_t encoded = encodeNibble(candidate, poly);
        if (encoded == codeword) {
            nibble = candidate;
            return true;
        }
    }

    uint8_t bestCandidate = 0;
    uint8_t bestDistance = 8;
    bool uniqueBest = false;
    for (uint8_t candidate = 0; candidate < 16; ++candidate) {
        const uint8_t encoded = encodeNibble(candidate, poly);
        const uint8_t distance = popcount7(static_cast<uint8_t>(encoded ^ codeword));
        if (distance < bestDistance) {
            bestDistance = distance;
            bestCandidate = candidate;
            uniqueBest = true;
        } else if (distance == bestDistance) {
            uniqueBest = false;
        }
    }

    if (uniqueBest && bestDistance == 1) {
        nibble = bestCandidate;
        corrected = true;
        return true;
    }

    return false;
}
