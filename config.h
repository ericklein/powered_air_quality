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

// Open Weather Map (OWM)
const String OWMServer = "http://api.openweathermap.org/data/2.5/";
const String OWMWeatherPath =  "weather?";
const String OWMAQMPath = "air_pollution?";
// OWM Air Pollution scale from https://openweathermap.org/api/air-pollution
const String OWMPollutionLabel[5] = {"Good", "Fair", "Moderate", "Poor", "Very Poor"};

// UI
enum screenNames {sSaver, sMain, sCO2, sPM25, sVOC, sNOX};

// screen layout assists in pixels
const uint8_t xMargins = 5;
const uint8_t yMargins = 5;
const uint8_t cornerRoundRadius = 4;
const uint8_t wifiBarWidth = 3;
const uint8_t wifiBarHeightIncrement = 3;
const uint8_t wifiBarSpacing = 5;
const uint8_t legendHeight =  20;
const uint8_t legendWidth =   10;

const uint8_t screenRotation = 3; // CYD 2.8; horizontal orientation with USB port on left side
// const uint8_t screenRotation = 1; // CYD 3.2; horizontal orientation with USB port on right side

// How many data samples are retained for graphing
const uint8_t graphPoints = 10;

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
const uint8_t OWMAQIMin = 1;  // https://openweathermap.org/api/air-pollution
const uint8_t OWMAQIMax = 5;

const uint16_t OWMPM25Min = 0;
const uint16_t OWMPM25Max = 100; 

const uint8_t networkRSSIMin = 30; // ESP32 abs(WiFi.RSSI()) spec
const uint8_t networkRSSIMax = 100;

// tempF value threshholds
const uint16_t sensorTempFMin =       14; // -10C per SCD40, SEN66 datasheet
const uint8_t sensorTempFComfortMin = 65;
const uint8_t sensorTempFComfortMax = 80;
#ifdef SCD40
  const uint16_t sensorTempFMax =       140; // 60C per SCD4X datasheet
#else
  const uint16_t sensorTempFMax =       122; // 50C per SEN66 datasheet
#endif

// humidity value thresholds
const uint16_t sensorHumidityMin =    0; // RH% per datasheet
const uint8_t sensorHumidityComfortMin = 40;
const uint8_t sensorHumidityComfortMax = 60;
const uint16_t sensorHumidityMax =    100;

// CO2 value thresholds
const uint16_t sensorCO2Min =   400;   // in ppm
const uint16_t sensorCO2Fair =  800;
const uint16_t sensorCO2Poor =  1200;
const uint16_t sensorCO2Bad =   1600;
#ifdef SCD40
  const uint16_t sensorCO2Max =   2000; // SCD4x raw up to 40000
#else 
  const uint16_t sensorCO2Max =   5000; // SEN6x raw up to 40000
#endif
const uint8_t co2SensorReadFailureLimit = 20;
const uint8_t sensorCO2VariabilityRange = 30;

// Particulates (pm1, pm2.5, pm4, pm10) value thresholds
const uint16_t sensorPMMin =  0;  // per datasheet
const uint16_t sensorPMFair = 25;
const uint16_t sensorPMPoor = 50;
const uint16_t sensorPMBad =  150;
const uint16_t sensorPMMax =  1000; // per SEN54, SEN66 datasheet

// VOC (volatile organic compounds) index value thresholds
const uint16_t  sensorVOCMin =  0;    // per SEN54, SEN66 datasheet
const uint16_t  sensorVOCFair = 150;
const uint16_t  sensorVOCPoor = 250;
const uint16_t  sensorVOCBad =  400;
const uint16_t  sensorVOCMax =  500;  // per SEN54, SEN66 datasheet

// NOx (nitrogen oxide) index value thresholds, Sensiron Info_Note_NOx_Index.pdf
const uint16_t sensorNOxMin =   0;    // per SEN66 datasheet
const uint16_t sensorNOxFair =  49;
const uint16_t sensorNOxPoor =  150;
const uint16_t sensorNOxBad =   300;
const uint16_t sensorNOxMax =   500;  // per SEN66 datasheet

// timers
// Internet and network endpoints
const uint32_t timeNetworkTimeoutSeconds = 10; // how long to attempt network connects before failing
const uint32_t timeNetworkKeepAliveMS = 30000;
const uint32_t timeOWMRenewMS = 1800000; // min time between OWM calls

const uint32_t timeHardwareSleepTimeμS = 10000000;  // sleep time if hardware error occurs
// button
const uint16_t timeStartPortalHoldMS = 5000;  // long-press duration to start config portal
const uint16_t timeDeviceResetHoldMS = 10000; // Long-press duration to wipe config

const uint32_t timeScreenSaverStartMS = 300000; // switch to screen saver if no input after this period

// sampling and reporting intervals
#ifdef DEBUG
  const uint32_t timeSensorSampleMS = 30000;  // time between samples
  const uint32_t timeReportMS = 90000;        // time between reports
#else
  const uint32_t timeSensorSampleMS = 60000;
  const uint32_t timeReportMS = 900000;
#endif
const uint8_t reportFailureThreshold = 3; // report attempt failures before UI alert starts

// touchscreen calibration
const uint16_t touchscreenMinX = 200;
const uint16_t touchscreenMaxX = 3700;
const uint16_t touchscreenMinY = 240;
const uint16_t touchscreenMaxY = 3800;

// CYD variations
// CYD ESP32-2432S028R (2.8" TFT, micro-USB)
// static constexpr uint8_t pinButton = 0; // boot button on most ESP32 boards
// // static constexpr uint8_t pinSensorSDA = 22;
// // static constexpr uint8_t pinSensorSCL = 27;
// static constexpr uint8_t pinTouchIRQ = 36;
// static constexpr uint8_t pinTouchMOSI = 32;
// static constexpr uint8_t pinTouchMISO = 39;
// static constexpr uint8_t pinTouchCLK = 25;
// static constexpr uint8_t pinTouchCS = 33;
// static constexpr uint8_t ledStrip1DataPin = 4;
// static constexpr uint8_t ledStripPixelCount = 3; // number of LEDs on each strip
// static constexpr uint8_t pinAudio = 26;
// static constexpr uint32_t PWM_FREQ = 5000; // Hz
// static constexpr uint8_t  PWM_BITS = 8;    // 0..255
// static constexpr uint16_t MAX_DUTY = (1u << PWM_BITS) - 1u;

// CYD Freenode FNK0103L_3P2 (3.2" TFT, usb-c)
// static constexpr uint8_t pinSensorSDA = 32;
// static constexpr uint8_t pinSensorSCL = 25;

// JC2432W328
static constexpr uint8_t pinButton = 0; // boot button on most ESP32 boards
static constexpr uint8_t pinSensorSDA = 22;
static constexpr uint8_t pinSensorSCL = 21;
static constexpr uint8_t pinTouchSDA = 33;
static constexpr uint8_t pinTouchSCL = 32;
static constexpr uint8_t pinTouchRST = 25;
static constexpr uint8_t pinTouchIRQ = -1;
static constexpr uint8_t ledStrip1DataPin = 4;
static constexpr uint8_t ledStripPixelCount = 3; // number of LEDs on each strip
static constexpr uint8_t pinAudio = 26;
static constexpr uint32_t PWM_FREQ = 5000; // Hz
static constexpr uint8_t  PWM_BITS = 8;    // 0..255
static constexpr uint16_t MAX_DUTY = (1u << PWM_BITS) - 1u;