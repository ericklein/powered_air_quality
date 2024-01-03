/*
  Project:        AQ_powered
  Description:    Sample and log indoor air quality via AC powered device

  See README.md for target information
*/

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT
#include "secrets.h"

#ifndef SIMULATE_SENSOR
  // initialize pm25 sensor
  #include "Adafruit_PM25AQI.h"
  Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

  // initialize scd40 environment sensor
  #include <SensirionI2CScd4x.h>
  SensirionI2CScd4x envSensor;
#endif

// activate only if using network data endpoints
#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
  #elif defined(ESP32)
    #include <WiFi.h>
  #elif
    #include <WiFiNINA.h> // PyPortal
  #endif
#endif

#ifdef SCREEN
  // 3.2″ ILI9341 320 x 240 color TFT with resistive touch screen
  #include <Adafruit_GFX.h>    // Core graphics library
  #include "Adafruit_ILI9341.h"
  Adafruit_ILI9341 display = Adafruit_ILI9341(tft8bitbus, TFT_D0, TFT_WR, TFT_DC, TFT_CS, TFT_RST, TFT_RD);

  #include <Fonts/FreeSans9pt7b.h>
  #include <Fonts/FreeSans12pt7b.h>
  #include <Fonts/FreeSans18pt7b.h>
#endif

// global variables

// environment sensor data
typedef struct envData
{
  float massConcentrationPm2p5;       // PM2.5 [µg/m³]
  // float massConcentrationPm1p0;    // PM1.0 [µg/m³], NAN if unknown
  // float massConcentrationPm10p0;   // PM10.0 [µg/m³], NAN if unknown

  // SEN5X, temp sensor, or SCD40 data
  float ambientHumidity;      // RH [%]
  float ambientTemperatureF;

  // SCD40 specific data
  uint16_t  ambientCO2;

  // SEN5x specific data
  // float massConcentrationPm4p0;   // PM4.0 [µg/m³], NAN if unknown
  // float vocIndex;                 // Sensiron VOC Index, NAN in unknown
  // float noxIndex;                 // NAN for unsupported devices (SEN54), also NAN for first 10-11 seconds

  // plantower specific data
  // uint16_t pm10_env;        // Environmental PM1.0
  // uint16_t pm25_env;        // Environmental PM2.5
  // uint16_t pm100_env;       // Environmental PM10.0
  // uint16_t particles_03um, //< 0.3um Particle Count
  //   particles_05um,      //< 0.5um Particle Count
  //   particles_10um,      //< 1.0um Particle Count
  //   particles_25um,      //< 2.5um Particle Count
  //   particles_50um,      //< 5.0um Particle Count
  //   particles_100um;     //< 10.0um Particle Count
} envData;
envData sensorData;

// hardware status data
typedef struct hdweData
{
  // float batteryPercent;
  // float batteryVoltage;
  // float batteryTemperatureF;
  int rssi; // WiFi RSSI value
} hdweData;
hdweData hardwareData;

hardwareData.rssi = 0;

unsigned long prevSampleMs  = 0;  // Timestamp for measuring elapsed sample time

// External function dependencies
#ifdef INFLUX
  extern bool post_influx(float pm25, float aqi, float temperatureF, float vocIndex, float humidity, uint16_t co2, int rssi);
#endif

#ifdef MQTT
  // MQTT uses WiFiClient class to create TCP connections
  WiFiClient client;

  // MQTT interface depends on the underlying network client object, which is defined and
  // managed here (so needs to be defined here).
  #include <Adafruit_MQTT.h>
  #include <Adafruit_MQTT_Client.h>
  Adafruit_MQTT_Client aq_mqtt(&client, MQTT_BROKER, MQTT_PORT, DEVICE_ID, MQTT_USER, MQTT_PASS);

  extern bool mqttDeviceWiFiUpdate(int rssi);
  extern bool mqttSensorTemperatureFUpdate(float temperatureF);
  extern bool mqttSensorHumidityUpdate(float humidity);
  extern bool mqttSensorPM25Update(float pm25);
  extern bool mqttSensorAQIUpdate(float aqi);
  // extern bool mqttSensorVOCIndexUpdate(float vocIndex);
  extern bool mqttSensorCO2Update(uint16_t co2)
  #ifdef HASSIO_MQTT
    extern void hassio_mqtt_publish(float pm25, float aqi, float temperatureF, float vocIndex, float humidity, uint16_t co2ß);
  #endif
