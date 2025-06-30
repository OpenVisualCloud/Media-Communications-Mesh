#pragma once
#include <cstdint>

/* wire header that prefixes every RDMA payload */
#pragma pack(push, 1)
struct FrameHdr {
    uint32_t frame;   /* network-order */
    uint64_t tx_ns;   /* network-order, CLOCK_REALTIME in ns */
};
#pragma pack(pop)

/*UDP message RX→TX with results */
struct StatsMsg {
    uint32_t payload_mb;        /* MB for this run              */
    uint32_t queue;             /* queue size                   */
    double   ttlb_spaced_ms;    /* TTLB @60fps                  */
    double   ttlb_full_ms;      /* TTLB @ max throughput        */
    double   cpu_tx_pct;        /* TX process CPU-load percent  */
    double   cpu_rx_pct;        /* RX process CPU-load percent  */
};