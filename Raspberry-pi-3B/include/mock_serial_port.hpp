#ifndef MOCK_SERIAL_PORT_HPP
#define MOCK_SERIAL_PORT_HPP

#include <queue>
#include <string>

class MockSerialPort {
private:
    std::queue<std::string> read_buffer_;
    std::queue<std::string> write_buffer_;
    bool is_open_;
    
public:
    MockSerialPort() : is_open_(false) {}
    
    void open() {
        is_open_ = true;
    }
    
    void close() {
        is_open_ = false;
    }
    
    std::string readLine() {
        if (!read_buffer_.empty()) {
            std::string line = read_buffer_.front();
            read_buffer_.pop();
            return line;
        }
        return "";
    }
    
    void writeLine(const std::string& data) {
        write_buffer_.push(data);
    }
    
    bool isOpen() const {
        return is_open_;
    }
    
    // Test helpers
    void addToReadBuffer(const std::string& data) {
        read_buffer_.push(data);
    }
    
    std::string getLastWritten() {
        if (!write_buffer_.empty()) {
            std::string data = write_buffer_.front();
            write_buffer_.pop();
            return data;
        }
        return "";
    }
    
    bool hasWrittenData() const {
        return !write_buffer_.empty();
    }
};

#endif // MOCK_SERIAL_PORT_HPP