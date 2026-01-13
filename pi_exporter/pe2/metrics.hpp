#pragma once
#include <mutex>

struct StreamMetrics {
    double bitrate_bytes_per_sec = 0.0;
    double rtp_packets_per_sec = 0.0;
    int readers = 0;
};

class IMetricsFetcher {
public:
    virtual ~IMetricsFetcher() = default;
    virtual void fetch() = 0;
};

class IMetricsExporter {
public:
    virtual ~IMetricsExporter() = default;
    virtual void serve() = 0;
};
