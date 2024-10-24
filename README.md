# Powered Air Quality

### Purpose
Powered Air Quality samples and displays temperature, humidity, CO2, and airborne particulate levels. It can also log this data to a number of network endpoints.

### Features

### Target configuration
- Important access settings for WiFi (SSID and password), network endpoints (MQTT, InfluxDB), and location information are contained in a `secrets.h` file that is not included in this repo.  Instead you'll find the file `secrets_template.h`, which should be copied to `secrets.h` and then edited to supply the right access credentials and configuration values to match your deployment environment.
- See config.h for device parameter configuration instructions

### Bill of Materials (BOM)
- MCU
    - ESP32
- WiFi
    - Supported hardware
        - ESP32 based boards
- environment sensor
    - [SCD40 True CO2, Temperature and Humidity Sensor](https://www.adafruit.com/product/5187)
- Screen
    - Supported hardware
        - 1.54" e-paper display with 200x200 pixels
            - [Adafruit 1.54" Monochrome ePaper Display, 200x200 with SSD1681](https://www.adafruit.com/product/4196)
        - [Adafruit eInk Breakout Friend with 32KB SRAM](https://www.adafruit.com/product/4224)
            - bare epd display
    - Technical References
        - https://cdn-learn.adafruit.com/downloads/pdf/adafruit-gfx-graphics-library.pdf

### Pinouts
- Optional LC709203F
    - Stemma QT cable between MCU board and LC709203F board
    - Battery connected to LC709203F board
    - Power connector between LC709203F board and MCU board
    - 10K thermistor between thermistor pin and ground pin on LC709203F board (required to measure battery temperature)
- SPDT switch (on/off)
    - MCU EN to SPD rightmost pin
    - MCU GND to SPD
- SCD40
    - Stemma QT cable between MCU board and SCD40 board
- EPD screen
    - EPD VIN to MCU 3V
    - EPD GND to MCU GND
    - EPD SCK to MCU SCK
    - EPD MISO to MCU MISO
    - EPD MOSI to MCU MOSI
    - see config.h for these pins
        - EPD ECS
        - EPD D/C
        - EPD SRCS
        - EPD RST
        - EPD BUSY

### Supported Internet Services for data logging
- The routines that post data to back end services are generalized as much as practical, though do need to be customized to match the data fieles of interest both within the scope of the project and based on what users want to report and monitor.  Configuration values in config.h help with basic customization, e.g. name of the device, tags to use for Influx data, though in some cases code may need to be modified in the associated post routine.

- MQTT Broker
    - uncomment #define MQTT
    - set appropriate parameters in config.h and secrets.h
    - Technical References
        - https://hackaday.com/2017/10/31/review-iot-data-logging-services-with-mqtt/
- InfluxDB
    - uncomment #define INFLUX
    - set appropriate parameters in config.h and secrets.h

### Information Sources
- NTP
    - https://github.com/PaulStoffregen/Time/tree/master/examples/TimeNTP
- Sensors 
    - https://cdn-learn.adafruit.com/assets/assets/000/104/015/original/Sensirion_CO2_Sensors_SCD4x_Datasheet.pdf?1629489682
    - https://github.com/Sensirion/arduino-i2c-scd4x
    - https://github.com/sparkfun/SparkFun_SCD4x_Arduino_Library
    - https://emariete.com/en/sensor-co2-sensirion-scd40-scd41-2/

### Issues and Feature Requests
- See GitHub Issues for project

### .plan (big ticket items)
