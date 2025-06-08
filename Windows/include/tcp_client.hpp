#ifndef TCP_CLIENT_HPP
#define TCP_CLIENT_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

class TcpClient {
private:
    int socket_fd_;
    std::string server_ip_;
    int server_port_;
    
public:
    TcpClient(const std::string& server_ip, int server_port) 
        : server_ip_(server_ip), server_port_(server_port), socket_fd_(-1) {}
    
    ~TcpClient() {
        disconnect();
    }
    
    void connect() {
        socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        
        if (inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) <= 0) {
            throw std::runtime_error("Invalid address: " + server_ip_);
        }
        
        if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
            throw std::runtime_error("Failed to connect to server");
        }
    }
    
    void disconnect() {
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
            socket_fd_ = -1;
        }
    }
    
    bool isConnected() const {
        return socket_fd_ >= 0;
    }
    
    std::string readLine() {
        std::string result;
        char c;
        
        while (recv(socket_fd_, &c, 1, 0) == 1) {
            if (c == '\n') {
                break;
            }
            result += c;
        }
        
        return result;
    }
    
    void writeLine(const std::string& data) {
        std::string line = data + "\n";
        send(socket_fd_, line.c_str(), line.length(), 0);
    }
    
    int getFd() const {
        return socket_fd_;
    }
};