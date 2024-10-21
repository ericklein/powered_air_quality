/*
  Project:        Powered Air Quality
  Description:    Sample and log indoor air quality via AC powered device

  See README.md for target information
*/

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT
#include "secrets.h"

#ifndef HARDWARE_SIMULATE
  // sensor support
  // instanstiate pm hardware object
  #include "Adafruit_PM25AQI.h"
  Adafruit_PM25AQI pmSensor = Adafruit_PM25AQI();

  // instanstiate scd40 hardware object
  #include <SensirionI2CScd4x.h>
  SensirionI2CScd4x co2Sensor;

  // WiFi support
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
  #elif defined(ESP32)
    #include <WiFi.h>
    #include <HTTPClient.h>
  #else
    #include <WiFiNINA.h> // PyPortal
  #endif
#endif

// button support
// #include <ezButton.h>
// ezButton buttonOne(buttonD1Pin);

// screen support
// 3.2″ ILI9341 320x240 color TFT w/resistive touch screen
#include "Adafruit_ILI9341.h"
Adafruit_ILI9341 display = Adafruit_ILI9341(tft8bitbus, TFT_D0, TFT_WR, TFT_DC, TFT_CS, TFT_RST, TFT_RD);

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
// #include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#include "Fonts/meteocons20pt7b.h"
#include "Fonts/meteocons16pt7b.h"
#include "Fonts/meteocons12pt7b.h"

// Library needed to access Open Weather Map
#include "ArduinoJson.h"  // Needed by OWM retrieval routines

// UI glyphs
#include "glyphs.h"

#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
  WiFiClient client;
  // NTP setup using Esperiff library
  #include <time.h>
#endif

// external function dependencies
#ifdef INFLUX
  extern bool post_influx(float pm25, float aqi, float temperatureF, float vocIndex, float humidity, uint16_t co2, int rssi);
#endif

#ifdef MQTT
  // MQTT uses WiFiClient class to create TCP connections
  WiFiClient client;
  #include <HTTPClient.h>


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

// global variables

// environment sensor data
typedef struct envData
{
  // SCD40 data
  float ambientHumidity;      // RH [%]
  float ambientTemperatureF;
  uint16_t  ambientCO2;

  // Common data to PMSA003I and SEN5x
  float massConcentrationPm2p5;       // PM2.5 [µg/m³]
  // float massConcentrationPm1p0;    // PM1.0 [µg/m³], NAN if unknown
  // float massConcentrationPm10p0;   // PM10.0 [µg/m³], NAN if unknown

  // SEN5x specific data
  // float massConcentrationPm4p0;   // PM4.0 [µg/m³], NAN if unknown
  // float vocIndex;                 // Sensiron VOC Index, NAN in unknown
  // float noxIndex;                 // NAN for unsupported devices (SEN54), also NAN for first 10-11 seconds

  // PMSA003I specific data
  // uint16_t pm10_env;         // Environmental PM1.0
  // uint16_t pm25_env;         // Environmental PM2.5
  // uint16_t pm100_env;        // Environmental PM10.0
  // uint16_t particles_03um;   //< 0.3um Particle Count
  // unit16_t particles_05um;   //< 0.5um Particle Count
  // unit16_t particles_10um;   //< 1.0um Particle Count
  // unit16_t particles_25um;   //< 2.5um Particle Count
  // unit16_t particles_50um;   //< 5.0um Particle Count
  // unit16_t particles_100um;  //< 10.0um Particle Count
} envData;
envData sensorData;

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
  // float lon;    // "lon": 8.54
  // float lat;    // "lat": 47.37
  uint16_t aqi;  // "aqi": 2  [European standards body value]
  // float co;     // "co": 453.95, in μg/m3
  // float no;     // "no": 0.47, in μg/m3
  // float no2;    // "no2": 52.09, in μg/m3
  // float o3;     // "o3": 17.17, in μg/m3
  // float so2;    // "so2": 7.51, in μg/m3
  float pm25;  // "pm2.5": 8.04, in μg/m3
  // float pm10;   // "pm10": 9.96, in μg/m3
  // float nh3;    // "nh3": 0.86, in μg/m3
} OpenWeatherMapAirQuality;
OpenWeatherMapAirQuality owmAirQuality;  // global variable for OWM current data

