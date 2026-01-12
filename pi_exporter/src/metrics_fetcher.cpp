#include <curl/curl.h>
#include <chrono>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

struct StreamMetrics {
    double bitrate_bytes_per_sec = 0.0;
    double rtp_packets_per_sec = 0.0;
    int readers = 0;
};

static std::mutex metrics_mutex;
static StreamMetrics current_metrics;

static uint64_t last_bytes_sent = 0;
static uint64_t last_rtp_packets = 0;
static auto last_time = std::chrono::steady_clock::now();

static size_t curl_write_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append((char*)contents, total);
    return total;
}

void fetch_metrics() {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:9998/metrics");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000);

    if (curl_easy_perform(curl) != CURLE_OK) {
        curl_easy_cleanup(curl);
        return;
    }

    curl_easy_cleanup(curl);

    uint64_t bytes_sent = 0;
    uint64_t rtp_packets = 0;
    int readers = 0;

    std::istringstream iss(response);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.find("webrtc_sessions_bytes_sent") != std::string::npos &&
            line.find("path=\"cam\"") != std::string::npos) {
            bytes_sent = std::stoull(line.substr(line.find_last_of(' ') + 1));
        }

        if (line.find("webrtc_sessions_rtp_packets_sent") != std::string::npos &&
            line.find("path=\"cam\"") != std::string::npos) {
            rtp_packets = std::stoull(line.substr(line.find_last_of(' ') + 1));
        }

        if (line.find("paths_readers") != std::string::npos &&
            line.find("name=\"cam\"") != std::string::npos) {
            readers = std::stoi(line.substr(line.find_last_of(' ') + 1));
        }
    }

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_time).count();
    if (dt <= 0.0) return;

    std::lock_guard<std::mutex> lock(metrics_mutex);
    current_metrics.bitrate_bytes_per_sec =
        (bytes_sent - last_bytes_sent) / dt;

    current_metrics.rtp_packets_per_sec =
        (rtp_packets - last_rtp_packets) / dt;

    current_metrics.readers = readers;

    last_bytes_sent = bytes_sent;
    last_rtp_packets = rtp_packets;
    last_time = now;
}

StreamMetrics get_metrics() {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    return current_metrics;
}
