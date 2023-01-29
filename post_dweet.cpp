// this is a stub to add DWEET publish support to PM2.5
// code comes from Air Quality project
// this code has not been updated or tested

// Post a dweet to report sensor data readings.  This routine blocks while
// talking to the network, so may take a while to execute.

#ifdef DWEET

// David's original PM2.5 dweet code
const char* dweet_host = "dweet.io";
void post_dweet(float pm25, float minaqi, float maxaqi, float aqi, float tempF, float vocIndex, float humidity)
{
  
  if(WiFi.status() != WL_CONNECTED) {
    Serial.print("Lost network connection to '");
    Serial.print(WIFI_SSID);
    Serial.println("'!");
    return;
  }
  
  Serial.print("connecting to ");
  Serial.print(dweet_host);
  Serial.print(" as ");
  Serial.println(String(DWEET_DEVICE));
      
  // Use our WiFiClient to connect to dweet
  if (!client.connect(dweet_host, 80)) {
    Serial.println("connection failed");
    return;
  }

  long rssi = WiFi.RSSI();
  IPAddress ip = WiFi.localIP();

  // Use HTTP post and send a data payload as JSON
  
  String postdata = "{\"wifi_rssi\":\""     + String(rssi)           + "\"," +
                     "\"AQI\":\""           + String(aqi,2)          + "\"," +
                     "\"address\":\""       + ip.toString()          + "\"," +
                     "\"temperature\":\""   + String(tempF,1)        + "\"," +
                     "\"vocIndex\":\""      + String(vocIndex,1)     + "\"," +
                     "\"humidity\":\""      + String(humidity,1)     + "\"," +
                     "\"PM25_value\":\""    + String(pm25,2)         + "\"," +
                     "\"min_AQI\":\""       + String(minaqi,2)       + "\"," + 
                     "\"max_AQI\":\""       + String(maxaqi,2)       + "\"}";
  // Note that the dweet device 'name' gets set here, is needed to fetch values
  client.println("POST /dweet/for/" + String(DWEET_DEVICE) + " HTTP/1.1");
  client.println("Host: dweet.io");
  client.println("User-Agent: ESP8266 (orangemoose)/1.0");
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/json");
  client.print("Content-Length: ");
  client.println(postdata.length());
  client.println();
  client.println(postdata);
  Serial.println(postdata);

  delay(1500);  
  // Read all the lines of the reply from server (if any) and print them to Serial Monitor
  while(client.available()){
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }
   
  Serial.println("closing connection");
  Serial.println();
}


// the post_dweet function from Air Quality, partially modified to send PM 2.5 data

  #include "Arduino.h"

  // hardware and internet configuration parameters
  #include "config.h"

  #include <HTTPClient.h> 

  // Shared helper functions called from project files
  extern void debugMessage(String messageText);

  void post_dweet(float pm25, float minaqi, float maxaqi, float aqi, float tempF, float vocIndex, float humidity)
  {
    if(WiFi.status() != WL_CONNECTED)
    {
      debugMessage("Lost network connection to '" + String(WIFI_SSID) + "'!");
      return;
    }
    debugMessage("connecting to " + String(DWEET_HOST) + " as " + String(DWEET_DEVICE));

    String dweeturl = "http://" + String(DWEET_HOST) + "/dweet/for/" + String(DWEET_DEVICE);

    // Use our WiFiClient to connect to dweet
    if (!client.connect(dweet_host, 80))
    {
      Serial.println("connection failed");
      return;
    }

    // Use HTTP class to publish dweet
    HTTPClient dweetio;
    dweetio.begin(client,dweeturl);  
    dweetio.addHeader("Content-Type", "application/json");


    // Use HTTP post and send a data payload as JSON
    String device_info = "{\"rssi\":\""   + String(rssi)        + "\"," +
                         "\"ipaddr\":\"" + WiFi.localIP().toString()  + "\",";
    String battery_info;
    if((battpct !=10000)&&(battv != 10000)) {
      battery_info = "\"battery_percent\":\"" + String(battpct) + "\"," +
                     "\"battery_voltage\":\"" + String(battv)   + "\",";
    }
    else {
      battery_info = "";
    }

    String sensor_info;
    if (co2 !=10000)
    {
      sensor_info = "\"co2\":\""         + String(co2)             + "\"," +
                         "\"temperature\":\"" + String(tempF, 2)         + "\"," +
                         "\"humidity\":\""    + String(humidity, 2)      + "\"}";
    }
    else
    {
      sensor_info = "\"temperature\":\"" + String(tempF, 2)         + "\"," +
                         "\"humidity\":\""    + String(humidity, 2)      + "\"}";
    }

    String postdata = device_info + battery_info + sensor_info;
    int httpCode = aq_network.httpPOSTRequest(dweeturl,"application/json",postdata);
    
    // httpCode will be negative on error, but HTTP status might indicate failure
    if (httpCode > 0) {
      // HTTP POST complete, print result code
      debugMessage("HTTP POST [dweet.io], result code: " + String(httpCode) );
    } else {
      debugMessage("HTTP POST [dweet.io] failed, result code: " + String(httpCode));
    }
  }
#endif