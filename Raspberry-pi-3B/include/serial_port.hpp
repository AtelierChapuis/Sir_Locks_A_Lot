// File: serial_port.hpp
#ifndef SERIAL_PORT_HPP
#define SERIAL_PORT_HPP

#include <string>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>

class SerialPort {
private:
    int fd_;
    std::string port_name_;
    struct termios old_tio_;
    
public:
    SerialPort(const std::string& port_name) : port_name_(port_name), fd_(-1) {}
    
    ~SerialPort() {
        close();
    }
    
    void open() {
        fd_ = ::open(port_name_.c_str(), O_RDWR | O_NOCTTY);
        if (fd_ < 0) {
            throw std::runtime_error("Failed to open serial port: " + port_name_);
        }
        
        // Save current terminal settings
        tcgetattr(fd_, &old_tio_);
        
        // Configure serial port
        struct termios new_tio;
        memset(&new_tio, 0, sizeof(new_tio));
        
        new_tio.c_cflag = B115200 | CS8 | CLOCAL | CREAD;
        new_tio.c_iflag = IGNPAR;
        new_tio.c_oflag = 0;
        new_tio.c_lflag = 0;
        new_tio.c_cc[VTIME] = 0;
        new_tio.c_cc[VMIN] = 1;
        
        tcflush(fd_, TCIFLUSH);
        tcsetattr(fd_, TCSANOW, &new_tio);
    }
    
    void close() {
        if (fd_ >= 0) {
            tcsetattr(fd_, TCSANOW, &old_tio_);
            ::close(fd_);
            fd_ = -1;
        }
    }
    
    std::string readLine() {
        std::string result;
        char c;
        
        while (read(fd_, &c, 1) == 1) {
            if (c == '\n') {
                break;
            }
            result += c;
        }
        
        return result;
    }
    
    void writeLine(const std::string& data) {
        std::string line = data + "\n";
        write(fd_, line.c_str(), line.length());
    }
    
    bool isOpen() const {
        return fd_ >= 0;
    }
    
    int getFd() const {
        return fd_;
    }
};