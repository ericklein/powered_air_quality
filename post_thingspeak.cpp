/*
  Project Name:   Powered Air Quality
  Description:    Write sensor data to ThingSpeak (https://thingspeak.mathworks.com)
*/

#include "Arduino.h"
#include <HTTPClient.h>

#include "config.h"               // hardware and internet configuration parameters
#include "powered_air_quality.h"  // PAQ main header
#include "secrets.h"              // ThingSpeak private credentials

#ifdef THINGSPEAK
  // Shared helper function(s)
  extern void debugMessage(String messageText, uint8_t messageLevel);

  bool post_thingspeak(float pm25, float co2, float temperatureF, float humidity, float voc, float aqi) {  
    
    HTTPClient http;

    // explicitly use unencrypted HTTP on port 80
    const char* serverURL = "http://api.thingspeak.com/update";

    if (!http.begin(serverURL)) {
        debugMessage("ThingSpeak HTTP initialization failed", 1);
        return false;
    }

    http.addHeader("Content-Type","application/x-www-form-urlencoded");

    String requestBody;
    requestBody.reserve(256);

    requestBody =
      "api_key=" + String(THINGS_APIKEY) +
      "&field1=" + String(pm25) +
      "&field2=" + String(co2) +
      "&field3=" + String(temperatureF) +
      "&field4=" + String(humidity) +
      "&field5=" + String(voc) +
      "&field7=" + String(aqi) +
      "&field8=" + String(endpointPath.deviceID);

    int httpCode = http.POST(requestBody);

    if (httpCode <= 0) {
      debugMessage("ThingSpeak connection issue: " + String(HTTPClient::errorToString(httpCode)),1);
      http.end();
      return false;
    }

    String response = http.getString();
    http.end();

    // ThingSpeak returns HTTP 200 and the new entry ID
    // A body of "0" indicates that the update was not accepted
    if (httpCode == HTTP_CODE_OK && response.toInt() > 0) {
        debugMessage("ThingSpeak update successful, entry ID: " + response,1);
        return true;
    }

    debugMessage("ThingSpeak issue, HTTP code: " + String(httpCode) + ", response: " + response,1);
    return false;
  }
#endif