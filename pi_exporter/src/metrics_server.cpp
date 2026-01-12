#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sstream>
#include <string>

struct StreamMetrics {
    double bitrate_bytes_per_sec;
    double rtp_packets_per_sec;
    int readers;
};

StreamMetrics get_metrics();

void run_metrics_server(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 8);

    while (true) {
        int client = accept(server_fd, nullptr, nullptr);
        if (client < 0) continue;

        char buffer[1024] = {0};
        read(client, buffer, sizeof(buffer));

        std::string req(buffer);
        if (req.find("GET /metrics") == std::string::npos) {
            close(client);
            continue;
        }

        StreamMetrics m = get_metrics();

        std::ostringstream body;
        body << "mediamtx_stream_bitrate_bytes_per_sec{path=\"cam\"} "
             << m.bitrate_bytes_per_sec << "\n";
        body << "mediamtx_stream_rtp_packets_per_sec{path=\"cam\"} "
             << m.rtp_packets_per_sec << "\n";
        body << "mediamtx_stream_readers{path=\"cam\"} "
             << m.readers << "\n";

        std::string body_str = body.str();

        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/plain; version=0.0.4\r\n"
             << "Content-Length: " << body_str.size() << "\r\n\r\n"
             << body_str;

        std::string resp_str = resp.str();
        send(client, resp_str.c_str(), resp_str.size(), 0);

        close(client);
    }
}
