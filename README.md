# Guardian-Node Smart Parking System

Guardian-Node is an ESP32-based smart parking automation system developed for the 3707ICT Automation and IoT course.

The system monitors a parking space using an ultrasonic distance sensor and PIR motion sensor. The ESP32 processes the sensor readings locally, determines the parking state, controls the connected actuators, and uploads monitoring data to ThingSpeak.

## Hardware

The Wokwi prototype uses:

- ESP32 DevKit
- HC-SR04 ultrasonic sensor
- PIR motion sensor
- Red LED
- Green LED
- Servo motor
- Buzzer
- 220 ohm resistors

## Parking States

The system uses four operating states:

- **AVAILABLE** – The parking space is confirmed as available. The green LED is on and the barrier opens.
- **OCCUPIED** – The parking space is confirmed as occupied. The red LED is on and the barrier remains closed.
- **CHECKING** – Sensor readings are uncertain or conflicting. The barrier remains closed.
- **FAILSAFE** – An invalid ultrasonic reading is detected. The barrier remains closed, and a warning is activated.

The decision logic uses sensor fusion, distance hysteresis, and three consecutive matching readings to reduce unreliable state changes.

## Cloud Integration

The ESP32 connects to Wi-Fi and uploads data to ThingSpeak approximately every 20 seconds.

The ThingSpeak channel uses:

- Field 1 – Ultrasonic distance (cm)
- Field 2 – PIR motion (0 or 1)
- Field 3 – Parking state

Parking state values:

- `0` – AVAILABLE
- `1` – OCCUPIED
- `2` – CHECKING
- `3` – FAILSAFE

A ThingSpeak Write API key is required for cloud uploads. The public source code contains the placeholder:

`YOUR_WRITE_API_KEY`

A valid Write API key must be entered locally before running the cloud functionality. API keys should not be committed to a public repository.

## Project Files

- `src/main.cpp` – Main ESP32 program and automation logic
- `diagram.json` – Wokwi circuit configuration
- `platformio.ini` – PlatformIO project configuration and dependencies
- `wokwi.toml` – Wokwi simulation configuration

## Development Environment

The project was developed using:

- ESP32
- Arduino framework
- PlatformIO
- Wokwi
- ThingSpeak
- ESP32Servo library

## Running the Project

1. Open the project in Visual Studio Code with PlatformIO and the Wokwi extension.
2. Add a valid ThingSpeak Write API key to `src/main.cpp` if cloud uploads are required.
3. Build the project using PlatformIO.
4. Start the Wokwi simulation.
5. Change the HC-SR04 distance and PIR motion conditions to test the different parking states.
6. Use the Serial Monitor and ThingSpeak dashboard to observe the system output.
