# Martha Tent Monitor

> **ESP32-powered environmental control system** for King Oyster & Lion’s Mane mushroom cultivation, maintaining optimal temperature, humidity, and CO₂ levels automatically.

## Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Hardware](#hardware)
- [Setup](#setup)
- [Operation](#operation)
- [Configuration](#configuration)
- [Development Workflow](#development-workflow)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

Martha Tent Monitor uses an ESP32 to continuously measure temperature, humidity, and CO₂ inside a Martha-style grow tent. It drives a relay-controlled humidifier and a MOSFET-driven ventilation fan to keep environmental conditions within defined thresholds, displaying real-time status on an I²C OLED and optionally logging data to a microSD card or streaming over Wi‑Fi.

## Key Features

- **Temperature & Humidity**: Reads from a DHT22 sensor every 2 seconds.
- **CO₂ Monitoring**: Measures CO₂ (350–10 000 ppm) via an MG-811 sensor.
- **Automated Control**: Toggles humidifier and ventilation fan to maintain 90–95 % RH and < 1200 ppm CO₂.
- **Live Display**: Shows sensor readings and device states on a 0.96″ I²C OLED.
- **Data Logging** *(optional)*: Logs CSV data to microSD or publishes JSON over Wi‑Fi.

## Hardware

The project requires the following components:

- **ESP32 DEVKIT V1** — main Wi‑Fi/Bluetooth microcontroller
- **Breadboard** & **jumper wires** — for prototyping connections
- **5 mm LEDs** + **220 Ω resistors** — visual indicators
- **Pushbutton** + **10 kΩ resistor** — user input
- **DHT11** humiture sensor — measures temperature and humidity
- **0.96″ I²C OLED display** — real-time status readout
- **MG-811 CO₂ sensor** — analog CO₂ measurement (350–10 000 ppm)
- **2-channel relay module** or **MOSFET driver** — switches power to devices
- **Humidifier** & **12 V fan** — environmental control hardware
- **MicroSD module** *(optional)* — data logging

## Setup

Follow these steps to prepare your development environment and upload the firmware:

1. **Clone the repository**
   ```bash
   git clone https://github.com/YourUserName/martha-tent-monitor.git
   cd martha-tent-monitor/firmware
   ```

2. **Install ESP32 support in Arduino IDE**
   - Open **Arduino → Preferences** and add the following URL to **Additional Boards Manager URLs**:
     ```text
     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
     ```
   - Go to **Tools → Board → Boards Manager**, search for **esp32**, and install **"esp32 by Espressif Systems"**.

3. **Install required libraries**
   - In **Sketch → Include Library → Manage Libraries**, search and install:
     - **DHT sensor library** by Adafruit
     - **Adafruit Unified Sensor**
     - **Adafruit SSD1306** (for the OLED display)

4. **Configure the IDE**
   - **Tools → Board:** select **ESP32 Dev Module**
   - **Tools → Port:** choose the port labeled `/dev/cu.*` that corresponds to your ESP32

5. **Upload the firmware**
   - Open **MarthaTentMonitor.ino** in the IDE
   - Click the **Upload** button (→ icon)
   - Confirm that the console shows **"Done uploading."**

6. **Verify hardware wiring**
   - Ensure all sensors and actuators are connected according to the diagrams in `hardware/wiring-diagrams/`
   - Apply USB power to the ESP32 and confirm the OLED initializes on startup.

## Operation

1. **Power on** the ESP32 via USB.
2. The system will:
   - Poll sensors every 2 s.
   - Turn on humidifier if RH < 90 % and off when > 95 %.
   - Activate ventilation fan if CO₂ > 1200 ppm and stop when below 800 ppm.
   - Update readings and status on the OLED.
3. **Monitor** output in **Serial Monitor** at 115200 baud.

## Configuration

Adjust these constants at the top of `MarthaTentMonitor.ino`:

| Parameter        | Purpose                                 | Default  |
|------------------|-----------------------------------------|----------|
| `HUMIDITY_MIN`   | Lower humidity threshold (%)            | 90       |
| `HUMIDITY_MAX`   | Upper humidity threshold (%)            | 95       |
| `CO2_MIN`        | Minimum CO₂ level to disable fan (ppm)  | 800      |
| `CO2_MAX`        | Maximum CO₂ level to enable fan (ppm)   | 1200     |
| `POLL_INTERVAL`  | Sensor polling interval (milliseconds)  | 2000     |

## Development Workflow

1. **Branch** from `main`:
   ```bash
   git checkout -b feature/describe-feature
   ```
2. **Implement & test** changes locally.
3. **Commit** with descriptive message:
   ```bash
   git commit -m "Add OLED readout improvements"
   ```
4. **Push** and open a **Pull Request** on GitHub.

## Contributing

Contributions are welcome:

- Open an issue to discuss new features or bugs.
- Use descriptive commit messages.
- Ensure code follows existing style and includes comments.
- Add documentation or tests for new functionality.

## License

This project is licensed under the **MIT License**. See [LICENSE](LICENSE) for details.
