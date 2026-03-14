// SPDX-License-Identifier: GPL-2.0-or-later
#include "HoymilesRadio_SX1262.h"
#include "Hoymiles.h"
#include "Utils.h"
#include "crc.h"
#include <FunctionalInterrupt.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "hoymiles";

namespace {
constexpr CountryFrequencyDefinition_t getCountryDefinition(CountryModeId_t mode)
{
    switch (mode) {
    case MODE_US:
        return { FrequencyBand_t::BAND_900, 900000000U, 963500000U, 900000000U, 925000000U, 918000000U, 900000000U };
    case MODE_BR:
        return { FrequencyBand_t::BAND_900, 900000000U, 963500000U, 900000000U, 928000000U, 918000000U, 900000000U };
    case MODE_TH:
        return { FrequencyBand_t::BAND_900, 920000000U, 925000000U, 920000000U, 925000000U, 920250000U, 900000000U };
    case MODE_EU:
    default:
        return { FrequencyBand_t::BAND_860, 860250000U, 923500000U, 863000000U, 870000000U, 865000000U, 868000000U };
    }
}
} // namespace

void HoymilesRadio_SX1262::init(SPIClass* initialisedSpiBus, uint8_t pinCS, uint8_t pinBusy, int8_t pinReset, int8_t pinIrq)
{
    _dtuSerial.u64 = 0;

    _radio.reset(new SX1262Radio(initialisedSpiBus, pinCS, pinBusy, pinReset, pinIrq));
    if (!_radio->begin()) {
        ESP_LOGE(TAG, "SX1262: Initialization failed");
        return;
    }

    setCountryMode(MODE_EU);
    switchDtuFreq(_inverterTargetFrequency);

    if (!_radio->isChipConnected()) {
        ESP_LOGE(TAG, "SX1262: Connection error");
        return;
    }

    ESP_LOGI(TAG, "SX1262: Connection successful");

    _dio1Pin = pinIrq;
    if (pinIrq >= 0) {
        attachInterrupt(digitalPinToInterrupt(pinIrq), std::bind(&HoymilesRadio_SX1262::handleInt, this), RISING);
    }

    if (!_radio->startListening()) {
        ESP_LOGE(TAG, "SX1262: Failed to enter RX mode");
        return;
    }

    _isInitialized = true;
}

void HoymilesRadio_SX1262::loop()
{
    if (!_isInitialized) {
        return;
    }

    // Drain _rxBuffer completely before calling handleReceivedPackage().
    // Processing one-per-iteration caused timeouts when other tasks
    // (e.g. display refresh) delayed loop() iterations past _rxTimeout.
    while (!_rxBuffer.empty()) {
        fragment_t f = _rxBuffer.front();
        if (checkFragmentCrc(f)) {
            const serial_u dtuId = convertSerialToRadioId(_dtuSerial);
            if (memcmp(&f.fragment[5], &dtuId.b[1], 4) == 0) {
                std::shared_ptr<InverterAbstract> inv = Hoymiles.getInverterByFragment(f);
                if (inv != nullptr) {
                    ESP_LOGD(TAG, "RX %.2f MHz --> %s | %" PRId8 " dBm",
                        getFrequencyFromChannel(f.channel) / 1000000.0, Utils::dumpArray(f.fragment, f.len).c_str(), f.rssi);
                    if (f.fragment[0] == 0xD6) {
                        inv->RadioStats.RxD6Received++;
                        inv->RadioStats.LastD6ReceivedMs = millis();
                    }
                    inv->addRxFragment(f.fragment, f.len, f.rssi);
                }
            }
        } else {
            ESP_LOGW(TAG, "SX1262: Invalid fragment CRC8");
        }
        _rxBuffer.pop();
    }

    handleReceivedPackage();

    // After a command completes (e.g. ChannelChange on boot freq),
    // ensure radio is on work frequency before next command fires
    if (!_busyFlag) {
        const uint8_t workChannel = getChannelFromFrequency(_inverterTargetFrequency);
        if (workChannel != 0xFF && _radio->getChannel() != workChannel) {
            ESP_LOGD(TAG, "Restoring work freq %.3f MHz after command completion",
                _inverterTargetFrequency / 1000000.0);
            switchDtuFreq(_inverterTargetFrequency);
            _radio->stopListening();
            _radio->startListening();
        }
    }
}

void HoymilesRadio_SX1262::setPALevel(int8_t paLevel)
{
    if (_isInitialized && !_radio->setPALevel(paLevel)) {
        ESP_LOGE(TAG, "SX1262 TX power set failed");
    }
}

void HoymilesRadio_SX1262::setInverterTargetFrequency(uint32_t frequency)
{
    _inverterTargetFrequency = frequency;
    if (_isInitialized) {
        switchDtuFreq(_inverterTargetFrequency);
    }
}

