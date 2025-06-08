// File: tcp_server.hpp
#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

class TcpServer {
private:
    int server_fd_;
    int client_fd_;
    int port_;
    
public:
    TcpServer(int port) : port_(port), server_fd_(-1), client_fd_(-1) {}
    
    ~TcpServer() {
        close();
    }
    
    void start() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        
        int opt = 1;
        if (setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error("Failed to set socket options");
        }
        
        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port_);
        
        if (bind(server_fd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
            throw std::runtime_error("Failed to bind socket");
        }
        
        if (listen(server_fd_, 1) < 0) {
            throw std::runtime_error("Failed to listen on socket");
        }
    }
    
    void acceptClient() {
        struct sockaddr_in address;
        socklen_t addrlen = sizeof(address);
        
        client_fd_ = accept(server_fd_, (struct sockaddr*)&address, &addrlen);
        if (client_fd_ < 0) {
            throw std::runtime_error("Failed to accept client");
        }
    }
    
    void close() {
        if (client_fd_ >= 0) {
            ::close(client_fd_);
            client_fd_ = -1;
        }
        if (server_fd_ >= 0) {
            ::close(server_fd_);
            server_fd_ = -1;
        }
    }
    
    void closeClient() {
        if (client_fd_ >= 0) {
            ::close(client_fd_);
            client_fd_ = -1;
        }
    }
    
    std::string readLine() {
        std::string result;
        char c;
        
        while (recv(client_fd_, &c, 1, 0) == 1) {
            if (c == '\n') {
                break;
            }
            result += c;
        }
        
        return result;
    }
    
    void writeLine(const std::string& data) {
        std::string line = data + "\n";
        send(client_fd_, line.c_str(), line.length(), 0);
    }
    
    bool hasClient() const {
        return client_fd_ >= 0;
    }
    
    int getServerFd() const {
        return server_fd_;
    }
    
    int getClientFd() const {
        return client_fd_;
    }
};