/*
  Project:      Powered Air Quality
  Description:  write sensor data to hassio_mqtt
*/

/*
 * Additional routines for use in an MQTT-enabled environment with Home Assistant, allowing
 * sensor readings to be reported to Home Assistant.   
 * 
 * Requires the following addition to configuration.yaml for Home Assistant, with the
 * state topic declared to match MQTT_HASSIO_STATE as defined in config.h. Also, Unique IDs, 
 * which enable additional UI customization features for the sensors in Home Assistant,
 * can be generated here: https://www.uuidgenerator.net/version1.

   mqtt:
    sensor:
     - name: "Temperature"
      device_class: "temperature"
      state_topic: "homeassistant/sensor/aqi-1/state"
      unit_of_measurement: "°F"
      unique_id: "-- GENERATE A UUID TO USE HERE --"
      value_template: "{{ value_json.temperature }}"
    - name: "Humidity"
      device_class: "humidity"
      state_topic: "homeassistant/sensor/aqi-1/state"
      unit_of_measurement: "%"
      unique_id: "-- GENERATE A UUID TO USE HERE --"
      value_template: "{{ value_json.humidity }}"
    - name: "PM2.5"
      device_class: "pm25"
      state_topic: "homeassistant/sensor/aqi-1/state"
      unit_of_measurement: "µg/m³"
      unique_id: "-- GENERATE A UUID TO USE HERE --"
      value_template: "{{ value_json.pm25 }}"
    - name: "AQI"
      device_class: "aqi"
      state_topic: "homeassistant/sensor/aqi-1/state"
      unit_of_measurement: "AQI"
      unique_id: "-- GENERATE A UUID TO USE HERE --"
      value_template: "{{ value_json.aqi }}"
 */

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"

// Shared helper function
  extern void debugMessage(String messageText, int messageLevel);

