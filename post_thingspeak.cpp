// ThingSpeak data upload

#include "Arduino.h"

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT, weather provider
#include "secrets.h"


#ifdef THINGSPEAK
  // Shared helper function(s)
  extern void debugMessage(String messageText, int messageLevel);

  const char* ts_server = "api.thingspeak.com";
  void post_thingspeak(float pm25, float minaqi, float maxaqi, float aqi) {
        WiFiClient tspeak_client;
    
    if (tspeak_client.connect(ts_server, 80)) {

      // Construct API request body
      String body = "field1=";
             body += String(aqi,2);
             body += "&";
             body += "field2=";
             body += String(pm25,2);
             body += "&";
             body += "field3=";
             body += String(maxaqi,2);
             body += "&";
             body += "field4=";
             body += String(minaqi,2);


      tspeak_client.println("POST /update HTTP/1.1");
      tspeak_client.println("Host: api.thingspeak.com");
      tspeak_client.println("User-Agent: ESP32/ESP8266 (orangemoose)/1.0");
      tspeak_client.println("Connection: close");
      tspeak_client.println("X-THINGSPEAKAPIKEY: " + String(THINGS_APIKEY));
      tspeak_client.println("Content-Type: application/x-www-form-urlencoded");
      tspeak_client.println("Content-Length: " + String(body.length()));
      tspeak_client.println("");
      tspeak_client.print(body);
      debugMessage("ThingSpeak POST:",1);
      debugMessage(body,1);

    }
    delay(1500);
      
    // Read all the lines of the reply from server (if any) and print them to Serial Monitor
    #ifdef DEBUG
      debugMessageln("ThingSpeak server response:",1);
      while(tspeak_client.available()){
        String line = tspeak_client.readStringUntil('\r');
        debugMessage(line,1);
      }
      debugMessageln("-----",1);
    #endif

    tspeak_client.stop();
  }
#endif