uint32_t HoymilesRadio_SX1262::getInverterTargetFrequency() const
{
    return _inverterTargetFrequency;
}

bool HoymilesRadio_SX1262::isConnected() const
{
    return _isInitialized && _radio->isChipConnected();
}

uint32_t HoymilesRadio_SX1262::getMinFrequency() const
{
    return getCountryDefinition(_countryMode).Freq_Min;
}

uint32_t HoymilesRadio_SX1262::getMaxFrequency() const
{
    return getCountryDefinition(_countryMode).Freq_Max;
}

CountryModeId_t HoymilesRadio_SX1262::getCountryMode() const
{
    return _countryMode;
}

void HoymilesRadio_SX1262::setCountryMode(CountryModeId_t mode)
{
    _countryMode = mode;
    if (_radio) {
        _radio->setFrequencyBand(getCountryDefinition(mode).Band);
    }
}

uint32_t HoymilesRadio_SX1262::getInvBootFrequency() const
{
    return getCountryDefinition(_countryMode).Freq_StartUp;
}

uint32_t HoymilesRadio_SX1262::getFrequencyFromChannel(uint8_t channel) const
{
    return _radio->getBaseFrequency() + static_cast<uint32_t>(channel) * ChannelWidth;
}

uint8_t HoymilesRadio_SX1262::getChannelFromFrequency(uint32_t frequency) const
{
    if ((frequency % ChannelWidth) != 0) {
        ESP_LOGE(TAG, "%.3f MHz is not divisible by %" PRIu32 " kHz!", frequency / 1000000.0, ChannelWidth);
        return 0xFF;
    }

    if (frequency < getMinFrequency() || frequency > getMaxFrequency()) {
        ESP_LOGE(TAG, "%.2f MHz is out of Hoymiles/SX1262 range! (%.2f MHz - %.2f MHz)",
            frequency / 1000000.0, getMinFrequency() / 1000000.0, getMaxFrequency() / 1000000.0);
        return 0xFF;
    }

    const auto def = getCountryDefinition(_countryMode);
    if (frequency < def.Freq_Legal_Min || frequency > def.Freq_Legal_Max) {
        ESP_LOGE(TAG, "!!! caution: %.2f MHz is out of region legal range!", frequency / 1000000.0);
    }

    return static_cast<uint8_t>((frequency - _radio->getBaseFrequency()) / ChannelWidth);
}

std::vector<CountryFrequencyList_t> HoymilesRadio_SX1262::getCountryFrequencyList() const
{
    return {
        { MODE_EU, getCountryDefinition(MODE_EU) },
        { MODE_US, getCountryDefinition(MODE_US) },
        { MODE_BR, getCountryDefinition(MODE_BR) },
        { MODE_TH, getCountryDefinition(MODE_TH) },
    };
}

uint32_t HoymilesRadio_SX1262::getCurrentFrequency() const
{
    if (!_radio) {
        return 0;
    }

    return getFrequencyFromChannel(_radio->getChannel());
}

void ARDUINO_ISR_ATTR HoymilesRadio_SX1262::handleInt()
{
    // DIO1 interrupt handled via digitalRead() polling in blockingBurstReceive
}

