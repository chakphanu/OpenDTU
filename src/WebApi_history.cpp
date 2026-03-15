// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022-2025 Thomas Basler and others
 */
#include "WebApi_history.h"
#include "Configuration.h"
#include "HistoryStore.h"
#include "WebApi.h"
#include <AsyncJson.h>
#include <Hoymiles.h>

#undef TAG
static const char TAG[] = "WebApi_history";

void WebApiHistoryClass::init(AsyncWebServer& server, Scheduler& scheduler)
{
    using std::placeholders::_1;

    server.on("/api/history/status", HTTP_GET, std::bind(&WebApiHistoryClass::onHistoryStatus, this, _1));
    server.on("/api/history/data", HTTP_GET, std::bind(&WebApiHistoryClass::onHistoryData, this, _1));
}

void WebApiHistoryClass::onHistoryStatus(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    AsyncJsonResponse* response = new AsyncJsonResponse();
    auto& root = response->getRoot();

    root["max_records"] = HistoryStore.getMaxRecords();
    root["record_count"] = HistoryStore.getRecordCount();
    root["slot_size"] = HistoryStore.getSlotSize();
    root["total_bytes"] = HistoryStore.getMaxRecords() * HistoryStoreClass::SLOT_SIZE;
    root["oldest_timestamp"] = HistoryStore.getOldestTimestamp();
    root["newest_timestamp"] = HistoryStore.getNewestTimestamp();
    root["allocated"] = HistoryStore.isAllocated();

    WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
}