// forces immediate sample and report in loop()
long timeLastSample = -(sensorSampleInterval*1000);
long timeLastReport = -(sensorReportInterval*1000);

// set first screen to display
uint8_t screenCurrent = 0;

void setup() 
{
  // handle Serial first so debugMessage() works
  #ifdef DEBUG
    Serial.begin(115200);
    // wait for serial port connection
    while (!Serial);

    // Display key configuration parameters
    debugMessage("Powered Air Quality monitor started",1);
    debugMessage(String("Sample interval is ") + sensorSampleInterval + " seconds",1);
    #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
      debugMessage(String("Report interval is ") + sensorReportInterval + " minutes",1);
    #endif
    debugMessage(String("Internet reconnect delay is ") + networkConnectAttemptInterval + " seconds",1);
  #endif

  // initialize screen first to display hardware error messages
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  // pinMode(TFT_RST, OUTPUT);
  // digitalWrite(TFT_RST, HIGH);
  // delay(10);
  // digitalWrite(TFT_RST, LOW);
  // delay(10);
  // digitalWrite(TFT_RST, HIGH);
  // delay(10);

  display.begin();
  display.setTextWrap(false);
  display.fillScreen(ILI9341_BLACK);
  display.setRotation(screenRotation);

  hardwareData.rssi = 0;            // 0 = no WiFi

  // Initialize PM25 sensor
  if (!sensorPMInit()) {
    debugMessage("Environment sensor failed to initialize", 1);
    screenAlert("NO PM25 sensor");
  }

  // Initialize CO2 sensor
  if (!sensorCO2Init()) {
    debugMessage("Environment sensor failed to initialize",1);
    screenAlert("No SCD40");
    // This error often occurs right after a firmware flash and reset.
    // Hardware deep sleep typically resolves it, so quickly cycle the hardware
    //powerDisable(hardwareRebootInterval);
    while(1);
  }

  // buttonOne.setDebounceTime(buttonDebounceDelay); 
  networkConnect();
}

void loop()
{
  // update current timer value
  unsigned long timeCurrent = millis();

  // buttonOne.loop();
  // // check if buttons were pressed
  // if (buttonOne.isReleased())
  // {
  //   ((screenCurrent + 1) >= screenCount) ? screenCurrent = 0 : screenCurrent ++;
  //   debugMessage(String("button 1 press, switch to screen ") + screenCurrent,1);
  //   screenUpdate(true);
  // }

  // is it time to read the sensor?
  if((timeCurrent - timeLastSample) >= (sensorSampleInterval * 1000)) // converting sensorSampleInterval into milliseconds
  {
    if (!sensorPMRead())
    {
      debugMessage("Could not read PMSA003I sensor data",1);
      // TODO: what else to do here
    }
    sensorCO2Read();

    // Get local weather and air quality info from Open Weather Map
    if (!OWMCurrentWeatherRead()) {
      owmCurrentData.tempF = 10000;
      owmCurrentData.humidity = 10000;
    }
    
    if (!OWMAirPollutionRead()) {
      owmAirQuality.aqi = 10000;
    }
    screenCurrentInfo();
    // Save last sample time
    timeLastSample = millis();
  }

  // do we have network endpoints to report to?
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
    // is it time to report to the network endpoints?
    if ((timeCurrent - prevReportMs) >= (sensorReportInterval * 60 * 1000))  // converting sensorReportInterval into milliseconds
    {
      debugMessage("----- Reporting -----",1);
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
      // save last report time
      timeLastReport = millis();
    }
  #endif
}

