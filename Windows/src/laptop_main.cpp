#include <iostream>
#include <string>
#include <signal.h>
#include "door_control_client.hpp"
#include "console_ui.hpp"

std::unique_ptr<DoorControlClient> g_client;

void signal_handler(int sig) {
    if (g_client) {
        g_client->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> [server_port]" << std::endl;
        std::cerr << "Example: " << argv[0] << " 192.168.1.100 8080" << std::endl;
        return 1;
    }
    
    std::string server_ip = argv[1];
    int server_port = (argc >= 3) ? std::stoi(argv[2]) : 8080;
    
    // Set up signal handler
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        g_client = std::make_unique<DoorControlClient>(server_ip, server_port);
        g_client->start();
        
        ConsoleUI ui(*g_client);
        ui.run();
        
        g_client->stop();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}