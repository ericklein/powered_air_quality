/*
  Project:      AQ_powered
  Description:  public (non-secret) configuration data
*/

// Configuration Step 1: Set debug message output
// comment out to turn off; 1 = summary, 2 = verbose
// #define DEBUG 1

// simulate hardware inputs, returning random but plausible values
// comment out to turn off
// #define SENSOR_SIMULATE

#ifdef SENSOR_SIMULATE
  const uint16_t sensorTempMin =      1500; // will be divided by 100.0 to give floats
  const uint16_t sensorTempMax =      2500;
  const uint16_t sensorHumidityMin =  500; // will be divided by 100.0 to give floats
  const uint16_t sensorHumidityMax =  9500;
  const uint16_t sensorCO2Min =       400;
  const uint16_t sensorCO2Max =       3000;

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

// Configuration Step 2: Set network data endpoints
// #define MQTT     // log sensor data to MQTT broker
// #define HASSIO_MQTT  // And, if MQTT enabled, with Home Assistant too?
#define INFLUX // Log data to InfluxDB server

// Configuration Step 3: Select environment sensor read intervals

// Sensor(s) are sampled at one interval, generally fairly often.  Readings
// are averaged and reported at a longer interval.  Configure that behavior here,
// allowing for more frequent processing when in DEBUG mode.
#ifdef DEBUG
  #define SAMPLE_INTERVAL 30  // sensor sample interval in seconds
#else
  #define SAMPLE_INTERVAL 300
#endif

// Configuration Step 4: Set screen parameters, if desired
#define  SCREEN    // use screen as output

// Pin config for display
#ifdef SCREEN
  #define TFT_D0        34 // Data bit 0 pin (MUST be on PORT byte boundary)
  #define TFT_WR        26 // Write-strobe pin (CCL-inverted timer output)
  #define TFT_DC        10 // Data/command pin
  #define TFT_CS        11 // Chip-select pin
  #define TFT_RST       24 // Reset pin
  #define TFT_RD         9 // Read-strobe pin
  #define TFT_BACKLIGHT 25

  // rotation 1 orients the display so the pins are at the bottom of the display
  // rotation 2 orients the display so the pins are at the top of the display
  // rotation of 3 flips it so the wiring is on the left side of the display
  #define DISPLAY_ROTATION 3

  // screen layout assists in pixels
  const int xMargins = 5;
  const int yMargins = 2;
  const int wifiBarWidth = 3;
  const int wifiBarHeightIncrement = 3;
  const int wifiBarSpacing = 5;
  const int yTemperature = 23;
  const int yLegend = 95;
  const int legendHeight = 10;
  const int legendWidth = 5; 
#endif

#ifdef INFLUX  
  // Name of Measurements expected/used in the Influx DB.
  #define INFLUX_ENV_MEASUREMENT "weather"  // Used for environmental sensor data
  #define INFLUX_DEV_MEASUREMENT "device"   // Used for logging device data (e.g. battery)
#endif

// Post data to the internet via dweet.io.  Set DWEET_DEVICE to be a
// unique name you want associated with this reporting device, allowing
// data to be easily retrieved through the web or Dweet's REST API.
#ifdef DWEET
  #define DWEET_HOST "dweet.io"   // Typically dweet.io
  #define DWEET_DEVICE "makerhour-pm25"  // Must be unique across all of dweet.io
#endif

// Configuration variables that are less likely to require changes

// To allow for varying network singal strength and responsiveness, make multiple
// attempts to connect to internet services at a measured interval.  If your environment
// is more challenged you may want to allow for more connection attempts and/or a longer
// delay between connection attempts.
#define CONNECT_ATTEMPT_LIMIT 3     // max connection attempts to internet services
#define CONNECT_ATTEMPT_INTERVAL 10 // seconds between internet service connect attempts

// Sleep time in seconds if hardware error occurs
#define HARDWARE_ERROR_INTERVAL 10