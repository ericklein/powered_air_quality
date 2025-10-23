/*
  Project Name:   Powered Air Quality
  Description:    MQTT functions for Powered Air Quality
*/

#include "Arduino.h"

#include "powered_air_quality.h"
#include "config.h"   // hardware and internet configuration parameters
#include "secrets.h"  // private credentials for network, MQTT, weather provider

// only compile if MQTT enabled
#ifdef MQTT
  #include <PubSubClient.h>
  extern PubSubClient mqtt;

  // Shared helper function
  extern void debugMessage(String messageText, uint8_t messageLevel);

  #ifdef HASSIO_MQTT
    extern void hassio_mqtt_setup();
    extern void hassio_mqtt_publish(float pm25, float temperatureF, float vocIndex, float humidity);
  #endif

  const char* generateMQTTTopic(String key)
  // Utility function to streamline dynamically generating MQTT topics using site and device 
  // parameters defined in config.h and our standard naming scheme using values set in secrets.h
  {
    String topic = endpointPath.site + "/" + endpointPath.location + "/" + endpointPath.room +
            "/" + hardwareDeviceType + "/" + endpointPath.deviceID + "/" + key;
    debugMessage(String("Generated MQTT topic: ") + topic,2);
    return(topic.c_str());
  }

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
      debugMessage(String("MQTT connection to ") + mqttBrokerConfig.host + " failed, rc=" + mqtt.state(),1);
      }
    }
    return connected;
  }

  bool mqttPublish(const char* topic, const String& payload) {
    bool success = false;

    if (mqtt.connected()) {
      if (mqtt.publish(topic, payload.c_str())) {
        success = true;
        // IMPROVEMENT: topic is not being printed?
        debugMessage(String("MQTT publish topic is ") + topic + ", message is " + payload,2);
      }
      else
        // IMPROVEMENT: topic is not being printed
        debugMessage(String("MQTT publish to topic ") + topic + " failed",1);
    }
    else {
      debugMessage("MQTT not connected during publish",1);
    }
    return success;
  }
#endif