#endif

void setup() 
{
  // handle Serial first so debugMessage() works
  #ifdef DEBUG
    Serial.begin(115200);
    // wait for serial port connection
    while (!Serial);

    // Display key configuration parameters
    debugMessage("PM2.5 monitor started",1);
    debugMessage(String("Sample interval is ") + SAMPLE_INTERVAL + " seconds",1);
    debugMessage(String("Report interval is ") + REPORT_INTERVAL + " minutes",1);
    debugMessage(String("Internet service reconnect delay is ") + CONNECT_ATTEMPT_INTERVAL + " seconds",1);
  #endif

  #ifdef SCREEN
    pinMode(TFT_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_BACKLIGHT, HIGH);

    pinMode(TFT_RESET, OUTPUT);
    digitalWrite(TFT_RESET, HIGH);
    delay(10);
    digitalWrite(TFT_RESET, LOW);
    delay(10);
    digitalWrite(TFT_RESET, HIGH);
    delay(10);

    display.begin();
    display.setTextWrap(false);
    display.fillScreen(ILI9341_BLACK);
  #endif

  // Initialize environmental sensor
  if (!sensorPM25Init()) {
    debugMessage("Environment sensor failed to initialize", 1);
    screenAlert("NO PM25 sensor");
  }

  // Remember current clock time
  prevReportMs = prevSampleMs = millis();

  networkConnect();
}

