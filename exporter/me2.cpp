#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#define HTTP_PORT 9200
#define BUFFER_SIZE 8192

std::atomic<bool> running{true};

class Metric {
public:
    virtual ~Metric() = default;
    virtual std::string name() const = 0;
    virtual std::string type() const = 0;
    virtual std::string help() const = 0;
    virtual double value() = 0;
};

class FpsMetric : public Metric {
    std::atomic<uint64_t>& frames;
    uint64_t last_frames = 0;
    std::chrono::steady_clock::time_point last_time =
        std::chrono::steady_clock::now();

public:
    explicit FpsMetric(std::atomic<uint64_t>& f) : frames(f) {}

    std::string name() const override { return "mjpeg_fps"; }
    std::string type() const override { return "gauge"; }
    std::string help() const override { return "MJPEG frames per second"; }

    double value() override {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_time).count();
        if (dt <= 0.0) return 0.0;

        uint64_t cur = frames.load();
        double fps = (cur - last_frames) / dt;

        last_frames = cur;
        last_time = now;
        return fps;
    }
};

class BandwidthMetric : public Metric {
    std::atomic<uint64_t>& bytes;
    uint64_t last_bytes = 0;
    std::chrono::steady_clock::time_point last_time =
        std::chrono::steady_clock::now();

public:
    explicit BandwidthMetric(std::atomic<uint64_t>& b) : bytes(b) {}

    std::string name() const override { return "mjpeg_bytes_per_sec"; }
    std::string type() const override { return "gauge"; }
    std::string help() const override { return "MJPEG bandwidth"; }

    double value() override {
        auto now = std::chrono::steady_clock::now();
        double dt = std::chrono::duration<double>(now - last_time).count();
        if (dt <= 0.0) return 0.0;

        uint64_t cur = bytes.load();
        double rate = (cur - last_bytes) / dt;

        last_bytes = cur;
        last_time = now;
        return rate;
    }
};

class MetricsRegistry {
    std::vector<std::unique_ptr<Metric>> metrics;

public:
    void add(std::unique_ptr<Metric> m) {
        metrics.push_back(std::move(m));
    }

    std::string render() {
        std::ostringstream out;
        for (auto& m : metrics) {
            out << "# HELP " << m->name() << " " << m->help() << "\n";
            out << "# TYPE " << m->name() << " " << m->type() << "\n";
            out << m->name() << " " << m->value() << "\n";
        }
        out << "# HELP mjpeg_up Exporter health\n";
        out << "# TYPE mjpeg_up gauge\n";
        out << "mjpeg_up 1\n";
        return out.str();
    }
};

class MjpegReader {
    std::string host;
    int port;

public:
    std::atomic<uint64_t> frames{0};
    std::atomic<uint64_t> bytes{0};

    MjpegReader(std::string h, int p) : host(std::move(h)), port(p) {}

    void run() {
        addrinfo hints{}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
            return;

        int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (connect(sock, res->ai_addr, res->ai_addrlen) != 0)
            return;

        std::string request =
            "GET /?action=stream HTTP/1.1\r\n"
            "Host: " + host + "\r\n\r\n";

        send(sock, request.c_str(), request.size(), 0);

        char buffer[BUFFER_SIZE];
        while (running) {
            ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
            if (n <= 0) break;

            bytes += n;

            for (ssize_t i = 0; i < n - 1; ++i) {
                if ((uint8_t)buffer[i] == 0xFF &&
                    (uint8_t)buffer[i + 1] == 0xD8) {
                    frames++;
                }
            }
        }

        close(sock);
    }
};

class MetricsServer {
    MetricsRegistry& registry;

public:
    explicit MetricsServer(MetricsRegistry& r) : registry(r) {}

    void run() {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(HTTP_PORT);

        bind(server_fd, (sockaddr*)&addr, sizeof(addr));
        listen(server_fd, 5);

        while (running) {
            int client = accept(server_fd, nullptr, nullptr);
            if (client < 0) continue;

            std::string body = registry.render();
            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain; version=0.0.4\r\n"
                "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
                body;

            send(client, response.c_str(), response.size(), 0);
            close(client);
        }
    }
};

int main() {
    MjpegReader reader("192.168.86.22", 8080);

    MetricsRegistry registry;
    registry.add(std::make_unique<FpsMetric>(reader.frames));
    registry.add(std::make_unique<BandwidthMetric>(reader.bytes));

    MetricsServer server(registry);

    std::thread t1(&MjpegReader::run, &reader);
    std::thread t2(&MetricsServer::run, &server);

    t1.join();
    t2.join();
    return 0;
}
