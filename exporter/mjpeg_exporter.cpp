#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#define HTTP_PORT 9200
#define BUFFER_SIZE 8192

std::atomic<uint64_t> frame_count{0};
std::atomic<uint64_t> byte_count{0};
std::atomic<bool> running{true};

uint64_t last_frames = 0;
uint64_t last_bytes = 0;

auto last_fps_time = std::chrono::steady_clock::now();
auto last_bps_time = std::chrono::steady_clock::now();

double mjpeg_fps() {
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_fps_time).count();
    if (dt <= 0.0) return 0.0;

    uint64_t frames = frame_count.load();
    double fps = (frames - last_frames) / dt;

    last_frames = frames;
    last_fps_time = now;
    return fps;
}

double mjpeg_bytes_per_sec() {
    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_bps_time).count();
    if (dt <= 0.0) return 0.0;

    uint64_t bytes = byte_count.load();
    double rate = (bytes - last_bytes) / dt;

    last_bytes = bytes;
    last_bps_time = now;
    return rate;
}

void mjpeg_reader(const std::string& host, int port) {
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) {
        return;
    }

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    connect(sock, res->ai_addr, res->ai_addrlen);

    std::string request =
        "GET /?action=stream HTTP/1.1\r\n"
        "Host: " + host + "\r\n\r\n";

    send(sock, request.c_str(), request.size(), 0);

    char buffer[BUFFER_SIZE];
    while (running) {
        ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) break;

        byte_count += n;

        for (int i = 0; i < n - 1; i++) {
            if ((uint8_t)buffer[i] == 0xFF &&
                (uint8_t)buffer[i + 1] == 0xD8) {
                frame_count++;
            }
        }
    }

    close(sock);
}

void metrics_server() {
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

        double fps = mjpeg_fps();
        double bps = mjpeg_bytes_per_sec();

        std::string body =
            "# HELP mjpeg_fps MJPEG frames per second\n"
            "# TYPE mjpeg_fps gauge\n"
            "mjpeg_fps " + std::to_string(fps) + "\n"
            "# HELP mjpeg_bytes_per_sec MJPEG bandwidth\n"
            "# TYPE mjpeg_bytes_per_sec gauge\n"
            "mjpeg_bytes_per_sec " + std::to_string(bps) + "\n"
            "# HELP mjpeg_up Exporter health\n"
            "# TYPE mjpeg_up gauge\n"
            "mjpeg_up 1\n";

        std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; version=0.0.4\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" +
            body;

        send(client, response.c_str(), response.size(), 0);
        close(client);
    }
}

int main() {
    std::string camera_ip = "192.168.86.22";
    int camera_port = 8080;

    std::thread reader(mjpeg_reader, camera_ip, camera_port);
    std::thread server(metrics_server);

    reader.join();
    server.join();
    return 0;
}
