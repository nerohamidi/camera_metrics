#include "mediamtx_fetcher.hpp"

size_t MediaMTXFetcher::curl_cb(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

void MediaMTXFetcher::fetch() {
    CURL* curl = curl_easy_init();
    if (!curl) return;

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, "http://127.0.0.1:9998/metrics");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1000);

    if (curl_easy_perform(curl) != CURLE_OK) {
        curl_easy_cleanup(curl);
        return;
    }
    curl_easy_cleanup(curl);

    uint64_t bytes = 0;
    uint64_t rtp = 0;
    int readers = 0;

    std::istringstream iss(response);
    std::string line;

    while (std::getline(iss, line)) {
        if (line.find("webrtc_sessions_bytes_sent") != std::string::npos &&
            line.find("path=\"cam\"") != std::string::npos)
            bytes = std::stoull(line.substr(line.find_last_of(' ') + 1));

        if (line.find("webrtc_sessions_rtp_packets_sent") != std::string::npos &&
            line.find("path=\"cam\"") != std::string::npos)
            rtp = std::stoull(line.substr(line.find_last_of(' ') + 1));

        if (line.find("paths_readers") != std::string::npos &&
            line.find("name=\"cam\"") != std::string::npos)
            readers = std::stoi(line.substr(line.find_last_of(' ') + 1));
    }

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_time_).count();
    if (dt <= 0.0) return;

    StreamMetrics m;
    m.bitrate_bytes_per_sec = (bytes - last_bytes_) / dt;
    m.rtp_packets_per_sec = (rtp - last_rtp_) / dt;
    m.readers = readers;

    store_.update(m);

    last_bytes_ = bytes;
    last_rtp_ = rtp;
    last_time_ = now;
}
