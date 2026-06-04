# Soil Moisture Logger

A low-power IoT project for continuous soil moisture monitoring using an ESP32-C3 microcontroller. Readings are displayed on an OLED screen and logged to an SD card for data analysis.

## Features

- **Real-time monitoring**: Display current soil moisture on OLED screen
- **Data logging**: Continuous CSV data logging to SD card every 5 minutes
- **Low power**: Deep sleep mode between measurements to extend battery life
- **Offline operation**: No WiFi/Bluetooth required – perfect for remote locations

## Hardware Requirements

- **Microcontroller**: ESP32-C3 Super Mini (or compatible ESP32-C3 board)
- **Sensor**: Capacitive soil moisture sensor
- **Display**: SSD1306 OLED screen (128×64 pixels, I2C)
- **Storage**: Micro SD card module (SPI)
- **Power**: USB-C or battery (5V)

## Pin Connections

| Component | Pin | Connection |
|-----------|-----|-----------|
| Soil Sensor | GPIO0 | ADC input |
| OLED SDA | GPIO8 | I2C data |
| OLED SCL | GPIO9 | I2C clock |
| SD Card MOSI | GPIO5 | SPI data |
| SD Card MISO | GPIO4 | SPI data |
| SD Card CLK | GPIO6 | SPI clock |
| SD Card CS | GPIO7 | SPI select |

## Installation

### 1. Clone the Repository
```bash
git clone https://github.com/TheRealTurtler/SoilMoisture-Logger.git
cd SoilMoisture-Logger
```

### 2. Install Dependencies
- Install [PlatformIO](https://platformio.org/) in VS Code
- Dependencies are managed in `platformio.ini`

### 3. Build & Upload
```bash
platformio run --target upload
```

## Configuration

Edit constants in `src/main.cpp` to customize behavior:

```cpp
const String FILE_PATH = "/soil.csv";                    // SD card file path
const auto INTERVAL_WRITE_SD = std::chrono::minutes(5);  // Write interval
const bool USE_DEEP_SLEEP = true;                        // Enable/disable sleep mode
const uint8_t DISPLAY_I2C_ADDRESS = 0x3C;                // OLED I2C address
```

## Data Format

Data is stored as CSV on the SD card (`/soil.csv`):

```
timestamp;raw_adc_value;moisture_percentage
1234567;2048;48.5
1234872;2100;48.8
```

- **timestamp**: Time in seconds since boot
- **raw_adc_value**: ADC reading (0–4095)
- **moisture_percentage**: Calculated moisture (0–100%)

## Usage

1. Insert formatted SD card
2. Connect power (USB or battery)
3. Firmware initializes display and SD card
4. Device takes measurements and enters deep sleep
5. OLED shows current reading when awake
6. Data logged to SD card every 5 minutes

### OLED Display Layout
```
Time:        12:34:56
Value Raw:       2048
Moisture [%]:    48.5
SD Mounted:    YES/NO
```

## Serial Debug Output

Connect via USB and open serial monitor (115200 baud) to see debug logs:

```
========== START ==========
Display Connected: 1
SD-Card Mounted:   1
Appending line to SD: 1234567;2048;48.5
Entering deep sleep mode...
```

## Contributing

Found a bug or have a feature request? Open an issue or submit a pull request!