void HoymilesRadio_SX1262::sendEsbPacket(CommandAbstract& cmd)
{
    cmd.incrementSendCount();
    cmd.setRouterAddress(DtuSerial().u64);

    _radio->stopListening();

    const bool isChannelChange = (cmd.getDataPayload()[0] == 0x56);

    if (isChannelChange) {
        // Dual-frequency bootstrap: alternate 900/915 MHz on each retry
        // Use setChannel(0) for 900 MHz and setChannel(60) for 915 MHz
        // to bypass country-mode range validation on boot frequencies
        const uint8_t bootChannels[] = { 0, 60 };
        const uint8_t idx = (cmd.getSendCount() - 1) % 2;
        _radio->setChannel(bootChannels[idx]);
        ESP_LOGI(TAG, "ChannelChange: target_ch=%u target_freq=%.3f MHz, TX on boot_freq=%.3f MHz (attempt %u)",
            cmd.getDataPayload()[12],
            getFrequencyFromChannel(cmd.getDataPayload()[12]) / 1000000.0,
            getFrequencyFromChannel(bootChannels[idx]) / 1000000.0,
            cmd.getSendCount());
    } else {
        // Ensure radio is on work freq before TX (hop may have changed it)
        switchDtuFreq(_inverterTargetFrequency);
        ESP_LOGD(TAG, "DataCmd: TX on work_freq=%.3f MHz (ch=%u)",
            _inverterTargetFrequency / 1000000.0,
            getChannelFromFrequency(_inverterTargetFrequency));
    }

    if (isChannelChange) {
        auto inv = Hoymiles.getInverterBySerial(cmd.getTargetAddress());
        if (inv != nullptr) {
            inv->RadioStats.TxD6Sent++;
            inv->RadioStats.LastD6SentMs = millis();
        }
    }

    ESP_LOGD(TAG, "TX %s %.2f MHz --> %s",
        cmd.getCommandName().c_str(), getFrequencyFromChannel(_radio->getChannel()) / 1000000.0, cmd.dumpDataPayload().c_str());

    size_t codedLen = 0;
    if (!HoymilesAirFrame::encodeCmtPacket(cmd.getDataPayload(), cmd.getDataSize(), _codedTxBuffer.data(), _codedTxBuffer.size(), codedLen)) {
        ESP_LOGE(TAG, "SX1262: Failed to encode air frame");
    } else if (!_radio->write(_codedTxBuffer.data(), static_cast<uint8_t>(codedLen))) {
        ESP_LOGE(TAG, "SX1262: TX timeout");
    }

    // Allow SX1262 state machine to settle after TX
    delayMicroseconds(500);

    // After ChannelChange: stay on boot freq to receive D6 reply
    // After other commands: switch to target (work) freq for RX
    if (!isChannelChange) {
        switchDtuFreq(_inverterTargetFrequency);
    }

    // Blocking burst receive: collect all fragments before returning.
    // Hop across 3 channels for data commands, stay on boot freq for D6.
    // Use per-inverter phase prediction when available.
    int8_t startPattern = -1;
    auto inv = Hoymiles.getInverterBySerial(cmd.getTargetAddress());
    if (!isChannelChange && inv != nullptr && inv->RadioStats.LastHopPattern >= 0) {
        startPattern = static_cast<int8_t>(
            (inv->RadioStats.LastHopPattern + inv->RadioStats.LastFragCount) % 3);
    }

    const auto burstResult = blockingBurstReceive(800, !isChannelChange, startPattern);

    // Update per-inverter hop tracking
    if (!isChannelChange && inv != nullptr) {
        inv->RadioStats.LastHopPattern = burstResult.detectedPhase;
        inv->RadioStats.LastFragCount = burstResult.fragCount;
        inv->RadioStats.LastBurstRxCount = burstResult.fragCount;
        if (burstResult.detectedPhase >= 0) {
            inv->RadioStats.PredictedHopPattern = static_cast<int8_t>(
                (burstResult.detectedPhase + burstResult.fragCount) % 3);
        } else {
            inv->RadioStats.PredictedHopPattern = -1;
        }
    }

    _busyFlag = true;
    // Short timeout: fragments already collected, just let loop() drain _rxBuffer
    _rxTimeout.set(100);
}

bool HoymilesRadio_SX1262::readAndBufferPacket()
{
    if (!_radio->rxFifoAvailable()) {
        return false;
    }

    if (_rxBuffer.size() > FragmentBufferSize) {
        ESP_LOGE(TAG, "SX1262: Buffer full");
        _radio->flush_rx();
        return false;
    }

    memset(_codedRxBuffer.data(), 0x00, _codedRxBuffer.size());
    memset(_logicalBuffer.data(), 0x00, _logicalBuffer.size());

    fragment_t f;
    memset(f.fragment, 0xcc, MAX_RF_PAYLOAD_SIZE);
    const uint8_t codedLen = _radio->getDynamicPayloadSize();
    f.channel = _radio->getChannel();
    f.rssi = _radio->getRssiDBm();
    f.wasReceived = false;
    f.mainCmd = 0x00;

    const uint8_t actualCodedLen = std::min<uint8_t>(codedLen, _codedRxBuffer.size());
    _radio->readContinuous(_codedRxBuffer.data(), actualCodedLen);

    const auto decode = HoymilesAirFrame::decodeCmtPacket(
        _codedRxBuffer.data(), actualCodedLen, _logicalBuffer.data(), _logicalBuffer.size());
    if (!decode.success) {
        ESP_LOGW(TAG, "SX1262: Failed to decode air frame (decoded=%u logical=%u corrected=%s air_crc=%s)",
            static_cast<unsigned>(decode.decodedBytes),
            static_cast<unsigned>(decode.logicalLength),
            decode.corrected ? "yes" : "no",
            decode.airCrcValid ? "ok" : "bad");
        return false;
    }

    f.len = decode.logicalLength;
    memcpy(f.fragment, _logicalBuffer.data(), f.len);
    _rxBuffer.push(f);
    return true;
}

