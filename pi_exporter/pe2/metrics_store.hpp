#pragma once
#include "metrics.hpp"

class MetricsStore {
public:
    void update(const StreamMetrics& m) {
        std::lock_guard<std::mutex> lock(mutex_);
        metrics_ = m;
    }

    StreamMetrics snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return metrics_;
    }

private:
    mutable std::mutex mutex_;
    StreamMetrics metrics_;
};
