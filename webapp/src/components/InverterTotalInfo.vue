<template>
    <div class="row row-cols-1 row-cols-md-3 g-3">
        <div class="col">
            <CardElement centerContent textVariant="text-bg-primary" :text="$t('invertertotalinfo.TotalYieldTotal')">
                <h2>
                    {{
                        $n(totalData.YieldTotal.v, 'decimal', {
                            minimumFractionDigits: totalData.YieldTotal.d,
                            maximumFractionDigits: totalData.YieldTotal.d,
                        })
                    }}
                    <small class="text-muted">{{ totalData.YieldTotal.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col">
            <CardElement centerContent textVariant="text-bg-primary" :text="$t('invertertotalinfo.TotalYieldDay')">
                <div class="sparkline-wrapper">
                    <div v-if="sparkYd.length > 1" class="sparkline-bg">
                        <SparkLine :values="sparkYd" color="#ffffff" :fillOpacity="0.15" />
                    </div>
                    <h2 class="sparkline-value">
                        {{
                            $n(totalData.YieldDay.v, 'decimal', {
                                minimumFractionDigits: totalData.YieldDay.d,
                                maximumFractionDigits: totalData.YieldDay.d,
                            })
                        }}
                        <small class="text-muted">{{ totalData.YieldDay.u }}</small>
                    </h2>
                </div>
            </CardElement>
        </div>
        <div class="col">
            <CardElement centerContent textVariant="text-bg-primary" :text="$t('invertertotalinfo.TotalPower')">
                <div class="sparkline-wrapper">
                    <div v-if="sparkPower.length > 1" class="sparkline-bg">
                        <SparkLine :values="sparkPower" color="#ffffff" :fillOpacity="0.15" />
                    </div>
                    <h2 class="sparkline-value">
                        {{
                            $n(totalData.Power.v, 'decimal', {
                                minimumFractionDigits: totalData.Power.d,
                                maximumFractionDigits: totalData.Power.d,
                            })
                        }}
                        <small class="text-muted">{{ totalData.Power.u }}</small>
                    </h2>
                </div>
            </CardElement>
        </div>
    </div>
</template>

<script lang="ts">
import type { Total } from '@/types/LiveDataStatus';
import CardElement from './CardElement.vue';
import SparkLine from './SparkLine.vue';
import { defineComponent, ref, onMounted, onBeforeUnmount, type PropType } from 'vue';
import { authHeader } from '@/utils/authentication';

export default defineComponent({
    components: {
        CardElement,
        SparkLine,
    },
    props: {
        totalData: { type: Object as PropType<Total>, required: true },
    },
    setup() {
        const sparkPower = ref<number[]>([]);
        const sparkYd = ref<number[]>([]);
        let timer: ReturnType<typeof setInterval> | null = null;

        async function fetchSparkline() {
            try {
                const resp = await fetch('/api/history/sparkline', { headers: authHeader() });
                if (!resp.ok) return;
                const data = await resp.json();
                if (data.power) sparkPower.value = data.power;
                if (data.yd) sparkYd.value = data.yd;
            } catch {
                // ignore fetch errors
            }
        }

        onMounted(() => {
            fetchSparkline();
            timer = setInterval(fetchSparkline, 60000);
        });

        onBeforeUnmount(() => {
            if (timer) clearInterval(timer);
        });

        return { sparkPower, sparkYd };
    },
});
</script>

<style scoped>
.sparkline-wrapper {
    position: relative;
}
.sparkline-bg {
    position: absolute;
    bottom: 0;
    left: 0;
    right: 0;
    height: 70%;
    pointer-events: none;
}
.sparkline-value {
    position: relative;
    z-index: 1;
    margin: 0;
}
</style>
