// SPDX-License-Identifier: GPL-2.0-or-later
#include "HistoryStore.h"
#include "Configuration.h"
#include <Arduino.h>
#include <Hoymiles.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#undef TAG
static const char* TAG = "HistoryStore";

HistoryStoreClass HistoryStore;

HistoryStoreClass::HistoryStoreClass()
    : _loopTask(60 * TASK_SECOND, TASK_FOREVER, std::bind(&HistoryStoreClass::loop, this))
{
}

void HistoryStoreClass::init(Scheduler& scheduler)
{
    // Calculate desired number of records based on available memory
    size_t desired = 0;
    size_t psramSize = ESP.getPsramSize();

    if (psramSize > 0) {
        size_t budget = (psramSize * HISTORY_PSRAM_PERCENT) / 100;
        desired = budget / SLOT_SIZE;
        if (desired > HISTORY_MAX_RECORDS) {
            desired = HISTORY_MAX_RECORDS;
        }
        ESP_LOGI(TAG, "PSRAM detected: %u bytes, budget %u bytes, desired %u records",
            psramSize, budget, desired);
    } else {
        desired = HISTORY_SRAM_BUDGET / SLOT_SIZE;
        ESP_LOGI(TAG, "No PSRAM, SRAM budget %u bytes, desired %u records",
            HISTORY_SRAM_BUDGET, desired);
    }

    if (desired < HISTORY_MIN_RECORDS) {
        desired = HISTORY_MIN_RECORDS;
    }

    // Retry loop: reduce by 25% on each failure
    while (desired >= HISTORY_MIN_RECORDS) {
        size_t allocSize = desired * SLOT_SIZE;
        _records = static_cast<HistoryRecord*>(malloc(allocSize));
        if (_records != nullptr) {
            memset(_records, 0, allocSize);
            _maxRecords = desired;
            ESP_LOGI(TAG, "Allocated %u records (%u bytes, %u bytes/slot)",
                _maxRecords, allocSize, SLOT_SIZE);
            break;
        }
        ESP_LOGW(TAG, "Failed to allocate %u records (%u bytes), reducing by 25%%",
            desired, allocSize);
        desired = desired * 3 / 4;
    }

    if (_records == nullptr) {
        ESP_LOGW(TAG, "Failed to allocate history buffer, history disabled");
    }

    scheduler.addTask(_loopTask);
    _loopTask.enable();
}

void HistoryStoreClass::loop()
{
    if (_records == nullptr) {
        return;
    }

    // Check NTP is synced
    if (time(nullptr) < 1600000000) {
        return;
    }

    // Wait for radio idle
    if (!Hoymiles.isAllRadioIdle()) {
        _loopTask.forceNextIteration();
        return;
    }

    // Check if at least one inverter is reachable
    bool anyReachable = false;
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv != nullptr && inv->isReachable()) {
            anyReachable = true;
            break;
        }
    }
    if (!anyReachable) {
        return;
    }

    std::lock_guard<std::mutex> lock(_mutex);

    // Get pointer to current slot (INV_MAX_COUNT records per slot)
    HistoryRecord* slot = _records + (_head * INV_MAX_COUNT);
    memset(slot, 0, SLOT_SIZE);

    uint32_t now = time(nullptr);

    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv == nullptr || !inv->isReachable()) {
            // Leave slot zeroed (flags=0)
            continue;
        }

        HistoryRecord& rec = slot[i];
        rec.timestamp = now;
        rec.acPower = static_cast<uint16_t>(
            inv->Statistics()->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC));

        auto dcChannels = inv->Statistics()->getChannelsByType(TYPE_DC);
        uint8_t chIdx = 0;
        for (auto& c : dcChannels) {
            if (chIdx < INV_MAX_CHAN_COUNT) {
                rec.dcPower[chIdx] = static_cast<uint16_t>(
                    inv->Statistics()->getChannelFieldValue(TYPE_DC, c, FLD_PDC));
                chIdx++;
            }
        }
        rec.channelCount = chIdx;

        rec.temperature = static_cast<int8_t>(
            inv->Statistics()->getChannelFieldValue(TYPE_INV, CH0, FLD_T));
        rec.yieldDay = static_cast<uint16_t>(
            inv->Statistics()->getChannelFieldValue(TYPE_INV, CH0, FLD_YD));

        rec.rssi = inv->getLastRssi();

        rec.flags = FLAG_VALID | FLAG_REACHABLE;
        if (inv->isProducing()) {
            rec.flags |= FLAG_PRODUCING;
        }
    }

    // Advance ring buffer
    _head = (_head + 1) % _maxRecords;
    if (_count < _maxRecords) {
        _count++;
    }
}

size_t HistoryStoreClass::getMaxRecords() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _maxRecords;
}

size_t HistoryStoreClass::getRecordCount() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _count;
}

size_t HistoryStoreClass::getSlotSize() const
{
    return SLOT_SIZE;
}

bool HistoryStoreClass::isAllocated() const
{
    return _records != nullptr;
}

const HistoryRecord* HistoryStoreClass::getRecord(size_t slotAge, size_t invIndex) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (slotAge >= _count || invIndex >= INV_MAX_COUNT) {
        return nullptr;
    }
    size_t idx = (_head - 1 - slotAge + _maxRecords) % _maxRecords;
    return &_records[idx * INV_MAX_COUNT + invIndex];
}

uint32_t HistoryStoreClass::getOldestTimestamp() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_count == 0) {
        return 0;
    }
    // Oldest slot is at (_head - _count + _maxRecords) % _maxRecords
    size_t oldestSlot = (_head - _count + _maxRecords) % _maxRecords;
    // Return the first non-zero timestamp in the slot
    for (size_t i = 0; i < INV_MAX_COUNT; i++) {
        uint32_t ts = _records[oldestSlot * INV_MAX_COUNT + i].timestamp;
        if (ts > 0) {
            return ts;
        }
    }
    return 0;
}

uint32_t HistoryStoreClass::getNewestTimestamp() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (_count == 0) {
        return 0;
    }
    // Newest slot is at (_head - 1 + _maxRecords) % _maxRecords
    size_t newestSlot = (_head - 1 + _maxRecords) % _maxRecords;
    for (size_t i = 0; i < INV_MAX_COUNT; i++) {
        uint32_t ts = _records[newestSlot * INV_MAX_COUNT + i].timestamp;
        if (ts > 0) {
            return ts;
        }
    }
    return 0;
}
