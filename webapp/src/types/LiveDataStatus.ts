export interface ValueObject {
    v: number; // value
    u: string; // unit
    d: number; // digits
    max: number;
}

export interface InverterStatistics {
    name: ValueObject;
    Power?: ValueObject;
    Voltage?: ValueObject;
    Current?: ValueObject;
    'Power DC'?: ValueObject;
    YieldDay?: ValueObject;
    YieldTotal?: ValueObject;
    Frequency?: ValueObject;
    Temperature?: ValueObject;
    PowerFactor?: ValueObject;
    ReactivePower?: ValueObject;
    Efficiency?: ValueObject;
    Irradiation?: ValueObject;
}

export interface RadioStatistics {
    tx_request: number;
    tx_re_request: number;
    rx_success: number;
    rx_fail_nothing: number;
    rx_fail_partial: number;
    rx_fail_corrupt: number;
    d6_tx: number;
    d6_rx: number;
    d6_last_tx_ms: number;
    d6_last_rx_ms: number;
    rssi: number;
    last_hop_pattern: number;
    predicted_hop_pattern: number;
    last_frag_count: number;
    last_burst_rx: number;
}

export interface Inverter {
    serial: string;
    name: string;
    order: number;
    data_age: number;
    data_age_ms: number;
    poll_enabled: boolean;
    reachable: boolean;
    producing: boolean;
    limit_relative: number;
    limit_absolute: number;
    events: number;
    AC: InverterStatistics[];
    DC: InverterStatistics[];
    INV: InverterStatistics[];
    radio_stats: RadioStatistics;
}

export interface Total {
    Power: ValueObject;
    YieldDay: ValueObject;
    YieldTotal: ValueObject;
}

export interface Hints {
    time_sync: boolean;
    default_password: boolean;
    radio_problem: boolean;
    pin_mapping_issue: boolean;
}

export interface LiveData {
    inverters: Inverter[];
    total: Total;
    hints: Hints;
}
