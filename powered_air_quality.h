/*
  Project:      Powered Air Quality
  Description:  core data structures
*/

#ifndef PAQ_H
  #define PAQ_H

  #include <Arduino.h>  // for String, uint16_t

  // device data
  struct hdweData {
    // float batteryPercent;
    // float batteryVoltage;
    // float batteryTemperatureF;
    uint8_t rssi; // WiFi RSSI value
    uint16_t altitude;
    float latitude;
    float longitude;
  };
  extern hdweData hardwareData;

  // Sensor data type indicators, e.g., used to handle different comfort zones for
  // different measurements in graphing
  #define UNK_DATA  0  // Unknown or undeclared (general) data
  #define CO2_DATA  1  // Carbon Dioxide data
  #define PM_DATA   2  // Particulate Matter (esp. PM2.5) data
  #define VOC_DATA  3  // Volatile Organic Compounds (VOC) Index data -- Sensirion specific
  #define TEMP_DATA 5  // Temperature data 
  #define HUM_DATA  6  // Humidity data

// Use bit flags to aggregate overall daily weather conditions, only used internally
#define WX_CLEAR  0b0001
#define WX_CLOUDY 0b0010
#define WX_RAINY  0b0100
#define WX_SNOWY  0b1000

// Overall forecast conditions, used to map to icons on the weather forecast screen
#define FCST_NONE     0   // Used to indicate no forcast data obtained
#define FCST_CLEAR    1
#define FCST_CLOUDY   2
#define FCST_PTCLOUDY 3
#define FCST_RAIN     4
#define FCST_SNOW     5

const String fcstName[] = {
  "None",          // FCST_NONE (0)
  "Clear",         // FCST_CLEAR (1)
  "Cloudy",        // FCST_CLOUDY (2)
  "Partly Cloudy", // FCST_PTCLOUDY (3)
  "Rain",          // FCST_RAIN (4)
  "Snow"          // FCST_SNOW (5)
};

// Map aggregate (or'ed) condition flags into forecast conditions
const uint8_t forecastMap[16] = {
  FCST_NONE,     // flags = 0, Data missing or condition not recognized
  FCST_CLEAR,    // flags = 1, Uniformly clear
  FCST_CLOUDY,   // flags = 2, Uniformly cloudy
  FCST_PTCLOUDY, // flags = 3, Combination of cloudy & clear
  FCST_RAIN,     // flags = 4, Uniformly rainy
  FCST_RAIN,     // flags = 5, Combination of rainy and clear
  FCST_RAIN,     // flags = 6, Combination of rainy and cloudy
  FCST_RAIN,     // flags = 7, Combination of rainy, cloudy, and clear
  FCST_SNOW,     // flags = 8, Uniformly snowy
  FCST_SNOW,     // flags = 9, Combination of snowy and clear
  FCST_SNOW,     // flags = 10, Combination of snowy and cloudy
  FCST_SNOW,     // flags = 11, Combination of snowy, cloudy and clear
  FCST_SNOW,     // flags = 12, Combination of snowy and rainy
  FCST_SNOW,     // flags = 13, Combination of snowy, rainy, and clear
  FCST_SNOW,     // flags = 14, Combination of snowy, rainy and cloudy
  FCST_SNOW      // flags = 15, Combination of snowy, rainy, cloudy and clear (!)
};

// Forecast data covers forty three-hour intervals, which could
// span six actual days (though we're only going to display five).
// This structure holds details for a single daily forecast
struct DailyForecast {
  float maxTempF;
  float minTempF;
  float humidity;
  uint8_t wxFcst;
  uint8_t wday;
  uint8_t count;
};

// Overall information for our 5-day forecast. Populated by OWMFetchForecast(), used
// in screens.cpp to display the forecast screen.
struct SiteForecast {
  String cityName;
  struct DailyForecast forecastData[5];
};
extern SiteForecast siteForecast;

// OpenWeatherMap Air Pollution; https://openweathermap.org/api/air-pollution
struct OpenWeatherMapAirQuality {
  // float lon;   // longitude
  // float lat;   // latitude
  uint8_t aqi;   // OWM [list][0].main.aqi : e.g. 1-5, AQI index, composite score of all components, not regionally adjusted
  // float co;    // carbon monoxide in μg/m3
  // float no;    // nitrogen oxide in μg/m3
  // float no2;   // nitrogen dioxide in μg/m3
  // float o3;    // ozone in μg/m3
  // float so2;   // sulphur dioxide in μg/m3
  float pm25;     // OWM list[0].components.pm2_5 : in μg/m3
  // float pm10;  // pm10 in μg/m3
  // float nh3;   // ammonia in μg/m3
};
extern OpenWeatherMapAirQuality owmAirQuality;

  struct MqttConfig {
      String host;
      uint16_t port;
      String user;
      String password;
    };
  extern MqttConfig mqttBrokerConfig;

  struct influxConfig {
    String host;
    uint16_t port;
    String org;
    String bucket;
    String envMeasurement;
    String devMeasurement;
    };
  extern influxConfig influxdbConfig;

  struct networkEndpointConfig {
    String site;
    String location;
    String room;
    String deviceID;
  };
  extern networkEndpointConfig endpointPath;
#endif