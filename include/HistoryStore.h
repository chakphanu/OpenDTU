// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Configuration.h"
#include <Hoymiles.h>
#include <TaskSchedulerDeclarations.h>
#include <cstring>
#include <mutex>

struct __attribute__((packed)) HistoryRecord {
    uint32_t timestamp;                         // 4 bytes - epoch seconds
    uint16_t acPower;                           // 2 bytes - watts
    uint16_t dcPower[INV_MAX_CHAN_COUNT];        // 12 bytes - per MPPT watts
    int8_t temperature;                         // 1 byte - degrees C
    uint16_t yieldDay;                          // 2 bytes - Wh
    uint8_t channelCount;                       // 1 byte - actual DC channels
    uint8_t flags;                              // 1 byte - bit0=valid, bit1=reachable, bit2=producing
    int8_t rssi;                                // 1 byte - last RSSI dBm
};
// Total: 24 bytes

static_assert(sizeof(HistoryRecord) == 24, "HistoryRecord must be 24 bytes");

class HistoryStoreClass {
public:
    static constexpr uint8_t FLAG_VALID = 0x01;
    static constexpr uint8_t FLAG_REACHABLE = 0x02;
    static constexpr uint8_t FLAG_PRODUCING = 0x04;

    static constexpr size_t SLOT_SIZE = sizeof(HistoryRecord) * INV_MAX_COUNT;
    static constexpr size_t HISTORY_MAX_RECORDS = 10080;  // 7 days max
    static constexpr size_t HISTORY_MIN_RECORDS = 60;
    static constexpr size_t HISTORY_SRAM_BUDGET = 50 * 1024;
    static constexpr size_t HISTORY_PSRAM_PERCENT = 50;

    HistoryStoreClass();
    void init(Scheduler& scheduler);

    size_t getMaxRecords() const;
    size_t getRecordCount() const;
    size_t getSlotSize() const;
    bool isAllocated() const;

    // Get a record for a specific time slot and inverter index
    // slotAge: 0 = newest, 1 = one before newest, etc.
    const HistoryRecord* getRecord(size_t slotAge, size_t invIndex) const;

    // Get oldest and newest timestamps
    uint32_t getOldestTimestamp() const;
    uint32_t getNewestTimestamp() const;

private:
    void loop();

    Task _loopTask;

    mutable std::mutex _mutex;

    HistoryRecord* _records = nullptr;
    size_t _maxRecords = 0;
    size_t _head = 0;
    size_t _count = 0;
};

extern HistoryStoreClass HistoryStore;
