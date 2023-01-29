// ThingSpeak data upload
// this code has not been updated or tested

#ifdef THINGSPEAK
  const char* ts_server = "api.thingspeak.com";
  void post_thingspeak(float pm25, float minaqi, float maxaqi, float aqi) {
    if (client.connect(ts_server, 80)) {
      
      // Measure Signal Strength (RSSI) of Wi-Fi connection
      long rssi = WiFi.RSSI();
      Serial.print("RSSI: ");
      Serial.println(rssi);

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


      client.println("POST /update HTTP/1.1");
      client.println("Host: api.thingspeak.com");
      client.println("User-Agent: ESP8266 (orangemoose)/1.0");
      client.println("Connection: close");
      client.println("X-THINGSPEAKAPIKEY: " + String(THINGS_APIKEY));
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.println("Content-Length: " + String(body.length()));
      client.println("");
      client.print(body);
      Serial.println(body);

    }
    delay(1500);
      
    // Read all the lines of the reply from server (if any) and print them to Serial Monitor
    while(client.available()){
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    client.stop();
  }
#endif