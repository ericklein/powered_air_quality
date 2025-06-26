/*
  Project:        Powered Air Quality
  Description:    Sample and log indoor air quality via AC powered device

  See README.md for target information
*/

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT
#include "secrets.h"

// Utility class for easy handling of aggregate sensor data
#include "measure.h"

#ifndef HARDWARE_SIMULATE
  // instanstiate SEN5X hardware object
  #include <SensirionI2CSen5x.h>
  SensirionI2CSen5x pmSensor;

  // instanstiate SCD4X hardware object
  #include <SensirionI2cScd4x.h>
  SensirionI2cScd4x co2Sensor;

  // WiFi support
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
  #elif defined(ESP32)
    #include <WiFi.h>
  #endif
  #include <HTTPClient.h>
  WiFiClient client;   // used by OWM and MQTT
#endif

#include <SPI.h>
// Note: the ESP32 has 2 SPI ports, to have ESP32-2432S028R work with the TFT and Touch on different SPI ports each needs to be defined and passed to the library
SPIClass hspi = SPIClass(HSPI);
SPIClass vspi = SPIClass(VSPI);

// screen support
// 3.2″ 320x240 color TFT w/resistive touch screen, ILI9341 driver
#include "Adafruit_ILI9341.h"

Adafruit_ILI9341 display = Adafruit_ILI9341(&hspi, TFT_DC, TFT_CS, TFT_RST);
// works without SPIClass call, slower
// Adafruit_ILI9341 display = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST, TFT_MISO);
#include <XPT2046_Touchscreen.h>
XPT2046_Touchscreen ts(XPT2046_CS,XPT2046_IRQ);

// IMPROVEMENT: not all of these are used
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include "Fonts/meteocons20pt7b.h"
#include "Fonts/meteocons16pt7b.h"
#include "Fonts/meteocons12pt7b.h"

// Library needed to access Open Weather Map
#include "ArduinoJson.h"  // Needed by OWM retrieval routines

// UI glyphs
#include "glyphs.h"

#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
  // NTP setup using Esperiff library
  #include <time.h>
#endif

#ifdef THINGSPEAK
  extern void post_thingspeak(float pm25, float minaqi, float maxaqi, float aqi);
#endif

#ifdef INFLUX
  extern bool post_influx(float pm25, float temperatureF, float vocIndex, float humidity, uint16_t co2, uint8_t rssi);
#endif

#ifdef MQTT
  // MQTT interface depends on the underlying network client object, which is defined and
  // managed here (so needs to be defined here).
  #include <Adafruit_MQTT.h>
  #include <Adafruit_MQTT_Client.h>
  Adafruit_MQTT_Client aq_mqtt(&client, MQTT_BROKER, MQTT_PORT, DEVICE_ID, MQTT_USER, MQTT_PASS);

  extern bool mqttDeviceWiFiUpdate(uint8_t rssi);
  extern bool mqttSensorTemperatureFUpdate(float temperatureF);
  extern bool mqttSensorHumidityUpdate(float humidity);
  extern bool mqttSensorCO2Update(uint16_t co2);
  extern bool mqttSensorPM25Update(float pm25);
  extern bool mqttSensorVOCIndexUpdate(float vocIndex);
  #ifdef HASSIO_MQTT
    extern void hassio_mqtt_publish(float pm25, float temperatureF, float vocIndex, float humidity, uint16_t co2);
  #endif
#endif

// global variables

// environment sensor data
typedef struct envData
{
  // SCD40 data
  float ambientTemperatureF;    // range -10C to 60C
  float ambientHumidity;        // RH [%], range 0 to 100
  uint16_t  ambientCO2;         // ppm, range 400 to 2000

  // SEN5x data
  float pm25;                   // PM2.5 [µg/m³], (SEN54 -> range 0 to 1000, NAN if unknown)
  float pm1;                    // PM1.0 [µg/m³], (SEN54 -> range 0 to 1000, NAN if unknown)
  float pm10;                   // PM10.0 [µg/m³], (SEN54 -> range 0 to 1000, NAN if unknown)
  float pm4;                    // PM4.0 [µg/m³], range 0 to 1000, NAN if unknown
  float vocIndex;               // Sensiron VOC Index, range 0 to 500, NAN in unknown
  float noxIndex;               // NAN for SEN54, also NAN for first 10-11 seconds on SEN55
} envData;
envData sensorData;

// Utility class used to streamline accumulating sensor values, averages, min/max &c.
Measure totalTemperatureF, totalHumidity, totalCO2, totalVOC, totalPM25;

uint8_t numSamples = 0;       // Number of overall sensor readings over reporting interval
uint32_t timeLastSample = 0;  // timestamp (in ms) for last captured sample 
uint32_t timeLastReport = 0;  // timestamp (in ms) for last report to network endpoints
uint32_t timeLastInput =  0;  // timestamp (in ms) for last user input (screensaver)

// used by thingspeak
float MinPm25 = 1999; /* Observed minimum PM2.5 */
float MaxPm25 = -99;  /* Observed maximum PM2.5 */

// hardware status data
typedef struct hdweData
{
  // float batteryPercent;
  // float batteryVoltage;
  // float batteryTemperatureF;
  uint8_t rssi; // WiFi RSSI value
} hdweData;
hdweData hardwareData;

// OpenWeatherMap Current data
typedef struct {
  // float lon;              // "lon": 8.54
  // float lat;              // "lat": 47.37
  // uint16_t weatherId;     // "id": 521
  // String main;            // "main": "Rain"
  // String description;     // "description": "shower rain"
  String icon;               // "icon": "09d"
  float tempF;                // "temp": 90.56, in F (API request for imperial units)
  // uint16_t pressure;      // "pressure": 1013, in hPa
  uint16_t humidity;         // "humidity": 87, in RH%
  // float tempMin;          // "temp_min": 89.15
  // float tempMax;          // "temp_max": 92.15
  // uint16_t visibility;    // visibility: 10000, in meters
  // float windSpeed;        // "wind": {"speed": 1.5}, in meters/s
  // float windDeg;          // "wind": {deg: 226.505}
  // uint8_t clouds;         // "clouds": {"all": 90}, in %
  // time_t observationTime; // "dt": 1527015000, in UTC
  // String country;         // "country": "CH"
  // time_t sunrise;         // "sunrise": 1526960448, in UTC
  // time_t sunset;          // "sunset": 1527015901, in UTC
  String cityName;           // "name": "Zurich"
  // time_t timezone;        // shift in seconds from UTC
} OpenWeatherMapCurrentData;
OpenWeatherMapCurrentData owmCurrentData;  // global variable for OWM current data

// OpenWeatherMap Air Quality data
typedef struct {
  // float lon;   // "lon": 8.54
  // float lat;   // "lat": 47.37
  uint16_t aqi;   // "aqi": 2
  // float co;    // "co": 453.95, in μg/m3
  // float no;    // "no": 0.47, in μg/m3
  // float no2;   // "no2": 52.09, in μg/m3
  // float o3;    // "o3": 17.17, in μg/m3
  // float so2;   // "so2": 7.51, in μg/m3
  float pm25;     // "pm2.5": 8.04, in μg/m3
  // float pm10;  // "pm10": 9.96, in μg/m3
  // float nh3;   // "nh3": 0.86, in μg/m3
} OpenWeatherMapAirQuality;
OpenWeatherMapAirQuality owmAirQuality;  // global variable for OWM current data