void loop()
{

  // read sensor if time
  // update the screen
  // update endpoints

  // update current timer value
  unsigned long currentMillis = millis();

  // is it time to read the sensor?
  if((currentMillis - prevSampleMs) >= (SAMPLE_INTERVAL * 1000)) // converting SAMPLE_INTERVAL into milliseconds
  {
    if (sensorPM25Read())
    {
      debugMessage(String("PM2.5 reading is ") + sensorData.massConcentrationPm2p5 + " or AQI " + pm25toAQI(sensorData.massConcentrationPm2p5),1);
      // debugMessage(String("Particles > 0.3um / 0.1L air:") + sensorData.particles_03um,2);
      // debugMessage(String("Particles > 0.5um / 0.1L air:") + sensorData.particles_05um,2);
      // debugMessage(String("Particles > 1.0um / 0.1L air:") + sensorData.particles_10um,2);
      // debugMessage(String("Particles > 2.5um / 0.1L air:") + sensorData.particles_25um,2);
      // debugMessage(String("Particles > 5.0um / 0.1L air:") + sensorData.particles_50um,2);
      // debugMessage(String("Particles > 10 um / 0.1L air:") + sensorData.particles_100um,2);
      debugMessage(String("Sample count is ") + numSamples,1);
      debugMessage(String("Running PM25 total for this sample period is ") + pm25Total,1);

    if (!sensorCO2Read())
    {
      debugMessage("SCD40 returned no/bad data",1);
      screenAlert(40, ((display.height()/2)+6),"SCD40 read issue");
      powerDisable(HARDWARE_ERROR_INTERVAL);
    }

      screenPM();

      // add to the running totals
      pm25Total += sensorData.massConcentrationPm2p5;

      // Save sample time
      prevSampleMs = currentMillis;
    }
    else
    {
      debugMessage("Could not read PMSA003I sensor data",1);
    }
  }

    // do we have network endpoints to report to?
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
    // is it time to report to the network endpoints?
    if ((currentMillis - prevReportMs) >= (REPORT_INTERVAL * 60 * 1000))  // converting REPORT_INTERVAL into milliseconds
    {
      // do we have samples to report?
      if (numSamples != 0) 
      {
        float avgPM25 = pm25Total / numSamples;

        avgtemperatureF = temperatureFTotal / numSamples;
        avgVOC = vocTotal / numSamples;
        avgHumidity = humidityTotal / numSamples;

        debugMessage("----- Reporting -----",1);
        debugMessage(String("Reporting averages (") + REPORT_INTERVAL + String(" minute): "),1);
        debugMessage(String("PM2.5: ") + avgPM25 + String(" = AQI ") + pm25toAQI(avgPM25),1);
        debugMessage(String("Temp: ") + avgtemperatureF + String(" F"),1);
        debugMessage(String("Humidity: ") + avgHumidity + String("%"),1);
        debugMessage(String("VoC: ") + avgVOC,1);

        if (networkConnect())
        {
          /* Post both the current readings and historical max/min readings to the internet */
          #ifdef INFLUX
            if (!post_influx(avgPM25, pm25toAQI(avgPM25), avgtemperatureF, avgVOC, avgHumidity, hardwareData.rssi))
              debugMessage("Did not write to influxDB",1);
          #endif

          #ifdef MQTT
            if (!mqttDeviceWiFiUpdate(hardwareData.rssi))
                debugMessage("Did not write device data to MQTT broker",1);
            if ((!mqttSensorTemperatureFUpdate(avgtemperatureF)) || (!mqttSensorHumidityUpdate(avgHumidity)) || (!mqttSensorPM25Update(avgPM25)) || (!mqttSensorAQIUpdate(pm25toAQI(avgPM25))) || (!mqttSensorVOCIndexUpdate(avgVOC)))
                debugMessage("Did not write environment data to MQTT broker",1);
            #ifdef HASSIO_MQTT
              debugMessage("Establishing MQTT for Home Assistant",1);
              // Either configure sensors in Home Assistant's configuration.yaml file
              // directly or attempt to do it via MQTT auto-discovery
              // hassio_mqtt_setup();  // Config for MQTT auto-discovery
              hassio_mqtt_publish(avgPM25, pm25toAQI(avgPM25), avgtemperatureF, avgVOC, avgHumidity);
            #endif
          #endif
        }
        // Reset counters and accumulators
        prevReportMs = currentMillis;
        numSamples = 0;
        pm25Total = 0;
        temperatureFTotal = 0;
        vocTotal = 0;
        humidityTotal = 0;
      }
    }
  #endif
}

