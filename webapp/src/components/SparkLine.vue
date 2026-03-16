<template>
    <canvas ref="canvas" class="sparkline-canvas"></canvas>
</template>

<script lang="ts">
import { defineComponent, ref, watch, onMounted, onBeforeUnmount, type PropType } from 'vue';

export default defineComponent({
    props: {
        values: { type: Array as PropType<number[]>, required: true },
        color: { type: String, default: '#ffffff' },
        fillOpacity: { type: Number, default: 0.15 },
    },
    setup(props) {
        const canvas = ref<HTMLCanvasElement | null>(null);
        let observer: ResizeObserver | null = null;

        function draw() {
            const el = canvas.value;
            if (!el || !props.values.length) return;

            const rect = el.getBoundingClientRect();
            const w = rect.width;
            const h = rect.height;
            if (w === 0 || h === 0) return;

            const dpr = window.devicePixelRatio || 1;
            el.width = w * dpr;
            el.height = h * dpr;

            const ctx = el.getContext('2d');
            if (!ctx) return;
            ctx.scale(dpr, dpr);

            const vals = props.values as number[];
            const len = vals.length;
            if (len < 2) return;

            const min = Math.min(...vals);
            const max = Math.max(...vals);
            const range = max - min || 1;
            const pad = 2;

            ctx.clearRect(0, 0, w, h);
            ctx.beginPath();
            for (let i = 0; i < len; i++) {
                const x = (i / (len - 1)) * w;
                const y = h - pad - ((vals[i]! - min) / range) * (h - pad * 2);
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }

            // Fill
            ctx.lineTo(w, h);
            ctx.lineTo(0, h);
            ctx.closePath();
            ctx.fillStyle = props.color;
            ctx.globalAlpha = props.fillOpacity;
            ctx.fill();

            // Stroke
            ctx.beginPath();
            for (let i = 0; i < len; i++) {
                const x = (i / (len - 1)) * w;
                const y = h - pad - ((vals[i]! - min) / range) * (h - pad * 2);
                if (i === 0) ctx.moveTo(x, y);
                else ctx.lineTo(x, y);
            }
            ctx.globalAlpha = props.fillOpacity * 2;
            ctx.strokeStyle = props.color;
            ctx.lineWidth = 1.5;
            ctx.stroke();
        }

        watch(() => props.values, draw, { deep: true });

        onMounted(() => {
            observer = new ResizeObserver(draw);
            if (canvas.value) observer.observe(canvas.value);
            draw();
        });

        onBeforeUnmount(() => {
            if (observer) observer.disconnect();
        });

        return { canvas };
    },
});
</script>

<style scoped>
.sparkline-canvas {
    display: block;
    width: 100%;
    height: 100%;
}
</style>