// void screenUpdate(bool firstTime) 
// {
//   switch(screenCurrent) {
//     case 0:
//       screenSaver();
//       break;
//     case 1:
//       screenCurrentData();
//       break;
//     case 2:
//       screenAggregateData();
//       break;
//     case 3:
//       screenColor();
//       break;
//     case 4:
//       screenGraph();
//       break;
//     default:
//       // This shouldn't happen, but if it does...
//       screenCurrentData();
//       debugMessage("bad screen ID",1);
//       break;
//   }
// }

void screenCurrentInfo() 
// Display current particulate matter, CO2, and local weather on screen
{
  // screen layout assists in pixels
  const uint16_t xOutdoorMargin = ((display.width() / 2) + xMargins);
  const uint16_t yTemperature = 35;
  const uint16_t yLegend = 105;
  const uint16_t legendHeight = 10;
  const uint16_t legendWidth = 5;
  const uint16_t xPMCircle = 46;
  const uint16_t yPMCircle = 75;
  const uint16_t circleRadius = 31;
  const uint16_t xPMLabel = 33;
  const uint16_t yPMLabel = 110;
  const uint16_t xPMValue = 40;
  const uint16_t yPMValue = 80;
  const uint16_t yCO2 = 160;
  const uint16_t ySparkline = 40;

  debugMessage("screenCurrentInfo() start", 1);

  // clear screen
  display.fillScreen(ILI9341_BLACK);

  // borders
  // display.drawFastHLine(0, yStatus, display.width(), GxEPD_BLACK);
  // splitting sensor vs. outside values
  display.drawFastVLine((display.width() / 2), 0, display.height(), ILI9341_WHITE);

  // screen helper routines
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), (yMargins + (5*wifiBarHeightIncrement)), wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);

  // Indoor
  // Indoor temp
  display.setFont(&FreeSans12pt7b);
  display.setCursor(xMargins, yTemperature);
  display.print(String((uint8_t)(sensorData.ambientTemperatureF + .5)));
  display.setFont(&meteocons12pt7b);
  display.print("+");

  // Indoor humidity
  display.setFont(&FreeSans12pt7b);
  display.setCursor(xMargins + 60, yTemperature);
  if ((sensorData.ambientHumidity<40) || (sensorData.ambientHumidity>60))
    display.setTextColor(ILI9341_RED);
  else
    display.setTextColor(ILI9341_GREEN);
  display.print(String((uint8_t)(sensorData.ambientHumidity + 0.5)));
  // original icon ratio was 5:7?
  display.drawBitmap(xMargins + 90, yTemperature - 21, epd_bitmap_humidity_icon_sm4, 20, 28, ILI9341_WHITE);

  // pm25 legend
  display.fillRect(xMargins,yLegend,legendWidth,legendHeight,ILI9341_BLUE);
  display.fillRect(xMargins,yLegend-legendHeight,legendWidth,legendHeight,ILI9341_GREEN);
  display.fillRect(xMargins,(yLegend-(2*legendHeight)),legendWidth,legendHeight,ILI9341_YELLOW);
  display.fillRect(xMargins,(yLegend-(3*legendHeight)),legendWidth,legendHeight,ILI9341_ORANGE);
  display.fillRect(xMargins,(yLegend-(4*legendHeight)),legendWidth,legendHeight,ILI9341_RED);
  display.fillRect(xMargins,(yLegend-(5*legendHeight)),legendWidth,legendHeight,ILI9341_MAGENTA);

  // pm25 level circle
  switch (int(sensorData.massConcentrationPm2p5/50))
  {
    case 0: // good
      display.fillCircle(xPMCircle,yPMCircle,circleRadius,ILI9341_BLUE);
      break;
    case 1: // moderate
      display.fillCircle(xPMCircle,yPMCircle,circleRadius,ILI9341_GREEN);
      break;
    case 2: // unhealthy for sensitive groups
      display.fillCircle(xPMCircle,yPMCircle,circleRadius,ILI9341_YELLOW);
      break;
    case 3: // unhealthy
      display.fillCircle(xPMCircle,yPMCircle,circleRadius,ILI9341_ORANGE);
      break;
    case 4: // very unhealthy
      display.fillCircle(xPMCircle,yPMCircle,circleRadius,ILI9341_RED);
      break;
    case 5: // very unhealthy
      display.fillCircle(xPMCircle,yPMCircle,circleRadius,ILI9341_RED);
      break;
    default: // >=6 is hazardous
      display.fillCircle(xPMCircle,yPMCircle,circleRadius,ILI9341_MAGENTA);
      break;
  }

  // // VoC level circle
  // switch (int(sensorData.vocIndex/100))
  // {
  //   case 0: // great
  //     display.fillCircle(114,75,31,ILI9341_BLUE);
  //     break;
  //   case 1: // good
  //     display.fillCircle(114,75,31,ILI9341_GREEN);
  //     break;
  //   case 2: // moderate
  //     display.fillCircle(114,75,31,ILI9341_YELLOW);
  //     break;
  //   case 3: // 
  //     display.fillCircle(114,75,31,ILI9341_ORANGE);
  //     break;
  //   case 4: // bad
  //     display.fillCircle(114,75,31,ILI9341_RED);
  //     break;
  // }

  // // VoC legend
  // display.fillRect(display.width()-xMargins,yLegend,legendWidth,legendHeight,ILI9341_BLUE);
  // display.fillRect(display.width()-xMargins,yLegend-legendHeight,legendWidth,legendHeight,ILI9341_GREEN);
  // display.fillRect(display.width()-xMargins,(yLegend-(2*legendHeight)),legendWidth,legendHeight,ILI9341_YELLOW);
  // display.fillRect(display.width()-xMargins,(yLegend-(3*legendHeight)),legendWidth,legendHeight,ILI9341_ORANGE);
  // display.fillRect(display.width()-xMargins,(yLegend-(4*legendHeight)),legendWidth,legendHeight,ILI9341_RED);

  // circle labels
  display.setTextColor(ILI9341_WHITE);
  display.setFont();
  display.setCursor(xPMLabel,yPMLabel);
  display.print("PM2.5");
  // display.setCursor(106,110);
  // display.print("VoC"); 

  // pm25 level
  display.setFont(&FreeSans9pt7b);
  display.setCursor(xPMValue,yPMValue);
  display.print(int(sensorData.massConcentrationPm2p5));

  // VoC level
  // display.setCursor(100,80);
  // display.print(int(sensorData.vocIndex));

  // CO2 level
  // CO2 label line
  display.setFont(&FreeSans12pt7b);
  display.setCursor(xMargins, yCO2);
  display.print("CO");
  display.setCursor(xMargins + 50, yCO2);
  display.setTextColor(co2Color[co2Range(sensorData.ambientCO2)]);  // Use highlight color look-up table
  display.print(": " + String(co2Labels[co2Range(sensorData.ambientCO2)]));
  // subscript
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(xMargins + 35, (yCO2 + 10));
  display.print("2");
  // CO2 value line
  display.setFont();
  display.setCursor((xMargins + 88), (yCO2 + 7));
  display.print("(" + String(sensorData.ambientCO2) + ")");

  // Outside
  // location label
  display.setFont();
  display.setCursor((display.width() * 5 / 8), yMargins);
  display.print(owmCurrentData.cityName);

  // Outside temp
  if (owmCurrentData.tempF != 10000) {
    display.setFont(&FreeSans12pt7b);
    display.setCursor(xOutdoorMargin, yTemperature);
    display.print(String((uint8_t)(owmCurrentData.tempF + 0.5)));
    display.setFont(&meteocons12pt7b);
    display.print("+");
  }

  // Outside humidity
  if (owmCurrentData.humidity != 10000) {
    display.setFont(&FreeSans12pt7b);
    display.setCursor(xOutdoorMargin + 60, yTemperature);
    if ((sensorData.ambientHumidity<40) || (sensorData.ambientHumidity>60))
      display.setTextColor(ILI9341_RED);
    else
      display.setTextColor(ILI9341_GREEN);
    display.print(String((uint8_t)(owmCurrentData.humidity + 0.5)));
    display.drawBitmap(xOutdoorMargin + 90, yTemperature - 21, epd_bitmap_humidity_icon_sm4, 20, 28, ILI9341_WHITE);
  }

  // Outside air quality index (AQI) + PM25 value
  if (owmAirQuality.aqi != 10000) {
    // main line
    display.setFont(&FreeSans9pt7b);
    display.setCursor(xOutdoorMargin, yLegend);
    display.setTextColor(ILI9341_WHITE);
    // European standards-body AQI value
    //display.print(aqiEuropeanLabels[(owmAirQuality.aqi-1)]);

    // US standards-body AQI value
    display.print(aqiUSALabels[aqiUSLabelValue(owmAirQuality.pm25)]);
    display.print(" AQI");
    // value line
    display.setFont();
    display.setCursor((xOutdoorMargin + 20), (yLegend + 3));
    display.print("(" + String(owmAirQuality.pm25) + ")");
  }

    // weather icon
  String weatherIcon = OWMtoMeteoconIcon(owmCurrentData.icon);
  // if getMeteoIcon doesn't have a matching symbol, skip display
  if (weatherIcon != ")") {
    // display icon
    display.setFont(&meteocons20pt7b);
    display.setCursor((display.width() * 17 / 20), (display.height() / 2) + 10);
    display.print(weatherIcon);
  }
  debugMessage("screenCurrentInfo() end", 1);
}

