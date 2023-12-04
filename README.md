# FirebaseDeskWeatherStation
Connect Temperature/Humidity sensor to Firebase using ESP32

![Weather Station](https://github.com/ayushchinmay/FirebaseDeskWeatherStation/blob/main/readme_ref/weather-station.jpg)

## Software Requirements
* Arduino IDE
* Web Browser

### Arduino Libraries
* Adafruit BME280 Library
* Adafruit GFX Library
* Adafruit SSD1306
* Adafruit Unified Sensor
### Installing ESP32 Board
* Open Arduino IDE, navigate to the following and paste the following in the textbox: __File -> Preferences -> Additional Boards Manager__ `(CTRL+,)`
  * ```
    http://arduino.esp8266.com/stable/package_esp8266com_index.json
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    ```
  * ![Additional Boards](https://github.com/ayushchinmay/FirebaseDeskWeatherStation/blob/main/readme_ref/esp32-board.png)
  * Open Boards Manager tab by navigating to: __Tools -> Board -> Boards Manager...__ `(CTRL+Shift+B)`
    * Search for `esp32 by Espressif` and download
  * Select the ESP32 Board by navigating to __Tools -> Board -> esp32 -> ESP32 Dev Module__

## Hardware Requirements
* ESP32 Devkit Module
* SSD1306 I2C OLED
* BME 280 Temperature Sensor
* Breadboard
* Jumper Wires
* USB A - Micro USB Cable (for Data/Power)

## Circuit Diagram
* SSD1306 OLED <=> ESP32
  * SDA <--> SDA _(GPIO21)_
  * SCL <--> SCL _(GPIO22)_
  * VCC <--> 3.3V
  * GND <--> GND

* BME 280 <=> ESP32
  * SDA <--> SDA _(GPIO21)_
  * SCL <--> SCL _(GPIO22)_
  * VCC <--> 3.3V
  * GND <--> GND

## Setting up Firebase
* Go to: [Firebase](https://console.firebase.google.com/)
* Create a new Project
* Enable and Setup Authentication
  * ![Authentication](https://github.com/ayushchinmay/FirebaseDeskWeatherStation/blob/main/readme_ref/fbase-auth.png) 
* Setup a Realtime Database according to the following structure:
  * ![Realtime Database](https://github.com/ayushchinmay/FirebaseDeskWeatherStation/blob/main/readme_ref/rtdb-struct.png)
* Setup RTDB Rules
  * ![RTDB Rules](https://github.com/ayushchinmay/FirebaseDeskWeatherStation/blob/main/readme_ref/rtdb-rules.png)
