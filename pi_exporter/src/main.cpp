#include <thread>
#include <chrono>

void fetch_metrics();
void run_metrics_server(int port);

int main() {
    std::thread server(run_metrics_server, 9108);

    while (true) {
        fetch_metrics();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    server.join();
    return 0;
}
