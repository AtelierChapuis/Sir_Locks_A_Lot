// File: door_relay_service.hpp
#ifndef DOOR_RELAY_SERVICE_HPP
#define DOOR_RELAY_SERVICE_HPP

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <iostream>
#include <poll.h>
#include "serial_port.hpp"
#include "tcp_server.hpp"
#include "database_logger.hpp"
#include "json_parser.hpp"

class DoorRelayService {
private:
    std::unique_ptr<SerialPort> serial_port_;
    std::unique_ptr<TcpServer> tcp_server_;
    std::unique_ptr<DatabaseLogger> db_logger_;
    
    std::string current_door_state_;
    std::string last_event_source_;
    std::string last_event_timestamp_;
    std::atomic<bool> running_;
    
    static constexpr int TCP_PORT = 8080;
    
public:
    DoorRelayService(const std::string& serial_port_name, const std::string& db_path)
        : serial_port_(std::make_unique<SerialPort>(serial_port_name)),
          tcp_server_(std::make_unique<TcpServer>(TCP_PORT)),
          db_logger_(std::make_unique<DatabaseLogger>(db_path)),
          current_door_state_("locked"),
          running_(false) {}
    
    void start() {
        try {
            serial_port_->open();
            tcp_server_->start();
            db_logger_->open();
            
            running_ = true;
            std::cout << "Door Relay Service started on port " << TCP_PORT << std::endl;
            
            run();
        } catch (const std::exception& e) {
            std::cerr << "Error starting service: " << e.what() << std::endl;
            throw;
        }
    }
    
    void stop() {
        running_ = false;
    }
    
private:
    void run() {
        while (running_) {
            // Accept client if none connected
            if (!tcp_server_->hasClient()) {
                std::cout << "Waiting for client connection..." << std::endl;
                tcp_server_->acceptClient();
                std::cout << "Client connected" << std::endl;
            }
            
            // Set up polling
            struct pollfd fds[2];
            fds[0].fd = serial_port_->getFd();
            fds[0].events = POLLIN;
            fds[1].fd = tcp_server_->getClientFd();
            fds[1].events = POLLIN;
            
            int ret = poll(fds, 2, 100);  // 100ms timeout
            
            if (ret > 0) {
                // Check serial port
                if (fds[0].revents & POLLIN) {
                    handleSerialMessage();
                }
                
                // Check TCP client
                if (fds[1].revents & POLLIN) {
                    handleTcpMessage();
                }
                
                // Check for disconnection
                if (fds[1].revents & (POLLHUP | POLLERR)) {
                    std::cout << "Client disconnected" << std::endl;
                    tcp_server_->closeClient();
                }
            }
        }
    }
    
    void handleSerialMessage() {
        try {
            std::string message = serial_port_->readLine();
            if (message.empty()) return;
            
            std::cout << "Received from STM32: " << message << std::endl;
            
            auto json = SimpleJSON::parse(message);
            
            // Validate message
            if (json.find("source") != json.end() && 
                json.find("event") != json.end() &&
                json["source"] == "stm32") {
                
                // Update state
                if (json["event"] == "door_locked") {
                    current_door_state_ = "locked";
                } else if (json["event"] == "door_unlocked") {
                    current_door_state_ = "unlocked";
                }
                
                last_event_source_ = json["source"];
                last_event_timestamp_ = json["timestamp"];
                
                // Log to database
                db_logger_->logEvent(current_door_state_, last_event_source_);
                
                // Send acknowledgment to STM32
                SimpleJSON::Object ack;
                ack["type"] = "ack";
                ack["status"] = "ok";
                serial_port_->writeLine(SimpleJSON::stringify(ack));
                
                // Relay to laptop if connected
                if (tcp_server_->hasClient()) {
                    tcp_server_->writeLine(message);
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling serial message: " << e.what() << std::endl;
        }
    }
    
    void handleTcpMessage() {
        try {
            std::string message = tcp_server_->readLine();
            if (message.empty()) return;
            
            std::cout << "Received from Laptop: " << message << std::endl;
            
            auto json = SimpleJSON::parse(message);
            
            // Handle SYNC request
            if (json.find("type") != json.end() && json["type"] == "SYNC") {
                SimpleJSON::Object response;
                response["type"] = "sync_response";
                response["door_state"] = current_door_state_;
                response["last_source"] = last_event_source_;
                response["last_timestamp"] = last_event_timestamp_;
                
                tcp_server_->writeLine(SimpleJSON::stringify(response));
                return;
            }
            
            // Handle lock/unlock commands
            if (json.find("command") != json.end()) {
                std::string command = json["command"];
                
                if (command == "lock" || command == "unlock") {
                    // Relay to STM32
                    serial_port_->writeLine(message);
                    
                    // Send acknowledgment to laptop
                    SimpleJSON::Object ack;
                    ack["type"] = "ack";
                    ack["status"] = "ok";
                    tcp_server_->writeLine(SimpleJSON::stringify(ack));
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error handling TCP message: " << e.what() << std::endl;
        }
    }
};

#endif // DOOR_RELAY_SERVICE_HPP