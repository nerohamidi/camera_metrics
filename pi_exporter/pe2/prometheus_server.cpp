#include "prometheus_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sstream>
#include <string>

void PrometheusServer::serve() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    bind(fd, (sockaddr*)&addr, sizeof(addr));
    listen(fd, 8);

    while (true) {
        int client = accept(fd, nullptr, nullptr);
        if (client < 0) continue;

        char buf[1024] = {};
        read(client, buf, sizeof(buf));

        if (std::string(buf).find("GET /metrics") == std::string::npos) {
            close(client);
            continue;
        }

        StreamMetrics m = store_.snapshot();

        std::ostringstream body;
        body << "mediamtx_stream_bitrate_bytes_per_sec{path=\"cam\"} "
             << m.bitrate_bytes_per_sec << "\n";
        body << "mediamtx_stream_rtp_packets_per_sec{path=\"cam\"} "
             << m.rtp_packets_per_sec << "\n";
        body << "mediamtx_stream_readers{path=\"cam\"} "
             << m.readers << "\n";

        std::string b = body.str();
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: text/plain; version=0.0.4\r\n"
             << "Content-Length: " << b.size() << "\r\n\r\n"
             << b;

        std::string r = resp.str();
        send(client, r.c_str(), r.size(), 0);
        close(client);
    }
}
