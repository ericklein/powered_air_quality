/*
  Project Name:   Powered Air Quality
  Description:    write sensor data to thingspeak

  Uses the ThingSpeak Arduino library from MathWorks
  https://github.com/mathworks/thingspeak-arduino
*/

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"


#ifdef THINGSPEAK
  #include "ThingSpeak.h"

  // Shared helper function(s)
  extern void debugMessage(String messageText, int messageLevel);

  bool post_thingspeak(float pm25, float co2, float temperatureF, float humidity, float voc, float nox, float aqi)
  {  
    int httpcode;
    WiFiClient ts_client;  

    // Initialize ThingSpeak
    ThingSpeak.begin(ts_client);

    // Set values for the Channel's fields to queue them up for a single batch post to ThingSpeak
    // Note that a channel cannot have more than eight fields (so choose wisely)
    ThingSpeak.setField(1,pm25);
    ThingSpeak.setField(2,co2);
    ThingSpeak.setField(3,temperatureF);
    ThingSpeak.setField(4,humidity);
    ThingSpeak.setField(5,voc);
    ThingSpeak.setField(6,nox);
    ThingSpeak.setField(7,aqi);

    // Batch write all the field updates to ThingSpeak and check HTTP return code
    httpcode = ThingSpeak.writeFields(THINGS_CHANID,THINGS_APIKEY);
    debugMessage("ThingSpeak update, return code: " + String(httpcode),1);

    // HTTP return code 200 means success, otherwise some problem occurred
    if(httpcode == 200) return true;
    else return false;
  }
#endif