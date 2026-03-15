export interface HistoryData {
    serial: string;
    channels: number;
    count: number;
    names?: string[];
    data: {
        t: number[];
        ac: number[];
        dc0?: number[];
        dc1?: number[];
        dc2?: number[];
        dc3?: number[];
        dc4?: number[];
        dc5?: number[];
        temp: number[];
        yd: number[];
        total?: number[];
        inv0?: number[];
        inv1?: number[];
        inv2?: number[];
        inv3?: number[];
        inv4?: number[];
        inv5?: number[];
        inv6?: number[];
        inv7?: number[];
        inv8?: number[];
        inv9?: number[];
    };
}

export interface HistoryStatus {
    max_records: number;
    record_count: number;
    slot_size: number;
    total_bytes: number;
    oldest_timestamp: number;
    newest_timestamp: number;
    allocated: boolean;
}