bool screenWasTouched = false;

// Set first screen to display.  If that first screen is the screen saver then we need to
// have the saved screen be something else so it'll get switched to on the first touch
uint8_t screenCurrent = SCREEN_SAVER;
uint8_t screenSaved   = SCREEN_INFO; // Saved when screen saver engages so it can be restored

void setup() 
{
  // handle Serial first so debugMessage() works
  #ifdef DEBUG
    Serial.begin(115200);
    // wait for serial port connection
    while (!Serial);
    // Display key configuration parameters
    debugMessage(String("Starting Powered Air Quality with ") + sensorSampleInterval + " second sample interval",1);
    #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
      debugMessage(String("Report interval is ") + sensorReportInterval + " minutes",1);
    #endif
    debugMessage(String("Internet reconnect delay is ") + networkConnectAttemptInterval + " seconds",2);
  #endif

  // generate random numbers for every boot cycle
  randomSeed(analogRead(0));

  // initialize screen first to display hardware error messages
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  display.begin();
  display.setRotation(screenRotation);
  display.setTextWrap(false);
  display.fillScreen(ILI9341_BLACK);
  screenAlert("Initializing");

  // Initialize PM25 sensor
  if (!sensorPMInit()) {
    debugMessage("SEN5X initialization failure", 1);
    screenAlert("No SEN5X");
  }

  // Initialize SCD4X
  if (!sensorCO2Init()) {
    debugMessage("SCD4X initialization failure",1);
    screenAlert("No SCD4X");
    // This error often occurs right after a firmware flash and reset.
    // Hardware deep sleep typically resolves it, so quickly cycle the hardware
    powerDisable(hardwareRebootInterval);
  }

  // Setup the VSPI to use custom pins for the touchscreen
  vspi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(vspi);

  // start WiFi (for OWM)
  if (!networkConnect())
    hardwareData.rssi = 0;            // 0 = no WiFi

  // start tracking timers
  timeLastSample = -(sensorSampleInterval*1000); // forces immediate sample in loop()
  timeLastInput = timeLastReport = millis(); // We'll count starup as a "touch"
}

void loop()
{
  // update current timer value
  uint32_t timeCurrent = millis();

  // User input = change screens. If screen saver is active a touch means revert to the
  // previously active screen, otherwise step to the next screen
  boolean istouched = ts.touched();
  if (istouched)
  {
    // If screen saver was active, switch to the previous active screen
    if( screenCurrent == SCREEN_SAVER) {
        screenCurrent = screenSaved;
        debugMessage(String("touchscreen pressed, screen saver off => ") + screenCurrent,1);
        screenUpdate(true);
    }
    // Otherwise step to the next screen (and wrap around if necessary)
    else {
      ((screenCurrent + 1) >= screenCount) ? screenCurrent = 0 : screenCurrent ++;
      // Allow stepping through screens to include the screen saver screen, in which case
      // it is up not because the screen saver timer went off but because it was the next
      // screen in sequence.  In that case, act like the "saved" screen is the INFO screen
      // so it'll be switched to next in the overall sequence.
      if(screenCurrent == SCREEN_SAVER) {
        screenSaved = SCREEN_INFO;  // Technically, (SCREEN_SAVER+1) to preserve the sequence
      }
      debugMessage(String("touchscreen pressed, switch to screen ") + screenCurrent,1);
      screenUpdate(true);
    }
    // Save time touch input occurred
    timeLastInput = millis();
  }
  //  No touch input received, is it time to enable the screensaver?
  else {
    // If we're not already in screen saver mode, is it time it should be enabled?
    if(screenCurrent != SCREEN_SAVER) {
      if( (timeCurrent - timeLastInput) > (screenSaverInterval*1000)) {
        // Activate screen saver, retaining current screen for easy return
        screenSaved = screenCurrent;
        screenCurrent = SCREEN_SAVER;
        debugMessage(String("Screen saver engaged, will restore to ") + screenSaved,1);
        screenUpdate(true);
      }
    }
  }

  // is it time to read the sensor?
  if((timeCurrent - timeLastSample) >= (sensorSampleInterval * 1000)) // converting sensorSampleInterval into milliseconds
  {
    if (!sensorPMRead())
    {
      // TODO: what else to do here, see OWM Reads...
    }

    if (!sensorCO2Read())
    {
      screenAlert("CO2 read fail");
    }

    // Get local weather and air quality info from Open Weather Map
    if (!OWMCurrentWeatherRead()) {
      owmCurrentData.tempF = 10000;
    }
    
    if (!OWMAirPollutionRead()) {
      owmAirQuality.aqi = 10000;
    }

    // add to the running totals
    numSamples++;
    totalTemperatureF.include(sensorData.ambientTemperatureF);
    totalHumidity.include(sensorData.ambientHumidity);
    totalCO2.include(sensorData.ambientCO2);
    totalVOC.include(sensorData.vocIndex);
    totalPM25.include(sensorData.pm25);

    debugMessage(String("Sample #") + numSamples + ", running totals: ",2);
    debugMessage(String("temperatureF total: ") + totalTemperatureF.getTotal(),2);
    debugMessage(String("Humidity total: ") + totalHumidity.getTotal(),2);
    debugMessage(String("CO2 total: ") + totalCO2.getTotal(),2);    
    debugMessage(String("VOC total: ") + totalVOC.getTotal(),2);
    debugMessage(String("PM25 total: ") + totalPM25.getTotal(),2);

    screenUpdate(false);

    // Save last sample time
    timeLastSample = millis();
  }

  // do we have network endpoints to report to?
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(HARDWARE_SIMULATE)
    // is it time to report to the network endpoints?
    if ((timeCurrent - timeLastReport) >= (sensorReportInterval * 60 * 1000))  // converting sensorReportInterval into milliseconds
    {
      // do we have samples to report?
      if (numSamples != 0) 
      {
        // Get averaged sample values from the Measure utliity class objects
        float avgtemperatureF = totalTemperatureF.getAverage(); // average temperature over report interval
        float avgHumidity = totalHumidity.getAverage();         // average humidity over report interval
        uint16_t avgCO2 = totalCO2.getAverage();                // average CO2 over report interval
        float avgVOC = totalVOC.getAverage();                   // average VOC over report interval
        float avgPM25 = totalPM25.getAverage();                 // average PM2.5 over report interval
        float maxPM25 = totalPM25.getMax();                     // maximum PM2.5 over report interval
        float minPM25 = totalPM25.getMin();                     // minimum PM2.5 over report interval

        debugMessage("----- Reporting -----",1);
        debugMessage(String("Reporting averages (") + sensorReportInterval + " minute): ",1);
        debugMessage(String("Temp: ") + avgtemperatureF + " F",1);
        debugMessage(String("Humidity: ") + avgHumidity + "%",1);
        debugMessage(String("CO2: ") + avgCO2 + " ppm",1);
        debugMessage(String("VOC: ") + avgVOC,1);
        debugMessage(String("PM2.5: ") + avgPM25 + " = AQI " + pm25toAQI(avgPM25),1);

        if (networkConnect())
        {
          /* Post both the current readings and historical max/min readings to the internet */
          // Also post the AQI sensor data to ThingSpeak
          #ifdef THINGSPEAK
            post_thingspeak(avgPM25, pm25toAQI(minPm25), pm25toAQI(maxPm25), pm25toAQI(avgPM25));
          #endif

          #ifdef INFLUX
            if (!post_influx(avgPM25, avgtemperatureF, avgVOC, avgHumidity, avgCO2 , hardwareData.rssi))
              debugMessage("Did not write to influxDB",1);
          #endif

          #ifdef MQTT
            if (!mqttDeviceWiFiUpdate(hardwareData.rssi))
                debugMessage("Did not write device data to MQTT broker",1);
            if ((!mqttSensorTemperatureFUpdate(avgtemperatureF)) || (!mqttSensorHumidityUpdate(avgHumidity)) || (!mqttSensorPM25Update(avgPM25)) || (!mqttSensorVOCIndexUpdate(avgVOC)) || (!mqttSensorCO2Update(avgCO2)))
                debugMessage("Did not write environment data to MQTT broker",1);
            #ifdef HASSIO_MQTT
              debugMessage("Establishing MQTT for Home Assistant",1);
              // Either configure sensors in Home Assistant's configuration.yaml file
              // directly or attempt to do it via MQTT auto-discovery
              // hassio_mqtt_setup();  // Config for MQTT auto-discovery
              hassio_mqtt_publish(avgPM25, avgtemperatureF, avgVOC, avgHumidity);
            #endif
          #endif
        }
        // Reset sample counters
        numSamples = 0;
        totalTemperatureF.clear();
        totalHumidity.clear();
        totalCO2.clear();
        totalVOC.clear();
        totalPM25.clear();

        // save last report time
        timeLastReport = millis();
      }
    }
  #endif
}