void screenPM() {
#ifdef SCREEN
  debugMessage("Starting screenPM refresh", 1);

  // clear screen
  display.fillScreen(ST77XX_BLACK);

  // screen helper routines
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), (yMargins + (5*wifiBarHeightIncrement)), wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);

  // temperature and humidity
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(ST77XX_WHITE);
  display.setCursor(xMargins, yTemperature);
  display.print(sensorData.ambientTemperatureF,1);
  display.print("F ");
  if ((sensorData.ambientHumidity<40) || (sensorData.ambientHumidity>60))
    display.setTextColor(ST7735_RED);
  else
    display.setTextColor(ST7735_GREEN);
  display.print(sensorData.ambientHumidity,1);
  display.print("%");

  // pm25 level circle
  switch (int(sensorData.massConcentrationPm2p5/50))
  {
    case 0: // good
      display.fillCircle(46,75,31,ST77XX_BLUE);
      break;
    case 1: // moderate
      display.fillCircle(46,75,31,ST77XX_GREEN);
      break;
    case 2: // unhealthy for sensitive groups
      display.fillCircle(46,75,31,ST77XX_YELLOW);
      break;
    case 3: // unhealthy
      display.fillCircle(46,75,31,ST77XX_ORANGE);
      break;
    case 4: // very unhealthy
      display.fillCircle(46,75,31,ST77XX_RED);
      break;
    case 5: // very unhealthy
      display.fillCircle(46,75,31,ST77XX_RED);
      break;
    default: // >=6 is hazardous
      display.fillCircle(46,75,31,ST77XX_MAGENTA);
      break;
  }

  // pm25 legend
  display.fillRect(xMargins,yLegend,legendWidth,legendHeight,ST77XX_BLUE);
  display.fillRect(xMargins,yLegend-legendHeight,legendWidth,legendHeight,ST77XX_GREEN);
  display.fillRect(xMargins,(yLegend-(2*legendHeight)),legendWidth,legendHeight,ST77XX_YELLOW);
  display.fillRect(xMargins,(yLegend-(3*legendHeight)),legendWidth,legendHeight,ST77XX_ORANGE);
  display.fillRect(xMargins,(yLegend-(4*legendHeight)),legendWidth,legendHeight,ST77XX_RED);
  display.fillRect(xMargins,(yLegend-(5*legendHeight)),legendWidth,legendHeight,ST77XX_MAGENTA);


  // VoC level circle
  switch (int(sensorData.vocIndex/100))
  {
    case 0: // great
      display.fillCircle(114,75,31,ST77XX_BLUE);
      break;
    case 1: // good
      display.fillCircle(114,75,31,ST77XX_GREEN);
      break;
    case 2: // moderate
      display.fillCircle(114,75,31,ST77XX_YELLOW);
      break;
    case 3: // 
      display.fillCircle(114,75,31,ST77XX_ORANGE);
      break;
    case 4: // bad
      display.fillCircle(114,75,31,ST77XX_RED);
      break;
  }

  // VoC legend
  display.fillRect(display.width()-xMargins,yLegend,legendWidth,legendHeight,ST77XX_BLUE);
  display.fillRect(display.width()-xMargins,yLegend-legendHeight,legendWidth,legendHeight,ST77XX_GREEN);
  display.fillRect(display.width()-xMargins,(yLegend-(2*legendHeight)),legendWidth,legendHeight,ST77XX_YELLOW);
  display.fillRect(display.width()-xMargins,(yLegend-(3*legendHeight)),legendWidth,legendHeight,ST77XX_ORANGE);
  display.fillRect(display.width()-xMargins,(yLegend-(4*legendHeight)),legendWidth,legendHeight,ST77XX_RED);

  // circle labels
  display.setTextColor(ST77XX_WHITE);
  display.setFont();
  display.setCursor(33,110);
  display.print("PM2.5");
  display.setCursor(106,110);
  display.print("VoC"); 


  // pm25 level
  display.setFont(&FreeSans9pt7b);
  display.setCursor(40,80);
  display.print(int(sensorData.massConcentrationPm2p5));

  // VoC level
  display.setCursor(100,80);
  display.print(int(sensorData.vocIndex));
#endif
}

void screenAlert(String messageText)
// Display critical error message on screen
{
  #ifdef SCREEN
    debugMessage("screenAlert start",1);

    int16_t x1, y1;
    uint16_t width,height;

    display.fillScreen(ST77XX_BLACK);
    display.getTextBounds(messageText.c_str(), 0, 0, &x1, &y1, &width, &height);
    display.setTextColor(ST77XX_WHITE);
    display.setFont(&FreeSans12pt7b);

    if (width >= display.width())
    {
      debugMessage("ERROR: screenAlert message text too long", 1);
    }
    display.setCursor(display.width()/2-width/2,display.height()/2+height/2);
    display.print(messageText);
    debugMessage("screenAlert end",1);
  #endif
}

