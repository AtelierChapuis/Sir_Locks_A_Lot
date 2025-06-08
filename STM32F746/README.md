# STM32F746 Door Control System

## Overview
This is the STM32F746 Discovery board firmware for the distributed door control system. It controls a door (simulated using the onboard LD1 LED) and communicates with a Raspberry Pi relay service via USART.

## Features
- **Door States**:
  - Locked: LED continuously ON
  - Unlocked: LED blinking every 500ms
  
- **User Interface**:
  - Blue user button (B1) toggles door state
  
- **Communication**:
  - USART1 (115200 baud) for JSON message exchange
  - Sends door events: `{"source":"stm32","event":"door_locked","timestamp":"2025-06-08T10:25:00Z"}`
  - Receives commands: `{"command":"lock"}` or `{"command":"unlock"}`
  - Acknowledges all valid commands

## Hardware Configuration
- **LED (LD1)**: PI1
- **User Button (B1)**: PI11
- **USART1 TX**: PA9
- **USART1 RX**: PB7

## Software Architecture
- **FreeRTOS Tasks**:
  1. **DoorTask**: Manages LED state based on door state
  2. **UartRxTask**: Receives and processes incoming JSON messages
  3. **UartTxTask**: Sends JSON messages from queue

- **Thread-Safe Design**:
  - Semaphore protects door state access
  - Message queue for UART transmission
  - ISR-safe button handling

## Building and Flashing
1. Open project in STM32CubeIDE
2. Configure for STM32F746NGH6 target
3. Build project (Ctrl+B)
4. Flash to board using ST-Link

## JSON Message Format

### Outgoing Messages (STM32 → Raspberry Pi)
```json
{
  "source": "stm32",
  "event": "door_locked",
  "timestamp": "2025-06-08T10:25:00Z"
}
```

### Incoming Commands (Raspberry Pi → STM32)
```json
{
  "command": "lock",
  "source": "laptop"
}
```

### Acknowledgment
```json
{
  "type": "ack",
  "status": "ok"
}
```

## Dependencies
- STM32 HAL Driver
- FreeRTOS with CMSIS-RTOS V2 API
- Standard C library for string operations

## Notes
- Timestamp in messages uses kernel tick count for simplicity
- Button debouncing handled by edge-triggered interrupt
- Robust against malformed JSON messages