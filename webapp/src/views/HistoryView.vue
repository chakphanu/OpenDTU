<template>
    <BasePage :title="$t('history.PowerHistory')" :isLoading="dataLoading" :show-reload="true" @reload="fetchData">
        <div class="row mb-3">
            <div class="col-sm-6">
                <select class="form-select" v-model="selectedSerial" @change="fetchData">
                    <option value="all">{{ $t('history.AllInverters') }}</option>
                    <option v-for="inv in inverters" :key="inv.serial" :value="inv.serial">
                        {{ inv.name }} ({{ inv.serial }})
                    </option>
                </select>
            </div>
        </div>

        <div class="row mb-3">
            <div class="col-auto">
                <div class="btn-group" role="group">
                    <button v-for="range in timeRanges" :key="range.label"
                        type="button"
                        class="btn btn-sm"
                        :class="selectedRange === range.minutes ? 'btn-primary' : 'btn-outline-secondary'"
                        @click="setTimeRange(range.minutes)">
                        {{ range.label }}
                    </button>
                </div>
            </div>
            <div class="col-auto">
                <div class="form-check form-switch mt-1">
                    <input class="form-check-input" type="checkbox" v-model="autoRefresh" @change="toggleAutoRefresh" id="autoRefreshCheck">
                    <label class="form-check-label" for="autoRefreshCheck">{{ $t('history.AutoRefresh') }}</label>
                </div>
            </div>
        </div>

        <div class="card mb-3">
            <div class="card-header">{{ selectedSerial === 'all' ? $t('history.TotalACPower') : $t('history.ACPower') }}</div>
            <div class="card-body">
                <PowerChart :data="acChartData" :series="acSeries" :timeRangeMinutes="selectedRange" />
            </div>
        </div>

        <div class="card mb-3" v-if="channels > 0 && selectedSerial !== 'all'">
            <div class="card-header">{{ $t('history.DCPower') }}</div>
            <div class="card-body">
                <PowerChart :data="dcChartData" :series="dcSeries" :timeRangeMinutes="selectedRange" />
            </div>
        </div>

        <div class="card">
            <div class="card-header">{{ $t('history.BufferStatus') }}</div>
            <div class="card-body">
                <table class="table table-sm table-striped">
                    <tbody>
                        <tr>
                            <th>{{ $t('history.Records') }}</th>
                            <td>{{ status.record_count }} / {{ status.max_records }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('history.BufferUsage') }}</th>
                            <td>{{ bufferUsagePercent }}%</td>
                        </tr>
                        <tr>
                            <th>{{ $t('history.MemoryUsed') }}</th>
                            <td>{{ formatBytes(status.total_bytes) }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('history.OldestRecord') }}</th>
                            <td>{{ formatTimestamp(status.oldest_timestamp) }}</td>
                        </tr>
                        <tr>
                            <th>{{ $t('history.NewestRecord') }}</th>
                            <td>{{ formatTimestamp(status.newest_timestamp) }}</td>
                        </tr>
                    </tbody>
                </table>
            </div>
        </div>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import PowerChart from '@/components/PowerChart.vue';
import type { HistoryData, HistoryStatus } from '@/types/HistoryData';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';
import type uPlot from 'uplot';

interface InverterEntry {
    serial: string;
    name: string;
}

const DC_COLORS = ['#e74c3c', '#3498db', '#2ecc71', '#f39c12', '#9b59b6', '#1abc9c'];

