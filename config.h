/*
  Project:      Powered Air Quality
  Description:  public (non-secret) configuration data
*/

// Configuration Step 1: Set debug message output
// comment out to turn off; 1 = summary, 2 = verbose
#define DEBUG 2

// Configuration Step 2: simulate WiFi and sensor hardware,
// returning random but plausible values
// comment out to turn off
// #define HARDWARE_SIMULATE

// Configuration Step 3: Set network data endpoints
// HARDWARE_SIMULATE can not be defined if any network data endpoints are defined
// #define MQTT     // log sensor data to MQTT broker
// #define HASSIO_MQTT  // And, if MQTT enabled, with Home Assistant too?
#define INFLUX // Log data to InfluxDB server
// #define DWEET       // Log data to Dweet service
// #define THINGSPEAK  // Log data to ThingSpeak

// Configuration variables that are less likely to require changes

// CYD specific i2c pin configuration
// used in Wire.begin()
#define CYD_SDA 22
#define CYD_SCL 27

// Buttons
// const uint8_t buttonD1Pin = 1; // initially LOW
// const int buttonDebounceDelay = 50; // time in milliseconds to debounce button

// Display
const uint8_t screenRotation = 3; // rotation 3 orients 0,0 next to D0 button
const uint8_t screenCount = 5;

// CYD
#define TFT_BACKLIGHT 21
#define TFT_CS 15
#define TFT_DC 2
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_RST -1

// screen layout assists in pixels
const uint16_t xMargins = 10;
const uint16_t yMargins = 2;
const uint16_t wifiBarWidth = 3;
const uint16_t wifiBarHeightIncrement = 3;
const uint16_t wifiBarSpacing = 5;

#ifdef DEBUG
  const uint16_t sensorSampleInterval = 30;   // time between samples in seconds
  const uint8_t sensorReportInterval = 2;     // time between reports in minutes
#else
  const uint16_t sensorSampleInterval = 60;
  const uint8_t sensorReportInterval = 15;
#endif

// Simulation boundary values
#ifdef HARDWARE_SIMULATE
  const uint16_t sensorTempMin =      1500; // in Celcius, divided by 100.0 to give floats
  const uint16_t sensorTempMax =      2500;
  const uint16_t sensorHumidityMin =  500; // RH%, divided by 100.0 to give floats
  const uint16_t sensorHumidityMax =  9500;

  // IMPROVEMENT: SWAG on values, check docs
  const uint8_t OWMAQIMin = 0;
  // European AQI values 
  const uint8_t OWMAQIMax = 5;
  // US AQI values 
  // const uint8_t OWMAQIMax = 6;
  const uint16_t OWMPM25Min = 0;  // will be divided by 100.0 to give float
  const uint32_t OWMPM25Max = 250000; // will be divided by 100.0 to give float

  const uint8_t networkRSSIMin = 30;
  const uint8_t networkRSSIMax = 90;

  const uint16_t sensorPM2p5Min = 0;
  const uint16_t sensorPM2p5Max = 360;
#endif

// Open Weather Map parameters
#define OWM_SERVER      "http://api.openweathermap.org/data/2.5/"
#define OWM_WEATHER_PATH  "weather?"
#define OWM_AQM_PATH    "air_pollution?"

// CO2 sensor
// Define CO2 values that constitute Red (Alarm) & Yellow (Warning) values
// US NIOSH (1987) recommendations:
// 250-350 ppm - normal outdoor ambient concentrations
// 600 ppm - minimal air quality complaints
// 600-1,000 ppm - less clearly interpreted
// 1,000 ppm - indicates inadequate ventilation; complaints such as headaches, fatigue, and eye and throat irritation will be more widespread; 1,000 ppm should be used as an upper limit for indoor levels

const uint16_t co2Warning = 800; // Associated with "OK"
const uint16_t co2Alarm = 1000; // Associated with "Poor"

const String co2Labels[3]={"Good", "So-So", "Poor"};
// Subjective color scheme using 16 bit ('565') RGB colors a la ST77XX display
const uint16_t co2Color[3] = {
    0x07E0,   // GREEN = "Good"
    0xFFE0,   // YELLOW = "So-So"
    0xF800    // RED = "Poor"
  };

const uint16_t sensorCO2Min =      400;   // in ppm
const uint16_t sensorCO2Max =      2000;  // in ppm
const uint16_t sensorTempCOffset = 0;     // in Celcius

// if using OWM aqi value, these are the European standards-body conversions from numeric valeu
const String aqiEuropeanLabels[5] = { "Good", "Fair", "Moderate", "Poor", "Very Poor" };
// US standards-body conversions from numeric value
// const String aqiUSALabels[6] = {"Good", "Moderate", "Unhealthy (SG)", "Unhealthy", "Very Unhealthy", "Hazardous"};

// Time
// NTP time parameters
const String networkNTPAddress = "pool.ntp.org";
const String networkTimeZone = "PST8PDT,M3.2.0,M11.1.0"; // America/Los_Angeles
const String weekDays[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// Data endpoints
#ifdef INFLUX  
  // Specify Measurement to use with InfluxDB for sensor and device info
  const String influxEnvMeasurement = "weather";  // Used for environmental sensor data
  const String influxDevMeasurement =  "device";   // Used for logging AQI device data (e.g. battery)
#endif

// Network
// max connection attempts to network services
const uint8_t networkConnectAttemptLimit = 3;
// seconds between network service connect attempts
const uint8_t networkConnectAttemptInterval = 10;

// Hardware
// Sleep time in seconds if hardware error occurs
const uint8_t hardwareRebootInterval = 10;