HoymilesRadio_SX1262::BurstResult HoymilesRadio_SX1262::blockingBurstReceive(uint32_t timeout_ms, bool enableHop, int8_t startPattern)
{
    // DIO1-based fast channel cycling with phase prediction.
    //
    // For data commands (enableHop=true):
    //   - Use predicted phase to select starting channel
    //   - Cycle 3 channels with 4ms DIO1 digitalRead timeout each
    //   - After preamble detect: wait RxDone via SPI, read packet, hop next
    //   - Detect phase from first RX channel for next burst prediction
    //
    // For ChannelChange (enableHop=false):
    //   - Stay on current freq, poll for D6 reply

    const uint8_t txCh = _radio->getChannel();

    // Hop patterns per phase
    static constexpr int8_t hop_patterns[3][3] = {
        {  0, +1, -1 },  // Phase A
        { +1, -1,  0 },  // Phase B
        { -1,  0, +1 },  // Phase C
    };

    // Use caller-provided startPattern if valid, else fall back to singleton prediction
    int8_t predictedPhase;
    if (startPattern >= 0 && startPattern < 3) {
        predictedPhase = startPattern;
    } else if (_lastKnownPhase >= 0) {
        predictedPhase = static_cast<int8_t>((_lastKnownPhase + _lastFragCount) % 3);
    } else {
        predictedPhase = 0;
    }
    const int8_t* offsets = hop_patterns[predictedPhase];

    // Enter RX via STDBY_RC path (full packet engine reset)
    if (enableHop) {
        const uint8_t startCh = static_cast<uint8_t>(
            static_cast<int8_t>(txCh) + offsets[0]);
        _radio->hopToChannel(startCh);
    } else {
        _radio->hopToChannel(txCh);
    }

    const uint32_t startUs = micros();
    const uint32_t timeoutUs = timeout_ms * 1000UL;
    uint8_t offIdx = 0;
    uint8_t pktCount = 0;
    uint8_t firstRxCh = txCh;

    if (!enableHop || _dio1Pin < 0) {
        // D6 or no DIO1: simple SPI polling
        while ((micros() - startUs) < timeoutUs) {
            if (readAndBufferPacket()) {
                pktCount++;
            }
        }
    } else {
        // DIO1-based fast channel cycling
        while ((micros() - startUs) < timeoutUs && pktCount < 12) {
            // Wait for DIO1 HIGH (PreambleDetected) -- 4ms timeout
            const uint32_t waitStart = micros();
            bool preambleFound = false;

            while ((micros() - waitStart) < 4000) {
                if (digitalRead(_dio1Pin)) {
                    preambleFound = true;
                    break;
                }
            }

            if (!preambleFound) {
                // Cycle to next channel
                offIdx = (offIdx + 1) % 3;
                const uint8_t nextCh = static_cast<uint8_t>(
                    static_cast<int8_t>(txCh) + offsets[offIdx]);
                _radio->hopToChannel(nextCh);
                continue;
            }

            // Preamble detected! Poll readAndBufferPacket() until RxDone (up to 40ms).
            const uint32_t rxStart = micros();
            bool gotPacket = false;
            while ((micros() - rxStart) < 40000) {
                if (readAndBufferPacket()) {
                    gotPacket = true;
                    break;
                }
            }

            if (gotPacket) {
                if (pktCount == 0) {
                    firstRxCh = _radio->getChannel();
                }
                pktCount++;
            }

            // Immediately hop to next channel
            offIdx = (offIdx + 1) % 3;
            const uint8_t nextCh = static_cast<uint8_t>(
                static_cast<int8_t>(txCh) + offsets[offIdx]);
            _radio->hopToChannel(nextCh);
        }
    }

    // Phase detection from first RX channel offset
    BurstResult result = { -1, pktCount };
    if (enableHop && pktCount > 0) {
        const int8_t rxOffset = static_cast<int8_t>(firstRxCh) - static_cast<int8_t>(txCh);
        for (int8_t p = 0; p < 3; p++) {
            if (hop_patterns[p][0] == rxOffset) {
                _lastKnownPhase = p;
                result.detectedPhase = p;
                break;
            }
        }
        _lastFragCount = pktCount;
        ESP_LOGD(TAG, "SX1262: Burst %u frags, phase=%c, predict next=%c",
            pktCount, "ABC"[_lastKnownPhase >= 0 ? _lastKnownPhase : 0],
            "ABC"[(_lastKnownPhase + pktCount) % 3]);
    }

    return result;
}

bool HoymilesRadio_SX1262::switchDtuFreq(uint32_t frequency)
{
    const uint8_t channel = getChannelFromFrequency(frequency);
    if (channel == 0xFF) {
        return false;
    }

    _radio->setChannel(channel);
    return true;
}

