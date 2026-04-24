# 🛸 Project Quadcopter

A high-performance, low-latency drone flight control system built from scratch using **STM32F411**and **Arduino Nano** utilizing **nRF24L01** radio modules for communication.

---

## 🚀 Current Status: "The Wireless Link"
The core communication layer is **Live**. We have established a robust, interrupt-driven SPI bridge between the Arduino Transmitter and the STM32 Flight Controller.

- [x] **nRF24L01 Driver:** Custom register-level implementation.
- [x] **Interrupt-Driven:** No CPU polling; the nRF24 "wakes up" the STM32 via EXTI lines.
- [x] **Joystick Mapping:** 4-axis control (Yaw, Pitch, Roll, Throttle) + 4 auxiliary switches.

---

## 🛠 Hardware Architecture

### 📡 Remote Controller
* **MCU:** Arduino Nano (ATmega328P)
* **Radio:** nRF24L01+ (PA/LNA version)
* **Input:** Dual Analog Joysticks + 4 Toggle Switches

### 🚁 Flight Controller
* **MCU:** STM32F411CEU6 ("Black Pill")
* **Radio:** nRF24L01+
* **IMU:** MPU9250
  

---

## 📂 Project Structure

```text
.
├── flight_controller/     # STM32 CubeMX + CMake Source
│   ├── Core/              # Main Logic & Drivers
│   └── Drivers/           # HAL & CMSIS Libraries
├── remote_controller/     # Arduino Transmitter Sketch
└── checklist/             # Engineering notes & progress tracking
