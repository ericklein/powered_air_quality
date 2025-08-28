/*
  Project Name:   Powered Air Quality
  Description:    write sensor data to MQTT broker
*/

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// Overall data and metadata naming scheme
#include "data.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"

// only compile if MQTT enabled
#ifdef MQTT
  // MQTT setup
  #include <Adafruit_MQTT.h>
  #include <Adafruit_MQTT_Client.h>
  extern Adafruit_MQTT_Client aq_mqtt;

  // Shared helper function
  extern void debugMessage(String messageText, int messageLevel);

  #ifdef HASSIO_MQTT
    extern void hassio_mqtt_setup();
    extern void hassio_mqtt_publish(float pm25, float temperatureF, float vocIndex, float humidity);
  #endif

  bool mqttConnect()
  // Connects and reconnects to MQTT broker, call as needed to maintain connection
  {
    // exit if already connected
    if (bl_mqtt.connected())
    {
      debugMessage(String("Already connected to MQTT broker ") + MQTT_BROKER,2);
      return true;
    }

    // Q: does this need to be signed?
    int8_t mqttErr;

    for(uint8_t loop = 1; loop <= networkConnectAttemptLimit; loop++)
    {
      if ((mqttErr = bl_mqtt.connect()) == 0)
      {
        debugMessage(String("Connected to MQTT broker ") + MQTT_BROKER,1);
        return true;
      }

      // report problem
      bl_mqtt.disconnect();
      debugMessage(String("MQTT connection attempt ") + loop + " of " + networkConnectAttemptLimit + " failed with error msg: " + bl_mqtt.connectErrorString(mqttErr),1);
      delay(timeNetworkConnectTimeoutMS);
    }
    // MQTT connection did not happen after multiple attempts
    return false;
  }

  // Utility function to streamline dynamically generating MQTT topics using site and device 
  // parameters defined in config.h and our standard naming scheme using values set in data.h
  String generateTopic(String key)
  {
    String topic;
    topic = DEVICE_SITE + "/" + DEVICE_LOCATION + "/" + DEVICE_ROOM +
          "/" + DEVICE + "/" + DEVICE_ID + "/" + key;
    debugMessage(String("Generated MQTT topic: ") + topic,2);
    return(topic);
  }

  bool mqttDeviceWiFiUpdate(uint32_t rssi)
  {
    bool published = false;

    if (mqttConnect()){
      String topic = generateTopic(VALUE_KEY_RSSI);  // Generate topic using secrets.h and data.h parameters 
      // add ,MQTT_QOS_1); if problematic, remove QOS parameter
      Adafruit_MQTT_Publish rssiLevelPub = Adafruit_MQTT_Publish(&aq_mqtt, topic.c_str());

      if (rssiLevelPub.publish(rssi))
      {
        debugMessage("MQTT publish: WiFi RSSI succeeded",1);
        published = true;
      }
      else
        debugMessage("MQTT publish: WiFi RSSI failed",1);
    }
    else
      debugMessage("No MQTT connection to publish with",1);
    return(published);
  }

  bool mqttSensorTemperatureFUpdate(float temperatureF)
  // Publishes temperature data to MQTT broker
  {
    bool result = false;
    String topic;
    topic = generateTopic(VALUE_KEY_TEMPERATURE);  // Generate topic using config.h and data.h parameters
    // add ,MQTT_QOS_1); if problematic, remove QOS parameter
    Adafruit_MQTT_Publish tempPub = Adafruit_MQTT_Publish(&aq_mqtt, topic.c_str());
    
    mqttConnect();

    // Attempt to publish sensor data
    if(tempPub.publish(temperatureF))
    {
      debugMessage("MQTT publish: Temperature succeeded",1);
      result = true;
    }
    else
      debugMessage("MQTT publish: Temperature failed",1);
    return(result);
  }

  bool mqttSensorHumidityUpdate(float humidity)
  // Publishes humidity data to MQTT broker
  {
    bool result = false;
    String topic;
    topic = generateTopic(VALUE_KEY_HUMIDITY);  // Generate topic using config.h and data.h parameters
    // add ,MQTT_QOS_1); if problematic, remove QOS parameter
    Adafruit_MQTT_Publish humidityPub = Adafruit_MQTT_Publish(&aq_mqtt, topic.c_str());
    
    mqttConnect();
    
    // Attempt to publish sensor data
    if(humidityPub.publish(humidity))
    {
      debugMessage("MQTT publish: Humidity succeeded",1);
      result = true;
    }
    else
      debugMessage("MQTT publish: Humidity failed",1);
    return(result);
  }

  bool mqttSensorPM25Update(float pm25)
  // Publishes pm2.5 data to MQTT broker
  {
    bool result = false;
    String topic;
    topic = generateTopic(VALUE_KEY_PM25);  // Generate topic using config.h and data.h parameters
    // add ,MQTT_QOS_1); if problematic, remove QOS parameter
    Adafruit_MQTT_Publish pm25Pub = Adafruit_MQTT_Publish(&aq_mqtt, topic.c_str());
    
    mqttConnect();
    
    // Attempt to publish sensor data
    if(pm25Pub.publish(pm25))
    {
      debugMessage("MQTT publish: pm2.5 succeeded",1);
      result = true;
    }
    else
      debugMessage("MQTT publish: pm2.5 failed",1);
    return(result);
  }

  bool mqttSensorVOCIndexUpdate(float vocIndex)
  // Publishes VoC Index data to MQTT broker
  {
    bool result = false;
    String topic;
    topic = generateTopic(VALUE_KEY_VOC);  // Generate topic using config.h and data.h parameters
    // add ,MQTT_QOS_1); if problematic, remove QOS parameter
    Adafruit_MQTT_Publish vocPub = Adafruit_MQTT_Publish(&aq_mqtt, topic.c_str());
    
    mqttConnect();
    
    // Attempt to publish sensor data
    if(vocPub.publish(vocIndex))
    {
      debugMessage("MQTT publish: VoC Index succeeded",1);
      result = true;
    }
    else
      debugMessage("MQTT publish: VoC Index failed",1);
    return(result);
  }

  bool mqttSensorCO2Update(uint16_t co2)
  // Publishes CO2 data to MQTT broker
  {
    bool result = false;
    String topic;
    topic = generateTopic(VALUE_KEY_CO2);  // Generate topic using config.h and data.h parameters
    // add ,MQTT_QOS_1); if problematic, remove QOS parameter
    Adafruit_MQTT_Publish co2Pub = Adafruit_MQTT_Publish(&aq_mqtt, topic.c_str());   
    
    mqttConnect();

    // Attempt to publish sensor data
    if(co2Pub.publish((uint32_t)co2))
    {
      debugMessage("MQTT publish: CO2 succeeded",1);
      result = true;
    }
    else
      debugMessage("MQTT publish: CO2 failed",1);
    return(result);
  } 
#endif