/*
  Project:      Climatron - Your personal air quality monitoring robot
  Description:  Publish sensor data to Home Assistant via MQTT
*/

/*
 * Additional routines for use in an MQTT-enabled environment with Home Assistant, allowing
 * sensor readings to be reported to Home Assistant.   
 * 
 * For Home Assistant to recognize Climatron sensors as entities and allow their display,
 * integration into automations, etc., its configuration.yaml file needs to modified to incorporate
 * proper details of each of those sensors and Climatron overall as a device.  A future version
 * of Home Assistant integration for Climatron will implement auto-discovery so those details
 * are published directly by Climatron to Home Assistant.
 *
 * In the meantime manual integration is required.  This looks like the following, where
 * <site>, <location>, and <room> are replaced with device details as set in secrets.h and/or
 * through the onboard configuration portal.

# Configuration for Climatron-0857 integration with Home Assistant
mqtt:
  sensor:
    - name: "Temperature"
      device_class: "temperature"
      unit_of_measurement: "°F"
      state_topic: "homeassistant/<site>>/<location>/<room>/Climatron-0857/state"
      unique_id: "Climatron-0857-temperature"
      value_template: "{{ value_json.temperatureF }}"
      state_class: "measurement"
      device:
        name: "Climatron"
        identifiers:
          - "Climatron-0857" 
    - name: "Humidity"
      device_class: "humidity"
      unit_of_measurement: "%"
      state_topic: "homeassistant/<site>>/<location>/<room>/Climatron-0857/state"
      unique_id: "Climatron-0857-humidity"
      value_template: "{{ value_json.humidity }}"
      state_class: "measurement"
      device:
        name: "Climatron"
        identifiers:
          - "Climatron-0857" 
    - name: "CO2"
      device_class: "carbon_dioxide"
      unit_of_measurement: "ppm"
      state_topic: "homeassistant/<site>>/<location>/<room>/Climatron-0857/state"
      unique_id: "Climatron-0857-co2"
      value_template: "{{ value_json.co2 }}"
      state_class: "measurement"
      device:
        name: "Climatron"
        identifiers:
          - "Climatron-0857" 
    - name: "PM2.5"
      device_class: "pm25"
      state_topic: "homeassistant/<site>>/<location>/<room>/Climatron-0857/state"
      unit_of_measurement: "µg/m³"
      unique_id: "Climatron-0857-pm25"
      value_template: "{{ value_json.pm25 }}"
      state_class: "measurement"
      device:
        name: "Climatron"
        identifiers:
          - "Climatron-0857" 
    - name: "AQI"
      device_class: "aqi"
      state_topic: "homeassistant/<site>>/<location>/<room>/Climatron-0857/state"
      unique_id: "Climatron-0857-aqi"
      value_template: "{{ value_json.aqi }}"
      state_class: "measurement"
      device:
        name: "Climatron"
        identifiers:
          - "Climatron-0857" 
    - name: "VOCIndex"
      device_class: "aqi"
      state_topic: "homeassistant/<site>>/<location>/<room>/Climatron-0857/state"
      unique_id: "Climatron-0857-vocIndex"
      value_template: "{{ value_json.vocIndex }}"
      state_class: "measurement"
      device:
        name: "Climatron"
        identifiers:
          - "Climatron-0857" 
    - name: "NOxIndex"
      device_class: "aqi"
      state_topic: "homeassistant/<site>>/<location>/<room>/Climatron-0857/state"
      unique_id: "Climatron-0857-noxIndex"
      value_template: "{{ value_json.noxIndex }}"
      state_class: "measurement"
      device:
        name: "Climatron"
        identifiers:
          - "Climatron-0857" 
 */

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
#include "powered_air_quality.h"  // overall header info for Powered Air Quality
// private credentials for network, MQTT, weather provider
#include "secrets.h"

#if defined MQTT && defined HASSIO_MQTT
  // MQTT setup
  #include <PubSubClient.h>
  #include <ArduinoJson.h>
  extern PubSubClient mqtt;

  // Shared helper function
  extern void debugMessage(String messageText, uint8_t messageLevel);

  // Called to publish sensor readings as a JSON payload, as part of overall MQTT
  // publishing as implemented in post_mqtt().  Should only be invoked if 
  // Home Assistant MQTT integration is enabled in config.h.
  // Note that it depends on the value of the state topic matching what's in Home
  // Assistant's configuration file (configuration.yaml).
  
  bool hassio_mqtt_publish(float pm25, float co2, float temperatureF, float humidity, float vocIndex, float noxIndex, float aqi) {
    bool success = false;
    const int capacity = JSON_OBJECT_SIZE(7);
    StaticJsonDocument<capacity> doc;

    // Declare buffer to hold serialized object
    char output[1024];
    String topic;
    // Generate the device state topic for Home Assistant using deployment parameters from
    // config.h.  Note that this must match the state topic as specified in Home Assistant's
    // configuration file (configuration.yaml) for this device.
    topic = "homeassistant/" + endpointPath.site + "/" + endpointPath.location + "/" +
      endpointPath.room + "/" + endpointPath.deviceID + "/state";

    debugMessage("Publishing Climatron values to Home Assistant via MQTT (state topic below)",1);
    debugMessage(topic,1);

    // Generate the state topic payload (as JSON)
    doc["temperatureF"] = temperatureF;
    doc["humidity"] = humidity;
    doc["co2"] = co2;
    doc["pm25"] = pm25;
    doc["vocIndex"] = vocIndex;
    doc["noxIndex"] = noxIndex;
    doc["aqi"] = aqi;

    // Serialize the payload so it can be posted via MQTT
    serializeJson(doc,output);
    debugMessage(output,1);  // Print the payload for review

    // Publish state info to its topic
    // Confirm we're connected to the MQTT broker, and if so publish to the state topic
    if (mqtt.connected()) {
      if (mqtt.publish(topic.c_str(), output)) {
        success = true;
        debugMessage(String("MQTT publish topic is ") + topic + ", message is " + output,2);
      }
      else {
        debugMessage(String("MQTT publish to topic ") + topic + " failed",1);
      }
    }
    else {
      debugMessage("MQTT not connected during publish to Home Assistant",1);
    }
    return success;
  }
#endif