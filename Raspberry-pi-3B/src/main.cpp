#include <iostream>
#include <signal.h>
#include "door_relay_service.hpp"

std::unique_ptr<DoorRelayService> g_service;

void signal_handler(int sig) {
    if (g_service) {
        std::cout << "\nShutting down..." << std::endl;
        g_service->stop();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <serial_port> [database_path]" << std::endl;
        std::cerr << "Example: " << argv[0] << " /dev/ttyUSB0 door_events.db" << std::endl;
        return 1;
    }
    
    std::string serial_port = argv[1];
    std::string db_path = (argc >= 3) ? argv[2] : "door_events.db";
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        g_service = std::make_unique<DoorRelayService>(serial_port, db_path);
        g_service->start();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}