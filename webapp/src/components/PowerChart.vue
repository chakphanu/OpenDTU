<template>
    <div ref="chartContainer" class="chart-container"></div>
</template>

<script lang="ts">
import { defineComponent, type PropType } from 'vue';
import uPlot from 'uplot';
import 'uplot/dist/uPlot.min.css';

export default defineComponent({
    name: 'PowerChart',
    props: {
        data: {
            type: Array as PropType<uPlot.AlignedData>,
            required: true,
        },
        series: {
            type: Array as PropType<uPlot.Series[]>,
            required: true,
        },
        title: {
            type: String,
            default: '',
        },
        timeRangeMinutes: {
            type: Number,
            default: 1440,
        },
    },
    data() {
        return {
            chart: null as uPlot | null,
            resizeObserver: null as ResizeObserver | null,
        };
    },
    mounted() {
        this.createChart();
        this.resizeObserver = new ResizeObserver(() => {
            if (this.chart && this.$refs.chartContainer) {
                const el = this.$refs.chartContainer as HTMLElement;
                this.chart.setSize({ width: el.clientWidth, height: 300 });
            }
        });
        this.resizeObserver.observe(this.$refs.chartContainer as HTMLElement);
    },
    beforeUnmount() {
        if (this.resizeObserver) {
            this.resizeObserver.disconnect();
        }
        if (this.chart) {
            this.chart.destroy();
        }
    },
    watch: {
        data: {
            handler() {
                this.createChart();
            },
            deep: true,
        },
        timeRangeMinutes() {
            this.createChart();
        },
    },
    methods: {
        isDarkTheme(): boolean {
            return document.documentElement.getAttribute('data-bs-theme') === 'dark';
        },
        createChart() {
            if (this.chart) {
                this.chart.destroy();
            }

            const container = this.$refs.chartContainer as HTMLElement;
            if (!container || !this.data || this.data.length === 0 || this.data[0].length === 0) {
                return;
            }

            const dark = this.isDarkTheme();
            const textColor = dark ? '#ccc' : '#333';
            const gridColor = dark ? '#444' : '#eee';

            // Set X-axis range based on time range selection
            const now = Math.floor(Date.now() / 1000);
            const rangeSeconds = this.timeRangeMinutes * 60;
            const xMin = now - rangeSeconds;
            const xMax = now;

            const opts: uPlot.Options = {
                title: this.title,
                width: container.clientWidth,
                height: 300,
                cursor: {
                    drag: { x: true, y: false },
                },
                scales: {
                    x: {
                        min: xMin,
                        max: xMax,
                    },
                },
                axes: [
                    {
                        stroke: textColor,
                        grid: { stroke: gridColor },
                    },
                    {
                        stroke: textColor,
                        grid: { stroke: gridColor },
                        size: 60,
                    },
                ],
                series: this.series,
            };

            this.chart = new uPlot(opts, this.data, container);
        },
    },
});
</script>

<style scoped>
.chart-container {
    width: 100%;
    min-height: 300px;
}
</style>
