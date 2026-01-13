#pragma once
#include "metrics.hpp"
#include "metrics_store.hpp"

class PrometheusServer final : public IMetricsExporter {
public:
    PrometheusServer(MetricsStore& store, int port)
        : store_(store), port_(port) {}

    void serve() override;

private:
    MetricsStore& store_;
    int port_;
};
