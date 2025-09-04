/*
  Project:      Powered Air Quality
  Description:  write sensor data to InfluxDB

  See README.md for target information and revision history
*/

#include "Arduino.h"

#include "powered_air_quality.h"
#include "config.h"   // hardware and internet configuration parameters
#include "secrets.h"  // private credentials for network, MQTT, weather provider
#include "data.h"     // Overall data and metadata naming scheme

// Only compile if InfluxDB enabled
#ifdef INFLUX
  #include <InfluxDbClient.h>

  // Shared helper function
  extern void debugMessage(String messageText, int messageLevel);

  // InfluxDB 2.x setup.  See config.h and secrets.h for site-specific settings. Code
  // reflects a number of presumptions about the data schema and InfluxDB configuration:

  // InfluxDB client instance
  String influxURL = influxdbConfig.host + ":" + String(influxdbConfig.port);
  InfluxDBClient dbclient(influxURL, influxdbConfig.org, influxdbConfig.bucket, influxKey);

  // Post data to Influx DB using the connection established during setup
  boolean post_influx(float temperatureF, float humidity, uint16_t co2, float pm25, float vocIndex, float noxIndex, uint8_t rssi)
  {
    bool result = false;

    // InfluxDB Data point, binds to InfluxDB 'measurement' to use for data. See config.h for value used
    Point dbenvdata(influxdbConfig.envMeasurement);
    Point dbdevdata(influxdbConfig.devMeasurement);
    
    // Add constant Influx data point tags - only do once, will be added to all individual data points
    // Modify if required to reflect your InfluxDB data model (and set values in config.h)
    // First for environmental data
    dbenvdata.addTag(TAG_KEY_DEVICE, hardwareDeviceType);
    dbenvdata.addTag(TAG_KEY_SITE, endpointPath.site);
    dbenvdata.addTag(TAG_KEY_LOCATION, endpointPath.location);
    dbenvdata.addTag(TAG_KEY_ROOM, endpointPath.room);
    dbenvdata.addTag(TAG_KEY_DEVICE_ID, endpointPath.deviceID);
    // And again for device data
    dbdevdata.addTag(TAG_KEY_DEVICE, hardwareDeviceType);
    dbdevdata.addTag(TAG_KEY_SITE, endpointPath.site);
    dbdevdata.addTag(TAG_KEY_LOCATION, endpointPath.location);
    dbdevdata.addTag(TAG_KEY_ROOM, endpointPath.room);
    dbdevdata.addTag(TAG_KEY_DEVICE_ID, endpointPath.deviceID);

    // Attempts influxDB connection, and if unsuccessful, re-attempts after networkConnectAttemptInterval second delay for networkConnectAttempLimit times
    for (int tries = 1; tries <= networkConnectAttemptLimit; tries++) {
      if (dbclient.validateConnection()) {
        debugMessage(String("Connected to InfluxDB: ") + dbclient.getServerUrl(),1);
        result = true;
        break;
      }
      debugMessage(String("influxDB connection attempt ") + tries + " of " + networkConnectAttemptLimit + " failed with error msg: " + dbclient.getLastErrorMessage(),1);
      delay(networkConnectAttemptInterval*1000);
    }
    if (result)
    {
      // Connected, so store sensor values into timeseries data points
      dbenvdata.clearFields();
      // Report sensor readings
      dbenvdata.addField(VALUE_KEY_PM25, pm25);
      dbenvdata.addField(VALUE_KEY_TEMPERATURE, temperatureF);
      dbenvdata.addField(VALUE_KEY_HUMIDITY, humidity);
      dbenvdata.addField(VALUE_KEY_VOC, vocIndex);
      dbenvdata.addField(VALUE_KEY_CO2, co2);
      dbdevdata.addField(VALUE_KEY_NOX, noxIndex);
      // Write point via connection to InfluxDB host
      if (!dbclient.writePoint(dbenvdata)) {
        debugMessage(String("InfluxDB environment data write failed: ") + dbclient.getLastErrorMessage(),1);
        result = false;
      }
      else
      {
        debugMessage(String("InfluxDB environment data write success: ") + dbclient.pointToLineProtocol(dbenvdata),1);
      }

      // Now store device information 
      dbdevdata.clearFields();
      // Report device readings
      dbdevdata.addField(VALUE_KEY_RSSI, rssi);
      // Write point via connection to InfluxDB host
      if (!dbclient.writePoint(dbdevdata))
      {
        debugMessage(String("InfluxDB device data write failed: ") + dbclient.getLastErrorMessage(),1);
        result = false;
      }
      else
      {
        debugMessage(String("InfluxDB device data write success: ") + dbclient.pointToLineProtocol(dbdevdata),1);
      }
    dbclient.flushBuffer();  // Clear pending writes
    }
    return(result);
  }
#endif