void screenHelperWiFiStatus(int initialX, int initialY, int barWidth, int barHeightIncrement, int barSpacing)
// helper function for screenXXX() routines that draws WiFi signal strength
{
#ifdef SCREEN
  if (hardwareData.rssi != 0) {
    // Convert RSSI values to a 5 bar visual indicator
    // >90 means no signal
    int barCount = constrain((6 - ((hardwareData.rssi / 10) - 3)), 0, 5);
    if (barCount > 0) {
      // <50 rssi value = 5 bars, each +10 rssi value range = one less bar
      // draw bars to represent WiFi strength
      for (int b = 1; b <= barCount; b++) {
        display.fillRect((initialX + (b * barSpacing)), (initialY - (b * barHeightIncrement)), barWidth, b * barHeightIncrement, ST77XX_WHITE);
      }
      debugMessage(String("WiFi signal strength on screen as ") + barCount + " bars", 2);
    } else {
      // you could do a visual representation of no WiFi strength here
      debugMessage("RSSI too low, no display", 1);
    }
  }
#endif
}

bool networkConnect() 
{
  #ifdef SIMULATE_SENSOR
    // IMPROVEMENT : Could simulate IP address
    // testing range is 30 to 90 (no signal)
    hardwareData.rssi  = random(30, 90);
    return true;
  #endif

  // Run only if using network data endpoints
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)

    // reconnect to WiFi only if needed
    if (WiFi.status() == WL_CONNECTED) 
    {
      debugMessage("Already connected to WiFi",2);
      return true;
    }
    // set hostname has to come before WiFi.begin
    WiFi.hostname(DEVICE_ID);

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    for (int tries = 1; tries <= CONNECT_ATTEMPT_LIMIT; tries++)
    // Attempts WiFi connection, and if unsuccessful, re-attempts after CONNECT_ATTEMPT_INTERVAL second delay for CONNECT_ATTEMPT_LIMIT times
    {
      if (WiFi.status() == WL_CONNECTED) {
        hardwareData.rssi = abs(WiFi.RSSI());
        debugMessage(String("WiFi IP address lease from ") + WIFI_SSID + " is " + WiFi.localIP().toString(), 1);
        debugMessage(String("WiFi RSSI is: ") + hardwareData.rssi + " dBm", 1);
        return true;
      }
      debugMessage(String("Connection attempt ") + tries + " of " + CONNECT_ATTEMPT_LIMIT + " to " + WIFI_SSID + " failed", 1);
      // use of delay() OK as this is initialization code
      delay(CONNECT_ATTEMPT_INTERVAL * 1000);  // converted into milliseconds
    }
  #endif
  return false;
}

bool sensorPM25Init()
{
  // If we're simulating the sensor there's nothing to init & we're good to go
  #ifdef SIMULATE_SENSOR
    return true;
  #endif

  if (aqi.begin_I2C()) 
  {
    debugMessage("PMSA003I initialized",1);
    return true;
  }
  return false;
}

void  sensorPM25Simulate()
{
  //sensorData.massConcentrationPm1p0 = random(0, 360) / 10.0;
  sensorData.massConcentrationPm2p5 = random(0, 360) / 10.0;
  //sensorData.massConcentrationPm4p0 = random(0, 720) / 10.0;
  //sensorData.massConcentrationPm10p0 = random(0, 1550) / 10.0;
  // testing range is 5 to 95
  sensorData.ambientHumidity = 5 + (random(0, 900) / 10.0);
  // keep this value in C, not F. Converted after sensorPM25Read()
  // testing range is 15 to 25
  sensorData.ambientTemperatureF = 15.0 + (random(0, 101) / 10.0);
  sensorData.vocIndex = random(0, 500) / 10.0;
  sensorData.noxIndex = random(0, 2500) / 10.0;  
}

bool sensorPM25Read()
{
    // If we're simulating the sensor generate some reasonable values for measurements
  #ifdef SIMULATE_SENSOR
    sensorPM25Simulate();
    return true;
  #endif

  PM25_AQI_Data data;
  if (! aqi.read(&data)) 
  {
    return false;
  }
  // successful read, store data
  // sensorData.massConcentrationPm1p0 = data.pm10_standard;
  sensorData.massConcentrationPm2p5 = data.pm25_standard;
  // sensorData.massConcentrationPm10p0 = data.pm100_standard;
  // sensorData.pm25_env = data.pm25_env;
  // sensorData.particles_03um = data.particles_03um;
  // sensorData.particles_05um = data.particles_05um;
  // sensorData.particles_10um = data.particles_10um;
  // sensorData.particles_25um = data.particles_25um;
  // sensorData.particles_50um = data.particles_50um;
  // sensorData.particles_100um = data.particles_100um;
  return true;
}

