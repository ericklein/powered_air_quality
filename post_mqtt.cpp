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

// only compile if MQTT enabled
#ifdef MQTT

  // Shared helper function
  extern void debugMessage(String messageText);

  // Status variables shared across various functions
  extern bool internetAvailable;

  // MQTT setup
  #include <Adafruit_MQTT.h>
  #include <Adafruit_MQTT_Client.h>
  extern Adafruit_MQTT_Client pm25_mqtt;

  void mqttConnect()
  // Connects and reconnects to MQTT broker, call as needed to maintain connection
  {
    int8_t mqttErr;
    int8_t tries;
  
    // exit if already connected
    if (pm25_mqtt.connected())
    {
      debugMessage(String("Already connected to MQTT broker ") + MQTT_BROKER);
      return;
    }
    for(tries =1; tries <= CONNECT_ATTEMPT_LIMIT; tries++)
    {
      debugMessage(String(MQTT_BROKER) + " connect attempt " + tries + " of " + CONNECT_ATTEMPT_LIMIT);
      if ((mqttErr = pm25_mqtt.connect()) == 0)
      {
        debugMessage("Connected to MQTT broker");
        return;
      }
      else
      {
        // generic MQTT error
        debugMessage(pm25_mqtt.connectErrorString(mqttErr));
  
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
        debugMessage(String("Attempt failed, trying again in ") + CONNECT_ATTEMPT_INTERVAL + " seconds");
        delay(CONNECT_ATTEMPT_INTERVAL*1000);
      }
    }
    debugMessage(String("Connection failed to MQTT broker: ") + MQTT_BROKER);
  } 

  int mqttDeviceWiFiUpdate(int rssi)
  {
    int result = 0;
    if (internetAvailable)
    {
      // Adafruit_MQTT_Publish rssiLevelPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_TOPIC3, MQTT_QOS_1); // if problematic, remove QOS parameter
      Adafruit_MQTT_Publish rssiLevelPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_TOPIC3);
      mqttConnect();

      if (rssiLevelPub.publish(rssi))
      {
        debugMessage("MQTT publish: WiFi RSSI succeeded");
        result = 1;
      }
      else
      {
        debugMessage("MQTT publish: WiFi RSSI failed");
      }
      pm25_mqtt.disconnect();
    }
    return(result);
  }
  
  int mqttSensorUpdate(float pm25, float aqi)
  // Publishes sensor data to MQTT broker
  {
    // Adafruit_MQTT_Publish tempPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_TOPIC1, MQTT_QOS_1); // if problematic, remove QOS parameter
    // Adafruit_MQTT_Publish humidityPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_TOPIC2, MQTT_QOS_1);
    Adafruit_MQTT_Publish pm25Pub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_TOPIC1);
    Adafruit_MQTT_Publish aqiPub = Adafruit_MQTT_Publish(&pm25_mqtt, MQTT_PUB_TOPIC2);
    int result = 1;
    
    mqttConnect();
    // Attempt to publish sensor data
    if(pm25Pub.publish(pm25))
    {
      debugMessage("MQTT publish: Temperature succeeded");
    }
    else {
      debugMessage("MQTT publish: Temperature failed");
      result = 0;
    }
    
    if(aqiPub.publish(aqi))
    {
      debugMessage("MQTT publish: Humidity succeeded");
    }
    else {
      debugMessage("MQTT publish: Humidity failed");
      result = 0;
    }
    pm25_mqtt.disconnect();
    return(result);
  }
#endif