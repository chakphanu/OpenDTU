// SPDX-License-Identifier: GPL-2.0-or-later
#include "sx1262wrapper.h"

namespace {
constexpr uint8_t CmdSetFs = 0xC1;
constexpr uint8_t CmdSetStandby = 0x80;
constexpr uint8_t CmdSetTx = 0x83;
constexpr uint8_t CmdSetRx = 0x82;
constexpr uint8_t CmdCalibrate = 0x89;
constexpr uint8_t CmdSetRegulatorMode = 0x96;
constexpr uint8_t CmdSetDio3AsTcxoCtrl = 0x97;
constexpr uint8_t CmdCalibrateImage = 0x98;
constexpr uint8_t CmdSetTxFallbackMode = 0x93;
constexpr uint8_t CmdSetDio2AsRfSwitchCtrl = 0x9D;
constexpr uint8_t CmdWriteRegister = 0x0D;
constexpr uint8_t CmdReadRegister = 0x1D;
constexpr uint8_t CmdWriteBuffer = 0x0E;
constexpr uint8_t CmdReadBuffer = 0x1E;
constexpr uint8_t CmdSetDioIrqParams = 0x08;
constexpr uint8_t CmdGetIrqStatus = 0x12;
constexpr uint8_t CmdClearIrqStatus = 0x02;
constexpr uint8_t CmdSetRfFrequency = 0x86;
constexpr uint8_t CmdSetPacketType = 0x8A;
constexpr uint8_t CmdGetPacketType = 0x11;
constexpr uint8_t CmdSetPaConfig = 0x95;
constexpr uint8_t CmdSetTxParams = 0x8E;
constexpr uint8_t CmdSetModulationParams = 0x8B;
constexpr uint8_t CmdSetPacketParams = 0x8C;
constexpr uint8_t CmdSetBufferBaseAddress = 0x8F;
constexpr uint8_t CmdGetRxBufferStatus = 0x13;
constexpr uint8_t CmdGetPacketStatus = 0x14;
constexpr uint8_t CmdGetDeviceErrors = 0x17;
constexpr uint8_t CmdClearDeviceErrors = 0x07;
constexpr uint8_t PacketTypeGfsk = 0x00;
constexpr uint8_t StandbyRc = 0x00;
constexpr uint8_t RegulatorDcDc = 0x01;
constexpr uint8_t RxTxFallbackStandbyRc = 0x20;
constexpr uint8_t GfskFilterGauss05 = 0x09;
constexpr uint8_t GfskRxBw117_3 = 0x0B;
constexpr uint8_t GfskPreambleDetect16 = 0x05;
constexpr uint8_t GfskPacketFixed = 0x00;
constexpr uint8_t GfskCrcOff = 0x01;
constexpr uint8_t GfskWhiteningOff = 0x00;
constexpr uint8_t Dio2AsRfSwitchEnabled = 0x01;
constexpr uint8_t Dio3Output1_8 = 0x02;
constexpr uint8_t CalibrateAllBlocks = 0x7F;
constexpr uint16_t RegWhiteningInitialMsb = 0x06B8;
constexpr uint16_t RegCrcInitialMsb = 0x06BC;
constexpr uint16_t RegCrcPolynomialMsb = 0x06BE;
constexpr uint16_t RegSyncWord0 = 0x06C0;
constexpr uint16_t RegOcpConfiguration = 0x08E7;
constexpr uint8_t CalImg863_1 = 0xD7;
constexpr uint8_t CalImg863_2 = 0xDB;
constexpr uint8_t CalImg902_1 = 0xE1;
constexpr uint8_t CalImg902_2 = 0xE9;
} // namespace

SX1262Radio::SX1262Radio(SPIClass* spi, uint8_t pin_cs, uint8_t pin_busy, int8_t pin_rst, int8_t pin_irq)
    : _spi(spi)
    , _pinCs(pin_cs)
    , _pinBusy(pin_busy)
    , _pinRst(pin_rst)
    , _pinIrq(pin_irq)
{
}

