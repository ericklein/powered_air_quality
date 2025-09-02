/*
  Project:      Powered Air Quality
  Description:  data pair configuration
*/

#ifndef DATA_H
#define DATA_H

// Defines value keys (strings) used for representing stored 
// sensor and device data as key/value pairs in InfluxDB and
// as parts of MQTT topics.  Must be unique, and must conform
// to syntax required for field keys by InfluxDB and MQTT.

#define VALUE_KEY_TEMPERATURE   "tempF"
#define VALUE_KEY_HUMIDITY      "humidity"
#define VALUE_KEY_CO2           "co2"
// #define VALUE_KEY_PRESSURE   "pressure"
#define VALUE_KEY_PM25          "pm25"
#define VALUE_KEY_AQI           "aqi"
#define VALUE_KEY_VOC           "vocIndex"
#define VALUE_KEY_RSSI          "rssi"
#define VALUE_KEY_NOX           "nox"

#endif  // #ifdef DATA_H