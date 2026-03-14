// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "HoymilesRadio_SubGhz.h"
#include "HoymilesAirFrame.h"
#include "commands/CommandAbstract.h"
#include <array>
#include <Arduino.h>
#include <memory>
#include <queue>
#include <sx1262wrapper.h>

class HoymilesRadio_SX1262 : public HoymilesRadio_SubGhz {
public:
    void init(SPIClass* initialisedSpiBus, uint8_t pinCS, uint8_t pinBusy, int8_t pinReset, int8_t pinIrq);
    void loop();

    void setPALevel(int8_t paLevel) override;
    void setInverterTargetFrequency(uint32_t frequency) override;
    uint32_t getInverterTargetFrequency() const override;
    bool isConnected() const override;

    uint32_t getMinFrequency() const override;
    uint32_t getMaxFrequency() const override;
    CountryModeId_t getCountryMode() const override;
    void setCountryMode(CountryModeId_t mode) override;
    uint32_t getInvBootFrequency() const override;
    uint32_t getFrequencyFromChannel(uint8_t channel) const override;
    uint8_t getChannelFromFrequency(uint32_t frequency) const override;
    std::vector<CountryFrequencyList_t> getCountryFrequencyList() const override;
    uint32_t getCurrentFrequency() const;
    static constexpr uint32_t getChannelWidth() { return ChannelWidth; }

private:
    static constexpr uint32_t ChannelWidth = HOYMILES_FH_OFFSET * HOYMILES_FH_STEP_SIZE;
    static constexpr size_t FragmentBufferSize = 30;

    void ARDUINO_ISR_ATTR handleInt();
    void sendEsbPacket(CommandAbstract& cmd) override;
    bool switchDtuFreq(uint32_t frequency);
    bool readAndBufferPacket();
    struct BurstResult {
        int8_t detectedPhase;
        uint8_t fragCount;
    };
    BurstResult blockingBurstReceive(uint32_t timeout_ms, bool enableHop, int8_t startPattern = -1);

    std::unique_ptr<SX1262Radio> _radio;
    std::queue<fragment_t> _rxBuffer;
    std::array<uint8_t, HoymilesAirFrame::MaxCodedBytes> _codedRxBuffer = { 0 };
    std::array<uint8_t, HoymilesAirFrame::MaxLogicalBytes> _logicalBuffer = { 0 };
    std::array<uint8_t, HoymilesAirFrame::MaxCodedBytes> _codedTxBuffer = { 0 };
    uint32_t _inverterTargetFrequency = 865000000U;
    CountryModeId_t _countryMode = MODE_EU;

    int8_t _dio1Pin = -1;
    int8_t _lastKnownPhase = -1;
    uint8_t _lastFragCount = 5;

};