bool SX1262Radio::begin()
{
    pinMode(_pinCs, OUTPUT);
    digitalWrite(_pinCs, HIGH);
    pinMode(_pinBusy, INPUT);
    if (_pinIrq >= 0) {
        pinMode(_pinIrq, INPUT);
    }
    if (_pinRst >= 0) {
        pinMode(_pinRst, OUTPUT);
    }

    if (!resetChip()) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    if (!standby()) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    if (!clearDeviceErrors()) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    if (!setTcxoControl(Dio3Output1_8, 5000)) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    if (!calibrateAll()) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    if (!setPacketType(PacketTypeGfsk)) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    if (!configureRadio()) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    // Reduce TCXO warmup for subsequent operations (initial 5000us was for calibration)
    if (!setTcxoControl(Dio3Output1_8, 300)) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    uint8_t packetType = 0xFF;
    _connected = readCommand(CmdGetPacketType, &packetType, 1) && packetType == PacketTypeGfsk;
    return _connected;
}

bool SX1262Radio::isChipConnected() const
{
    return _connected;
}

bool SX1262Radio::startListening()
{
    if (!standby()) {
        return false;
    }
    if (!setPacketParamsGfsk(240, GfskPreambleDetect16, GfskCrcOff, 32, GfskWhiteningOff, GfskPacketFixed, MaxPacketLength)) {
        return false;
    }
    if (!setBufferBaseAddress()) {
        return false;
    }
    if (!clearIrqStatus()) {
        return false;
    }
    if (!setDioIrqParams(IrqRxDone | IrqTimeout | IrqPreambleDetected | IrqSyncWordValid,
            IrqRxDone | IrqTimeout | IrqPreambleDetected | IrqSyncWordValid)) {
        return false;
    }
    return setRxContinuous();
}

bool SX1262Radio::stopListening()
{
    _hasPacketStatus = false;
    return standby() && clearIrqStatus();
}

void SX1262Radio::read(void* buf, uint8_t len)
{
    if (_lastRxLength == 0) {
        getDynamicPayloadSize();
    }

    const uint8_t actualLen = std::min<uint8_t>(len, _lastRxLength);
    readBuffer(static_cast<uint8_t*>(buf), actualLen, _lastRxOffset);
    clearIrqStatus();
    _lastRxLength = 0;
    _lastRxOffset = 0;
    _hasPacketStatus = false;
    startListening();
}

void SX1262Radio::readContinuous(void* buf, uint8_t len)
{
    // Fast read without restarting RX - stays in continuous RX mode.
    // Only clears IRQ so next packet triggers a new RxDone.
    // Radio is never taken offline, eliminating the ~1.5ms gap.
    // Note: FIFO overwrite at address 0x00 is unavoidable in continuous RX
    // (SetBufferBaseAddress requires STDBY mode per datasheet).
    // The 14ms inter-fragment gap gives enough time to read before overwrite.
    if (_lastRxLength == 0) {
        getDynamicPayloadSize();
    }

    const uint8_t actualLen = std::min<uint8_t>(len, _lastRxLength);
    readBuffer(static_cast<uint8_t*>(buf), actualLen, _lastRxOffset);
    clearIrqStatus(IrqRxDone | IrqPreambleDetected | IrqSyncWordValid);
    _lastRxLength = 0;
    _lastRxOffset = 0;
    _hasPacketStatus = false;
}

bool SX1262Radio::write(const uint8_t* buf, uint8_t len)
{
    if (!standby()) {
        return false;
    }
    if (!setPacketParamsGfsk(240, GfskPreambleDetect16, GfskCrcOff, 32, GfskWhiteningOff, GfskPacketFixed, len)) {
        return false;
    }
    if (!setDioIrqParams(IrqTxDone | IrqTimeout, IrqTxDone | IrqTimeout)) {
        return false;
    }
    if (!setBufferBaseAddress()) {
        return false;
    }
    if (!writeBuffer(buf, len)) {
        return false;
    }
    if (!clearIrqStatus()) {
        return false;
    }
    if (!setTx()) {
        return false;
    }

    const uint32_t started = millis();
    while (millis() - started < 200) {
        uint16_t irq = 0;
        if (getIrqStatus(irq) && (irq & (IrqTxDone | IrqTimeout))) {
            clearIrqStatus();
            return (irq & IrqTxDone) != 0;
        }
        yield();
    }

    clearIrqStatus();
    return false;
}

void SX1262Radio::setChannel(uint8_t channel)
{
    _channel = channel;
    setRfFrequencyHz(getBaseFrequency() + static_cast<uint32_t>(channel) * HOYMILES_FH_OFFSET * HOYMILES_FH_STEP_SIZE);
}

