/*
  Project Name:   Climatron - Your personal air quality monitoring robot
  Description:    MQTT endpoint support functions
*/

#include "Arduino.h"

#include "config.h"               // hardware and internet configuration parameters
#include "powered_air_quality.h"  // overall header info for Powered Air Quality
#include "secrets.h"              // private credentials for network, MQTT, weather provider

// only compile if MQTT enabled
#ifdef MQTT
  #include <PubSubClient.h>
  extern PubSubClient mqtt;

  // Shared helper function
  extern void debugMessage(String messageText, uint8_t messageLevel);

  #ifdef HASSIO_MQTT
    extern bool hassio_mqtt_publish(float pm25, float co2, float temperatureF, float humidity, float vocIndex, float noxIndex, float aqi);
  #endif

  bool mqttConnect() {
    bool connected = false;

    if (mqttBrokerConfig.host.isEmpty() || mqttBrokerConfig.port == 0) {
      debugMessage("No MQTT host configured",1);
    }
    else {
      mqtt.setServer(mqttBrokerConfig.host.c_str(), mqttBrokerConfig.port);
      if (mqttBrokerConfig.user.length() > 0) {
        connected = mqtt.connect(endpointPath.deviceID.c_str(), mqttBrokerConfig.user.c_str(), mqttBrokerConfig.password.c_str());
      }
      else {
        connected = mqtt.connect(endpointPath.deviceID.c_str());
      }
      if (connected) {
        debugMessage(String("Connected to MQTT broker ") + mqttBrokerConfig.host,1);
      } 
      else {
      debugMessage(String("MQTT connection to ") + mqttBrokerConfig.host + String(":") + mqttBrokerConfig.port + " failed, rc=" + mqtt.state(),1);
      debugMessage(String("MQTT user: ") + mqttBrokerConfig.user.c_str() + String(", password: ") + mqttBrokerConfig.password.c_str(),1);
      }
    }
    return connected;
  }

  // Publish a value to MQTT, synthesizing the relevant topic based on device and site
  // configuration data.  The KEY passed in should correspond to the value being published,
  // and is defined in data.h.
  bool mqttPublishValue(String key, const String& payload) {
      bool success = false;

    // Generate the MQTT topic from site configuration data and the provided value-specific key
    String topic = endpointPath.site + "/" + endpointPath.location + "/" + endpointPath.room +
      "/" + hardwareDeviceType + "/" + endpointPath.deviceID + "/" + key;

    // Confirm we're connected to the MQTT broker, and if so publish to the generated topic
    if (mqtt.connected()) {
      if (mqtt.publish(topic.c_str(), payload.c_str())) {
        success = true;
        debugMessage(String("MQTT publish topic is ") + topic + ", message is " + payload,2);
      }
      else
        debugMessage(String("MQTT publish to topic ") + topic + " failed",1);
    }
    else {
      debugMessage("MQTT not connected during publish",1);
    }
    return success;
  }
#endif