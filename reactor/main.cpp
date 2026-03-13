#include "reactor.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <format>

int main() {
    std::cout << "=== C++20 Reactor Demo ===\n";
    std::cout << "Features: Event-driven, High concurrency, Connection pool\n\n";

    Reactor reactor;
    if (!reactor.init()) {
        std::cerr << "Failed to initialize reactor\n";
        return 1;
    }

    TCPServer server(reactor);

    if (!server.bind("0.0.0.0", 8888)) {
        std::cerr << "Failed to bind to port 8888\n";
        return 1;
    }

    server.on_connection([](Connection::Ptr conn) {
        std::cout << "New connection from fd: " << conn->get_fd() << "\n";

        // Echo server
        if (auto tcp_conn = std::dynamic_pointer_cast<TCPConnection>(conn)) {
            tcp_conn->send("Hello from Reactor server!\n");
        }
    });

    std::cout << "Server listening on port 8888\n";
    std::cout << "Running for 5 seconds...\n\n";

    // 运行 5 秒
    std::thread reactor_thread([&reactor]() {
        reactor.run();
    });

    std::this_thread::sleep_for(std::chrono::seconds(5));

    reactor.stop();
    reactor_thread.join();

    std::cout << "\n=== Demo Complete ===\n";

    return 0;
}