export default defineComponent({
    components: {
        BasePage,
        PowerChart,
    },
    data() {
        return {
            dataLoading: true,
            inverters: [] as InverterEntry[],
            selectedSerial: 'all',
            historyData: null as HistoryData | null,
            status: {
                max_records: 0,
                record_count: 0,
                slot_size: 0,
                total_bytes: 0,
                oldest_timestamp: 0,
                newest_timestamp: 0,
                allocated: false,
            } as HistoryStatus,
            selectedRange: 1440, // default 24h
            autoRefresh: false,
            refreshTimer: null as ReturnType<typeof setInterval> | null,
            timeRanges: [
                { label: '2h', minutes: 120 },
                { label: '6h', minutes: 360 },
                { label: '24h', minutes: 1440 },
                { label: '7d', minutes: 10080 },
            ],
        };
    },
    computed: {
        channels(): number {
            return this.historyData?.channels ?? 0;
        },
        isAllMode(): boolean {
            return this.selectedSerial === 'all';
        },
        filteredData(): HistoryData['data'] | null {
            if (!this.historyData || !this.historyData.data) {
                return null;
            }
            const d = this.historyData.data;
            if (!d.t || d.t.length === 0) {
                return d;
            }
            const cutoff = Math.floor(Date.now() / 1000) - this.selectedRange * 60;
            const startIdx = d.t.findIndex((ts) => ts >= cutoff);
            if (startIdx < 0) {
                // No data in range
                if (this.isAllMode) {
                    return { t: [], ac: [], temp: [], yd: [], total: [] };
                }
                return { t: [], ac: [], temp: [], yd: [] };
            }
            if (this.isAllMode) {
                const result: HistoryData['data'] = {
                    t: d.t.slice(startIdx),
                    ac: [],
                    temp: [],
                    yd: [],
                    total: d.total ? d.total.slice(startIdx) : [],
                };
                for (let i = 0; i < 10; i++) {
                    const key = ('inv' + i) as keyof HistoryData['data'];
                    if (d[key]) {
                        (result as Record<string, number[]>)[key] = (d[key] as number[]).slice(startIdx);
                    }
                }
                return result;
            }
            const result: HistoryData['data'] = {
                t: d.t.slice(startIdx),
                ac: d.ac.slice(startIdx),
                temp: d.temp.slice(startIdx),
                yd: d.yd.slice(startIdx),
            };
            for (let i = 0; i < 6; i++) {
                const key = ('dc' + i) as keyof HistoryData['data'];
                if (d[key]) {
                    (result as Record<string, number[]>)[key] = (d[key] as number[]).slice(startIdx);
                }
            }
            return result;
        },
        acChartData(): uPlot.AlignedData {
            const d = this.filteredData;
            if (!d || !d.t || d.t.length === 0) {
                return [new Float64Array(0), new Float64Array(0)] as uPlot.AlignedData;
            }
            if (this.isAllMode && d.total) {
                const result: (number[] | Float64Array)[] = [d.t, d.total];
                const names = this.historyData?.names ?? [];
                for (let i = 0; i < names.length; i++) {
                    const key = ('inv' + i) as keyof HistoryData['data'];
                    const arr = d[key];
                    if (arr) {
                        result.push(arr as number[]);
                    }
                }
                return result as uPlot.AlignedData;
            }
            return [d.t, d.ac] as uPlot.AlignedData;
        },
        acSeries(): uPlot.Series[] {
            if (this.isAllMode) {
                const series: uPlot.Series[] = [
                    {},
                    {
                        label: 'Total',
                        stroke: '#ecf0f1',
                        width: 3,
                        fill: 'rgba(236,240,241,0.08)',
                    },
                ];
                const names = this.historyData?.names ?? [];
                for (let i = 0; i < names.length; i++) {
                    series.push({
                        label: names[i] || ('Inverter ' + (i + 1)),
                        stroke: DC_COLORS[i % DC_COLORS.length],
                        width: 1.5,
                    });
                }
                return series;
            }
            return [
                {},
                {
                    label: 'AC Power',
                    stroke: '#f39c12',
                    width: 2,
                    fill: 'rgba(243,156,18,0.1)',
                },
            ];
        },
        dcChartData(): uPlot.AlignedData {
            const d = this.filteredData;
            if (!d || !d.t || d.t.length === 0) {
                return [new Float64Array(0)] as uPlot.AlignedData;
            }
            const result: (number[] | Float64Array)[] = [d.t];
            for (let i = 0; i < this.channels; i++) {
                const key = ('dc' + i) as keyof HistoryData['data'];
                const arr = d[key];
                if (arr) {
                    result.push(arr as number[]);
                }
            }
            return result as uPlot.AlignedData;
        },
        dcSeries(): uPlot.Series[] {
            const series: uPlot.Series[] = [{}];
            for (let i = 0; i < this.channels; i++) {
                series.push({
                    label: 'DC' + (i + 1),
                    stroke: DC_COLORS[i % DC_COLORS.length],
                    width: 2,
                });
            }
            return series;
        },
        bufferUsagePercent(): string {
            if (!this.status.max_records || this.status.max_records === 0) {
                return '0';
            }
            return ((this.status.record_count / this.status.max_records) * 100).toFixed(1);
        },
    },
    created() {
        this.fetchInverters();
    },
    beforeUnmount() {
        if (this.refreshTimer) {
            clearInterval(this.refreshTimer);
        }
    },
    methods: {
        fetchInverters() {
            this.dataLoading = true;
            fetch('/api/livedata/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    if (data.inverters && data.inverters.length > 0) {
                        this.inverters = data.inverters.map((inv: { serial: string; name: string }) => ({
                            serial: inv.serial,
                            name: inv.name,
                        }));
                    }
                    this.fetchData();
                });
        },
        fetchData() {
            if (!this.selectedSerial) {
                this.dataLoading = false;
                return;
            }
            this.dataLoading = true;

            const dataPromise = fetch('/api/history/data?inv=' + this.selectedSerial, { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router));

            const statusPromise = fetch('/api/history/status', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router));

            Promise.all([dataPromise, statusPromise])
                .then(([histData, statusData]) => {
                    this.historyData = histData as HistoryData;
                    this.status = statusData as HistoryStatus;
                    this.dataLoading = false;
                })
                .catch(() => {
                    this.dataLoading = false;
                });
        },
        setTimeRange(minutes: number) {
            this.selectedRange = minutes;
        },
        toggleAutoRefresh() {
            if (this.refreshTimer) {
                clearInterval(this.refreshTimer);
                this.refreshTimer = null;
            }
            if (this.autoRefresh) {
                this.refreshTimer = setInterval(() => {
                    this.fetchData();
                }, 60000);
            }
        },
        formatBytes(bytes: number): string {
            if (!bytes || bytes === 0) return '0 B';
            const units = ['B', 'KB', 'MB'];
            let idx = 0;
            let val = bytes;
            while (val >= 1024 && idx < units.length - 1) {
                val /= 1024;
                idx++;
            }
            return val.toFixed(idx > 0 ? 1 : 0) + ' ' + units[idx];
        },
        formatTimestamp(ts: number): string {
            if (!ts || ts === 0) return '-';
            return new Date(ts * 1000).toLocaleString();
        },
    },
});
</script>
