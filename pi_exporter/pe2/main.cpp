#include "metrics_store.hpp"
#include "mediamtx_fetcher.hpp"
#include "prometheus_server.hpp"

#include <thread>
#include <chrono>

int main() {
    MetricsStore store;

    MediaMTXFetcher fetcher(store);
    PrometheusServer server(store, 9108);

    std::thread server_thread([&]() {
        server.serve();
    });

    while (true) {
        fetcher.fetch();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server_thread.join();
    return 0;
}