void screenAlert(String messageText)
// Display error message centered on screen
{
  debugMessage(String("screenAlert '") + messageText + "' start",1);
  // Clear the screen
  display.fillScreen(ILI9341_BLACK);

  int16_t x1, y1;
  uint16_t width, height;

  display.setTextColor(ILI9341_WHITE);
  display.setFont(&FreeSans24pt7b);
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
  // screenGraph specific screen layout assists
  // const uint16_t ySparkline = 95;
  // const uint16_t sparklineHeight = 40;

  debugMessage("screenGraph start",1);
  screenAlert("Graph");
  debugMessage("screenGraph end",1);
}

void screenHelperWiFiStatus(uint16_t initialX, uint16_t initialY, uint8_t barWidth, uint8_t barHeightIncrement, uint8_t barSpacing)
// helper function for screenXXX() routines that draws WiFi signal strength
{
  #ifdef SCREEN
    // screen layout assists in pixels
    const uint16_t wifiBarWidth = 3;
    const uint16_t wifiBarHeightIncrement = 3;
    const uint16_t wifiBarSpacing = 5;

    if (hardwareData.rssi != 0) {
      // Convert RSSI values to a 5 bar visual indicator
      // >90 means no signal
      uint8_t barCount = constrain((6 - ((hardwareData.rssi / 10) - 3)), 0, 5);
      if (barCount > 0) {
        // <50 rssi value = 5 bars, each +10 rssi value range = one less bar
        // draw bars to represent WiFi strength
        for (uint8_t loop = 1; loop <= barCount; loop++) {
          display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, GxEPD_BLACK);
        }
        debugMessage(String("WiFi signal strength on screen as ") + barCount + " bars", 2);
      } else {
        // you could do a visual representation of no WiFi strength here
        debugMessage("RSSI too low, no display", 1);
      }
    }
  #endif
}

