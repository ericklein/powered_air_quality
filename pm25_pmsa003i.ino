/*
  Project Name:   PM2.5 monitor
  Description:    Regularly sample and log PM 2.5 levels

  See README.md for target information and revision history
*/

// Adafruit PMSA003I
#include "Adafruit_PM25AQI.h"

#include "config.h"
#include "secrets.h"

#ifdef WIFI
  // ESP32 WiFi
  #include <WiFi.h>

  // ESP8266 WiFi
  // #include <ESP8266WiFi.h>

  // Use WiFiClient class to create TCP connections and talk to hosts
  WiFiClient client;
#endif

//create Plantower sensor instance
Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

// global variables

// PM sensor data
typedef struct
{
  float massConcentrationPm1p0;
  float massConcentrationPm2p5;
  float massConcentrationPm10p0;
  uint16_t pm10_env;        // Environmental PM1.0
  uint16_t pm25_env;        // Environmental PM2.5
  uint16_t pm100_env;       // Environmental PM10.0
  uint16_t particles_03um, //< 0.3um Particle Count
    particles_05um,      //< 0.5um Particle Count
    particles_10um,      //< 1.0um Particle Count
    particles_25um,      //< 2.5um Particle Count
    particles_50um,      //< 5.0um Particle Count
    particles_100um;     //< 10.0um Particle Count
} envData;
envData sensorData;

float pm25Total = 0;      // running total of humidity over report interval
float avgPM25;            // average PM2.5 over report interval

unsigned long prevReportMs = 0;   // Timestamp for measuring elapsed capture time
unsigned long prevSampleMs  = 0;  // Timestamp for measuring elapsed sample time
unsigned int numSamples = 0;      // Number of overall sensor readings over reporting interval
unsigned int numReports = 0;      // Number of capture intervals observed

bool internetAvailable;
int rssi;

// External function dependencies
#ifdef INFLUX
  extern boolean post_influx(float pm25, float aqi, int rssi);
#endif

#ifdef MQTT
  // MQTT interface depends on the underlying network client object, which is defined and
  // managed here (so needs to be defined here).
  #include <Adafruit_MQTT.h>
  #include <Adafruit_MQTT_Client.h>
  Adafruit_MQTT_Client pm25_mqtt(&client, MQTT_BROKER, MQTT_PORT, CLIENT_ID, MQTT_USER, MQTT_PASS);

  extern int mqttDeviceWiFiUpdate(int rssi);
  extern int mqttSensorUpdate(float pm25, float aqi);
#endif

void setup() 
{
  // handle Serial first so debugMessage() works
  #ifdef DEBUG
    Serial.begin(115200);
    // wait for serial port connection
    while (!Serial);

    // Confirm key site configuration parameters
    debugMessage("PM2.5 monitor started");
    debugMessage("Client ID: " + String(CLIENT_ID));
    debugMessage(String(SAMPLE_INTERVAL) + " second sample interval");
    debugMessage(String(REPORT_INTERVAL) + " minute report interval");
  #endif

  initSensor();
  // Remember current clock time
  prevReportMs = prevSampleMs = millis();
}

void loop()
{
  // update current timer value
  unsigned long currentMillis = millis();

  // is it time to read the sensor
  if((currentMillis - prevSampleMs) >= (SAMPLE_INTERVAL * 1000)) // converting SAMPLE_INTERVAL into milliseconds
  {
    if (!readSensor())
    {
      // handle error condition
    }
    numSamples++;

    // add to the running totals
    pm25Total += sensorData.massConcentrationPm2p5;

    debugMessage(String("PM2.5 reading is ") + sensorData.massConcentrationPm2p5 + " or AQI " + pm25toAQI(sensorData.massConcentrationPm2p5));

    debugMessage(String("PM2.5 env reading is ") + sensorData.pm25_env + " or AQI " + pm25toAQI(sensorData.pm25_env));
    debugMessage(String("Particles > 0.3um / 0.1L air:") + sensorData.particles_03um);
    debugMessage(String("Particles > 0.5um / 0.1L air:") + sensorData.particles_05um);
    debugMessage(String("Particles > 1.0um / 0.1L air:") + sensorData.particles_10um);
    debugMessage(String("Particles > 2.5um / 0.1L air:") + sensorData.particles_25um);
    debugMessage(String("Particles > 5.0um / 0.1L air:") + sensorData.particles_50um);
    debugMessage(String("Particles > 10 um / 0.1L air:") + sensorData.particles_100um);

    // Save sample time
    prevSampleMs = currentMillis;
  }

  // is it time to report averaged values
  if((currentMillis - prevReportMs) >= (REPORT_INTERVAL * 60 *1000)) // converting REPORT_INTERVAL into milliseconds
  {
    #ifdef WIFI
      if(networkBegin())
      {
        internetAvailable = true;
      }
    #endif
    if (numSamples != 0) 
    {
      avgPM25 = pm25Total / numSamples;
      debugMessage(String("Average PM2.5 reading for this ") + REPORT_INTERVAL + " minute report interval  is " + avgPM25 + " or AQI " + pm25toAQI(avgPM25));
  
      // And store data to Influx DB    
      #ifdef INFLUX
        post_influx(avgPM25, pm25toAQI(avgPM25), rssi);
      #endif

      #ifdef MQTT
        mqttDeviceWiFiUpdate(rssi);
        mqttSensorUpdate(avgPM25, pm25toAQI(avgPM25));
      #endif

      // Reset counters and accumulators
      prevReportMs = currentMillis;
      numSamples = 0;
      pm25Total = 0;
    }
  }
}