uint8_t SX1262Radio::getChannel() const
{
    return _channel;
}

uint8_t SX1262Radio::getDynamicPayloadSize()
{
    if (!getRxBufferStatus(_lastRxLength, _lastRxOffset)) {
        _lastRxLength = 0;
        _lastRxOffset = 0;
    } else if (_lastRxLength == 0) {
        _lastRxLength = MaxPacketLength;
    }
    return _lastRxLength;
}

int SX1262Radio::getRssiDBm()
{
    if (!_hasPacketStatus && !getPacketStatus(_lastPacketStatus)) {
        return -127;
    }
    _hasPacketStatus = true;
    return getLastRssiDBm();
}

bool SX1262Radio::setPALevel(int8_t level)
{
    if (level < -9) {
        level = -9;
    } else if (level > 22) {
        level = 22;
    }

    // Match the proven debug configuration (SetPaConfig bytes: duty=0x04, hpMax=0x07, deviceSel=0x00).
    if (!setPaConfig(0x04, 0x07, 0x00)) {
        return false;
    }

    return setTxParams(level);
}

bool SX1262Radio::rxFifoAvailable()
{
    uint16_t irq = 0;
    if (!getIrqStatus(irq)) {
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    _lastIrqStatus = irq;
    if (irq == 0) {
        return false;
    }

    if (irq & IrqTimeout) {
        clearIrqStatus(IrqTimeout);
        return false;
    }

    if (!(irq & IrqRxDone)) {
        clearIrqStatus(static_cast<uint16_t>(irq & (IrqPreambleDetected | IrqSyncWordValid)));
        return false;
    }

    _hasPacketStatus = getPacketStatus(_lastPacketStatus);
    if (!_hasPacketStatus) {
        clearIrqStatus(IrqRxDone);
        getDeviceErrors(_lastDeviceErrors);
        return false;
    }

    const uint8_t rxStatus = _lastPacketStatus[0];
    if (rxStatus & (RxStatusPreambleErr | RxStatusSyncErr | RxStatusAddrErr | RxStatusLengthErr | RxStatusAbortErr)) {
        clearIrqStatus(IrqRxDone);
        return false;
    }

    return true;
}

void SX1262Radio::flush_rx()
{
    clearIrqStatus();
    _lastRxLength = 0;
    _lastRxOffset = 0;
    _hasPacketStatus = false;
    startListening();
}

uint16_t SX1262Radio::getLastIrqStatus() const
{
    return _lastIrqStatus;
}

uint8_t SX1262Radio::getLastRxStatus() const
{
    return _lastPacketStatus[0];
}

int SX1262Radio::getLastRssiSyncDBm() const
{
    return -static_cast<int>(_lastPacketStatus[1]) / 2;
}

int SX1262Radio::getLastRssiDBm() const
{
    return -static_cast<int>(_lastPacketStatus[2]) / 2;
}

uint16_t SX1262Radio::getLastDeviceErrors() const
{
    return _lastDeviceErrors;
}

uint8_t SX1262Radio::getLastCommandStatus() const
{
    return _lastCommandStatus;
}

uint16_t SX1262Radio::getConfiguredIrqMask() const
{
    return _configuredIrqMask;
}

uint16_t SX1262Radio::getConfiguredDio1Mask() const
{
    return _configuredDio1Mask;
}

uint32_t SX1262Radio::getIrqReadCount() const
{
    return _irqReadCount;
}

uint32_t SX1262Radio::getIrqReadFailCount() const
{
    return _irqReadFailCount;
}

uint32_t SX1262Radio::getIrqZeroCount() const
{
    return _irqZeroCount;
}

uint32_t SX1262Radio::getIrqNonZeroCount() const
{
    return _irqNonZeroCount;
}

uint32_t SX1262Radio::getBusyTimeoutCount() const
{
    return _busyTimeoutCount;
}

uint32_t SX1262Radio::getBaseFrequency() const
{
    return getHoymilesBaseFrequency(_frequencyBand);
}

void SX1262Radio::setFrequencyBand(FrequencyBand_t mode)
{
    _frequencyBand = mode;
    calibrateImage(getBaseFrequency());
    setChannel(_channel);
}

bool SX1262Radio::configureRadio()
{
    const uint32_t bitRateRaw = static_cast<uint32_t>((static_cast<uint64_t>(CrystalFreqHz) * 32ULL) / 20000ULL);
    const uint32_t freqDevRaw = static_cast<uint32_t>((20000ULL * FrequencyDiv) / CrystalFreqHz);
    static constexpr uint8_t syncWord[] = { 0x4D, 0x48, 0x5A, 0x48 };

    const bool configured = setRegulatorMode(RegulatorDcDc)
        && setDio2AsRfSwitch(true)
        && calibrateImage(getBaseFrequency())
        && setTxFallbackMode(RxTxFallbackStandbyRc)
        && setBufferBaseAddress()
        && setModulationParamsGfsk(bitRateRaw, GfskFilterGauss05, _rxBandwidth, freqDevRaw)
        && setSyncWord(syncWord, sizeof(syncWord))
        && setPacketParamsGfsk(240, GfskPreambleDetect16, GfskCrcOff, 32, GfskWhiteningOff, GfskPacketFixed, MaxPacketLength)
        && setPALevel(13)
        && clearIrqStatus();
    if (!configured) {
        getDeviceErrors(_lastDeviceErrors);
    }
    return configured;
}

bool SX1262Radio::setTcxoControl(uint8_t voltage, uint32_t delayUs)
{
    const uint32_t delayValue = delayUs / 16U;
    const uint8_t data[4] = {
        voltage,
        static_cast<uint8_t>((delayValue >> 16) & 0xFF),
        static_cast<uint8_t>((delayValue >> 8) & 0xFF),
        static_cast<uint8_t>(delayValue & 0xFF)
    };
    return writeCommand(CmdSetDio3AsTcxoCtrl, data, sizeof(data));
}

bool SX1262Radio::setDio2AsRfSwitch(bool enabled)
{
    const uint8_t data = enabled ? Dio2AsRfSwitchEnabled : 0x00;
    return writeCommand(CmdSetDio2AsRfSwitchCtrl, &data, 1);
}

bool SX1262Radio::calibrateAll()
{
    const uint8_t data = CalibrateAllBlocks;
    return writeCommand(CmdCalibrate, &data, 1);
}

bool SX1262Radio::clearDeviceErrors()
{
    const uint8_t data[2] = { 0x00, 0x00 };
    return writeCommand(CmdClearDeviceErrors, data, sizeof(data));
}

bool SX1262Radio::resetChip()
{
    if (_pinRst < 0) {
        return true;
    }

    digitalWrite(_pinRst, LOW);
    delay(1);
    digitalWrite(_pinRst, HIGH);
    delay(5);
    return waitWhileBusy(100);
}

bool SX1262Radio::standby()
{
    const uint8_t mode = StandbyRc;
    return writeCommand(CmdSetStandby, &mode, 1);
}

bool SX1262Radio::setPacketType(uint8_t packetType)
{
    return writeCommand(CmdSetPacketType, &packetType, 1);
}

bool SX1262Radio::setRegulatorMode(uint8_t mode)
{
    return writeCommand(CmdSetRegulatorMode, &mode, 1);
}

bool SX1262Radio::setTxFallbackMode(uint8_t mode)
{
    return writeCommand(CmdSetTxFallbackMode, &mode, 1);
}

bool SX1262Radio::setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask)
{
    const uint8_t data[8] = {
        static_cast<uint8_t>((irqMask >> 8) & 0xFF), static_cast<uint8_t>(irqMask & 0xFF),
        static_cast<uint8_t>((dio1Mask >> 8) & 0xFF), static_cast<uint8_t>(dio1Mask & 0xFF),
        0x00, 0x00,
        0x00, 0x00
    };
    const bool ok = writeCommand(CmdSetDioIrqParams, data, sizeof(data));
    if (ok) {
        _configuredIrqMask = irqMask;
        _configuredDio1Mask = dio1Mask;
    }
    return ok;
}