void screenHelperStatusMessage(uint16_t initialX, uint16_t initialY, String messageText)
// helper function for screenXXX() routines that draws a status message
// uses system default font, so text drawn x+,y+ from initialX,Y
{
  // IMPROVEMENT : Screen dimension boundary checks for function parameters
  #ifdef SCREEN
    display.setFont();  // resets to system default monospace font (6x8 pixels)
    display.setCursor(initialX, initialY);
    display.print(messageText);
  #endif
}

void OWMCurrentWeatherSimulate()
// Simulates Open Weather Map (OWM) Current Weather data
{
  // Improvement - variable length names
  owmCurrentData.cityName = "Pleasantville";
  // Temperature
  owmCurrentData.tempF = ((random(sensorTempMin,sensorTempMax) / 100.0) * 1.8) + 32;
  // Humidity
  owmCurrentData.humidity = random(sensorHumidityMin,sensorHumidityMax) / 100.0;
  // IMPROVEMENT - variable icons
  owmCurrentData.icon = "09d";
  debugMessage(String("SIMULATED OWM Current Weather: ") + owmCurrentData.tempF + "F, " + owmCurrentData.humidity + "%", 1);
}

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
      debugMessage(String("OWM Current Weather set: ") + owmCurrentData.tempF + "F, " + owmCurrentData.humidity + "%", 1);
      return true;
    }
    return false;
  #endif
}

