/*
* Sir Locks-A-Lot
* Version: 1.0
*
* Filename: SLAL_windows.cpp
* 
* Description:
* A simple door control program for Windows
* This program allows users to lock and unlock a door, check its status, and exit the program.
* It features a text-based user interface with a frame that displays the current door status and available commands.
*
* Usage:
* This program works in conjunction with the associated Sir Lock-A-Lot software running on the Raspberry Pi 3B and STM32F4 Discovery board. 
* All 3 devices must be running for full functionality
*/



#include <iostream>
#include <string>
#include <cstdlib>

#define WIDTH 100  // Width of the frame
#define HEIGHT 50 // Height of the frame
using namespace std;

void clearScreen() {
    system("cls");
}

void drawFrame(string doorStatus) {
    clearScreen();
    
    // Top border
    cout << "+";
    for(int i = 0; i <WIDTH-2; i++) cout << "=";
    cout << "+" << endl;
    
    // Menu section (top half - 24 lines)
    cout << "|                                      DOOR CONTROL MENU                                           |" << endl;
    cout << "|                                                                                                  |" << endl;
    cout << "|    Available Commands:                                                                           |" << endl;
    cout << "|                                                                                                  |" << endl;
    cout << "|    1. status   - Check current door status                                                       |" << endl;
    cout << "|    2. lock     - Lock the door                                                                   |" << endl;
    cout << "|    3. unlock   - Unlock the door                                                                 |" << endl;
    cout << "|    4. quit     - Exit program                                                                    |" << endl;
    cout << "|                                                                                                  |" << endl;
    
   
    // Middle divider
    cout << "|";
    for(int i = 0; i < WIDTH-2; i++) cout << "-";
    cout << "|" << endl;
    
    // Status section (bottom half - 24 lines)
    cout << "|                                      CURRENT DOOR STATUS                                         |" << endl;
    cout << "|                                                                                                  |" << endl;
    
    // Center the door status
    //string statusLine = doorStatus;
    int padding = 40;
    cout << "|";
    for(int i = 0; i < padding; i++) cout << " ";
    cout << doorStatus;
    for(int i = 0; i < (WIDTH-2 - padding - doorStatus.length()); i++) cout << " ";
    cout << "|" << endl;
    
    cout << "|                                                                                                  |" << endl;
    
    // Bottom border
    cout << "+";
    for(int i = 0; i < WIDTH-2; i++) cout << "=";
    cout << "+" << endl;
}

int main() {
    string doorStatus = "UNKNOWN";
    string userInput;
    
    while(true) {
        drawFrame(doorStatus);
        
        cout << "Enter command: ";
        getline(cin, userInput);
        
        if(userInput == "quit") {
            break;
        }
        else if(userInput == "status") {
            // Status is already displayed, just refresh
            continue;
        }
        else if(userInput == "lock") {
            doorStatus = "LOCKED";
        }
        else if(userInput == "unlock") {
            doorStatus = "UNLOCKED";
        }
        else {
            doorStatus = "ERROR - Invalid command";
        }
    }
    
    cout << "System shutting down..." << endl;
    return 0;
}