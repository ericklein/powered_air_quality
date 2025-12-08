/*
  Project:      Powered Air Quality
  Description:  public (non-secret) configuration data
*/

// Configuration Step 1: Create and/or configure secrets.h. Use secrets_template.h as guide to create secrets.h

// Configuration Step 2: Set debug message output
// comment out to turn off; 1 = summary, 2 = verbose
#define DEBUG 2

// Configuration Step 3: Simulate WiFi and sensor hardware, returning random but plausible values.
// Comment out to turn off
// #define HARDWARE_SIMULATE

// Configuration Step 4: Set network data endpoints
// #define MQTT     // log sensor data to MQTT broker
// #define HASSIO_MQTT  // And, if MQTT enabled, with Home Assistant too?
// #define INFLUX // Log data to InfluxDB server
// #define THINGSPEAK  // Log data to ThingSpeak

// Configuration Step 5: Which sensor configuration do we have?  Later generation devices
// use Sensirion SEN66 sensor which measures CO2, particulates, VOC, NOX, temperature and humidity
// in one package.  Earlier generation devices use a combination of the SEN54 particulates
// sensor and the SCD40 CO2 sensor (which also provides VOC, temperature and humidity readings).
// Note that only the newer SEN66 configuration provides NOX readings (using Sensirion's 
// NOX Index).
// Use the one that corresponds to your device hardware and leave the other commented out.
// #define SENSOR_SEN66
#define SENSOR_SEN54SCD40

// Configuration variables that are less likely to require changes

// Internet and network endpoints
const uint32_t timeNetworkConnectTimeoutMS = 10000;
const uint32_t timeNetworkRetryIntervalMS = 30000;
const uint32_t timeMQTTKeepAliveIntervalMS = 10000; // ping MQTT broker to keep alive

// Open Weather Map (OWM)
const String OWMServer = "http://api.openweathermap.org/data/2.5/";
const String OWMWeatherPath =  "weather?";
const String OWMAQMPath = "air_pollution?";
// OWM Air Pollution scale from https://openweathermap.org/api/air-pollution
const String OWMPollutionLabel[5] = {"Good", "Fair", "Moderate", "Poor", "Very Poor"};
const uint32_t OWMIntervalMS = 1800000;

// sampling and reporting intervals
#ifdef DEBUG
  const uint32_t sensorSampleIntervalMS = 30000;  // time between samples
  const uint32_t reportIntervalMS = 90000;        // time between reports
#else
  const uint32_t sensorSampleIntervalMS = 60000;
  const uint32_t reportIntervalMS = 900000;
#endif
const uint8_t reportFailureThreshold = 3; // number of times reporting has to fail before UI reflects issue

// UI
enum screenNames {sSaver, sMain, sCO2, sPM25, sVOC, sNOX};

// screen layout assists in pixels
const uint8_t xMargins = 5;
const uint8_t yMargins = 5;
const uint8_t cornerRoundRadius = 4;
const uint8_t wifiBarWidth = 3;
const uint8_t wifiBarHeightIncrement = 3;
const uint8_t wifiBarSpacing = 5;

// How many data samples are retained for graphing
const uint8_t graphPoints = 10;

const uint32_t screenSaverIntervalMS = 300000; // switch to screen saver if no input after this period

// warnings
// const String warningLabels[4]={"Good", "Fair", "Poor", "Bad"};
// Subjective color scheme using 16 bit ('565') RGB colors
const uint16_t warningColor[4] = {
    0x07E0, // Green = "Good"
    0xFFE0, // Yellow = "Fair"
    0xFD20, // Orange = "Poor"
    0xF800  // Red = "Bad"
  };

// hardware
const String hardwareDeviceType = "AirQuality";

// sensors
// simulation boundary values
#ifdef HARDWARE_SIMULATE
  const uint16_t sensorTempMinF =       14; // -10C per datasheet
  const uint16_t sensorTempMaxF =       140; // 60C per datasheet
  const uint16_t sensorHumidityMin =    0; // RH% per datasheet
  const uint16_t sensorHumidityMax =    100;

  const uint8_t OWMAQIMin = 1;  // https://openweathermap.org/api/air-pollution
  const uint8_t OWMAQIMax = 5;

  const uint16_t OWMPM25Min = 0;
  const uint16_t OWMPM25Max = 100; 

  const uint8_t networkRSSIMin = 30;
  const uint8_t networkRSSIMax = 90;
#endif

// tempF value threshholds
const uint8_t sensorTempFComfortMin = 65;
const uint8_t sensorTempFComfortMax = 80;

// humidity value thresholds
const uint8_t sensorHumidityComfortMin = 40;
const uint8_t sensorHumidityComfortMax = 60;

// CO2 value thresholds
const uint16_t sensorCO2Min =   400;   // in ppm
const uint16_t sensorCO2Fair =  800;
const uint16_t sensorCO2Poor =  1200;
const uint16_t sensorCO2Bad =   1600;
const uint16_t sensorCO2Max =   2000;
const uint8_t co2SensorReadFailureLimit = 20;
const uint8_t sensorCO2VariabilityRange = 30;

// Particulates (pm1, pm2.5, pm4, pm10) value thresholds
const uint16_t sensorPMMin =  0;
const uint16_t sensorPMFair = 25;
const uint16_t sensorPMPoor = 50;
const uint16_t sensorPMBad =  150;
const uint16_t sensorPMMax =  1000;

// VOC (volatile organic compounds) index value thresholds
const uint8_t   sensorVOCMin =  0;
const uint16_t  sensorVOCFair = 150;
const uint16_t  sensorVOCPoor = 250;
const uint16_t  sensorVOCBad =  400;
const uint16_t  sensorVOCMax =  500;

// NOx (nitrogen oxide) index value thresholds, Sensiron Info_Note_NOx_Index.pdf
const uint16_t noxFair = 49;
const uint16_t noxPoor = 150;
const uint16_t noxBad = 300;

const uint32_t hardwareErrorSleepTimeμS = 10000000;  // sleep time if hardware error occurs

// button
const uint8_t hardwareWipeButton = 0; // boot button on most ESP32 boards
const uint16_t timeResetButtonHoldMS = 10000; // Long-press duration to wipe config

// touchscreen pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// touchscreen calibration
const uint16_t touchscreenMinX = 200;
const uint16_t touchscreenMaxX = 3700;
const uint16_t touchscreenMinY = 240;
const uint16_t touchscreenMaxY = 3800;

// CYD variations
// standard CYD (2.8" TFT, micro-USB)
// i2c pins
#define CYD_SDA 22
#define CYD_SCL 27
const uint8_t screenRotation = 3; // CYD 2.8; horizontal orientation with USB port on left side

// 3.2" TFT, usb-c CYD (Freenode FNK0103L_3P2)
// i2c pins
// #define CYD_SDA 32
// #define CYD_SCL 25
// const uint8_t screenRotation = 1; // CYD 3.2; horizontal orientation with USB port on right side