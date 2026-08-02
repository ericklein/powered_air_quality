/*
  Project Name:   Powered Air Quality
  Description:    Write sensor data to ThingSpeak (https://thingspeak.mathworks.com)
*/

#include "Arduino.h"
#include <WiFi.h>

#include "config.h"               // hardware and internet configuration parameters
#include "powered_air_quality.h"  // overall header info for Powered Air Quality
#include "secrets.h"              // private credentials for network, MQTT, weather provider

#ifdef THINGSPEAK
  #include <ThingSpeak.h>         // https://github.com/mathworks/thingspeak-arduino

  // Shared helper function(s)
  extern void debugMessage(String messageText, uint8_t messageLevel);

  bool post_thingspeak(float pm25, float co2, float temperatureF, float humidity, float voc, float aqi)
  {  
    static WiFiClient thingSpeakClient;
    // Initialize ThingSpeak
    ThingSpeak.begin(thingSpeakClient);

    // Set values for the Channel's fields to queue them up for a single batch post to ThingSpeak
    // Note that a channel cannot have more than eight fields (so choose wisely)
    ThingSpeak.setField(1,pm25);
    ThingSpeak.setField(2,co2);
    ThingSpeak.setField(3,temperatureF);
    ThingSpeak.setField(4,humidity);
    ThingSpeak.setField(5,voc);
    ThingSpeak.setField(7,aqi);

    // Identify the publishing unit via its internal deviceID
    ThingSpeak.setField(8,endpointPath.deviceID);

    // Batch write all the field updates to ThingSpeak and check HTTP return code
    int16_t httpcode = ThingSpeak.writeFields(THINGS_CHANID,THINGS_APIKEY);

    if (httpcode == 200) {
      debugMessage("ThingSpeak update successful",1);
      return true;
    }
    else {
      debugMessage("ThingSpeak issue, return code: " + String(httpcode),1);
      return false;
    }
  }
#endif