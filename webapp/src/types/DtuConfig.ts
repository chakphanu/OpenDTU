export interface CountryDef {
    freq_default: number;
    freq_min: number;
    freq_max: number;
    freq_legal_min: number;
    freq_legal_max: number;
}

export interface DtuConfig {
    serial: string;
    pollinterval: number;
    nrf_enabled: boolean;
    nrf_palevel: number;
    cmt_enabled: boolean;
    cmt_palevel: number;
    cmt_frequency: number;
    cmt_country: number;
    country_def: Array<CountryDef>;
    cmt_chan_width: number;
    sx1262_enabled: boolean;
    sx1262_palevel: number;
    sx1262_frequency: number;
    sx1262_country: number;
    sx1262_chan_width: number;
    sx1262_country_def: Array<CountryDef>;
}