bool SX1262Radio::clearIrqStatus(uint16_t irqMask)
{
    const uint8_t data[2] = { static_cast<uint8_t>((irqMask >> 8) & 0xFF), static_cast<uint8_t>(irqMask & 0xFF) };
    return writeCommand(CmdClearIrqStatus, data, sizeof(data));
}

bool SX1262Radio::getIrqStatus(uint16_t& irqStatus)
{
    _irqReadCount++;
    uint8_t data[2] = { 0 };
    if (!readCommand(CmdGetIrqStatus, data, sizeof(data))) {
        _irqReadFailCount++;
        return false;
    }
    irqStatus = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    if (irqStatus == 0) {
        _irqZeroCount++;
    } else {
        _irqNonZeroCount++;
    }
    return true;
}

bool SX1262Radio::setBufferBaseAddress(uint8_t txBaseAddress, uint8_t rxBaseAddress)
{
    const uint8_t data[2] = { txBaseAddress, rxBaseAddress };
    return writeCommand(CmdSetBufferBaseAddress, data, sizeof(data));
}

bool SX1262Radio::setRfFrequencyHz(uint32_t frequency)
{
    const uint32_t frf = static_cast<uint32_t>((static_cast<uint64_t>(frequency) * FrequencyDiv) / CrystalFreqHz);
    const uint8_t data[4] = {
        static_cast<uint8_t>((frf >> 24) & 0xFF),
        static_cast<uint8_t>((frf >> 16) & 0xFF),
        static_cast<uint8_t>((frf >> 8) & 0xFF),
        static_cast<uint8_t>(frf & 0xFF)
    };
    return writeCommand(CmdSetRfFrequency, data, sizeof(data));
}