bool sensorCO2Init() {
  #ifdef SENSOR_SIMULATE
    return true;
  #else
    char errorMessage[256];

    #if defined(ARDUINO_ADAFRUIT_QTPY_ESP32S2) || defined(ARDUINO_ADAFRUIT_QTPY_ESP32S3_NOPSRAM) || defined(ARDUINO_ADAFRUIT_QTPY_ESP32S3) || defined(ARDUINO_ADAFRUIT_QTPY_ESP32_PICO)
      // these boards have two I2C ports so we have to initialize the appropriate port
      Wire1.begin();
      envSensor.begin(Wire1);
    #else
      // only one I2C port
      Wire.begin();
      envSensor.begin(Wire);
    #endif

    envSensor.wakeUp();
    envSensor.setSensorAltitude(SITE_ALTITUDE);  // optimizes CO2 reading

    uint16_t error = envSensor.startPeriodicMeasurement();
    if (error) {
      // Failed to initialize SCD40
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD40 startPeriodicMeasurement()",1);
      return false;
    } 
    else
    {
      debugMessage("SCD40 initialized",2);
      return true;
    }
  #endif
}

void sensorCO2Simulate()
// Simulate ranged data from the SCD40
// Improvement - implement stable, rapid rise and fall 
{
  #ifdef SENSOR_SIMULATE
    // Temperature
    // keep this value in C, as it is converted to F in sensorCO2Read
    sensorData.ambientTemperatureF = random(sensorTempMin,sensorTempMax) / 100.0;
    // Humidity
    sensorData.ambientHumidity = random(sensorHumidityMin,sensorHumidityMax) / 100.0;
    // CO2
    sensorData.ambientCO2 = random(sensorCO2Min, sensorCO2Max);
  #endif
}

bool sensorCO2Read()
// reads SCD40 READS_PER_SAMPLE times then stores last read
{
  #ifdef SENSOR_SIMULATE
    sensorCO2Simulate();
  #else
    char errorMessage[256];

    screenAlert(40, ((display.height()/2)+6), "CO2 check");
    for (int loop=1; loop<=READS_PER_SAMPLE; loop++)
    {
      // SCD40 datasheet suggests 5 second delay between SCD40 reads
      // assume sensorSampleInterval will create needed delay for loop == 1 
      if (loop > 1) delay(5000);
      uint16_t error = envSensor.readMeasurement(sensorData.ambientCO2, sensorData.ambientTemperatureF, sensorData.ambientHumidity);
      // handle SCD40 errors
      if (error) {
        errorToString(error, errorMessage, 256);
        debugMessage(String(errorMessage) + " error during SCD4X read",1);
        return false;
      }
      if (sensorData.ambientCO2<400 || sensorData.ambientCO2>6000)
      {
        debugMessage("SCD40 CO2 reading out of range",1);
        return false;
      }
      debugMessage(String("SCD40 read ") + loop + " of " + READS_PER_SAMPLE + " : " + sensorData.ambientTemperatureF + "C, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
    }
  #endif
  //convert temperature from Celcius to Fahrenheit
  sensorData.ambientTemperatureF = (sensorData.ambientTemperatureF * 1.8) + 32;
  #ifdef SENSOR_SIMULATE
    debugMessage(String("SIMULATED SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
  #else
    debugMessage(String("SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
  #endif
  return true;
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

void debugMessage(String messageText, int messageLevel)
// wraps Serial.println as #define conditional
{
#ifdef DEBUG
  if (messageLevel <= DEBUG) {
    Serial.println(messageText);
    Serial.flush();  // Make sure the message gets output (before any sleeping...)
  }
#endif
}