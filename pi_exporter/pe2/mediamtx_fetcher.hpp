#pragma once
#include "metrics.hpp"
#include "metrics_store.hpp"

#include <curl/curl.h>
#include <chrono>
#include <sstream>
#include <string>

class MediaMTXFetcher final : public IMetricsFetcher {
public:
    explicit MediaMTXFetcher(MetricsStore& store)
        : store_(store) {}

    void fetch() override;

private:
    static size_t curl_cb(void* contents, size_t size, size_t nmemb, void* userp);

    MetricsStore& store_;
    uint64_t last_bytes_ = 0;
    uint64_t last_rtp_ = 0;
    std::chrono::steady_clock::time_point last_time_ =
        std::chrono::steady_clock::now();
};