#if defined MQTT && defined HASSIO_MQTT
  // MQTT setup
  #include <Adafruit_MQTT.h>
  #include <Adafruit_MQTT_Client.h>
  #include <ArduinoJson.h>
  extern Adafruit_MQTT_Client aq_mqtt;

    // The code below attempts to enable MQTT auto-discovery for Home Assistant,
  // but so far doesn't work to do that.  Instead this function isn't called from
  // post_mqtt(), and manual sensor configuration is enabled in the Home Assistant
  // configuration file.  See hassio_mqtt.cpp for more details.
  #define TCONFIG_TOPIC "homeassistant/sensor/pm25-1T/config"
  #define HCONFIG_TOPIC "homeassistant/sensor/pm25-1H/config"
  #define PCONFIG_TOPIC "homeassistant/sensor/pm25-1P/config"
  #define ACONFIG_TOPIC "homeassistant/sensor/pm25-1A/config"
  // #define VCONFIG_TOPIC "homeassistant/sensor/pm25-1V/config"
  #define VCONFIG_TOPIC "homeassistant/sensor/pm25-1C/config"

  void hassio_mqtt_setup() {
    const int capacity = JSON_OBJECT_SIZE(5);
    StaticJsonDocument<capacity> doc;

    // Declare buffer to hold serialized object
    char output[1024];
    String topic;
    // Generate the device state topic for Home Assistant using deployment parameters from
    // config.h.  Note that this must match the state topic as specified in Home Assiatant's
    // configuration file (configuration.yaml) for this device.
    topic = String(DEVICE_SITE) + "/" + String(DEVICE) + "/" + String(DEVICE_ID) + "/state";

    // Create MQTT publish objects for the config channels
    Adafruit_MQTT_Publish tconfigPub = Adafruit_MQTT_Publish(&aq_mqtt,TCONFIG_TOPIC);
    Adafruit_MQTT_Publish hconfigPub = Adafruit_MQTT_Publish(&aq_mqtt,HCONFIG_TOPIC);
    Adafruit_MQTT_Publish pconfigPub = Adafruit_MQTT_Publish(&aq_mqtt,PCONFIG_TOPIC);
    Adafruit_MQTT_Publish aconfigPub = Adafruit_MQTT_Publish(&aq_mqtt,ACONFIG_TOPIC);
    // Adafruit_MQTT_Publish vconfigPub = Adafruit_MQTT_Publish(&aq_mqtt,VCONFIG_TOPIC);
    Adafruit_MQTT_Publish vconfigPub = Adafruit_MQTT_Publish(&aq_mqtt,CCONFIG_TOPIC);


    debugMessage(String("Configuring PM25 for Home Assistant MQTT auto-discovery"),1);
    // Create config info for temperature sensor
    doc["device_class"] = "temperature";
    doc["name"] = "Temperature";
    doc["state_topic"] = topic.c_str();
    doc["unit_of_measurement"] = "°F";
    doc["value_template"] = "{{ value_json.temperature}}";

    serializeJson(doc,output);
    // Publish temperature config to its topic (TCONFIG_TOPIC) as a retained message
    debugMessage(output,1);
    tconfigPub.publish(output,true);

    // Reset config data for humidity sensor
    doc["device_class"] = "humidity";
    doc["name"] = "Humidity";
    doc["state_topic"] = topic.c_str();
    doc["unit_of_measurement"] = "%";
    doc["value_template"] = "{{ value_json.humidity}}";

    serializeJson(doc,output);
    // Publish humidity config to its topic (HCONFIG_TOPIC) as a retained message
    debugMessage(output,1);
    hconfigPub.publish(output,true);

    // Reset config data for pm25 sensor
    doc["device_class"] = "pm25";
    doc["name"] = "pm25";
    doc["state_topic"] = topic.c_str();
    doc["unit_of_measurement"] = "ppm";
    doc["value_template"] = "{{ value_json.co2}}";

    serializeJson(doc,output);
    // Publish humidity config to its topic (CCONFIG_TOPIC) as a retained message
    debugMessage(output,1);
    pconfigPub.publish(output,true);

    // Reset config data for AQI sensor
    doc["device_class"] = "aqi";
    doc["name"] = "aqi";
    doc["state_topic"] = topic.c_str();
    doc["unit_of_measurement"] = "hybrid";
    doc["value_template"] = "{{ value_json.co2}}";

    serializeJson(doc,output);
    // Publish humidity config to its topic (CCONFIG_TOPIC) as a retained message
    debugMessage(output,1);
    aconfigPub.publish(output,true);

    // Reset config data for voc index sensor
    doc["device_class"] = "voc";
    doc["name"] = "voc";
    doc["state_topic"] = topic.c_str();
    doc["unit_of_measurement"] = "index";
    doc["value_template"] = "{{ value_json.co2}}";

    serializeJson(doc,output);
    // Publish humidity config to its topic (CCONFIG_TOPIC) as a retained message
    debugMessage(output,1);
    vconfigPub.publish(output,true);

    // Reset config data for co2 sensor
    doc["device_class"] = "carbon_dioxide";
    doc["name"] = "CO2";
    doc["state_topic"] = topic.c_str();
    doc["unit_of_measurement"] = "ppm";
    doc["value_template"] = "{{ value_json.co2}}";

    serializeJson(doc,output);
    // Publish humidity config to its topic (CCONFIG_TOPIC) as a retained message
    debugMessage(output,1);
    cconfigPub.publish(output,true);
  }

  // Called to publish sensor readings as a JSON payload, as part of overall MQTT
  // publishing as implemented in post_mqtt().  Should only be invoked if 
  // Home Assistant MQTT integration is enabled in config.h.
  // Note that it depends on the value of the state topic matching what's in Home
  // Assistant's configuration file (configuration.yaml).
  void hassio_mqtt_publish(float pm25, float aqi, float temperatureF, float vocIndex, float humidity) {
    const int capacity = JSON_OBJECT_SIZE(5);
    StaticJsonDocument<capacity> doc;

    // Declare buffer to hold serialized object
    char output[1024];
    String topic;
    // Generate the device state topic for Home Assistant using deployment parameters from
    // config.h.  Note that this must match the state topic as specified in Home Assiatant's
    // configuration file (configuration.yaml) for this device.
    topic = String(DEVICE_SITE) + "/" + String(DEVICE) + "/" + String(DEVICE_ID) + "/state";
    Adafruit_MQTT_Publish pm252StatePub = Adafruit_MQTT_Publish(&aq_mqtt,topic.c_str());

    debugMessage("Publishing RCO2 values to Home Assistant via MQTT (topic below)",1);
    debugMessage(topic,1);

    doc["temperatureF"] = temperatureF;
    doc["humidity"] = humidity;
    doc["aqi"] = aqi;
    doc["pm25"] = pm25;
    doc["voc"] = vocIndex;

    serializeJson(doc,output);
    // Publish state info to its topic (MQTT_HASSIO_STATE)
    debugMessage(output,1);
    pm252StatePub.publish(output);
  }
#endif