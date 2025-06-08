#ifndef CONSOLE_UI_HPP
#define CONSOLE_UI_HPP

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "door_control_client.hpp"

class ConsoleUI {
private:
    DoorControlClient& client_;
    std::atomic<bool> running_;
    
public:
    ConsoleUI(DoorControlClient& client) : client_(client), running_(false) {}
    
    void run() {
        running_ = true;
        
        // Start status display thread
        std::thread status_thread(&ConsoleUI::displayStatus, this);
        
        // Main menu loop
        while (running_) {
            displayMenu();
            
            std::string input;
            std::getline(std::cin, input);
            
            if (input == "1") {
                client_.sendLockCommand();
            } else if (input == "2") {
                client_.sendUnlockCommand();
            } else if (input == "3") {
                client_.requestSync();
            } else if (input == "q" || input == "Q") {
                running_ = false;
            } else {
                std::cout << "Invalid option. Please try again." << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        status_thread.join();
    }
    
private:
    void displayMenu() {
        std::cout << "\n=== Door Control Menu ===" << std::endl;
        std::cout << "1. Lock door" << std::endl;
        std::cout << "2. Unlock door" << std::endl;
        std::cout << "3. Sync status" << std::endl;
        std::cout << "Q. Quit" << std::endl;
        std::cout << "Enter choice: ";
    }
    
    void displayStatus() {
        while (running_) {
            // Clear line and display status
            std::cout << "\r[Status] Door is: " << client_.getCurrentState() 
                     << "        " << std::flush;
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
};