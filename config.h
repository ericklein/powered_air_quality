/*
  Project:      Powered Air Quality
  Description:  public (non-secret) configuration data
*/

// Configuration Step 1: Set debug message output
// comment out to turn off; 1 = summary, 2 = verbose
#define DEBUG 2

// Configuration Step 2: simulate hardware inputs, returning random but plausible values
// comment out to turn off
// #define SENSOR_SIMULATE

// Configuration Step 3: Set network data endpoints
// #define MQTT     // log sensor data to MQTT broker
// #define HASSIO_MQTT  // And, if MQTT enabled, with Home Assistant too?
// #define INFLUX // Log data to InfluxDB server

// Configuration variables that are less likely to require changes

// Buttons
// const uint8_t buttonD1Pin = 1; // initially LOW
// const int buttonDebounceDelay = 50; // time in milliseconds to debounce button

// Display
const uint8_t screenRotation = 3; // rotation 3 orients 0,0 next to D0 button
const uint8_t screenCount = 5;

#define TFT_D0        34 // Data bit 0 pin (MUST be on PORT byte boundary)
#define TFT_WR        26 // Write-strobe pin (CCL-inverted timer output)
#define TFT_DC        10 // Data/command pin
#define TFT_CS        11 // Chip-select pin
#define TFT_RST       24 // Reset pin
#define TFT_RD         9 // Read-strobe pin
#define TFT_BACKLIGHT 25

// screen layout assists in pixels
const uint16_t xMargins = 5;
const uint16_t yMargins = 2;
const uint16_t wifiBarWidth = 3;
const uint16_t wifiBarHeightIncrement = 3;
const uint16_t wifiBarSpacing = 5;

#ifdef DEBUG
  // time between samples in seconds
  const uint16_t sensorSampleInterval = 30;
  const uint8_t sensorReportInterval = 1; // Interval at which samples are averaged & reported in minutes)
#else
  const uint16_t sensorSampleInterval = 60;
  const uint8_t sensorReportInterval = 15; // Interval at which samples are averaged & reported in minutes)
#endif

// Simulation values
#ifdef SENSOR_SIMULATE
  const uint16_t sensorTempMin =      1500; // will be divided by 100.0 to give floats
  const uint16_t sensorTempMax =      2500;
  const uint16_t sensorHumidityMin =  500; // will be divided by 100.0 to give floats
  const uint16_t sensorHumidityMax =  9500;

  //sensorData.massConcentrationPm1p0 = random(0, 360) / 10.0;
  const uint16_t sensorPM2p5Min = 0;
  const uint16_t sensorPM2p5Max = 360;

  sensorData.massConcentrationPm2p5 = random(0, 360) / 10.0;
  //sensorData.massConcentrationPm4p0 = random(0, 720) / 10.0;
  //sensorData.massConcentrationPm10p0 = random(0, 1550) / 10.0;
  // testing range is 5 to 95
  sensorData.ambientHumidity = 5 + (random(0, 900) / 10.0);
  // keep this value in C, not F. Converted after sensorPM25Read()
  // testing range is 15 to 25
  sensorData.ambientTemperatureF = 15.0 + (random(0, 101) / 10.0);
  sensorData.vocIndex = random(0, 500) / 10.0;
  sensorData.noxIndex = random(0, 2500) / 10.0; 
#endif

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

const uint16_t sensorCO2Min =      400;
const uint16_t sensorCO2Max =      2000;
const uint16_t sensorTempCOffset = 0; // in C

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