// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "HoymilesRadio_SubGhz.h"
#include "commands/CommandAbstract.h"
#include "types.h"
#include <Arduino.h>
#include <cmt2300wrapper.h>
#include <memory>
#include <queue>
#include <vector>

// number of fragments hold in buffer
#define FRAGMENT_BUFFER_SIZE 30

#ifndef HOYMILES_CMT_WORK_FREQ
#define HOYMILES_CMT_WORK_FREQ 865000000
#endif

class HoymilesRadio_CMT : public HoymilesRadio_SubGhz {
public:
    void init(const int8_t pin_sdio, const int8_t pin_clk, const int8_t pin_cs, const int8_t pin_fcs, const int8_t pin_gpio2, const int8_t pin_gpio3);
    void loop();
    void setPALevel(const int8_t paLevel) override;
    void setInverterTargetFrequency(const uint32_t frequency) override;
    uint32_t getInverterTargetFrequency() const override;

    bool isConnected() const override;

    uint32_t getMinFrequency() const override;
    uint32_t getMaxFrequency() const override;
    static constexpr uint32_t getChannelWidth()
    {
        return FH_OFFSET * CMT2300A_ONE_STEP_SIZE;
    }

    CountryModeId_t getCountryMode() const override;
    void setCountryMode(const CountryModeId_t mode) override;

    uint32_t getInvBootFrequency() const override;

    uint32_t getFrequencyFromChannel(const uint8_t channel) const override;
    uint8_t getChannelFromFrequency(const uint32_t frequency) const override;

    std::vector<CountryFrequencyList_t> getCountryFrequencyList() const override;

private:
    void ARDUINO_ISR_ATTR handleInt1();
    void ARDUINO_ISR_ATTR handleInt2();

    void sendEsbPacket(CommandAbstract& cmd) override;

    std::unique_ptr<CMT2300A> _radio;

    volatile bool _packetReceived = false;
    volatile bool _packetSent = false;

    bool _gpio2_configured = false;
    bool _gpio3_configured = false;

    std::queue<fragment_t> _rxBuffer;
    TimeoutHelper _txTimeout;

    uint32_t _inverterTargetFrequency = HOYMILES_CMT_WORK_FREQ;

    bool cmtSwitchDtuFreq(const uint32_t to_frequency);

    CountryModeId_t _countryMode;
};
