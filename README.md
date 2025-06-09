# Sir_Locks_A_Lot
Door locking and monitoring using STM32F7, Raspberry Pi and Laptop.

## Build:
Raspberry pi: g++ -o SLAL-rasppi SLAL-rasppi.cpp -lsqlite3 -lserialport -lpthread <br>
Windows: Visual studio <br>
STM32: STM32CubeIDE

## Connections:
- STM32 USB to Raspberry Pi 3B USB.
- Raspberry Pi 3B to Windows through Wifi (need to use correct IP address in SLAL-windows.cpp)

## Run in order:
1. STM32
2. Rasp Pi - ./SLAL-rasppi
3. Windows - SLAL-windows.exe