void WebApiHistoryClass::onHistoryData(AsyncWebServerRequest* request)
{
    if (!WebApi.checkCredentialsReadonly(request)) {
        return;
    }

    // Check for "all" inverters mode before serial parsing
    if (request->hasParam("inv") && request->getParam("inv")->value() == "all") {
        uint8_t numInv = Hoymiles.getNumInverters();
        if (numInv == 0) {
            AsyncJsonResponse* response = new AsyncJsonResponse();
            auto& root = response->getRoot();
            root["error"] = "No inverters configured";
            response->setCode(404);
            WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
            return;
        }

        size_t count = HistoryStore.getRecordCount();
        auto* response = request->beginResponseStream("application/json");

        response->printf("{\"serial\":\"all\",\"count\":%u,\"names\":[", (unsigned)count);

        // Inverter names array
        for (uint8_t n = 0; n < numInv; n++) {
            auto inv = Hoymiles.getInverterByPos(n);
            if (n > 0)
                response->print(',');
            const char* name = "";
            if (inv != nullptr) {
                auto* cfg = Configuration.getInverterConfig(inv->serial());
                if (cfg != nullptr) {
                    name = cfg->Name;
                }
            }
            response->print('"');
            // Escape any quotes in name
            for (const char* p = name; *p; p++) {
                if (*p == '"' || *p == '\\')
                    response->print('\\');
                response->print(*p);
            }
            response->print('"');
        }
        response->print("],\"data\":{");

        // Timestamp array - use first valid record per slot
        response->print("\"t\":[");
        for (size_t i = 0; i < count; i++) {
            size_t age = count - 1 - i;
            uint32_t ts = 0;
            for (uint8_t n = 0; n < numInv; n++) {
                const auto* rec = HistoryStore.getRecord(age, n);
                if (rec && rec->timestamp > 0) {
                    ts = rec->timestamp;
                    break;
                }
            }
            if (i > 0)
                response->print(',');
            response->print(ts);
        }
        response->print(']');

        // Total AC power array (sum across all inverters)
        response->print(",\"total\":[");
        for (size_t i = 0; i < count; i++) {
            size_t age = count - 1 - i;
            uint32_t total = 0;
            for (uint8_t n = 0; n < numInv; n++) {
                const auto* rec = HistoryStore.getRecord(age, n);
                if (rec) {
                    total += rec->acPower;
                }
            }
            if (i > 0)
                response->print(',');
            response->print(total);
        }
        response->print(']');

        // Per-inverter AC power arrays
        for (uint8_t n = 0; n < numInv; n++) {
            response->printf(",\"inv%u\":[", n);
            for (size_t i = 0; i < count; i++) {
                size_t age = count - 1 - i;
                const auto* rec = HistoryStore.getRecord(age, n);
                if (i > 0)
                    response->print(',');
                response->print(rec ? rec->acPower : 0);
            }
            response->print(']');
        }

        response->print("}}");
        request->send(response);
        return;
    }

    uint64_t serial = WebApiClass::parseSerialFromRequest(request);
    if (serial == 0) {
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();
        root["error"] = "Missing or invalid 'inv' parameter";
        response->setCode(400);
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    // Find inverter index by matching serial
    int invIndex = -1;
    uint8_t channels = 0;
    for (uint8_t i = 0; i < Hoymiles.getNumInverters(); i++) {
        auto inv = Hoymiles.getInverterByPos(i);
        if (inv->serial() == serial) {
            invIndex = static_cast<int>(i);
            channels = inv->Statistics()->getChannelsByType(TYPE_DC).size();
            break;
        }
    }

    if (invIndex < 0) {
        AsyncJsonResponse* response = new AsyncJsonResponse();
        auto& root = response->getRoot();
        root["error"] = "Inverter not found";
        response->setCode(404);
        WebApi.sendJsonResponse(request, response, __FUNCTION__, __LINE__);
        return;
    }

    // Cap channel count to what HistoryRecord supports
    if (channels > INV_MAX_CHAN_COUNT) {
        channels = INV_MAX_CHAN_COUNT;
    }

    size_t count = HistoryStore.getRecordCount();

    auto* response = request->beginResponseStream("application/json");

    char serialStr[13];
    snprintf(serialStr, sizeof(serialStr), "%012llX", (unsigned long long)serial);

    response->printf("{\"serial\":\"%s\",\"channels\":%u,\"count\":%u,\"data\":{", serialStr, channels, (unsigned)count);

    // Timestamp array
    response->print("\"t\":[");
    for (size_t i = 0; i < count; i++) {
        size_t age = count - 1 - i; // oldest first
        const auto* rec = HistoryStore.getRecord(age, invIndex);
        if (i > 0)
            response->print(',');
        response->print(rec ? rec->timestamp : 0);
    }
    response->print(']');

    // AC power array
    response->print(",\"ac\":[");
    for (size_t i = 0; i < count; i++) {
        size_t age = count - 1 - i;
        const auto* rec = HistoryStore.getRecord(age, invIndex);
        if (i > 0)
            response->print(',');
        response->print(rec ? rec->acPower : 0);
    }
    response->print(']');

    // DC power arrays per channel
    for (uint8_t ch = 0; ch < channels; ch++) {
        response->printf(",\"dc%u\":[", ch);
        for (size_t i = 0; i < count; i++) {
            size_t age = count - 1 - i;
            const auto* rec = HistoryStore.getRecord(age, invIndex);
            if (i > 0)
                response->print(',');
            response->print(rec ? rec->dcPower[ch] : 0);
        }
        response->print(']');
    }

    // Temperature array
    response->print(",\"temp\":[");
    for (size_t i = 0; i < count; i++) {
        size_t age = count - 1 - i;
        const auto* rec = HistoryStore.getRecord(age, invIndex);
        if (i > 0)
            response->print(',');
        response->print(rec ? rec->temperature : 0);
    }
    response->print(']');

    // Yield day array
    response->print(",\"yd\":[");
    for (size_t i = 0; i < count; i++) {
        size_t age = count - 1 - i;
        const auto* rec = HistoryStore.getRecord(age, invIndex);
        if (i > 0)
            response->print(',');
        response->print(rec ? rec->yieldDay : 0);
    }
    response->print(']');

    response->print("}}");
    request->send(response);
}
