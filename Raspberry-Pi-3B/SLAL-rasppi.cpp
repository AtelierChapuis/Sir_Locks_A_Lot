/*
* Sir Locks-A-Lot - Raspberry Pi Server
* Version: 1.0
*
* Filename: SLAL-rasppi.cpp
* 
* Description:
* Central relay and database server for door control system
* Handles TCP communication with Windows laptop and serial communication with STM32
* Maintains SQLite database and text log files
*
* Compilation:
* sudo apt-get install libsqlite3-dev libserialport-dev
* g++ -o SLAL-rasppi.cpp -lsqlite3 -lserialport -lpthread
*
* Usage:
* ./SLAL-rasppi
*/

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <mutex>
#include <queue>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sqlite3.h>
#include <libserialport.h>
#include <ctime>
#include <iomanip>
#include <signal.h>

using namespace std;

class DoorServer {
private:
    // Network variables
    int server_socket;
    int client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    bool client_connected;
    
    // Serial variables
    struct sp_port *serial_port;
    bool serial_connected;
    
    // Database
    sqlite3 *db;
    
    // Synchronization
    mutex log_mutex;
    mutex db_mutex;
    mutex status_mutex;
    
    // Current door status
    string current_door_status;
    bool running;
    
    // Message queue for serial communication
    queue<string> serial_send_queue;
    mutex serial_queue_mutex;

public:
    DoorServer() : client_connected(false), serial_connected(false), 
                   current_door_status("UNKNOWN"), running(true) {
        initializeDatabase();
        initializeNetwork();
        initializeSerial();
    }
    
    ~DoorServer() {
        cleanup();
    }
    
    string getCurrentTimestamp() {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        
        struct tm timeinfo;
        localtime_r(&time_t, &timeinfo);  // POSIX safe version
        
        stringstream ss;
        ss << put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    string getCurrentDate() {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        
        struct tm timeinfo;
        localtime_r(&time_t, &timeinfo);
        
        stringstream ss;
        ss << put_time(&timeinfo, "%Y-%m-%d");
        return ss.str();
    }
    
    void initializeDatabase() {
        int rc = sqlite3_open("door_log.db", &db);
        if (rc) {
            cerr << "Can't open database: " << sqlite3_errmsg(db) << endl;
            return;
        }
        
        // Create table if it doesn't exist
        const char* sql = "CREATE TABLE IF NOT EXISTS door_events ("
                         "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                         "timestamp TEXT NOT NULL,"
                         "source TEXT NOT NULL,"
                         "event TEXT NOT NULL"
                         ");";
        
        char* errMsg = 0;
        rc = sqlite3_exec(db, sql, 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            cerr << "SQL error: " << errMsg << endl;
            sqlite3_free(errMsg);
        } else {
            cout << "Database initialized successfully" << endl;
        }
    }
    
    void initializeNetwork() {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == -1) {
            cerr << "Failed to create socket" << endl;
            return;
        }
        
        // Allow socket reuse
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(8080);
        
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            cerr << "Bind failed" << endl;
            return;
        }
        
        if (listen(server_socket, 1) == -1) {
            cerr << "Listen failed" << endl;
            return;
        }
        
