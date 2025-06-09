/*
* Sir Locks-A-Lot - Enhanced Version
* Version: 2.0
*
* Filename: SLAL_windows_wifi.cpp
*
* Description:
* Enhanced door control program for Windows with WiFi communication
* Sends and receives JSON messages to/from Raspberry Pi
* Features JSON parsing and network communication capabilities
*
* JSON Format:
* {
*   "source": "laptop|stm32|raspberry_pi",
*   "event": "lock|unlock|error|status_request",
*   "timestamp": "YYYY-MM-DD HH:MM:SS"
* }
*/

#include <iostream>
#include <string>
#include <cstdlib>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

#define WIDTH 100
#define HEIGHT 50
#define RASPBERRY_PI_IP "10.0.0.8"  // Change this to your Pi's IP
#define PORT 8080
#define BUFFER_SIZE 1024

using namespace std;

class DoorController {
private:
    SOCKET sock;
    struct sockaddr_in server_addr;
    string doorStatus;
    bool connected;

public:
    DoorController() : doorStatus("UNKNOWN"), connected(false) {
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            cout << "WSAStartup failed" << endl;
            return;
        }

        // Create socket
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            cout << "Socket creation failed" << endl;
            WSACleanup();
            return;
        }

        // Setup server address
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, RASPBERRY_PI_IP, &server_addr.sin_addr);

        connectToRaspberryPi();
    }

    ~DoorController() {
        if (sock != INVALID_SOCKET) {
            closesocket(sock);
        }
        WSACleanup();
    }

    bool connectToRaspberryPi() {
        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            cout << "Connection to Raspberry Pi failed. Operating in offline mode." << endl;
            connected = false;
            this_thread::sleep_for(chrono::seconds(2));
            return false;
        }
        connected = true;
        doorStatus = "CONNECTED";
        return true;
    }

    string getCurrentTimestamp() {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);

        struct tm timeinfo;
        localtime_s(&timeinfo, &time_t);  // Safe version for Windows

        stringstream ss;
        ss << put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    string createJSON(const string& source, const string& event) {
        string timestamp = getCurrentTimestamp();
        return "{\"source\":\"" + source + "\",\"event\":\"" + event + "\",\"timestamp\":\"" + timestamp + "\"}";
    }

    // Simple JSON parser - extracts values between quotes after key
    string parseJSONValue(const string& json, const string& key) {
        string searchKey = "\"" + key + "\":\"";
        size_t pos = json.find(searchKey);
        if (pos == string::npos) return "";

        pos += searchKey.length();
        size_t endPos = json.find("\"", pos);
        if (endPos == string::npos) return "";

        return json.substr(pos, endPos - pos);
    }

    bool sendJSON(const string& jsonMessage) {
        if (!connected) {
            cout << "Not connected to Raspberry Pi. Attempting to reconnect..." << endl;
            if (!connectToRaspberryPi()) {
                return false;
            }
        }

        int result = send(sock, jsonMessage.c_str(), jsonMessage.length(), 0);
        if (result == SOCKET_ERROR) {
            cout << "Send failed. Connection may be lost." << endl;
            connected = false;
            return false;
        }
        return true;
    }

    string receiveJSON() {
        if (!connected) return "";

        char buffer[BUFFER_SIZE];
        int bytesReceived = recv(sock, buffer, BUFFER_SIZE - 1, 0);

        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            return string(buffer);
        }
        else if (bytesReceived == 0) {
            cout << "Connection closed by Raspberry Pi" << endl;
            connected = false;
        }
        else {
            cout << "Receive failed" << endl;
            connected = false;
        }
        return "";
    }

    void processReceivedMessage(const string& jsonMessage) {
        string source = parseJSONValue(jsonMessage, "source");
        string event = parseJSONValue(jsonMessage, "event");
        string timestamp = parseJSONValue(jsonMessage, "timestamp");

        if (event == "lock") {
            doorStatus = "LOCKED (via " + source + ")";
        }
        else if (event == "unlock") {
            doorStatus = "UNLOCKED (via " + source + ")";
        }
        else if (event == "error") {
            doorStatus = "ERROR (from " + source + ")";
        }
    }

    void clearScreen() {
        system("cls");
    }

    void drawFrame() {
        clearScreen();

        // Top border
        cout << "+";
        for (int i = 0; i < WIDTH - 2; i++) cout << "=";
        cout << "+" << endl;

        // Menu section
        cout << "|                                      DOOR CONTROL MENU                                           |" << endl;
        cout << "|                                                                                                  |" << endl;
        cout << "|    Available Commands:                                                                           |" << endl;
        cout << "|                                                                                                  |" << endl;
        cout << "|    1. status   - Check current door status                                                       |" << endl;
        cout << "|    2. lock     - Lock the door                                                                   |" << endl;
        cout << "|    3. unlock   - Unlock the door                                                                 |" << endl;
        cout << "|    4. connect  - Reconnect to Raspberry Pi                                                       |" << endl;
        cout << "|    5. quit     - Exit program                                                                    |" << endl;
        cout << "|                                                                                                  |" << endl;

        // Middle divider
        cout << "|";
        for (int i = 0; i < WIDTH - 2; i++) cout << "-";
        cout << "|" << endl;

        // Status section
        cout << "|                                      CURRENT DOOR STATUS                                         |" << endl;
        cout << "|                                                                                                  |" << endl;

        // Center the door status
        int padding = 40;
        cout << "|";
        for (int i = 0; i < padding; i++) cout << " ";
        cout << doorStatus;
        for (int i = 0; i < (WIDTH - 2 - padding - doorStatus.length()); i++) cout << " ";
        cout << "|" << endl;

        cout << "|                                                                                                  |" << endl;

        // Connection status
        string connectionStatus = connected ? "CONNECTED TO RASPBERRY PI" : "OFFLINE MODE";
        cout << "|";
        for (int i = 0; i < padding; i++) cout << " ";
        cout << connectionStatus;
        for (int i = 0; i < (WIDTH - 2 - padding - connectionStatus.length()); i++) cout << " ";
        cout << "|" << endl;

        cout << "|                                                                                                  |" << endl;

        // Bottom border
        cout << "+";
        for (int i = 0; i < WIDTH - 2; i++) cout << "=";
        cout << "+" << endl;
    }

    void checkForIncomingMessages() {
        // Set socket to non-blocking mode to check for messages without waiting
        u_long mode = 1;
        ioctlsocket(sock, FIONBIO, &mode);

        string received = receiveJSON();
        if (!received.empty()) {
            processReceivedMessage(received);
        }

        // Set back to blocking mode
        mode = 0;
        ioctlsocket(sock, FIONBIO, &mode);
    }

    void run() {
        string userInput;

        while (true) {
            // Check for incoming messages from Raspberry Pi
            if (connected) {
                checkForIncomingMessages();
            }

            drawFrame();

            cout << "Enter command: ";
            getline(cin, userInput);

            if (userInput == "quit") {
                break;
            }
            else if (userInput == "status") {
                // Send status request to Raspberry Pi
                string jsonMsg = createJSON("laptop", "status_request");
                if (sendJSON(jsonMsg)) {
                    cout << "Status request sent to Raspberry Pi..." << endl;
                    this_thread::sleep_for(chrono::milliseconds(500));

                    // Try to receive response
                    string response = receiveJSON();
                    if (!response.empty()) {
                        processReceivedMessage(response);
                    }
                }
            }
            else if (userInput == "lock") {
                string jsonMsg = createJSON("laptop", "lock");
                if (sendJSON(jsonMsg)) {
                    doorStatus = "LOCK COMMAND SENT";
                    cout << "Lock command sent to Raspberry Pi..." << endl;
                    this_thread::sleep_for(chrono::milliseconds(500));
                }
                else {
                    doorStatus = "FAILED TO SEND LOCK COMMAND";
                }
            }
            else if (userInput == "unlock") {
                string jsonMsg = createJSON("laptop", "unlock");
                if (sendJSON(jsonMsg)) {
                    doorStatus = "UNLOCK COMMAND SENT";
                    cout << "Unlock command sent to Raspberry Pi..." << endl;
                    this_thread::sleep_for(chrono::milliseconds(500));
                }
                else {
                    doorStatus = "FAILED TO SEND UNLOCK COMMAND";
                }
            }
            else if (userInput == "connect") {
                cout << "Attempting to connect to Raspberry Pi..." << endl;
                if (connectToRaspberryPi()) {
                    doorStatus = "RECONNECTED";
                }
                else {
                    doorStatus = "CONNECTION FAILED";
                }
                this_thread::sleep_for(chrono::seconds(1));
            }
            else {
                doorStatus = "ERROR - Invalid command";
            }
        }

        cout << "System shutting down..." << endl;
    }
};

int main() {
    cout << "Sir Locks-A-Lot - Enhanced Version" << endl;
    cout << "Initializing network connection..." << endl;

    DoorController controller;
    controller.run();

    return 0;
}