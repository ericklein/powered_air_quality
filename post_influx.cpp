/*
  Project:      Powered Air Quality
  Description:  write PM2.5 sensor data to InfluxDB

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

  #include <InfluxDbClient.h>

  // InfluxDB setup.  See config.h and secrets.h for site-specific settings.  Both InfluxDB v1.X
  // and v2.X are supported here depending on configuration settings in secrets.h.  Code here
  // reflects a number of presumptions about the data schema and InfluxDB configuration:
  //

  #ifdef INFLUX_V1
  // InfluxDB client instance for InfluxDB 1
  InfluxDBClient dbclient(INFLUXDB_URL, INFLUXDB_DB_NAME);
  #endif

  #ifdef INFLUX_V2
  // InfluxDB client instance for InfluxDB 2
  InfluxDBClient dbclient(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
  #endif


  // Post data to Influx DB using the connection established during setup
  boolean post_influx(float pm25, float temperatureF, float vocIndex, float humidity, uint16_t co2, uint8_t rssi)
  {
    bool result = false;

    // InfluxDB Data point, binds to InfluxDB 'measurement' to use for data. See config.h for value used
    Point dbenvdata(influxEnvMeasurement);
    Point dbdevdata(influxDevMeasurement);

    if (rssi!=0)
    {
      #ifdef INFLUX_V1
        // Set InfluxDB v1.X authentication params using values defined in secrets.h.  Not needed as such
        // for InfluxDB v2.X (which uses a token-based scheme via the constructor).
        dbclient.setConnectionParamsV1(INFLUXDB_URL, INFLUXDB_DB_NAME, INFLUXDB_USER, INFLUXDB_PASSWORD);
      #endif
      
      // Add constant Influx data point tags - only do once, will be added to all individual data points
      // Modify if required to reflect your InfluxDB data model (and set values in config.h)
      // First for environmental data
      dbenvdata.addTag(TAG_KEY_DEVICE, DEVICE);
      dbenvdata.addTag(TAG_KEY_SITE, DEVICE_SITE);
      dbenvdata.addTag(TAG_KEY_LOCATION, DEVICE_LOCATION);
      dbenvdata.addTag(TAG_KEY_ROOM, DEVICE_ROOM);
      // DEVICE_ID not implemented yet
      // dbenvdata.addTag(TAG_KEY_DEVICE_ID, DEVICE_ID);
      // And again for device data
      dbdevdata.addTag(TAG_KEY_DEVICE, DEVICE);
      dbdevdata.addTag(TAG_KEY_SITE, DEVICE_SITE);
      dbdevdata.addTag(TAG_KEY_LOCATION, DEVICE_LOCATION);
      dbdevdata.addTag(TAG_KEY_ROOM, DEVICE_ROOM);

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
        // Write point via connection to InfluxDB host
        if (!dbclient.writePoint(dbenvdata)) {
          debugMessage(String("InfluxDB write failed: ") + dbclient.getLastErrorMessage(),1);
          result = false;
        }
        else
        {
          debugMessage(String("InfluxDB write success: ") + dbclient.pointToLineProtocol(dbenvdata),1);
        }

        // Now store device information 
        dbdevdata.clearFields();
        // Report device readings
        dbdevdata.addField(VALUE_KEY_RSSI, rssi);
        // Write point via connection to InfluxDB host
        if (!dbclient.writePoint(dbdevdata))
        {
          debugMessage(String("InfluxDB write failed: ") + dbclient.getLastErrorMessage(),1);
          result = false;
        }
        else
        {
          debugMessage(String("InfluxDB write success: ") + dbclient.pointToLineProtocol(dbdevdata),1);
        }
      dbclient.flushBuffer();  // Clear pending writes
      }
    }
    return(result);
  }
#endif