#ifndef PAQ_H
  #define PAQ_H

  #include <Arduino.h>  // for String, uint16_t

  // environment sensor data
  struct envData {
    float ambientTemperatureF;        // range -10C to 60C
    float ambientHumidity;            // RH [%], range 0 to 100
    int16_t  ambientCO2[graphPoints]; // ppm, range 400 to 2000, FIFO queue, -1 = no data, errant values ~ first 7 seconds (SCD4x, SEN6x)
    float pm25;                       // PM2.5 [µg/m³], (SEN54 -> range 0 to 1000, NAN if unknown)
    float pm1;                        // PM1.0 [µg/m³], (SEN54 -> range 0 to 1000, NAN if unknown)
    float pm10;                       // PM10.0 [µg/m³], (SEN54 -> range 0 to 1000, NAN if unknown)
    float pm4;                        // PM4.0 [µg/m³], range 0 to 1000, NAN if unknown
    float vocIndex[graphPoints];      // Sensiron VOC Index, range 0 to 500 (SEN54, SEN66)
    float noxIndex;                   // Sensiron NOx Index, range 0 to 500, NAN for first ~11 seconds with SEN66, always NAN for SEN54
  };
  extern envData sensorData;

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

  // OpenWeatherMap Current data
  struct OpenWeatherMapCurrentData {
    // float lon;              // "lon": 8.54
    // float lat;              // "lat": 47.37
    // uint16_t weatherId;     // "id": 521
    // String main;            // "main": "Rain"
    // String description;     // "description": "shower rain"
    String icon;               // "icon": "09d"
    float tempF;                // "temp": 90.56, in F (API request for imperial units)
    // uint16_t pressure;      // "pressure": 1013, in hPa
    uint16_t humidity;         // "humidity": 87, in RH%
    // float tempMin;          // "temp_min": 89.15
    // float tempMax;          // "temp_max": 92.15
    // uint16_t visibility;    // visibility: 10000, in meters
    // float windSpeed;        // "wind": {"speed": 1.5}, in meters/s
    // float windDeg;          // "wind": {deg: 226.505}
    // uint8_t clouds;         // "clouds": {"all": 90}, in %
    // time_t observationTime; // "dt": 1527015000, in UTC
    // String country;         // "country": "CH"
    // time_t sunrise;         // "sunrise": 1526960448, in UTC
    // time_t sunset;          // "sunset": 1527015901, in UTC
    String cityName;           // "name": "Zurich"
    // time_t timezone;        // shift in seconds from UTC
  };
  extern OpenWeatherMapCurrentData owmCurrentData;

  // OpenWeatherMap Air Quality data
  struct OpenWeatherMapAirQuality {
    // float lon;   // "lon": 8.54
    // float lat;   // "lat": 47.37
    uint16_t aqi;   // "aqi": 2
    // float co;    // "co": 453.95, in μg/m3
    // float no;    // "no": 0.47, in μg/m3
    // float no2;   // "no2": 52.09, in μg/m3
    // float o3;    // "o3": 17.17, in μg/m3
    // float so2;   // "so2": 7.51, in μg/m3
    float pm25;     // "pm2.5": 8.04, in μg/m3
    // float pm10;  // "pm10": 9.96, in μg/m3
    // float nh3;   // "nh3": 0.86, in μg/m3
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