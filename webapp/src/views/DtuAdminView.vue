<template>
    <BasePage :title="$t('dtuadmin.DtuSettings')" :isLoading="dataLoading">
        <BootstrapAlert
            v-model="alert.show"
            dismissible
            :variant="alert.type"
            :auto-dismiss="alert.type != 'success' ? 0 : 5000"
        >
            {{ alert.message }}
        </BootstrapAlert>

        <form @submit="saveDtuConfig">
            <CardElement :text="$t('dtuadmin.DtuConfiguration')" textVariant="text-bg-primary">
                <InputElement
                    :label="$t('dtuadmin.Serial')"
                    v-model="dtuConfigList.serial"
                    type="text"
                    minlength="12"
                    maxlength="12"
                    :tooltip="$t('dtuadmin.SerialHint')"
                />

                <InputElement
                    :label="$t('dtuadmin.PollInterval')"
                    v-model="dtuConfigList.pollinterval"
                    type="number"
                    min="1"
                    max="86400"
                    :postfix="$t('dtuadmin.Seconds')"
                />

                <div class="row mb-3" v-if="dtuConfigList.nrf_enabled">
                    <label for="inputNrfPaLevel" class="col-sm-2 col-form-label">
                        {{ $t('dtuadmin.NrfPaLevel') }}
                        <BIconInfoCircle v-tooltip :title="$t('dtuadmin.NrfPaLevelHint')" />
                    </label>
                    <div class="col-sm-10">
                        <select id="inputNrfPaLevel" class="form-select" v-model="dtuConfigList.nrf_palevel">
                            <option v-for="palevel in nrfpalevelList" :key="palevel.key" :value="palevel.key">
                                {{ $t(`dtuadmin.` + palevel.value, { db: palevel.db }) }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3" v-if="dtuConfigList.cmt_enabled">
                    <label for="inputCmtPaLevel" class="col-sm-2 col-form-label">
                        {{ $t('dtuadmin.CmtPaLevel') }}
                        <BIconInfoCircle v-tooltip :title="$t('dtuadmin.CmtPaLevelHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="range"
                                class="form-control form-range"
                                v-model.number="dtuConfigList.cmt_palevel"
                                min="-10"
                                max="22"
                                id="inputCmtPaLevel"
                                aria-describedby="basic-addon1"
                                style="height: unset"
                            />
                            <span class="input-group-text" id="basic-addon1">{{ cmtPaLevelText }}</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3" v-if="dtuConfigList.cmt_enabled">
                    <label for="inputCmtCountry" class="col-sm-2 col-form-label">
                        {{ $t('dtuadmin.CmtCountry') }}
                        <BIconInfoCircle v-tooltip :title="$t('dtuadmin.CmtCountryHint')" />
                    </label>
                    <div class="col-sm-10">
                        <select id="inputCmtCountry" class="form-select" v-model="dtuConfigList.cmt_country">
                            <option v-for="(country, index) in dtuConfigList.country_def" :key="index" :value="index">
                                {{
                                    $t(`dtuadmin.country_` + index, {
                                        min: country.freq_min / 1e6,
                                        max: country.freq_max / 1e6,
                                    })
                                }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3" v-if="dtuConfigList.cmt_enabled">
                    <label for="cmtFrequency" class="col-sm-2 col-form-label">
                        {{ $t('dtuadmin.CmtFrequency') }}
                        <BIconInfoCircle v-tooltip :title="$t('dtuadmin.CmtFrequencyHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="range"
                                class="form-control form-range"
                                v-model.number="dtuConfigList.cmt_frequency"
                                :min="cmtMinFrequency"
                                :max="cmtMaxFrequency"
                                :step="dtuConfigList.cmt_chan_width"
                                id="cmtFrequency"
                                aria-describedby="basic-addon2"
                                style="height: unset"
                            />
                            <span class="input-group-text" id="basic-addon2">{{ cmtFrequencyText }}</span>
                        </div>
                        <div
                            class="alert alert-danger"
                            role="alert"
                            v-html="$t('dtuadmin.CmtFrequencyWarning')"
                            v-if="cmtIsOutOfLegalRange"
                        ></div>
                    </div>
                </div>
            </CardElement>

            <CardElement
                :text="$t('dtuadmin.Sx1262Configuration')"
                textVariant="text-bg-primary"
                v-if="dtuConfigList.sx1262_enabled"
            >
                <div class="row mb-3">
                    <label for="inputSx1262PaLevel" class="col-sm-2 col-form-label">
                        {{ $t('dtuadmin.Sx1262PaLevel') }}
                        <BIconInfoCircle v-tooltip :title="$t('dtuadmin.Sx1262PaLevelHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="range"
                                class="form-control form-range"
                                v-model.number="dtuConfigList.sx1262_palevel"
                                min="-10"
                                max="22"
                                id="inputSx1262PaLevel"
                                style="height: unset"
                            />
                            <span class="input-group-text">{{ sx1262PaLevelText }}</span>
                        </div>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="inputSx1262Country" class="col-sm-2 col-form-label">
                        {{ $t('dtuadmin.Sx1262Country') }}
                        <BIconInfoCircle v-tooltip :title="$t('dtuadmin.Sx1262CountryHint')" />
                    </label>
                    <div class="col-sm-10">
                        <select id="inputSx1262Country" class="form-select" v-model="dtuConfigList.sx1262_country">
                            <option
                                v-for="(country, index) in dtuConfigList.sx1262_country_def"
                                :key="index"
                                :value="index"
                            >
                                {{
                                    $t(`dtuadmin.country_` + index, {
                                        min: country.freq_min / 1e6,
                                        max: country.freq_max / 1e6,
                                    })
                                }}
                            </option>
                        </select>
                    </div>
                </div>

                <div class="row mb-3">
                    <label for="sx1262Frequency" class="col-sm-2 col-form-label">
                        {{ $t('dtuadmin.Sx1262Frequency') }}
                        <BIconInfoCircle v-tooltip :title="$t('dtuadmin.Sx1262FrequencyHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="range"
                                class="form-control form-range"
                                v-model.number="dtuConfigList.sx1262_frequency"
                                :min="sx1262MinFrequency"
                                :max="sx1262MaxFrequency"
                                :step="dtuConfigList.sx1262_chan_width"
                                id="sx1262Frequency"
                                style="height: unset"
                            />
                            <span class="input-group-text">{{ sx1262FrequencyText }}</span>
                        </div>
                        <div
                            class="alert alert-danger"
                            role="alert"
                            v-html="$t('dtuadmin.Sx1262FrequencyWarning')"
                            v-if="sx1262IsOutOfLegalRange"
                        ></div>
                    </div>
                </div>

            </CardElement>

            <FormFooter @reload="getDtuConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import type { AlertResponse } from '@/types/AlertResponse';
import type { DtuConfig } from '@/types/DtuConfig';
import { authHeader, handleResponse } from '@/utils/authentication';
import { BIconInfoCircle } from 'bootstrap-icons-vue';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        FormFooter,
        InputElement,
        BIconInfoCircle,
    },
    data() {
        return {
            dataLoading: true,
            dtuConfigList: {} as DtuConfig,
            nrfpalevelList: [
                { key: 0, value: 'Min', db: '-18' },
                { key: 1, value: 'Low', db: '-12' },
                { key: 2, value: 'High', db: '-6' },
                { key: 3, value: 'Max', db: '0' },
            ],
            alert: {} as AlertResponse,
        };
    },
    created() {
        this.getDtuConfig();
    },
    computed: {
        cmtFrequencyText() {
            return this.$t('dtuadmin.MHz', {
                mhz: this.$n(this.dtuConfigList.cmt_frequency / 1000000, 'decimalTwoDigits'),
            });
        },
        cmtPaLevelText() {
            return this.$t('dtuadmin.dBm', { dbm: this.$n(this.dtuConfigList.cmt_palevel * 1) });
        },
        cmtMinFrequency() {
            return this.dtuConfigList.country_def?.[this.dtuConfigList.cmt_country]?.freq_min;
        },
        cmtMaxFrequency() {
            return this.dtuConfigList.country_def?.[this.dtuConfigList.cmt_country]?.freq_max;
        },
        cmtIsOutOfLegalRange() {
            const country = this.dtuConfigList.country_def?.[this.dtuConfigList.cmt_country];
            if (!country) {
                return false;
            }
            return (
                this.dtuConfigList.cmt_frequency < country.freq_legal_min ||
                this.dtuConfigList.cmt_frequency > country.freq_legal_max
            );
        },
        sx1262FrequencyText() {
            return this.$t('dtuadmin.MHz', {
                mhz: this.$n(this.dtuConfigList.sx1262_frequency / 1000000, 'decimalTwoDigits'),
            });
        },
        sx1262PaLevelText() {
            return this.$t('dtuadmin.dBm', { dbm: this.$n(this.dtuConfigList.sx1262_palevel * 1) });
        },
        sx1262MinFrequency() {
            return this.dtuConfigList.sx1262_country_def?.[this.dtuConfigList.sx1262_country]?.freq_min;
        },
        sx1262MaxFrequency() {
            return this.dtuConfigList.sx1262_country_def?.[this.dtuConfigList.sx1262_country]?.freq_max;
        },
        sx1262IsOutOfLegalRange() {
            const country = this.dtuConfigList.sx1262_country_def?.[this.dtuConfigList.sx1262_country];
            if (!country) {
                return false;
            }
            return (
                this.dtuConfigList.sx1262_frequency < country.freq_legal_min ||
                this.dtuConfigList.sx1262_frequency > country.freq_legal_max
            );
        },
    },
    watch: {
        'dtuConfigList.cmt_country'(newValue, oldValue) {
            // Don't do anything on initial load (then oldValue equals undefined)
            if (oldValue != undefined) {
                this.$nextTick(() => {
                    if (this.dtuConfigList.country_def[newValue]) {
                        this.dtuConfigList.cmt_frequency = this.dtuConfigList.country_def[newValue].freq_default;
                    }
                });
            }
        },
        'dtuConfigList.sx1262_country'(newValue, oldValue) {
            if (oldValue != undefined) {
                this.$nextTick(() => {
                    if (this.dtuConfigList.sx1262_country_def[newValue]) {
                        this.dtuConfigList.sx1262_frequency =
                            this.dtuConfigList.sx1262_country_def[newValue].freq_default;
                    }
                });
            }
        },
    },
    methods: {
        getDtuConfig() {
            this.dataLoading = true;
            fetch('/api/dtu/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.dtuConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveDtuConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            formData.append('data', JSON.stringify(this.dtuConfigList));

            fetch('/api/dtu/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.alert.message = this.$t('apiresponse.' + response.code, response.param);
                    this.alert.type = response.type;
                    this.alert.show = true;
                });
        },
    },
});
</script>
