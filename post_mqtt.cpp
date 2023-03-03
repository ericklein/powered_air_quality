/*
  Project Name:   PM2.5
  Description:    write PM2.5 sensor data to InfluxDB

  See README.md for target information and revision history
*/

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"

// Shared helper function
extern void debugMessage(String messageText);

// only compile if MQTT enabled
#ifdef MQTT

  // MQTT setup
  #include <Adafruit_MQTT.h>
  #include <Adafruit_MQTT_Client.h>
  extern Adafruit_MQTT_Client pm25_mqtt;

  void mqttConnect()
  // Connects and reconnects to MQTT broker, call as needed to maintain connection
  {  
    // exit if already connected
    if (pm25_mqtt.connected())
    {
      debugMessage(String("Already connected to MQTT broker ") + MQTT_BROKER);
      return;
    }
    
    int8_t mqttErr;

    // Attempts MQTT connection, and if unsuccessful, re-attempts after CONNECT_ATTEMPT_INTERVAL second delay for CONNECT_ATTEMPT_LIMIT times
    for(int tries = 1; tries <= CONNECT_ATTEMPT_LIMIT; tries++)
    {
      if ((mqttErr = pm25_mqtt.connect()) == 0)
      {
        debugMessage(String("Connected to MQTT broker ") + MQTT_BROKER);
        return;
      }
      // Adafruit IO connect errors
      // switch (mqttErr)
      // {
      //   case 1: debugMessage("Adafruit MQTT: Wrong protocol"); break;
      //   case 2: debugMessage("Adafruit MQTT: ID rejected"); break;
      //   case 3: debugMessage("Adafruit MQTT: Server unavailable"); break;
      //   case 4: debugMessage("Adafruit MQTT: Incorrect user or password"); break;
      //   case 5: debugMessage("Adafruit MQTT: Not authorized"); break;
      //   case 6: debugMessage("Adafruit MQTT: Failed to subscribe"); break;
      //   default: debugMessage("Adafruit MQTT: GENERIC - Connection failed"); break;
      // }
      pm25_mqtt.disconnect();
      debugMessage(String("MQTT connection attempt ") + tries + " of " + CONNECT_ATTEMPT_LIMIT + " failed with error msg: " + pm25_mqtt.connectErrorString(mqttErr));
      delay(CONNECT_ATTEMPT_INTERVAL*1000);
    }
  } 

  bool mqttDeviceWiFiUpdate(int rssi)
  {
    bool result = false;
    if (rssi!=0)
    {
      // Adafruit_MQTT_Publish rssiLevelPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_RSSI, MQTT_QOS_1); // if problematic, remove QOS parameter
      Adafruit_MQTT_Publish rssiLevelPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_RSSI);

      mqttConnect();

      if (rssiLevelPub.publish(rssi))
      {
        debugMessage("MQTT publish: WiFi RSSI succeeded");
        result = true;
      }
      else
      {
        debugMessage("MQTT publish: WiFi RSSI failed");
      }
    }
    return(result);
  }
  
  bool mqttSensorPM25Update(float pm25)
  // Publishes sensor data to MQTT broker
  {
    bool result = false;
    //Adafruit_MQTT_Publish pm25Pub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_PM25, MQTT_QOS_1); // if problematic, remove QOS parameter
    Adafruit_MQTT_Publish pm25Pub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_PM25);

    mqttConnect();

    if(pm25Pub.publish(pm25))
    {
      debugMessage("MQTT publish: PM2.5 succeeded");
      result = true;
    }
    else
    {
      debugMessage("MQTT publish: PM2.5 failed");
    }
    return(result);
  }

    bool mqttSensorAQIUpdate(float aqi)
  // Publishes sensor data to MQTT broker
  {
    bool result = false;
    //Adafruit_MQTT_Publish aqiPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_AQIMQTT_QOS_1); // if problematic, remove QOS parameter
    Adafruit_MQTT_Publish aqiPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_AQI);

    mqttConnect();
    
    if(aqiPub.publish(aqi))
    {
      debugMessage("MQTT publish: AQI succeeded");
      result = true;
    }
    else
    {
      debugMessage("MQTT publish: AQI failed");
    }
    return(result);
  }
#endif