void screenUpdate(bool firstTime) 
{
  switch(screenCurrent) {
    case SCREEN_SAVER:
      screenSaver();
      break;
    case SCREEN_INFO:
      screenCurrentInfo();
      break;
    case SCREEN_VOC:
      screenVOC();
      break;
    case SCREEN_COLOR:
      screenColor();
      break;
    case SCREEN_AGGREGATE:
      screenAggregateData();
      break;
    case SCREEN_GRAPH:
      screenGraph();
      break;
    default:
      // This shouldn't happen, but if it does...
      screenCurrentInfo();
      debugMessage("bad screen ID",1);
      break;
  }
}

void screenCurrentInfo() 
// Display current particulate matter, CO2, and local weather on screen
{
  // screen layout assists in pixels
  const uint16_t yStatusRegion = display.height()/8;
  const uint16_t xOutdoorMargin = ((display.width() / 2) + xMargins);
  // temp & humidity
  const uint16_t xTempModifier = 15;
  const uint16_t xHumidityModifier = 60;
  const uint16_t yTempHumdidity = (display.height()*0.9);
  // pm25 rings
  const uint16_t xIndoorPMCircle = (display.width() / 4);
  const uint16_t xOutdoorPMCircle = ((display.width() / 4) * 3);
  const uint16_t yPMCircles = 110;
  const uint16_t circleRadius = 65;
  // inside the pm25 rings
  const uint16_t xIndoorCircleText = (xIndoorPMCircle - 18);
  const uint16_t yAQIValue = 160;
  const uint16_t xWeatherIcon = xOutdoorPMCircle - 18;
  const uint16_t yWeatherIcon = yPMCircles - 20;
  
  //IMPROVEMENT: removed deprecated assists and code
  // const uint16_t yCO2 = 160;
  // const uint16_t ySparkline = 40;
  // const uint8_t legendHeight = 15;
  // const uint8_t legendWidth = 10;
  // const uint16_t xLegend = ((display.width() / 2) - legendWidth);
  // const uint16_t yLegend = 110;
  // const uint16_t xPMLabel = ((display.width() / 2) - 25);
  // const uint16_t yPMLabel = 125;

  debugMessage("screenCurrentInfo() start", 1);

  // clear screen
  display.fillScreen(ILI9341_BLACK);

  // status region
  display.fillRect(0,0,display.width(),yStatusRegion,ILI9341_DARKGREY);
  // split indoor v. outside
  display.drawFastVLine((display.width() / 2), yStatusRegion, display.height(), ILI9341_WHITE);
  // screen helpers in status region
  // IMPROVEMENT: Pad the initial X coordinate by the actual # of bars
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), (yMargins + (5*wifiBarHeightIncrement)), wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);

  // // PM2.5 color legend
  // for(uint8_t loop = 0; loop < 4; loop++){
  //   display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  // }

  // // PM2.5 legend label
  // display.setTextColor(ILI9341_WHITE);
  // display.setFont();
  // display.setCursor(xPMLabel,yPMLabel);
  // display.print("PM2.5");

  // Indoor
  // Indoor temp
  display.setFont(&FreeSans12pt7b);
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(xMargins + xTempModifier, yTempHumdidity);
  display.print(String((uint8_t)(sensorData.ambientTemperatureF + .5)));
  display.setFont(&meteocons12pt7b);
  display.print("+");

  // Indoor humidity
  display.setFont(&FreeSans12pt7b);
  if ((sensorData.ambientHumidity<40) || (sensorData.ambientHumidity>60))
    display.setTextColor(ILI9341_RED);
  else
    display.setTextColor(ILI9341_GREEN);
  display.setCursor(xMargins + xTempModifier + xHumidityModifier, yTempHumdidity);
  display.print(String((uint8_t)(sensorData.ambientHumidity + 0.5)));
  // IMPROVEMENT: original icon ratio was 5:7?
  // IMPROVEMENT: move this into meteoicons so it can be inline text
  display.drawBitmap(xMargins + xTempModifier + xHumidityModifier + 35, yTempHumdidity - 21, epd_bitmap_humidity_icon_sm4, 20, 28, ILI9341_WHITE);

  // Indoor PM2.5 ring
  display.fillCircle(xIndoorPMCircle,yPMCircles,circleRadius,warningColor[aqiRange(sensorData.pm25)]);
  display.fillCircle(xIndoorPMCircle,yPMCircles,circleRadius*0.8,ILI9341_BLACK);

  // // Indoor PM2.5 value
  // display.setFont(&FreeSans12pt7b);
  // display.setTextColor(ILI9341_WHITE);
  // display.setCursor(xIndoorPMCircle,yPMCircles);
  // display.print(int(sensorData.pm25+.5));

  // Indoor CO2 level inside the circle
  // CO2 value line
  display.setFont(&FreeSans12pt7b);
  display.setTextColor(warningColor[co2Range(sensorData.ambientCO2)]);  // Use highlight color look-up table
  display.setCursor(xIndoorCircleText,yPMCircles);
  display.print(sensorData.ambientCO2);

  // CO2 label line
  display.setFont(&FreeSans12pt7b);
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(xIndoorCircleText,yPMCircles+23);
  display.print("CO");
  // subscript
  display.setFont(&FreeSans9pt7b);
  display.setCursor(xIndoorCircleText+35,yPMCircles+33);
  display.print("2");

  // Outside

    // do we have OWM Air Quality data to display?
  if (owmAirQuality.aqi != 10000) {

    // Outside PM2.5
    display.fillCircle(xOutdoorPMCircle,yPMCircles,circleRadius,warningColor[aqiRange(owmAirQuality.pm25)]);
    display.fillCircle(xOutdoorPMCircle,yPMCircles,circleRadius*0.8,ILI9341_BLACK);

    // // PM2.5 numeric value (displayed inside circle)
    // display.setFont(&FreeSans12pt7b);
    // display.setCursor(xOutdoorPMCircle,yPMCircles);
    // display.setTextColor(ILI9341_WHITE);
    // display.print(int(owmAirQuality.pm25+.5));

    // Outside air quality index (AQI)
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(ILI9341_WHITE);
    // IMPROVEMENT: Dynamic x coordinate based on text length
    display.setCursor((xOutdoorPMCircle - ((circleRadius*0.8)-10)), yPMCircles+10);
    display.print(OWMAQILabels[(owmAirQuality.aqi)]);
    display.setCursor(xOutdoorPMCircle-15,yPMCircles + 30);
    display.print("AQI");
  }

  // do we have OWM Current data to display?
  if (owmCurrentData.tempF != 10000) {
    // location label
    display.setFont(&FreeSans12pt7b);
    display.setTextColor(ILI9341_WHITE);
    // IMPROVEMENT: Dynamic x coordinate based on text length
    display.setCursor((display.width()/4), ((display.height()/8)-7));
    display.print(owmCurrentData.cityName);

    // Outside temp
    display.setCursor(xOutdoorMargin + xTempModifier, yTempHumdidity);
    display.print(String((uint8_t)(owmCurrentData.tempF + 0.5)));
    display.setFont(&meteocons12pt7b);
    display.print("+");

    // Outside humidity
    display.setFont(&FreeSans12pt7b);
    if ((owmCurrentData.humidity<40) || (owmCurrentData.humidity>60))
      display.setTextColor(ILI9341_RED);
    else
      display.setTextColor(ILI9341_GREEN);
    display.setCursor(xOutdoorMargin + xTempModifier + xHumidityModifier, yTempHumdidity);
    display.print(String((uint8_t)(owmCurrentData.humidity + 0.5)));
    // IMPROVEMENT: original icon ratio was 5:7?
    // IMPROVEMENT: move this into meteoicons so it can be inline text
    display.drawBitmap(xOutdoorMargin + xTempModifier + xHumidityModifier + 35, yTempHumdidity - 21, epd_bitmap_humidity_icon_sm4, 20, 28, ILI9341_WHITE);

    // weather icon
    String weatherIcon = OWMtoMeteoconIcon(owmCurrentData.icon);
    // if getMeteoIcon doesn't have a matching symbol, skip display
    if (weatherIcon != ")") {
      // display icon
      display.setFont(&meteocons16pt7b);
      display.setTextColor(ILI9341_WHITE);
      display.setCursor(xWeatherIcon, yWeatherIcon);
      display.print(weatherIcon);
    }
  }
  debugMessage("screenCurrentInfo() end", 1);
}