void OWMAirPollutionSimulate()
// Simulates Open Weather Map (OWM) Air Pollution data
{
  owmAirQuality.aqi = random(OWMAQIMin, OWMAQIMax);  // overrides error code value
  owmAirQuality.pm25 = random(OWMPM25Min, OWMPM25Max) / 100.0;
  debugMessage(String("SIMULATED OWM PM2.5: ") + owmAirQuality.pm25 + ", AQI: " + owmAirQuality.aqi,1);
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

void networkSimulate()
// Simulates successful WiFi connection data
{
  // IMPROVEMENT : simulate IP address?
  hardwareData.rssi = random(networkRSSIMin, networkRSSIMax);
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
        debugMessage(String("WiFi IP address lease from ") + WIFI_SSID + " is " + WiFi.localIP().toString(), 1);
        debugMessage(String("WiFi RSSI is: ") + hardwareData.rssi + " dBm", 1);
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
  debugMessage("power off: WiFi",1);
  #ifdef HARDWARE_SIMULATE
    return;
  #else
    // IMPROVEMENT: What if disconnect call fails?
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
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

void setTimeZone(String timezone)
// Set local time based on timezone set in config.h
{
  debugMessage(String("setting Timezone to ") + timezone.c_str(),2);
  setenv("TZ",networkTimeZone.c_str(),1);
  tzset();
  debugMessage(String("Local time: ") + dateTimeString("short"),1);
}

String dateTimeString(String formatType)
// Converts time into human readable string
{
  // https://cplusplus.com/reference/ctime/tm/

  String dateTime;
  struct tm timeInfo;

  if (getLocalTime(&timeInfo)) 
  {
    if (formatType == "short")
    {
      // short human readable format
      dateTime = weekDays[timeInfo.tm_wday];
      dateTime += " at ";
      if (timeInfo.tm_hour < 10) dateTime += "0";
      dateTime += timeInfo.tm_hour;
      dateTime += ":";
      if (timeInfo.tm_min < 10) dateTime += "0";
      dateTime += timeInfo.tm_min;
    }
    else if (formatType == "long")
    {
      // long human readable
      dateTime = weekDays[timeInfo.tm_wday];
      dateTime += ", ";
      if (timeInfo.tm_mon<10) dateTime += "0";
      dateTime += timeInfo.tm_mon;
      dateTime += "-";
      if (timeInfo.tm_wday<10) dateTime += "0";
      dateTime += timeInfo.tm_wday;
      dateTime += " at ";
      if (timeInfo.tm_hour<10) dateTime += "0";
      dateTime += timeInfo.tm_hour;
      dateTime += ":";
      if (timeInfo.tm_min<10) dateTime += "0";
      dateTime += timeInfo.tm_min;
    }
  }
  else dateTime = "Can't reach time service";
  return dateTime;
}
#endif

bool networkGetTime(String timezone)
// Set local time from NTP server specified in config.h
{
  // !!! ESP32 hardware dependent, using Esperif library
  // https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
  #ifdef HARDWARE_SIMULATE
    // IMPROVEMENT: Add random time
    return false;
  #else
    struct tm timeinfo;

    // connect to NTP server with 0 TZ offset
    configTime(0, 0, networkNTPAddress.c_str());
    if(!getLocalTime(&timeinfo))
    {
      debugMessage("Failed to obtain time from NTP Server",1);
      return false;
    }
    // set local timezone
    setTimeZone(timezone);
    return true;
  #endif
}

bool sensorPMInit()
{
  #ifdef HARDWARE_SIMULATE
    return true;
  #else
    if (pmSensor.begin_I2C()) 
    {
      debugMessage("PMSA003I initialized",1);
      return true;
    }
    return false;
  #endif
}

void  sensorPMSimulate()
// Simulates sensor reading from PMSA003I sensor
{
  // tempF and humidity come from SCD40 simulation

  // Common to PMSA003I and SEN5x
  //sensorData.massConcentrationPm1p0 = random(0, 360) / 10.0;
  //sensorData.massConcentrationPm10p0 = random(0, 1550) / 10.0;
  sensorData.massConcentrationPm2p5 = random(sensorPM2p5Min, sensorPM2p5Max) / 10.0;

  // PMSA003I specific values

  // SEN5x specific values
  // sensorData.massConcentrationPm4p0 = random(0, 720) / 10.0;
  // sensorData.vocIndex = random(0, 500) / 10.0;
  // sensorData.noxIndex = random(0, 2500) / 10.0;

  debugMessage(String("SIMULATED PM2.5: ")+sensorData.massConcentrationPm2p5+" ppm",1); 
}

bool sensorPMRead()
{
  #ifdef HARDWARE_SIMULATE
    sensorPMSimulate();
    return true;
  #else
    PM25_AQI_Data data;
    if (! pmSensor.read(&data)) 
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

    debugMessage(String("PM2.5 reading is ") + sensorData.massConcentrationPm2p5 + " or AQI " + pm25toAQI(sensorData.massConcentrationPm2p5),1);
    // debugMessage(String("Particles > 0.3um / 0.1L air:") + sensorData.particles_03um,2);
    // debugMessage(String("Particles > 0.5um / 0.1L air:") + sensorData.particles_05um,2);
    // debugMessage(String("Particles > 1.0um / 0.1L air:") + sensorData.particles_10um,2);
    // debugMessage(String("Particles > 2.5um / 0.1L air:") + sensorData.particles_25um,2);
    // debugMessage(String("Particles > 5.0um / 0.1L air:") + sensorData.particles_50um,2);
    // debugMessage(String("Particles > 10 um / 0.1L air:") + sensorData.particles_100um,2);
    return true;
  #endif
}

bool sensorCO2Init()
// initializes CO2 sensor to read
{
  #ifdef HARDWARE_SIMULATE
    return true;
 #else
    char errorMessage[256];
    uint16_t error;

    Wire.begin();
    co2Sensor.begin(Wire);

    // stop potentially previously started measurement.
    error = co2Sensor.stopPeriodicMeasurement();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD40 stopPeriodicMeasurement()",1);
      return false;
    }

    // Check onboard configuration settings while not in active measurement mode
    float offset;
    error = co2Sensor.getTemperatureOffset(offset);
    if (error == 0){
        error = co2Sensor.setTemperatureOffset(sensorTempCOffset);
        if (error == 0)
          debugMessage(String("Initial SCD40 temperature offset ") + offset + " ,set to " + sensorTempCOffset,2);
    }

    uint16_t sensor_altitude;
    error = co2Sensor.getSensorAltitude(sensor_altitude);
    if (error == 0){
      error = co2Sensor.setSensorAltitude(SITE_ALTITUDE);  // optimizes CO2 reading
      if (error == 0)
        debugMessage(String("Initial SCD40 altitude ") + sensor_altitude + " meters, set to " + SITE_ALTITUDE,2);
    }

    // Start Measurement.  For high power mode, with a fixed update interval of 5 seconds
    // (the typical usage mode), use startPeriodicMeasurement().  For low power mode, with
    // a longer fixed sample interval of 30 seconds, use startLowPowerPeriodicMeasurement()
    // uint16_t error = co2Sensor.startPeriodicMeasurement();
    error = co2Sensor.startLowPowerPeriodicMeasurement();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD40 startLowPowerPeriodicMeasurement()",1);
      return false;
    }
    else
    {
      debugMessage("SCD40 starting low power periodic measurements",1);
      return true;
    }
  #endif
}

