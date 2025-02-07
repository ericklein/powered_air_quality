/*
  Project:      Powered Air Quality
  Description:  Post PAQ sensor data and device info to InfluxDB

  See README.md for target information and revision history
*/

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// Overall data and metadata naming scheme
#include "data.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"

// Only compile if InfluxDB enabled
#ifdef INFLUX

  // Shared helper function
  extern void debugMessage(String messageText, int messageLevel);

  #include "InfluxClient.h"

  // InfluxDB setup.  See config.h and secrets.h for site-specific settings.  Both InfluxDB v1.X
  // and v2.X are supported here depending on configuration settings in secrets.h.  Code here
  // reflects a number of presumptions about the data schema and InfluxDB configuration:
  //

  #ifdef INFLUX_V1
    #error "InfluxDB Version 1 not supported!"
  #endif

  #ifdef INFLUX_V2
  // InfluxDB client instance for InfluxDB 2
  InfluxClient dbclient(INFLUXDB_HOST, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
  #endif


  // Post data to Influx DB using HTTP
  boolean post_influx(float pm25, float temperatureF, float vocIndex, float humidity, uint16_t co2, uint8_t rssi)
  {
    bool result = false;

    // InfluxDB Data point, binds to InfluxDB 'measurement' to use for data. See config.h for value used
    InfluxPoint dbenvdata(influxEnvMeasurement);
    // Point dbdevdata(influxDevMeasurement);

    if (rssi!=0)
    {      
      // Add constant Influx data point tags
      // Modify if required to reflect your InfluxDB data model (and set values in config.h)
      // Environmental (sesnsor) data
      dbenvdata.addTag(TAG_KEY_DEVICE, DEVICE);
      dbenvdata.addTag(TAG_KEY_SITE, DEVICE_SITE);
      dbenvdata.addTag(TAG_KEY_LOCATION, DEVICE_LOCATION);
      dbenvdata.addTag(TAG_KEY_ROOM, DEVICE_ROOM);
      // DEVICE_ID not implemented yet
      // dbenvdata.addTag(TAG_KEY_DEVICE_ID, DEVICE_ID);

      // Store sensor values into timeseries data points
      dbenvdata.clearFields();
      // Report sensor readings
      dbenvdata.addField(VALUE_KEY_PM25, String(pm25));
      dbenvdata.addField(VALUE_KEY_TEMPERATURE, String(temperatureF));
      dbenvdata.addField(VALUE_KEY_HUMIDITY, String(humidity));
      dbenvdata.addField(VALUE_KEY_VOC, String(vocIndex));
      dbenvdata.addField(VALUE_KEY_CO2, String(co2));

      // Write point via connection to InfluxDB host
      dbclient.writePoint(dbenvdata);
      result = true;
    }
    return(result);
  }
#endif