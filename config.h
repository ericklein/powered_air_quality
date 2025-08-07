/*
  Project:      Powered Air Quality
  Description:  public (non-secret) configuration data
*/

// Configuration Step 1: Create and/or configure secrets.h. Use secrets_template.h as guide to create secrets.h

// Configuration Step 2: Set debug message output
// comment out to turn off; 1 = summary, 2 = verbose
// #define DEBUG 2

// Configuration Step 3: Simulate WiFi and sensor hardware, returning random but plausible values.
// Comment out to turn off
// #define HARDWARE_SIMULATE

// Configuration Step 4: Set network data endpoints
// #define MQTT     // log sensor data to MQTT broker
// #define HASSIO_MQTT  // And, if MQTT enabled, with Home Assistant too?
#define INFLUX // Log data to InfluxDB server
#define THINGSPEAK  // Log data to ThingSpeak

// Configuration Step 5: Which sensor configuration do we have?  Later generation devices
// use Sensirion SEN66 sensor which measures CO2, particulates, VOC, NOX, temperature and humidity
// in one package.  Earlier generation devices use a combination of the SEN54 particulates
// sensor and the SCD40 CO2 sensor (which also provides VOC, temperature and humidity readings).
// Note that only the newer SEN66 configuration provides NOX readings (using Sensirion's 
// NOX Index).
// Use the one that corresponds to your device hardware and leave the other commented out.
#define SENSOR_SEN66
// #define SENSOR_SEN54SCD40

// Configuration variables that are less likely to require changes

// network endpoints
#ifdef INFLUX  
  // Specify Measurement to use with InfluxDB for sensor and device info
  const String influxEnvMeasurement = "weather";  // Used for environmental sensor data
  const String influxDevMeasurement =  "device";   // Used for logging AQI device data (e.g. battery)
#endif

// Open Weather Map (OWM)
#define OWM_SERVER      "http://api.openweathermap.org/data/2.5/"
#define OWM_WEATHER_PATH  "weather?"
#define OWM_AQM_PATH    "air_pollution?"
// aqi labels from https://openweathermap.org/api/air-pollution
const String OWMAQILabels[5] = {"Good", "Fair", "Moderate", "Poor", "Very Poor"};

// sampling and reporting intervals
#ifdef DEBUG
  const uint16_t sensorSampleInterval = 30;   // time between samples in seconds
  const uint8_t reportInterval = 2;     // time between reports in minutes
#else
  const uint16_t sensorSampleInterval = 60;
  const uint8_t reportInterval = 15;
#endif
const uint8_t reportFailureThreshold = 3; // number of times reporting has to fail before UI reflects issue

// Screen saver timeout.  Will automatically switch to screen saver if
// no user input (via touchscreen) in this many seconds
const uint16_t screenSaverInterval = 300;    // In seconds

// warnings
const String warningLabels[4]={"Good", "Fair", "Poor", "Bad"};
// Subjective color scheme using 16 bit ('565') RGB colors
const uint16_t warningColor[4] = {
    0x07E0, // Green = "Good"
    0xFFE0, // Yellow = "Fair"
    0xFD20, // Orange = "Poor"
    0xF800  // Red = "Bad"
  };

// Hardware

// Simulation boundary values
#ifdef HARDWARE_SIMULATE
  const uint16_t sensorTempMinF =       1400; // divided by 100.0 to give floats
  const uint16_t sensorTempMaxF =       14000;
  const uint16_t sensorHumidityMin =    0; // RH%, divided by 100.0 to give float
  const uint16_t sensorHumidityMax =    10000;

  const uint8_t OWMAQIMin = 1;  // https://openweathermap.org/api/air-pollution
  const uint8_t OWMAQIMax = 5;

  const uint16_t OWMPM25Min = 0;  // will be divided by 100.0 to give float
  const uint32_t OWMPM25Max = 10000; // will be divided by 100.0 to give float

  const uint8_t networkRSSIMin = 30;
  const uint8_t networkRSSIMax = 90;

  const uint16_t sensorPMMin = 0;
  const uint16_t sensorPMMax = 100000; // divided by 100.0 to give float

  const uint16_t sensorVOCMin = 0;
  const uint16_t sensorVOCMax = 50000; // divided by 100.0 to give float
#endif

// CO2 value thresholds for labeling
const uint16_t co2Fair = 800;
const uint16_t co2Poor = 1200;
const uint16_t co2Bad = 2000;

const uint16_t sensorCO2Min =      400;   // in ppm
const uint16_t sensorCO2Max =      2000;  // in ppm
const uint8_t sensorTempCOffset = 0;     // in Celsius
const uint8_t co2SensorReadFailureLimit = 20;

// Particulates (pm1, pm2.5, pm4, pm10) value thresholds for labeling
const uint16_t pmFair = 25;
const uint16_t pmPoor = 50;
const uint16_t pm2Bad = 150;

// VOC (volatile organic compounds) value thresholds for labeling
const uint16_t vocFair = 150;
const uint16_t vocPoor = 250;
const uint16_t vocBad = 400;

// Sleep time in seconds if hardware error occurs
const uint8_t hardwareRebootInterval = 10;

// Display related configuration information
const uint8_t screenRotation = 3; // rotation 3 orients 0,0 next to D0 button
// Manage the suported display screens
#define SCREEN_SAVER      0
#define SCREEN_INFO       1
#define SCREEN_VOC        2 
#define SCREEN_COLOR      3
#define SCREEN_GRAPH      4
#define SCREEN_AGGREGATE  5
const uint8_t screenCount = 6;

// How many CO2 points to retain for the graphing screen
#define GRAPH_POINTS 10

// CYD pinout
#define TFT_BACKLIGHT 21
#define TFT_CS 15
#define TFT_DC 2
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_RST -1

// screen layout assists in pixels
const uint8_t xMargins = 5;
const uint8_t yMargins = 5;
const uint8_t wifiBarWidth = 3;
const uint8_t wifiBarHeightIncrement = 3;
const uint8_t wifiBarSpacing = 5;

// Buttons
// const uint8_t buttonD1Pin = 1; // initially LOW
// const int buttonDebounceDelay = 50; // time in milliseconds to debounce button

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Network
// max connection attempts to network services
const uint8_t networkConnectAttemptLimit = 3;
// seconds between network service connect attempts
const uint8_t networkConnectAttemptInterval = 10;

// CYD specific i2c pin configuration
// used in Wire.begin()
#define CYD_SDA 22
#define CYD_SCL 27