bool SX1262Radio::calibrateImage(uint32_t frequency)
{
    uint8_t data[2] = { 0, 0 };
    const uint32_t freqMHz = frequency / 1000000U;
    if (freqMHz >= 902 && freqMHz <= 928) {
        data[0] = CalImg902_1;
        data[1] = CalImg902_2;
    } else {
        data[0] = CalImg863_1;
        data[1] = CalImg863_2;
    }

    return writeCommand(CmdCalibrateImage, data, sizeof(data));
}

bool SX1262Radio::setPaConfig(uint8_t dutyCycle, uint8_t hpMax, uint8_t deviceSel, uint8_t paLut)
{
    const uint8_t data[4] = { dutyCycle, hpMax, deviceSel, paLut };
    return writeCommand(CmdSetPaConfig, data, sizeof(data));
}

bool SX1262Radio::setTxParams(int8_t power, uint8_t rampTime)
{
    const uint8_t ocp = 0x38;
    if (!writeRegister(RegOcpConfiguration, &ocp, 1)) {
        return false;
    }

    const uint8_t data[2] = { static_cast<uint8_t>(power), rampTime };
    return writeCommand(CmdSetTxParams, data, sizeof(data));
}

bool SX1262Radio::setModulationParamsGfsk(uint32_t bitRateRaw, uint8_t pulseShape, uint8_t rxBandwidth, uint32_t freqDevRaw)
{
    const uint8_t data[8] = {
        static_cast<uint8_t>((bitRateRaw >> 16) & 0xFF),
        static_cast<uint8_t>((bitRateRaw >> 8) & 0xFF),
        static_cast<uint8_t>(bitRateRaw & 0xFF),
        pulseShape,
        rxBandwidth,
        static_cast<uint8_t>((freqDevRaw >> 16) & 0xFF),
        static_cast<uint8_t>((freqDevRaw >> 8) & 0xFF),
        static_cast<uint8_t>(freqDevRaw & 0xFF)
    };
    return writeCommand(CmdSetModulationParams, data, sizeof(data));
}

bool SX1262Radio::setPacketParamsGfsk(uint16_t preambleLenBits, uint8_t preambleDetectorLen, uint8_t crcType, uint8_t syncWordLenBits, uint8_t whitening, uint8_t packetType, uint8_t payloadLen)
{
    const uint8_t data[9] = {
        static_cast<uint8_t>((preambleLenBits >> 8) & 0xFF),
        static_cast<uint8_t>(preambleLenBits & 0xFF),
        preambleDetectorLen,
        syncWordLenBits,
        0x00,
        packetType,
        payloadLen,
        crcType,
        whitening
    };
    return writeCommand(CmdSetPacketParams, data, sizeof(data));
}

bool SX1262Radio::setSyncWord(const uint8_t* syncWord, size_t len)
{
    return writeRegister(RegSyncWord0, syncWord, len);
}