void screenAggregateData()
// Displays minimum, average, and maximum values for CO2, temperature and humidity
// using a table-style layout (with labels)
{

  const uint16_t xHeaderColumn = 10;
  const uint16_t xCO2Column = 70;
  const uint16_t xTempColumn = 130;
  const uint16_t xHumidityColumn = 200;
  const uint16_t yHeaderRow = 10;
  const uint16_t yMaxRow = 40;
  const uint16_t yAvgRow = 70;
  const uint16_t yMinRow = 100;

  // clear screen
  display.fillScreen(ILI9341_BLACK);

  // display headers
  display.setFont();  // Revert to built-in font
  display.setTextSize(2);
  display.setTextColor(ILI9341_WHITE);
  // column
  display.setCursor(xCO2Column, yHeaderRow); display.print("CO2");
  display.setCursor(xTempColumn, yHeaderRow); display.print("  F");
  display.setCursor(xHumidityColumn, yHeaderRow); display.print("RH");
  // row
  display.setCursor(xHeaderColumn, yMaxRow); display.print("Max");
  display.setCursor(xHeaderColumn, yAvgRow); display.print("Avg");
  display.setCursor(xHeaderColumn, yMinRow); display.print("Min");

  // Fill in the maximum values row
  display.setCursor(xCO2Column, yMaxRow);
  display.setTextColor(warningColor[co2Range(totalCO2.getMax())]);  // Use highlight color look-up table
  display.print(totalCO2.getMax(),0);
  display.setTextColor(ILI9341_WHITE);
  
  display.setCursor(xTempColumn, yMaxRow); display.print(totalTemperatureF.getMax(),1);
  display.setCursor(xHumidityColumn, yMaxRow); display.print(totalHumidity.getMax(),0);

  // Fill in the average value row
  display.setCursor(xCO2Column, yAvgRow);
  display.setTextColor(warningColor[co2Range(totalCO2.getAverage())]);  // Use highlight color look-up table
  display.print(totalCO2.getAverage(),0);
  display.setTextColor(ILI9341_WHITE);

  display.setCursor(xTempColumn, yAvgRow); display.print(totalTemperatureF.getAverage(),1);
  display.setCursor(xHumidityColumn, yAvgRow); display.print(totalHumidity.getAverage(),0);

  // Fill in the minimum value row
  display.setCursor(xCO2Column,yMinRow);
  display.setTextColor(warningColor[co2Range(totalCO2.getMin())]);  // Use highlight color look-up table
  display.print(totalCO2.getMin(),0);
  display.setTextColor(ILI9341_WHITE);

  display.setCursor(xTempColumn,yMinRow); display.print(totalTemperatureF.getMin(),1);
  display.setCursor(xHumidityColumn,yMinRow); display.print(totalHumidity.getMin(),0);
}

void screenAlert(String messageText)
// Display error message centered on screen
{
  debugMessage(String("screenAlert '") + messageText + "' start",1);

  int16_t x1, y1;
  uint16_t width, height;

  display.setTextColor(ILI9341_WHITE);
  display.setFont(&FreeSans24pt7b);
  // Clear the screen
  display.fillScreen(ILI9341_BLACK);
  display.getTextBounds(messageText.c_str(), 0, 0, &x1, &y1, &width, &height);
  if (width >= display.width()) {
    debugMessage(String("ERROR: screenAlert '") + messageText + "' is " + abs(display.width()-width) + " pixels too long", 1);
  }

  display.setCursor(display.width() / 2 - width / 2, display.height() / 2 + height / 2);
  display.print(messageText);
  debugMessage("screenAlert end",1);
}

