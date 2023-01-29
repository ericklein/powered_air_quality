/*
  Project Name:   PM2.5
  Description:    Regularly sample and log PM 2.5 levels

  See README.md for target information and revision history
*/

// Step 1: Set conditional compile flags
#define DEBUG     // Output to serial port
#define WIFI        // use WiFi
#define MQTT        // log sensor data to MQTT broker
#define INFLUX      // Log data to InfluxDB server
// #define DWEET       // Log data to Dweet service
// #define THINGSPEAK  // Log data to ThingSpeak

#ifdef DEBUG
  const int SAMPLE_INTERVAL = 5;  // sample interval for sensor in seconds
  const int REPORT_INTERVAL = 1; // Interval at which samples are averaged & reported in minutes)
#else
  const int SAMPLE_INTERVAL = 30;  // sample interval for sensor in seconds
  const int REPORT_INTERVAL = 15; // Interval at which samples are averaged & reported in minutes)
#endif

const int CONNECT_ATTEMPT_LIMIT = 3;      // max connection attempts to internet services
const int CONNECT_ATTEMPT_INTERVAL = 10;  // seconds between internet service connect attempts

// set client ID; used by mqtt and wifi
#define CLIENT_ID "RCO2"

#ifdef MQTT
  // Adafruit I/O
  // structure: username/feeds/groupname.feedname or username/feeds/feedname
  // e.g. #define MQTT_PUB_TOPIC1   "sircoolio/feeds/pocket-office.temperature"

  // structure: site/room/device/data 
  #define MQTT_PUB_TOPIC1   "7828/demo/pm25/pm25"
  #define MQTT_PUB_TOPIC2   "7828/demo/pm25/aqi"
  #define MQTT_PUB_TOPIC3   "7828/demo/pm25/rssi"
#endif

#ifdef INFLUX  
  // Name of Measurements expected/used in the Influx DB.
  #define INFLUX_ENV_MEASUREMENT "weather"  // Used for environmental sensor data
  #define INFLUX_DEV_MEASUREMENT "device"   // Used for logging AQI device data (e.g. battery)
  
  // Standard set of tag values used for each sensor data point stored to InfluxDB.  Reuses
  // CLIENT_ID as defined anove here in config.h as well as device location (e.g., room in 
  // the house) and site (indoors vs. outdoors, typically).

  #define DEVICE_LOCATION "PM25-test"
  // #define DEVICE_LOCATION "PM25-demo"
  //#define DEVICE_LOCATION "kitchen"
  // #define DEVICE_LOCATION "cellar"
  // #define DEVICE_LOCATION "lab-office"
  // #define DEVICE_LOCATION "master bedroom"
  // #define DEVICE_LOCATION "pocket-office"

  #define DEVICE_SITE "indoor"
  #define DEVICE_TYPE "pm25"
#endif

// Post data to the internet via dweet.io.  Set DWEET_DEVICE to be a
// unique name you want associated with this reporting device, allowing
// data to be easily retrieved through the web or Dweet's REST API.
#ifdef DWEET
  #define DWEET_HOST "dweet.io"   // Typically dweet.io
  #define DWEET_DEVICE "makerhour-airquality"  // Must be unique across all of dweet.io
#endif