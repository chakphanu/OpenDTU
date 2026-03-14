// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "HoymilesRadio.h"
#include <HoymilesProtocol.h>
#include <vector>

enum CountryModeId_t {
    MODE_EU,
    MODE_US,
    MODE_BR,
    MODE_TH,
    CountryModeId_Max
};

struct CountryFrequencyDefinition_t {
    FrequencyBand_t Band;
    uint32_t Freq_Min;
    uint32_t Freq_Max;
    uint32_t Freq_Legal_Min;
    uint32_t Freq_Legal_Max;
    uint32_t Freq_Default;
    uint32_t Freq_StartUp;
};

struct CountryFrequencyList_t {
    CountryModeId_t mode;
    CountryFrequencyDefinition_t definition;
};

class HoymilesRadio_SubGhz : public HoymilesRadio {
public:
    virtual void setPALevel(int8_t paLevel) = 0;
    virtual void setInverterTargetFrequency(uint32_t frequency) = 0;
    virtual uint32_t getInverterTargetFrequency() const = 0;

    virtual bool isConnected() const = 0;

    virtual uint32_t getMinFrequency() const = 0;
    virtual uint32_t getMaxFrequency() const = 0;

    virtual CountryModeId_t getCountryMode() const = 0;
    virtual void setCountryMode(CountryModeId_t mode) = 0;

    virtual uint32_t getInvBootFrequency() const = 0;

    virtual uint32_t getFrequencyFromChannel(uint8_t channel) const = 0;
    virtual uint8_t getChannelFromFrequency(uint32_t frequency) const = 0;

    virtual std::vector<CountryFrequencyList_t> getCountryFrequencyList() const = 0;
};