void screenGraph()
// Displays CO2 values over time as a graph
{
  // screen layout assists in pixels
  // const uint16_t ySparkline = 95;
  // const uint16_t sparklineHeight = 40;

  debugMessage("screenGraph start",1);
  screenAlert("Graph");
  debugMessage("screenGraph end",1);
}

void screenColor()
// Represents CO2 and PM25 values as a single color rectangles
{
  debugMessage("screenColor start",1);
  display.setFont(&FreeSans18pt7b);
  display.setTextColor(ILI9341_WHITE);
  display.fillScreen(ILI9341_BLACK);
  display.fillRoundRect(0, 0, ((display.width()/2)-2), display.height(), 4, warningColor[co2Range(sensorData.ambientCO2)]);
  display.setCursor(display.width()/8,display.height()/2);
  display.print("CO2");
  display.fillRoundRect(((display.width()/2)+2), 0, ((display.width()/2)-2), display.height(), 4, warningColor[aqiRange(sensorData.pm25)]);
  display.setCursor(display.width()*5/8,display.height()/2);
  display.print("PM25");
  debugMessage("screenColor end",1);
}

void screenSaver()
// Display current CO2 reading at a random location (e.g. "screen saver")
{
  debugMessage("screenSaver start",1);
  display.fillScreen(ILI9341_BLACK);
  display.setTextSize(1);  // Needed so custom fonts scale properly
  display.setFont(&FreeSans24pt7b);

  // Pick a random location that'll show up
  int16_t x = random(xMargins,display.width()-xMargins-72);  // 64 pixels leaves room for 4 digit CO2 value
  int16_t y = random(44,display.height()-yMargins); // 35 pixels leaves vertical room for text display
  display.setCursor(x,y);
  display.setTextColor(warningColor[co2Range(sensorData.ambientCO2)]);  // Use highlight color LUT
  display.println(sensorData.ambientCO2);
  debugMessage("screenSaver end",1);
}

void screenVOC()
{
  // screen layout assists in pixels
  const uint8_t legendHeight = 20;
  const uint8_t legendWidth = 10;
  const uint16_t xLegend = (display.width() - xMargins - legendWidth);
  const uint16_t yLegend = ((display.height() /2 ) + (legendHeight * 2));
  const uint16_t circleRadius = 100;
  const uint16_t xVOCCircle = (display.width() / 2);
  const uint16_t yVOCCircle = (display.height() / 2);
  const uint16_t xVOCLabel = (display.width() - xMargins - legendWidth);
  const uint16_t yVOCLabel = ((display.height() /2 ) + (legendHeight * 2) + 25);

  debugMessage("screenVOC start",1);

  // clear screen
  display.fillScreen(ILI9341_BLACK);

  // VOC color circle
  display.fillCircle(xVOCCircle,yVOCCircle,circleRadius,warningColor[vocRange(sensorData.vocIndex)]);

  // VOC color legend
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  // VOC legend label
  display.setTextColor(ILI9341_WHITE);
  display.setFont();
  display.setCursor(xVOCLabel,yVOCLabel);
  display.print("VOC");

  // VOC value (displayed inside circle)
  display.setFont(&FreeSans18pt7b);
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(xVOCCircle,yVOCCircle);
  display.print(int(sensorData.vocIndex+.5));

  debugMessage("screenVOC end",1);
}

void screenHelperWiFiStatus(uint16_t initialX, uint16_t initialY, uint8_t barWidth, uint8_t barHeightIncrement, uint8_t barSpacing)
// helper function for screenXXX() routines that draws WiFi signal strength
{
  if (hardwareData.rssi != 0) {
    // Convert RSSI values to a 5 bar visual indicator
    // >90 means no signal
    uint8_t barCount = constrain((6 - ((hardwareData.rssi / 10) - 3)), 0, 5);
    if (barCount > 0) {
      // <50 rssi value = 5 bars, each +10 rssi value range = one less bar
      // draw bars to represent WiFi strength
      for (uint8_t loop = 1; loop <= barCount; loop++) {
        display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, ILI9341_WHITE);
      }
      debugMessage(String("WiFi signal strength on screen as ") + barCount + " bars", 2);
    } else {
      // you could do a visual representation of no WiFi strength here
      debugMessage("RSSI too low, not displayed on screen", 1);
    }
  }
}

void screenHelperStatusMessage(uint16_t initialX, uint16_t initialY, String messageText)
// helper function for screenXXX() routines that draws a status message
// uses system default font, so text drawn x+,y+ from initialX,Y
{
  // IMPROVEMENT: Screen dimension boundary checks for function parameters
  #ifdef SCREEN
    display.setFont();  // resets to system default monospace font (6x8 pixels)
    display.setCursor(initialX, initialY);
    display.print(messageText);
  #endif
}