#ifdef WIFI
  bool networkBegin()
  {
    // set hostname has to come before WiFi.begin
    WiFi.hostname(CLIENT_ID);

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    for (int tries = 1; tries <= CONNECT_ATTEMPT_LIMIT; tries++)
    // Attempts WiFi connection, and if unsuccessful, re-attempts after CONNECT_ATTEMPT_INTERVAL second delay for CONNECT_ATTEMPT_LIMIT times
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        rssi = abs(WiFi.RSSI());
        debugMessage(String("WiFi IP address issued from ") + WIFI_SSID + " is " + WiFi.localIP().toString());
        debugMessage(String("WiFi RSSI is: ") + rssi + " dBm");
        return true;
      }
      debugMessage(String("Connection attempt ") + tries + " of " + CONNECT_ATTEMPT_LIMIT + " to " + WIFI_SSID + " failed, trying again in " + CONNECT_ATTEMPT_INTERVAL + " seconds");
      // use of delay() OK as this is initialization code
      delay(CONNECT_ATTEMPT_INTERVAL * 1000); // convered into milliseconds
    }
    debugMessage("Failed to connect to WiFi");
    return false;
  }
#endif

int initSensor()
{
  if (! aqi.begin_I2C()) 
  {
    debugMessage("Could not find PMSA003I sensor");
    while (1) delay(10);
  }
  debugMessage("PMSA003I initialized");
  return 1;
}

int readSensor()
{
  PM25_AQI_Data data;
  if (! aqi.read(&data)) 
  {
    debugMessage("Could not read PMSA003I sensor data");
    // ?? why is this here?
    //prevSampleMs = currentMillis;
    return 0;
  }
  // successful read
  sensorData.massConcentrationPm1p0 = data.pm10_standard;
  sensorData.massConcentrationPm2p5 = data.pm25_standard;
  sensorData.massConcentrationPm10p0 = data.pm100_standard;
  sensorData.pm25_env = data.pm25_env;
  sensorData.particles_03um = data.particles_03um;
  sensorData.particles_05um = data.particles_05um;
  sensorData.particles_10um = data.particles_10um;
  sensorData.particles_25um = data.particles_25um;
  sensorData.particles_50um = data.particles_50um;
  sensorData.particles_100um = data.particles_100um;
  debugMessage("Successful sensor read");
  return 1;
}

float pm25toAQI(float pm25)
// Converts pm25 reading to AQI using the AQI Equation
// (https://forum.airnowtech.org/t/the-aqi-equation/169)
{  
  if(pm25 <= 12.0)       return(fmap(pm25,  0.0, 12.0,  0.0, 50.0));
  else if(pm25 <= 35.4)  return(fmap(pm25, 12.1, 35.4, 51.0,100.0));
  else if(pm25 <= 55.4)  return(fmap(pm25, 35.5, 55.4,101.0,150.0));
  else if(pm25 <= 150.4) return(fmap(pm25, 55.5,150.4,151.0,200.0));
  else if(pm25 <= 250.4) return(fmap(pm25,150.5,250.4,201.0,300.0));
  else if(pm25 <= 500.4) return(fmap(pm25,250.5,500.4,301.0,500.0));
  else return(505.0);  // AQI above 500 not recognized
}

float fmap(float x, float xmin, float xmax, float ymin, float ymax)
{
    return( ymin + ((x - xmin)*(ymax-ymin)/(xmax - xmin)));
}

void debugMessage(String messageText)
// wraps Serial.println as #define conditional
{
#ifdef DEBUG
  Serial.println(messageText);
  Serial.flush();  // Make sure the message gets output (before any sleeping...)
#endif
}