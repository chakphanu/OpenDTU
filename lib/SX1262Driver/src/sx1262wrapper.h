// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <HoymilesProtocol.h>

class SX1262Radio {
public:
    SX1262Radio(SPIClass* spi, uint8_t pin_cs, uint8_t pin_busy, int8_t pin_rst, int8_t pin_irq);

    bool begin();
    bool isChipConnected() const;

    bool startListening();
    bool stopListening();
    void read(void* buf, uint8_t len);
    void readContinuous(void* buf, uint8_t len);
    bool write(const uint8_t* buf, uint8_t len);

    void setChannel(uint8_t channel);
    uint8_t getChannel() const;
    uint8_t getDynamicPayloadSize();
    int getRssiDBm();
    bool setPALevel(int8_t level);
    bool hopToChannel(uint8_t channel);
    bool rxFifoAvailable();
    void flush_rx();
    uint16_t getLastIrqStatus() const;
    uint8_t getLastRxStatus() const;
    int getLastRssiSyncDBm() const;
    int getLastRssiDBm() const;
    uint16_t getLastDeviceErrors() const;
    uint8_t getLastCommandStatus() const;
    uint16_t getConfiguredIrqMask() const;
    uint16_t getConfiguredDio1Mask() const;
    uint32_t getIrqReadCount() const;
    uint32_t getIrqReadFailCount() const;
    uint32_t getIrqZeroCount() const;
    uint32_t getIrqNonZeroCount() const;
    uint32_t getBusyTimeoutCount() const;

    uint32_t getBaseFrequency() const;
    void setFrequencyBand(FrequencyBand_t mode);

private:
    static constexpr uint32_t CrystalFreqHz = 32000000U;
    static constexpr uint32_t FrequencyDiv = (1UL << 25);
    static constexpr uint8_t MaxPacketLength = 64;
    static constexpr uint16_t IrqTxDone = 0x0001;
    static constexpr uint16_t IrqRxDone = 0x0002;
    static constexpr uint16_t IrqPreambleDetected = 0x0004;
    static constexpr uint16_t IrqSyncWordValid = 0x0008;
    static constexpr uint16_t IrqTimeout = 0x0200;
    static constexpr uint16_t IrqAll = 0x43FF;
    static constexpr uint8_t RxStatusPreambleErr = 0x80;
    static constexpr uint8_t RxStatusSyncErr = 0x40;
    static constexpr uint8_t RxStatusAddrErr = 0x20;
    static constexpr uint8_t RxStatusLengthErr = 0x08;
    static constexpr uint8_t RxStatusAbortErr = 0x04;

    bool configureRadio();
    bool setTcxoControl(uint8_t voltage, uint32_t delayUs);
    bool setDio2AsRfSwitch(bool enabled);
    bool calibrateAll();
    bool clearDeviceErrors();
    bool resetChip();
    bool standby();
    bool setPacketType(uint8_t packetType);
    bool setRegulatorMode(uint8_t mode);
    bool setTxFallbackMode(uint8_t mode);
    bool setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask);
    bool clearIrqStatus(uint16_t irqMask = IrqAll);
    bool getIrqStatus(uint16_t& irqStatus);
    bool setBufferBaseAddress(uint8_t txBaseAddress = 0x00, uint8_t rxBaseAddress = 0x00);
    bool setRfFrequencyHz(uint32_t frequency);
    bool calibrateImage(uint32_t frequency);
    bool setPaConfig(uint8_t dutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut = 0x01);
    bool setTxParams(int8_t power, uint8_t rampTime = 0x04);
    bool setModulationParamsGfsk(uint32_t bitRateRaw, uint8_t pulseShape, uint8_t rxBandwidth, uint32_t freqDevRaw);
    bool setPacketParamsGfsk(uint16_t preambleLenBits, uint8_t preambleDetectorLen, uint8_t crcType, uint8_t syncWordLenBits, uint8_t whitening, uint8_t packetType, uint8_t payloadLen);
    bool setSyncWord(const uint8_t* syncWord, size_t len);
    bool setCrc(uint8_t len, uint16_t initial, uint16_t polynomial, bool inverted);
    bool setWhitening(bool enabled, uint16_t initial);
    bool setRxContinuous();
    bool setTx();
    bool getRxBufferStatus(uint8_t& payloadLength, uint8_t& offset);
    bool getPacketStatus(uint8_t packetStatus[3]);
    bool getDeviceErrors(uint16_t& opError);
    bool writeBuffer(const uint8_t* data, size_t len, uint8_t offset = 0x00);
    bool readBuffer(uint8_t* data, size_t len, uint8_t offset);

    bool writeCommand(uint8_t opcode, const uint8_t* data, size_t len, bool wait_after = true);
    bool readCommand(uint8_t opcode, uint8_t* data, size_t len);
    bool writeRegister(uint16_t address, const uint8_t* data, size_t len);
    bool readRegister(uint16_t address, uint8_t* data, size_t len);
    bool waitWhileBusy(uint32_t timeout_ms = 50) const;
    bool waitWhileBusyFast(uint32_t timeout_us = 5000) const;
    void spiSendOpcode(uint8_t opcode, const uint8_t* data, size_t len);
    void select() const;
    void deselect() const;

    SPIClass* _spi;
    uint8_t _pinCs;
    uint8_t _pinBusy;
    int8_t _pinRst;
    int8_t _pinIrq;

    bool _connected = false;
    FrequencyBand_t _frequencyBand = FrequencyBand_t::BAND_860;
    uint8_t _rxBandwidth = 0x1A; // GfskRxBw156_2
    uint8_t _channel = 0;
    uint8_t _lastRxOffset = 0;
    uint8_t _lastRxLength = 0;
    uint16_t _lastIrqStatus = 0;
    uint16_t _lastDeviceErrors = 0;
    uint8_t _lastPacketStatus[3] = { 0 };
    uint8_t _lastCommandStatus = 0;
    uint16_t _configuredIrqMask = 0;
    uint16_t _configuredDio1Mask = 0;
    uint32_t _irqReadCount = 0;
    uint32_t _irqReadFailCount = 0;
    uint32_t _irqZeroCount = 0;
    uint32_t _irqNonZeroCount = 0;
    mutable uint32_t _busyTimeoutCount = 0;
    bool _hasPacketStatus = false;
};