// Hardware simulation routines
#ifdef HARDWARE_SIMULATE
  void OWMCurrentWeatherSimulate()
  // Simulates Open Weather Map (OWM) Current Weather data
  {
    // IMPROVEMENT: variable length names
    owmCurrentData.cityName = "Pleasantville";
    // Temperature
    owmCurrentData.tempF = (random(sensorTempMinF,sensorTempMaxF) / 100.0);
    // Humidity
    owmCurrentData.humidity = random(sensorHumidityMin,sensorHumidityMax) / 100.0;
    // IMPROVEMENT: variable icons
    owmCurrentData.icon = "09d";
    debugMessage(String("SIMULATED OWM Current Weather: ") + owmCurrentData.tempF + "F, " + owmCurrentData.humidity + "%", 1);
  }

  void OWMAirPollutionSimulate()
  // Simulates Open Weather Map (OWM) Air Pollution data
  {
    owmAirQuality.aqi = random(OWMAQIMin, OWMAQIMax);
    owmAirQuality.pm25 = random(OWMPM25Min, OWMPM25Max) / 100.0;
    debugMessage(String("SIMULATED OWM Air Pollution PM2.5: ") + owmAirQuality.pm25 + ", AQI: " + owmAirQuality.aqi,1);
  }

  void networkSimulate()
  // Simulates successful WiFi connection data
  {
    // IMPROVEMENT: simulate IP address?
    hardwareData.rssi = random(networkRSSIMin, networkRSSIMax);
    debugMessage(String("SIMULATED WiFi RSSI: ") + hardwareData.rssi,1);
  }

  void  sensorPMSimulate()
  // Simulates sensor reading from PMSA003I sensor
  {
    // Note: tempF and humidity come from SCD4X simulation

    sensorData.pm1 = random(sensorPMMin, sensorPMMax) / 100.0;
    sensorData.pm10 = random(sensorPMMin, sensorPMMax) / 100.0;
    sensorData.pm25 = random(sensorPMMin, sensorPMMax) / 100.0;
    sensorData.pm4 = random(sensorPMMin, sensorPMMax) / 100.0;
    sensorData.vocIndex = random(sensorVOCMin, sensorVOCMax) / 100.0;
    // IMPROVEMENT: not supported on SEN54, so return NAN
    //sensorData.noxIndex = random(sensorVOCMin, sensorVOCMax) / 10.0;

    debugMessage(String("SIMULATED SEN5x PM2.5: ")+sensorData.pm25+" ppm, VOC index: " + sensorData.vocIndex,1);
  }

  void sensorCO2Simulate()
  // Simulate ranged data from the SCD40
  // IMPROVEMENT: implement stable, rapid rise and fall 
  {
    sensorData.ambientTemperatureF = (random(sensorTempMinF,sensorTempMaxF) / 100.0);
    sensorData.ambientHumidity = random(sensorHumidityMin,sensorHumidityMax) / 100.0;
    sensorData.ambientCO2 = random(sensorCO2Min, sensorCO2Max);
    debugMessage(String("SIMULATED SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
  }
#endif

bool OWMCurrentWeatherRead()
// Gets Open Weather Map Current Weather data
{
  #ifdef HARDWARE_SIMULATE
    OWMCurrentWeatherSimulate();
    return true;
  #else
    // check for internet connectivity
    if (hardwareData.rssi != 0) 
    {
      String jsonBuffer;

      // Get local weather conditions
      String serverPath = String(OWM_SERVER) + OWM_WEATHER_PATH + OWM_LAT_LONG + "&units=imperial" + "&APPID=" + OWM_KEY;

      jsonBuffer = networkHTTPGETRequest(serverPath.c_str());
      debugMessage("Raw JSON from OWM Current Weather feed", 2);
      debugMessage(jsonBuffer, 2);
      if (jsonBuffer == "HTTP GET error") {
        return false;
      }

      DynamicJsonDocument doc(2048);

      DeserializationError error = deserializeJson(doc, jsonBuffer);

      if (error) {
        debugMessage(String("deserializeJson failed with error message: ") + error.c_str(), 1);
        return false;
      }

      uint8_t code = (uint8_t)doc["cod"];
      if (code != 200) {
        debugMessage(String("OWM error: ") + (const char *)doc["message"], 1);
        return false;
      }

      // owmCurrentData.lat = (float) doc["coord"]["lat"];
      // owmCurrentData.lon = (float) doc["coord"]["lon"];

      // owmCurrentData.main = (const char*) doc["weather"][0]["main"];
      // owmCurrentData.description = (const char*) doc["weather"][0]["description"];
      owmCurrentData.icon = (const char *)doc["weather"][0]["icon"];

      owmCurrentData.cityName = (const char *)doc["name"];
      // owmCurrentData.visibility = (uint16_t) doc["visibility"];
      // owmCurrentData.timezone = (time_t) doc["timezone"];

      // owmCurrentData.country = (const char*) doc["sys"]["country"];
      // owmCurrentData.observationTime = (time_t) doc["dt"];
      // owmCurrentData.sunrise = (time_t) doc["sys"]["sunrise"];
      // owmCurrentData.sunset = (time_t) doc["sys"]["sunset"];

      owmCurrentData.tempF = (float)doc["main"]["temp"];
      // owmCurrentData.pressure = (uint16_t) doc["main"]["pressure"];
      owmCurrentData.humidity = (uint8_t)doc["main"]["humidity"];
      // owmCurrentData.tempMin = (float) doc["main"]["temp_min"];
      // owmCurrentData.tempMax = (float) doc["main"]["temp_max"];

      // owmCurrentData.windSpeed = (float) doc["wind"]["speed"];
      // owmCurrentData.windDeg = (float) doc["wind"]["deg"];
      debugMessage(String("OWM Current Weather: ") + owmCurrentData.tempF + "F, " + owmCurrentData.humidity + "% RH", 1);
      return true;
    }
    return false;
  #endif
}

bool OWMAirPollutionRead()
// stores local air pollution info from Open Weather Map in environment global
{
  #ifdef HARDWARE_SIMULATE
    OWMAirPollutionSimulate();
    return true;
  #else
    // check for internet connectivity
    if (hardwareData.rssi != 0)
    {
      String jsonBuffer;

      // Get local AQI
      String serverPath = String(OWM_SERVER) + OWM_AQM_PATH + OWM_LAT_LONG + "&APPID=" + OWM_KEY;

      jsonBuffer = networkHTTPGETRequest(serverPath.c_str());
      debugMessage("Raw JSON from OWM AQI feed", 2);
      debugMessage(jsonBuffer, 2);
      if (jsonBuffer == "HTTP GET error") {
        return false;
      }

      DynamicJsonDocument doc(384);

      DeserializationError error = deserializeJson(doc, jsonBuffer);
      if (error) {
        debugMessage(String("deserializeJson failed with error message: ") + error.c_str(), 1);
        return false;
      }

      // owmAirQuality.lon = (float) doc["coord"]["lon"];
      // owmAirQuality.lat = (float) doc["coord"]["lat"];
      JsonObject list_0 = doc["list"][0];
      owmAirQuality.aqi = list_0["main"]["aqi"];
      JsonObject list_0_components = list_0["components"];
      // owmAirQuality.co = (float) list_0_components["co"];
      // owmAirQuality.no = (float) list_0_components["no"];
      // owmAirQuality.no2 = (float) list_0_components["no2"];
      // owmAirQuality.o3 = (float) list_0_components["o3"];
      // owmAirQuality.so2 = (float) list_0_components["so2"];
      owmAirQuality.pm25 = (float)list_0_components["pm2_5"];
      // owmAirQuality.pm10 = (float) list_0_components["pm10"];
      // owmAirQuality.nh3 = (float) list_0_components["nh3"];
      debugMessage(String("OWM PM2.5: ") + owmAirQuality.pm25 + ", AQI: " + owmAirQuality.aqi,1);
      return true;
    }
    return false;
  #endif
}

String OWMtoMeteoconIcon(String icon)
// Maps OWM icon data to the appropropriate Meteocon font character
// https://www.alessioatzeni.com/meteocons/#:~:text=Meteocons%20is%20a%20set%20of,free%20and%20always%20will%20be.
{
  if (icon == "01d")  // 01d = sunny = Meteocon "B"
    return "B";
  if (icon == "01n")  // 01n = clear night = Meteocon "C"
    return "C";
  if (icon == "02d")  // 02d = partially sunny = Meteocon "H"
    return "H";
  if (icon == "02n")  // 02n = partially clear night = Meteocon "4"
    return "4";
  if (icon == "03d")  // 03d = clouds = Meteocon "N"
    return "N";
  if (icon == "03n")  // 03n = clouds night = Meteocon "5"
    return "5";
  if (icon == "04d")  // 04d = broken clouds = Meteocon "Y"
    return "Y";
  if (icon == "04n")  // 04n = broken night clouds = Meteocon "%"
    return "%";
  if (icon == "09d")  // 09d = rain = Meteocon "R"
    return "R";
  if (icon == "09n")  // 09n = night rain = Meteocon "8"
    return "8";
  if (icon == "10d")  // 10d = light rain = Meteocon "Q"
    return "Q";
  if (icon == "10n")  // 10n = night light rain = Meteocon "7"
    return "7";
  if (icon == "11d")  // 11d = thunderstorm = Meteocon "P"
    return "P";
  if (icon == "11n")  // 11n = night thunderstorm = Meteocon "6"
    return "6";
  if (icon == "13d")  // 13d = snow = Meteocon "W"
    return "W";
  if (icon == "13n")  // 13n = night snow = Meteocon "#"
    return "#";
  if ((icon == "50d") || (icon == "50n"))  // 50d = mist = Meteocon "M"
    return "M";
  // Nothing matched
  debugMessage("OWM icon not matched to Meteocon, why?", 1);
  return ")";
}

bool networkConnect() 
// Connect to WiFi network specified in secrets.h
{
  #ifdef HARDWARE_SIMULATE
    networkSimulate();
    return true;
  #else
    // reconnect to WiFi only if needed
    if (WiFi.status() == WL_CONNECTED) 
    {
      debugMessage("Already connected to WiFi",2);
      return true;
    }
    // set hostname has to come before WiFi.begin
    WiFi.hostname(DEVICE_ID);

    WiFi.begin(WIFI_SSID, WIFI_PASS);

    for (uint8_t loop = 1; loop <= networkConnectAttemptLimit; loop++)
    // Attempts WiFi connection, and if unsuccessful, re-attempts after networkConnectAttemptInterval second delay for networkConnectAttemptLimit times
    {
      if (WiFi.status() == WL_CONNECTED) {
        hardwareData.rssi = abs(WiFi.RSSI());
        debugMessage(String("WiFi IP address lease from ") + WIFI_SSID + ": " + WiFi.localIP().toString(), 1);
        debugMessage(String("WiFi RSSI: ") + hardwareData.rssi + " dBm", 1);
        return true;
      }
      debugMessage(String("Connection attempt ") + loop + " of " + networkConnectAttemptLimit + " to " + WIFI_SSID + " failed", 1);
      debugMessage(String("WiFi status message ") + networkWiFiMessage(WiFi.status()),2);
      // use of delay() OK as this is initialization code
      delay(networkConnectAttemptInterval * 1000);  // converted into milliseconds
    }
    return false;
  #endif
}

void networkDisconnect()
// Disconnect from WiFi network
{
  hardwareData.rssi = 0;
  #ifdef HARDWARE_SIMULATE
    debugMessage("power off: SIMULATED WiFi",1);
    return;
  #else
    // IMPROVEMENT: What if disconnect call fails?
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    debugMessage("power off: WiFi",1);
  #endif
}

#ifndef HARDWARE_SIMULATE
  const char* networkWiFiMessage(wl_status_t status)
  // Converts WiFi.status() to string
  {
    switch (status) {
      case WL_NO_SHIELD: return "WL_NO_SHIELD";
      case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
      case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
      case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
      case WL_CONNECTED: return "WL_CONNECTED";
      case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
      case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
      case WL_DISCONNECTED: return "WL_DISCONNECTED";
    }
  }

  String networkHTTPGETRequest(const char* serverName) 
  {
    String payload = "{}";
    #ifdef HARDWARE_SIMULATE
      return payload;
    #else
      // !!! ESP32 hardware dependent, using Esperif library

      HTTPClient http;

      // servername is domain name w/URL path or IP address w/URL path
      http.begin(client, serverName);

      // Send HTTP GET request
      uint16_t httpResponseCode = http.GET();

      if (httpResponseCode == HTTP_CODE_OK) {
        // HTTP reponse OK code
        payload = http.getString();
      } else {
        debugMessage("HTTP GET error code: " + httpResponseCode,1);
        payload = "HTTP GET error";
      }
      // free resources
      http.end();
      return payload;
    #endif
  }
#endif

bool sensorPMInit()
{
  #ifdef HARDWARE_SIMULATE
    return true;
  #else
    uint16_t error;
    char errorMessage[256];

    // Wire.begin();
    Wire.begin(CYD_SDA, CYD_SCL);
    pmSensor.begin(Wire);

    error = pmSensor.deviceReset();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " error during SEN5x reset", 1);
      return false;
    }

    // set a temperature offset in degrees celsius
    // By default, the temperature and humidity outputs from the sensor
    // are compensated for the modules self-heating. If the module is
    // designed into a device, the temperature compensation might need
    // to be adapted to incorporate the change in thermal coupling and
    // self-heating of other device components.
    //
    // A guide to achieve optimal performance, including references
    // to mechanical design-in examples can be found in the app note
    // “SEN5x – Temperature Compensation Instruction” at www.sensirion.com.
    // Please refer to those application notes for further information
    // on the advanced compensation settings used
    // in `setTemperatureOffsetParameters`, `setWarmStartParameter` and
    // `setRhtAccelerationMode`.
    //
    // Adjust tempOffset to account for additional temperature offsets
    // exceeding the SEN module's self heating.
    // float tempOffset = 0.0;
    // error = pmSensor.setTemperatureOffsetSimple(tempOffset);
    // if (error) {
    //   errorToString(error, errorMessage, 256);
    //   debugMessage(String(errorMessage) + " error setting temp offset", 1);
    // } else {
    //   debugMessage(String("Temperature Offset set to ") + tempOffset + " degrees C", 2);
    // }

    // Start Measurement
    error = pmSensor.startMeasurement();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " error during SEN5x startMeasurement", 1);
      return false;
    }
    debugMessage("SEN5X starting periodic measurements",1);
    return true;
  #endif
}