        cout << "Server listening on port 8080" << endl;
    }
    
    void initializeSerial() {
        // Try common serial port names for STM32
        const char* port_names[] = {"/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyUSB0", "/dev/ttyUSB1"};
        
        for (const char* port_name : port_names) {
            sp_return result = sp_get_port_by_name(port_name, &serial_port);
            if (result == SP_OK) {
                result = sp_open(serial_port, SP_MODE_READ_WRITE);
                if (result == SP_OK) {
                    // Configure serial port
                    sp_set_baudrate(serial_port, 115200);
                    sp_set_bits(serial_port, 8);
                    sp_set_parity(serial_port, SP_PARITY_NONE);
                    sp_set_stopbits(serial_port, 1);
                    sp_set_flowcontrol(serial_port, SP_FLOWCONTROL_NONE);
                    
                    serial_connected = true;
                    cout << "Serial port connected: " << port_name << endl;
                    break;
                }
            }
        }
        
        if (!serial_connected) {
            cout << "Warning: No serial port found. STM32 communication disabled." << endl;
        }
    }
    
    string createJSON(const string& source, const string& event) {
        string timestamp = getCurrentTimestamp();
        return "{\"source\":\"" + source + "\",\"event\":\"" + event + "\",\"timestamp\":\"" + timestamp + "\"}";
    }
    
    // Simple JSON parser
    string parseJSONValue(const string& json, const string& key) {
        string searchKey = "\"" + key + "\":\"";
        size_t pos = json.find(searchKey);
        if (pos == string::npos) return "";
        
        pos += searchKey.length();
        size_t endPos = json.find("\"", pos);
        if (endPos == string::npos) return "";
        
        return json.substr(pos, endPos - pos);
    }
    
    void logToDatabase(const string& timestamp, const string& source, const string& event) {
        lock_guard<mutex> lock(db_mutex);
        
        const char* sql = "INSERT INTO door_events (timestamp, source, event) VALUES (?, ?, ?);";
        sqlite3_stmt* stmt;
        
        int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, timestamp.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, source.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, event.c_str(), -1, SQLITE_STATIC);
            
            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                cerr << "Database insert failed: " << sqlite3_errmsg(db) << endl;
            }
        }
        sqlite3_finalize(stmt);
    }
    
    void logToTextFile(const string& timestamp, const string& source, const string& event) {
        lock_guard<mutex> lock(log_mutex);
        
        string filename = getCurrentDate() + ".txt";
        ofstream logFile(filename, ios::app);
        
        if (logFile.is_open()) {
            logFile << timestamp << " [" << source << "] " << event << endl;
            logFile.close();
        }
    }
    
    void logEvent(const string& timestamp, const string& source, const string& event) {
        // Only log state changes (lock, unlock, error)
        if (event == "lock" || event == "unlock" || event == "error") {
            logToDatabase(timestamp, source, event);
            logToTextFile(timestamp, source, event);
            
            // Update current status
            lock_guard<mutex> lock(status_mutex);
            if (event == "lock") {
                current_door_status = "LOCKED";
            } else if (event == "unlock") {
                current_door_status = "UNLOCKED";
            } else if (event == "error") {
                current_door_status = "ERROR";
            }
        }
    }
    
    void sendToSerial(const string& message) {
        if (!serial_connected) return;
        
        string msg_with_newline = message + "\n";
        sp_return result = sp_blocking_write(serial_port, msg_with_newline.c_str(), msg_with_newline.length(), 1000);
        if (result < 0) {
            cerr << "Serial write failed" << endl;
        } else {
            cout << "Sent to STM32: " << message << endl;
        }
    }
    
    string readFromSerial() {
        if (!serial_connected) return "";
        
        char buffer[1024];
        sp_return result = sp_nonblocking_read(serial_port, buffer, sizeof(buffer) - 1);
        
        if (result > 0) {
            buffer[result] = '\0';
            string received(buffer);
            // Remove newline characters
            received.erase(std::remove(received.begin(), received.end(), '\n'), received.end());
            received.erase(std::remove(received.begin(), received.end(), '\r'), received.end());
            
            if (!received.empty()) {
                cout << "Received from STM32: " << received << endl;
                return received;
            }
        }
        return "";
    }
    
    void sendToClient(const string& message) {
        if (!client_connected) return;
        
        ssize_t result = send(client_socket, message.c_str(), message.length(), 0);
        if (result == -1) {
            cerr << "Failed to send to client" << endl;
            client_connected = false;
        } else {
            cout << "Sent to laptop: " << message << endl;
        }
    }
    
    string readFromClient() {
        if (!client_connected) return "";
        
        char buffer[1024];
        ssize_t result = recv(client_socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        
        if (result > 0) {
            buffer[result] = '\0';
            string received(buffer);
            cout << "Received from laptop: " << received << endl;
            return received;
        } else if (result == 0) {
            cout << "Client disconnected" << endl;
            client_connected = false;
        }
        return "";
    }
    
    void processMessage(const string& jsonMessage, const string& sourceDevice) {
        string source = parseJSONValue(jsonMessage, "source");
        string event = parseJSONValue(jsonMessage, "event");
        string timestamp = parseJSONValue(jsonMessage, "timestamp");
        
        if (source.empty() || event.empty() || timestamp.empty()) {
            cerr << "Malformed JSON received from " << sourceDevice << endl;
            return;
        }
        
        cout << "Processing: " << event << " from " << source << " at " << timestamp << endl;
        
        // Log the event if it's a state change
        logEvent(timestamp, source, event);
        
        // Route message to other devices
        if (sourceDevice == "laptop") {
            // Forward to STM32
            sendToSerial(jsonMessage);
        } else if (sourceDevice == "stm32") {
            // Forward to laptop
            sendToClient(jsonMessage);
        }
        
        // Handle status requests
        if (event == "status_request") {
            string status_response;
            {
                lock_guard<mutex> lock(status_mutex);
                status_response = createJSON("raspberry_pi", current_door_status);
            }
            
            if (sourceDevice == "laptop") {
                sendToClient(status_response);
            } else if (sourceDevice == "stm32") {
                sendToSerial(status_response);
            }
        }
    }
    
    void handleClient() {
        while (running && client_connected) {
            string message = readFromClient();
            if (!message.empty()) {
                processMessage(message, "laptop");
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    
    void handleSerial() {
        while (running) {
            if (serial_connected) {
                string message = readFromSerial();
                if (!message.empty()) {
                    processMessage(message, "stm32");
                }
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }
    
    void acceptConnections() {
        while (running) {
            cout << "Waiting for client connection..." << endl;
            client_len = sizeof(client_addr);
            client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            
            if (client_socket != -1) {
                client_connected = true;
                cout << "Client connected from " << inet_ntoa(client_addr.sin_addr) << endl;
                
                // Handle this client in a separate thread
                thread client_thread(&DoorServer::handleClient, this);
                client_thread.join();
                
                close(client_socket);
                client_connected = false;
                cout << "Client disconnected" << endl;
            }
            
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    }
    
    void run() {
        cout << "Door Control Server Starting..." << endl;
        cout << "Database: " << (db ? "Connected" : "Failed") << endl;
        cout << "Serial: " << (serial_connected ? "Connected" : "Disconnected") << endl;
        cout << "Network: Listening on port 8080" << endl;
        
        // Start serial handler thread
        thread serial_thread(&DoorServer::handleSerial, this);
        
        // Start accepting connections (blocking)
        acceptConnections();
        
        // Cleanup
        serial_thread.join();
    }
    
    void stop() {
        running = false;
    }
    
    void cleanup() {
        running = false;
        
        if (client_connected) {
            close(client_socket);
        }
        if (server_socket != -1) {
            close(server_socket);
        }
        if (serial_connected && serial_port) {
            sp_close(serial_port);
            sp_free_port(serial_port);
        }
        if (db) {
            sqlite3_close(db);
        }
    }
};

// Global server instance for signal handling
DoorServer* global_server = nullptr;

void signalHandler(int signal) {
    cout << "\nShutting down server..." << endl;
    if (global_server) {
        global_server->stop();
    }
}

int main() {
    // Setup signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    DoorServer server;
    global_server = &server;
    
    server.run();
    
    return 0;
}