#ifndef DOOR_CONTROL_CLIENT_HPP
#define DOOR_CONTROL_CLIENT_HPP

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <iostream>
#include <poll.h>
#include "tcp_client.hpp"
#include "json_parser.hpp"

class DoorControlClient {
private:
    std::unique_ptr<TcpClient> tcp_client_;
    std::string current_door_state_;
    std::mutex state_mutex_;
    std::atomic<bool> running_;
    std::thread receiver_thread_;
    
public:
    DoorControlClient(const std::string& server_ip, int server_port)
        : tcp_client_(std::make_unique<TcpClient>(server_ip, server_port)),
          current_door_state_("unknown"),
          running_(false) {}
    
    ~DoorControlClient() {
        stop();
    }
    
    void start() {
        try {
            tcp_client_->connect();
            running_ = true;
            
            // Start receiver thread
            receiver_thread_ = std::thread(&DoorControlClient::receiveLoop, this);
            
            // Request current state
            requestSync();
            
            std::cout << "Connected to Door Relay Service" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to connect: " << e.what() << std::endl;
            throw;
        }
    }
    
    void stop() {
        running_ = false;
        if (receiver_thread_.joinable()) {
            receiver_thread_.join();
        }
        tcp_client_->disconnect();
    }
    
    void sendLockCommand() {
        SimpleJSON::Object cmd;
        cmd["command"] = "lock";
        cmd["source"] = "laptop";
        
        sendCommand(cmd);
    }
    
    void sendUnlockCommand() {
        SimpleJSON::Object cmd;
        cmd["command"] = "unlock";
        cmd["source"] = "laptop";
        
        sendCommand(cmd);
    }
    
    void requestSync() {
        SimpleJSON::Object sync;
        sync["type"] = "SYNC";
        
        tcp_client_->writeLine(SimpleJSON::stringify(sync));
    }
    
    std::string getCurrentState() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(state_mutex_));
        return current_door_state_;
    }
    
private:
    void sendCommand(const SimpleJSON::Object& cmd) {
        try {
            std::string json_str = SimpleJSON::stringify(cmd);
            tcp_client_->writeLine(json_str);
            std::cout << "Sent command: " << json_str << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to send command: " << e.what() << std::endl;
        }
    }
    
    void receiveLoop() {
        while (running_) {
            try {
                struct pollfd fds[1];
                fds[0].fd = tcp_client_->getFd();
                fds[0].events = POLLIN;
                
                int ret = poll(fds, 1, 100);  // 100ms timeout
                
                if (ret > 0 && (fds[0].revents & POLLIN)) {
                    std::string message = tcp_client_->readLine();
                    if (!message.empty()) {
                        handleMessage(message);
                    }
                }
                
                if (fds[0].revents & (POLLHUP | POLLERR)) {
                    std::cerr << "Connection lost" << std::endl;
                    running_ = false;
                }
            } catch (const std::exception& e) {
                std::cerr << "Receive error: " << e.what() << std::endl;
            }
        }
    }
    
    void handleMessage(const std::string& message) {
        try {
            auto json = SimpleJSON::parse(message);
            
            // Handle sync response
            if (json.find("type") != json.end() && json["type"] == "sync_response") {
                std::lock_guard<std::mutex> lock(state_mutex_);
                current_door_state_ = json["door_state"];
                std::cout << "Current door state: " << current_door_state_ << std::endl;
            }
            
            // Handle door events from STM32
            else if (json.find("event") != json.end() && json.find("source") != json.end()) {
                std::lock_guard<std::mutex> lock(state_mutex_);
                
                if (json["event"] == "door_locked") {
                    current_door_state_ = "locked";
                } else if (json["event"] == "door_unlocked") {
                    current_door_state_ = "unlocked";
                }
                
                std::cout << "\n[UPDATE] Door is now: " << current_door_state_ 
                         << " (source: " << json["source"] << ")" << std::endl;
            }
            
            // Handle acknowledgments
            else if (json.find("type") != json.end() && json["type"] == "ack") {
                std::cout << "Command acknowledged" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to parse message: " << e.what() << std::endl;
        }
    }
};