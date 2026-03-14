<template>
    <CardElement :text="$t('radioinfo.RadioInformation')" textVariant="text-bg-primary" table>
        <div class="table-responsive">
            <table class="table table-hover table-condensed">
                <tbody>
                    <tr>
                        <th>{{ $t('radioinfo.Status', { module: 'nRF24' }) }}</th>
                        <td>
                            <StatusBadge
                                :status="systemStatus.nrf_configured"
                                true_text="radioinfo.Configured"
                                false_text="radioinfo.NotConfigured"
                                false_class="text-bg-secondary"
                            />
                        </td>
                    </tr>
                    <tr>
                        <th>{{ $t('radioinfo.ChipStatus', { module: 'nRF24' }) }}</th>
                        <td>
                            <span
                                class="badge"
                                :class="{
                                    'text-bg-danger': systemStatus.nrf_configured && !systemStatus.nrf_connected,
                                    'text-bg-success': systemStatus.nrf_configured && systemStatus.nrf_connected,
                                }"
                            >
                                <template v-if="systemStatus.nrf_configured && systemStatus.nrf_connected">{{
                                    $t('radioinfo.Connected')
                                }}</template>
                                <template v-else-if="systemStatus.nrf_configured && !systemStatus.nrf_connected">{{
                                    $t('radioinfo.NotConnected')
                                }}</template>
                            </span>
                        </td>
                    </tr>
                    <tr>
                        <th>{{ $t('radioinfo.ChipType', { module: 'nRF24' }) }}</th>
                        <td>
                            <span
                                class="badge"
                                :class="{
                                    'text-bg-danger': systemStatus.nrf_connected && !systemStatus.nrf_pvariant,
                                    'text-bg-success': systemStatus.nrf_connected && systemStatus.nrf_pvariant,
                                    'text-bg-secondary': !systemStatus.nrf_connected,
                                }"
                            >
                                <template v-if="systemStatus.nrf_connected && systemStatus.nrf_pvariant"
                                    >nRF24L01+</template
                                >
                                <template v-else-if="systemStatus.nrf_connected && !systemStatus.nrf_pvariant"
                                    >nRF24L01</template
                                >
                                <template v-else>{{ $t('radioinfo.Unknown') }}</template>
                            </span>
                        </td>
                    </tr>
                    <tr>
                        <th>{{ $t('radioinfo.Status', { module: 'CMT2300A' }) }}</th>
                        <td>
                            <StatusBadge
                                :status="systemStatus.cmt_configured"
                                true_text="radioinfo.Configured"
                                false_text="radioinfo.NotConfigured"
                                false_class="text-bg-secondary"
                            />
                        </td>
                    </tr>
                    <tr>
                        <th>{{ $t('radioinfo.ChipStatus', { module: 'CMT2300A' }) }}</th>
                        <td>
                            <span
                                class="badge"
                                :class="{
                                    'text-bg-danger': systemStatus.cmt_configured && !systemStatus.cmt_connected,
                                    'text-bg-success': systemStatus.cmt_configured && systemStatus.cmt_connected,
                                }"
                            >
                                <template v-if="systemStatus.cmt_configured && systemStatus.cmt_connected">{{
                                    $t('radioinfo.Connected')
                                }}</template>
                                <template v-else-if="systemStatus.cmt_configured && !systemStatus.cmt_connected">{{
                                    $t('radioinfo.NotConnected')
                                }}</template>
                            </span>
                        </td>
                    </tr>
                    <tr>
                        <th>{{ $t('radioinfo.Status', { module: 'SX1262' }) }}</th>
                        <td>
                            <StatusBadge
                                :status="systemStatus.sx1262_configured"
                                true_text="radioinfo.Configured"
                                false_text="radioinfo.NotConfigured"
                                false_class="text-bg-secondary"
                            />
                        </td>
                    </tr>
                    <tr>
                        <th>{{ $t('radioinfo.ChipStatus', { module: 'SX1262' }) }}</th>
                        <td>
                            <span
                                class="badge"
                                :class="{
                                    'text-bg-danger': systemStatus.sx1262_configured && !systemStatus.sx1262_connected,
                                    'text-bg-success': systemStatus.sx1262_configured && systemStatus.sx1262_connected,
                                }"
                            >
                                <template v-if="systemStatus.sx1262_configured && systemStatus.sx1262_connected">{{
                                    $t('radioinfo.Connected')
                                }}</template>
                                <template
                                    v-else-if="systemStatus.sx1262_configured && !systemStatus.sx1262_connected"
                                    >{{ $t('radioinfo.NotConnected') }}</template
                                >
                            </span>
                        </td>
                    </tr>
                    <tr>
                        <th>{{ $t('radioinfo.ChipType', { module: 'SX1262' }) }}</th>
                        <td>
                            <span
                                class="badge"
                                :class="{
                                    'text-bg-success': systemStatus.sx1262_connected,
                                    'text-bg-secondary': !systemStatus.sx1262_connected,
                                }"
                            >
                                <template v-if="systemStatus.sx1262_connected">SX1262</template>
                                <template v-else>{{ $t('radioinfo.Unknown') }}</template>
                            </span>
                        </td>
                    </tr>
                    <tr v-if="systemStatus.sx1262_configured">
                        <th>SX1262 Frequency</th>
                        <td>{{ (systemStatus.sx1262_frequency / 1000000).toFixed(2) }} MHz</td>
                    </tr>
                    <tr v-if="systemStatus.sx1262_configured">
                        <th>SX1262 RX Bandwidth</th>
                        <td>156.2 kHz</td>
                    </tr>
                </tbody>
            </table>
        </div>
    </CardElement>
</template>

<script lang="ts">
import CardElement from '@/components/CardElement.vue';
import StatusBadge from './StatusBadge.vue';
import type { SystemStatus } from '@/types/SystemStatus';
import { defineComponent, type PropType } from 'vue';

export default defineComponent({
    components: {
        CardElement,
        StatusBadge,
    },
    props: {
        systemStatus: { type: Object as PropType<SystemStatus>, required: true },
    },
});
</script>