bool SX1262Radio::setCrc(uint8_t len, uint16_t initial, uint16_t polynomial, bool inverted)
{
    (void)inverted;
    if (len == 0) {
        return true;
    }

    const uint8_t initData[2] = { static_cast<uint8_t>((initial >> 8) & 0xFF), static_cast<uint8_t>(initial & 0xFF) };
    const uint8_t polyData[2] = { static_cast<uint8_t>((polynomial >> 8) & 0xFF), static_cast<uint8_t>(polynomial & 0xFF) };
    return writeRegister(RegCrcInitialMsb, initData, sizeof(initData))
        && writeRegister(RegCrcPolynomialMsb, polyData, sizeof(polyData));
}

bool SX1262Radio::setWhitening(bool enabled, uint16_t initial)
{
    if (!enabled) {
        return true;
    }

    uint8_t msb = 0;
    if (!readRegister(RegWhiteningInitialMsb, &msb, 1)) {
        return false;
    }

    const uint8_t data[2] = {
        static_cast<uint8_t>((msb & 0xFE) | ((initial >> 8) & 0x01)),
        static_cast<uint8_t>(initial & 0xFF)
    };
    return writeRegister(RegWhiteningInitialMsb, data, sizeof(data));
}

bool SX1262Radio::setRxContinuous()
{
    const uint8_t data[3] = { 0xFF, 0xFF, 0xFF };
    return writeCommand(CmdSetRx, data, sizeof(data));
}

bool SX1262Radio::setTx()
{
    const uint8_t data[3] = { 0x00, 0x00, 0x00 };
    return writeCommand(CmdSetTx, data, sizeof(data));
}

bool SX1262Radio::getRxBufferStatus(uint8_t& payloadLength, uint8_t& offset)
{
    uint8_t data[2] = { 0 };
    if (!readCommand(CmdGetRxBufferStatus, data, sizeof(data))) {
        return false;
    }
    payloadLength = data[0];
    offset = data[1];
    return true;
}

bool SX1262Radio::getPacketStatus(uint8_t packetStatus[3])
{
    return readCommand(CmdGetPacketStatus, packetStatus, 3);
}

bool SX1262Radio::getDeviceErrors(uint16_t& opError)
{
    uint8_t data[2] = { 0 };
    if (!readCommand(CmdGetDeviceErrors, data, sizeof(data))) {
        return false;
    }
    opError = (static_cast<uint16_t>(data[0]) << 8) | data[1];
    return true;
}

bool SX1262Radio::writeBuffer(const uint8_t* data, size_t len, uint8_t offset)
{
    if (!waitWhileBusy()) {
        return false;
    }

    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    select();
    _spi->transfer(CmdWriteBuffer);
    _spi->transfer(offset);
    for (size_t i = 0; i < len; ++i) {
        _spi->transfer(data[i]);
    }
    deselect();
    _spi->endTransaction();
    return waitWhileBusy();
}

bool SX1262Radio::readBuffer(uint8_t* data, size_t len, uint8_t offset)
{
    if (!waitWhileBusy()) {
        return false;
    }

    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    select();
    // DMA bulk transfer: send header + read payload in one shot
    // Reduces read time from ~180us to ~95us for 62-byte packets
    const size_t total = 3 + len;
    uint8_t tx[3 + MaxPacketLength] = {};
    uint8_t rx[3 + MaxPacketLength] = {};
    tx[0] = CmdReadBuffer;
    tx[1] = offset;
    // tx[2..] = 0x00 (NOP bytes)
    _spi->transferBytes(tx, rx, total);
    memcpy(data, &rx[3], len);
    deselect();
    _spi->endTransaction();
    return waitWhileBusy();
}

bool SX1262Radio::writeCommand(uint8_t opcode, const uint8_t* data, size_t len, bool wait_after)
{
    if (!waitWhileBusy()) {
        return false;
    }

    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    select();
    _spi->transfer(opcode);
    for (size_t i = 0; i < len; ++i) {
        _spi->transfer(data[i]);
    }
    deselect();
    _spi->endTransaction();

    return !wait_after || waitWhileBusy();
}

bool SX1262Radio::readCommand(uint8_t opcode, uint8_t* data, size_t len)
{
    if (!waitWhileBusy()) {
        return false;
    }

    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    select();
    _spi->transfer(opcode);
    _lastCommandStatus = _spi->transfer(0x00);
    for (size_t i = 0; i < len; ++i) {
        data[i] = _spi->transfer(0x00);
    }
    deselect();
    _spi->endTransaction();

    return waitWhileBusy();
}