bool sensorCO2Read()
// sets global environment values from SCD40 sensor
{
  #ifdef HARDWARE_SIMULATE
    sensorCO2Simulate();
    return true;
  #else
    char errorMessage[256];
    bool status;
    uint16_t co2 = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    uint16_t error;

    debugMessage("CO2 sensor read initiated",1);

    // Loop attempting to read Measurement
    status = false;
    while(!status) {
      delay(100);

      // Is data ready to be read?
      bool isDataReady = false;
      error = co2Sensor.getDataReadyFlag(isDataReady);
      if (error) {
          errorToString(error, errorMessage, 256);
          debugMessage(String("Error trying to execute getDataReadyFlag(): ") + errorMessage,1);
          continue; // Back to the top of the loop
      }
      if (!isDataReady) {
          continue; // Back to the top of the loop
      }
      debugMessage("CO2 sensor data available",2);

      error = co2Sensor.readMeasurement(co2, temperature, humidity);
      if (error) {
          errorToString(error, errorMessage, 256);
          debugMessage(String("SCD40 executing readMeasurement(): ") + errorMessage,1);
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
        // Successfully read valid data
        sensorData.ambientTemperatureF = (temperature*1.8)+32.0;
        sensorData.ambientHumidity = humidity;
        sensorData.ambientCO2 = co2;
        debugMessage(String("SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
        // Update global sensor readings
        status = true;  // We have data, can break out of loop
      }
    }
  #endif
  return(true);
}

void sensorCO2Simulate()
// Simulate ranged data from the SCD40
// Improvement - implement stable, rapid rise and fall 
{
  // Temperature in Fahrenheit
  sensorData.ambientTemperatureF = ((random(sensorTempMin,sensorTempMax) / 100.0)*1.8)+32.0;
  // Humidity
  sensorData.ambientHumidity = random(sensorHumidityMin,sensorHumidityMax) / 100.0;
  // CO2
  sensorData.ambientCO2 = random(sensorCO2Min, sensorCO2Max);
  debugMessage(String("SIMULATED SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
}

uint8_t co2Range(uint16_t value)
// places CO2 value into a three band range for labeling and coloring. See config.h for more information
{
  if (value < co2Warning)
    return 0;
  else if (value < co2Alarm)
    return 1;
  else
    return 2;
}

uint8_t aqiUSLabelValue(float pm25)
// converts pm25 value to a 0-5 value associated with US AQI labels
{
  if (pm25 <= 12.0) return (0);
  else if (pm25 <= 35.4) return (1);
  else if (pm25 <= 55.4) return (2);
  else if (pm25 <= 150.4) return (3);
  else if (pm25 <= 250.4) return (4);
  else if (pm25 <= 500.4) return (5);
  else return (6);  // AQI above 500 not recognized
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
    Serial.flush();  // Make sure the message gets output (before any sleeping...)
  }
#endif
}