bool sensorPMRead()
{
  #ifdef HARDWARE_SIMULATE
    sensorPMSimulate();
    return true;
  #else
    uint16_t error;
    char errorMessage[256];
    // we'll use the SCD4X values for these
    // IMPROVEMENT: Compare to SCD4X values?
    float sen5xTempF;
    float sen5xHumidity;

    debugMessage("SEN5X read initiated",1);

    error = pmSensor.readMeasuredValues(
      sensorData.pm1, sensorData.pm25, sensorData.pm4,
      sensorData.pm10, sen5xHumidity, sen5xTempF, sensorData.vocIndex,
      sensorData.noxIndex);
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " error during SEN5x read",1);
      return false;
    }
    debugMessage(String("SEN5X PM2.5: ") + sensorData.pm25 + ", VOC: " + sensorData.vocIndex, 1);
    return true;
  #endif
}

bool sensorCO2Init()
// initializes SCD4X to read
{
  #ifdef HARDWARE_SIMULATE
    return true;
 #else
    char errorMessage[256];
    uint16_t error;

    // Wire.begin();
    // IMPROVEMENT: Do you need another Wire.begin() [see sensorPMInit()]?
    Wire.begin(CYD_SDA, CYD_SCL);
    co2Sensor.begin(Wire, SCD41_I2C_ADDR_62);

    // stop potentially previously started measurement.
    error = co2Sensor.stopPeriodicMeasurement();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD4X stopPeriodicMeasurement()",1);
      return false;
    }

    // Check onboard configuration settings while not in active measurement mode
    // IMPROVEMENT: These don't handle error conditions, which should be rare as caught above
    float offset;
    error = co2Sensor.getTemperatureOffset(offset);
    if (error == 0){
        error = co2Sensor.setTemperatureOffset(sensorTempCOffset);
        if (error == 0)
          debugMessage(String("Initial SCD4X temperature offset ") + offset + " ,set to " + sensorTempCOffset,2);
    }

    // IMPROVEMENT: These don't handle error conditions, which should be rare as caught above
    uint16_t sensor_altitude;
    error = co2Sensor.getSensorAltitude(sensor_altitude);
    if (error == 0){
      error = co2Sensor.setSensorAltitude(SITE_ALTITUDE);  // optimizes CO2 reading
      if (error == 0)
        debugMessage(String("Initial SCD4X altitude ") + sensor_altitude + " meters, set to " + SITE_ALTITUDE,2);
    }

    // Start Measurement.  For high power mode, with a fixed update interval of 5 seconds
    // (the typical usage mode), use startPeriodicMeasurement().  For low power mode, with
    // a longer fixed sample interval of 30 seconds, use startLowPowerPeriodicMeasurement()
    // uint16_t error = co2Sensor.startPeriodicMeasurement();
    error = co2Sensor.startLowPowerPeriodicMeasurement();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD4X startLowPowerPeriodicMeasurement()",1);
      return false;
    }
    else
    {
      debugMessage("SCD4X starting low power periodic measurements",1);
      return true;
    }
  #endif
}