bool SX1262Radio::writeRegister(uint16_t address, const uint8_t* data, size_t len)
{
    if (!waitWhileBusy()) {
        return false;
    }

    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    select();
    _spi->transfer(CmdWriteRegister);
    _spi->transfer(static_cast<uint8_t>((address >> 8) & 0xFF));
    _spi->transfer(static_cast<uint8_t>(address & 0xFF));
    for (size_t i = 0; i < len; ++i) {
        _spi->transfer(data[i]);
    }
    deselect();
    _spi->endTransaction();

    return waitWhileBusy();
}

bool SX1262Radio::readRegister(uint16_t address, uint8_t* data, size_t len)
{
    if (!waitWhileBusy()) {
        return false;
    }

    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    select();
    _spi->transfer(CmdReadRegister);
    _spi->transfer(static_cast<uint8_t>((address >> 8) & 0xFF));
    _spi->transfer(static_cast<uint8_t>(address & 0xFF));
    _spi->transfer(0x00);
    for (size_t i = 0; i < len; ++i) {
        data[i] = _spi->transfer(0x00);
    }
    deselect();
    _spi->endTransaction();

    return waitWhileBusy();
}

bool SX1262Radio::waitWhileBusy(uint32_t timeout_ms) const
{
    const uint32_t started = millis();
    while (digitalRead(_pinBusy) == HIGH) {
        if (millis() - started >= timeout_ms) {
            _busyTimeoutCount++;
            return false;
        }
        delayMicroseconds(50);
        yield();
    }
    return true;
}

bool SX1262Radio::waitWhileBusyFast(uint32_t timeout_us) const
{
    const uint32_t started = micros();
    while (digitalRead(_pinBusy) == HIGH) {
        if (micros() - started >= timeout_us) {
            _busyTimeoutCount++;
            return false;
        }
    }
    return true;
}

void SX1262Radio::spiSendOpcode(uint8_t opcode, const uint8_t* data, size_t len)
{
    _spi->beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    select();
    _spi->transfer(opcode);
    for (size_t i = 0; i < len; ++i) {
        _spi->transfer(data[i]);
    }
    deselect();
    _spi->endTransaction();
}

bool SX1262Radio::hopToChannel(uint8_t channel)
{
    // Hop via STDBY_RC for full packet engine reset (AGC, DC offset,
    // preamble detector, sync correlator all cleared).
    // SetFs-only path leaves stale baseband state that prevents preamble
    // detection on a new channel after receiving a packet.
    _channel = channel;
    const uint32_t freqHz = getBaseFrequency()
        + static_cast<uint32_t>(channel) * HOYMILES_FH_OFFSET * HOYMILES_FH_STEP_SIZE;

    // 1. STDBY_RC: full baseband reset (packet engine, AGC, correlator)
    const uint8_t stdbyRc = StandbyRc;
    if (!writeCommand(CmdSetStandby, &stdbyRc, 1)) return false;

    // 2. SetFs: keep PLL powered for faster relock on new frequency
    if (!writeCommand(CmdSetFs, nullptr, 0)) return false;

    // 3. SetRfFrequency: PLL relocks from warm FS state
    if (!setRfFrequencyHz(freqHz)) return false;

    // 4. Restore packet params (write() changes payloadLength for TX)
    if (!setPacketParamsGfsk(240, GfskPreambleDetect16, GfskCrcOff,
            32, GfskWhiteningOff, GfskPacketFixed, MaxPacketLength)) return false;

    // 5. Reset FIFO pointer for clean packet reception
    if (!setBufferBaseAddress(0x00, 0x00)) return false;

    // 6. Re-apply IRQ masks (lost after STDBY_RC)
    if (!setDioIrqParams(IrqRxDone | IrqTimeout | IrqPreambleDetected | IrqSyncWordValid,
            IrqRxDone | IrqTimeout | IrqPreambleDetected | IrqSyncWordValid)) return false;

    // 7. Clear any stale IRQ flags
    if (!clearIrqStatus()) return false;

    // 8. Enter RX continuous (no artificial settle delay needed)
    return setRxContinuous();
}

void SX1262Radio::select() const
{
    digitalWrite(_pinCs, LOW);
}

void SX1262Radio::deselect() const
{
    digitalWrite(_pinCs, HIGH);
}
