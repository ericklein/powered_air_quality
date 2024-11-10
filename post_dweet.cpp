/*
  Project:      pm25
  Description:  write sensor data to DWEET
*/

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"

#ifdef DWEET
  #include <HTTPClient.h> 

  // Shared helper function(s)
  extern void debugMessage(String messageText, int messageLevel);

  void post_dweet(float pm25, float minaqi, float maxaqi, float aqi, float temperatureF, float vocIndex, float humidity, int rssi)
  {
    WiFiClient dweet_client;

    if(WiFi.status() != WL_CONNECTED) {
      debugMessage("Lost network connection to " + String(WIFI_SSID) + "!",1);
      return;
    }

    // Use our WiFiClient to connect to dweet
    if (!dweet_client.connect(DWEET_HOST, 80)) {
      debugMessage("Dweet connection failed!",1);
      return;
    }

    // Transmit Dweet as HTTP post with a data payload as JSON

    // Use HTTP post and send a data payload as JSON
    
    String postdata = "{\"wifi_rssi\":\""     + String(rssi)          + "\"," +
                      "\"AQI\":\""           + String(aqi,2)          + "\"," +
                      "\"address\":\""       + ip.toString()          + "\"," +
                      "\"temperature\":\""   + String(temperatureF,1)        + "\"," +
                      "\"vocIndex\":\""      + String(vocIndex,1)     + "\"," +
                      "\"humidity\":\""      + String(humidity,1)     + "\"," +
                      "\"PM25_value\":\""    + String(pm25,2)         + "\"," +
                      "\"min_AQI\":\""       + String(minaqi,2)       + "\"," + 
                      "\"max_AQI\":\""       + String(maxaqi,2)       + "\"}";
    // Note that the dweet device 'name' gets set here, is needed to fetch values
    dweet_client.println("POST /dweet/for/" + String(DWEET_DEVICE) + " HTTP/1.1");
    dweet_client.println("Host: dweet.io");
    dweet_client.println("User-Agent: ESP32/ESP8266 (orangemoose)/1.0");
    dweet_client.println("Cache-Control: no-cache");
    dweet_client.println("Content-Type: application/json");
    dweet_client.print("Content-Length: ");
    dweet_client.println(postdata.length());
    dweet_client.println();
    dweet_client.println(postdata);
    debugMessage("Dweet POST:",1);
    debugMessage(postdata,1);

    delay(1500);  

    // Read all the lines of the reply from server (if any) and print them to Serial Monitor
    #ifdef DEBUG
      debugMessageln("Dweet server response:",2);
      while(dweet_client.available()){
        String line = dweet_client.readStringUntil('\r');
        debugMessage(line,2);
      }
      debugMessageln("-----",2);
    #endif
    
    // Close client connection to dweet server
    dweet_client.stop();
  }
#endif