bool sensorCO2Read()
// Description: Sets global environment values from SCD40 sensor
// Parameters: none
// Output : true if successful read, false if not
// Improvement : NA
{
  #ifdef HARDWARE_SIMULATE
    sensorCO2Simulate();
    debugMessage(String("SIMULATED SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
  #else
    char errorMessage[256];
    bool status = false;
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    uint16_t error;
    uint8_t errorCount = 0;

    debugMessage("CO2 sensor read initiated",1);
    while(!status) {
      errorCount++;
      if (errorCount>co2SensorReadFailureLimit)
        break;
      // Is data ready to be read?
      bool isDataReady = false;
      error = co2Sensor.getDataReadyStatus(isDataReady);
      if (error) {
          errorToString(error, errorMessage, 256);
          debugMessage(String("Error trying to execute getDataReadyFlag(): ") + errorMessage,1);
          continue; // Back to the top of the loop
      }
      error = co2Sensor.readMeasurement(co2, temperature, humidity);
      if (error) {
          errorToString(error, errorMessage, 256);
          debugMessage(String("SCD40 executing readMeasurement(): ") + errorMessage,2);
          // Implicitly continues back to the top of the loop
      }
      else if (co2 < sensorCO2Min || co2 > sensorCO2Max)
      {
        debugMessage(String("SCD40 CO2 reading: ") + sensorData.ambientCO2 + " is out of expected range",1);
        //(sensorData.ambientCO2 < sensorCO2Min) ? sensorData.ambientCO2 = sensorCO2Min : sensorData.ambientCO2 = sensorCO2Max;
        // Implicitly continues back to the top of the loop
      }
      else
      {
        // valid measurement available, update globals
        sensorData.ambientTemperatureF = (temperature*1.8)+32.0;
        sensorData.ambientHumidity = humidity;
        sensorData.ambientCO2 = co2;
        debugMessage(String("SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
        status = true;
        break;
      }
    delay(100); // reduces readMeasurement() Not enough data received errors
    }
  #endif
  return(status);
}

uint8_t co2Range(uint16_t co2)
// converts co2 value to index value for labeling and color
{
  uint8_t co2Range;
  if (co2 <= co2Fair) co2Range = 0;
  else if (co2 <= co2Poor) co2Range = 1;
  else if (co2 <= co2Bad) co2Range = 2;
  else co2Range =3;
  debugMessage(String("CO2 input of ") + co2 + " yields co2Range of " + co2Range,2);
  return co2Range;
}

uint8_t aqiRange(float pm25)
// converts pm25 value to index value for labeling and color
{
  uint8_t aqi;
  if (pm25 <= pmFair) aqi = 0;
  else if (pm25 <= pmPoor) aqi = 1;
  else if (pm25 <= pm2Bad) aqi = 2;
  else aqi = 3;
  debugMessage(String("PM2.5 input of ") + pm25 + " yields " + aqi + " aqi",2);
  return aqi;
}

uint8_t vocRange(float vocIndex)
// converts vocIndex value to index value for labeling and color
{
  uint8_t vocRange;
  if (vocIndex <= vocFair) vocRange = 0;
  else if (vocIndex <= vocPoor) vocRange = 1;
  else if (vocIndex <= vocBad) vocRange = 2;
  else vocRange = 3;
  debugMessage(String("VOC index input of ") + vocIndex + " yields " + vocRange + " vocRange",2);
  return vocRange;
}

float pm25toAQI(float pm25)
// Converts pm25 reading to AQI using the AQI Equation
// (https://forum.airnowtech.org/t/the-aqi-equation/169)
{  
  float aqiValue;
  if(pm25 <= 12.0)       aqiValue = (fmap(pm25,  0.0, 12.0,  0.0, 50.0));
  else if(pm25 <= 35.4)  aqiValue = (fmap(pm25, 12.1, 35.4, 51.0,100.0));
  else if(pm25 <= 55.4)  aqiValue = (fmap(pm25, 35.5, 55.4,101.0,150.0));
  else if(pm25 <= 150.4) aqiValue = (fmap(pm25, 55.5,150.4,151.0,200.0));
  else if(pm25 <= 250.4) aqiValue = (fmap(pm25,150.5,250.4,201.0,300.0));
  else if(pm25 <= 500.4) aqiValue = (fmap(pm25,250.5,500.4,301.0,500.0));
  else aqiValue = (505.0); // AQI above 500 not recognized
  debugMessage(String("PM2.5 value of ") + pm25 + " converts to AQI value of " + aqiValue, 2);

  return aqiValue;
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
      Serial.flush();      // Make sure the message gets output (before any sleeping...)
    }
  #endif
}

void powerDisable(uint8_t deepSleepTime)
// turns off component hardware then puts ESP32 into deep sleep mode for specified seconds
{
  debugMessage("powerDisable start",1);

  // power down SCD4X by stopping potentially started measurement then power down SCD4X
  #ifndef HARDWARE_SIMULATE
    uint16_t error = co2Sensor.stopPeriodicMeasurement();
    if (error) {
      char errorMessage[256];
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD4X stopPeriodicMeasurement()",1);
    }
    co2Sensor.powerDown();
    debugMessage("power off: SCD4X",2);
  #endif
  
  // turn off TFT backlight
  digitalWrite(TFT_BACKLIGHT, LOW);

  networkDisconnect();

  esp_sleep_enable_timer_wakeup(deepSleepTime*1000000); // ESP microsecond modifier
  debugMessage(String("powerDisable complete: ESP32 deep sleep for ") + (deepSleepTime) + " seconds",1);
  esp_deep_sleep_start();
}