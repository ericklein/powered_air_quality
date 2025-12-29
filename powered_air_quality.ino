/*
  Project:        Powered Air Quality
  Description:    Sample and log indoor air quality via AC powered device

  See README.md for target information
*/

#include "config.h"               // hardware and internet configuration parameters
#include "powered_air_quality.h"  // global data structures
#include "secrets.h"              // private credentials for network, MQTT
#include "measure.h"              // Utility class for easy handling of aggregate sensor data
#include "data.h"
#include <SPI.h>                  // TFT_eSPI and XPT2046_Touchscreen

#ifndef HARDWARE_SIMULATE
// environment sensors
  #ifdef SENSOR_SEN66
    // Instanstiate SEN66 hardware object, if being used
    #include <SensirionI2cSen66.h>
    SensirionI2cSen66 paqSensor;
  #endif // SENSOR_SEN66

  #ifdef SENSOR_SEN54SCD40
    // instanstiate SEN5X hardware object
    #include <SensirionI2CSen5x.h>
    SensirionI2CSen5x pmSensor;

    // instanstiate SCD4X hardware object
    #include <SensirionI2cScd4x.h>
    SensirionI2cScd4x co2Sensor;
  #endif // SENSOR_SEN54SCD40

  #include <HTTPClient.h>
  #include <ArduinoJson.h>      // https://github.com/bblanchon/ArduinoJson, used by OWM retrieval routines
  #include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager
  #include <Preferences.h>      // read-write to ESP32 persistent storage

  WiFiClient client;   // WiFi Managers loads WiFi.h, which is used by OWM and MQTT
  Preferences nvConfig;

  #ifdef THINGSPEAK
    extern bool post_thingspeak(float pm25, float co2, float temperatureF, float humidity, 
      float vocIndex, float noxIndex, float aqi);
  #endif

  #ifdef INFLUX
    influxConfig influxdbConfig;
    extern bool post_influx(float temperatureF, float humidity, uint16_t co2, float pm25, float vocIndex, float noxIndex, uint8_t rssi);
  #endif

  #ifdef MQTT
    #include <PubSubClient.h>     // https://github.com/knolleary/pubsubclient
    MqttConfig mqttBrokerConfig;
    PubSubClient mqtt(client);

    extern bool mqttConnect();
    extern void mqttPublish(const char* topic, const String& payload);
    extern const char* generateMQTTTopic(String key);

    #ifdef HASSIO_MQTT
      extern void hassio_mqtt_publish(float pm25, float temperatureF, float vocIndex, float humidity, uint16_t co2);
    #endif
  #endif
#endif

// 3.2″ 320x240 color TFT w/resistive touch screen
#include <TFT_eSPI.h>  // https://github.com/Bodmer/TFT_eSPI
TFT_eSPI display = TFT_eSPI();

// fonts and glyphs
#include "Fonts/meteocons12pt7b.h"
#include "Fonts/meteocons16pt7b.h"
#include "Fonts/meteocons24pt7b.h"
#include "glyphs.h"

// touchscreen
#include <XPT2046_Touchscreen.h> // https://github.com/PaulStoffregen/XPT2046_Touchscreen
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS,XPT2046_IRQ);

// global variables

// Arrange for the default unique device identifier to be automatically generated at runtime based
// on ESP32 MAC address and hardware device type as specified in config.h.  This is done using a
// custom function contained here (see below).
extern String deviceGetID(String prefix);
const String defaultDeviceID = deviceGetID(hardwareDeviceType);

// data structures defined in powered_air_quality.h
networkEndpointConfig endpointPath;
envData sensorData;
hdweData hardwareData;
OpenWeatherMapCurrentData owmCurrentData;
OpenWeatherMapAirQuality owmAirQuality; 

// Utility class used to streamline accumulating sensor values, averages, min/max &c.
Measure totalTemperatureF, totalHumidity, totalCO2, totalVOCIndex, totalPM25, totalNOxIndex;

uint32_t timeLastReportMS       = 0;  // timestamp for last report to network endpoints
uint32_t timeResetPressStartMS = 0; // IMPROVEMENT: Move this as static to CheckResetLongPress()
bool saveWFMConfig = false;
enum screenNames screenCurrent = sSaver; // Initial screen to display (on startup)

void setup() {
  // config Serial first for debugMessage()
  #ifdef DEBUG
    Serial.begin(115200);
    // wait for serial port connection
    while (!Serial);
    // Display key configuration parameters
    debugMessage(String("Starting Powered Air Quality with ") + (sensorSampleIntervalMS/1000) + String(" second sample interval"),1);
    #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
      debugMessage(String("Report interval is ") + (reportIntervalMS/60000) + " minutes",1);
    #endif
  #endif

  display.begin();
  display.setRotation(screenRotation);
  display.setTextWrap(false);
  display.fillScreen(TFT_BLACK);
  screenAlert("Initializing");

  // initialize touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS); // setup the VSPI to use CYD touchscreen pins
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(screenRotation);

  // initialize button
  pinMode(hardwareWipeButton, INPUT_PULLUP);

  #ifndef HARDWARE_SIMULATE
    // load before sensorInit() to get altitude data
    loadNVConfig();
  #endif

  // *** Initialize sensors and other connected/onboard devices ***
  if( !sensorInit()) {
    debugMessage("setup(): sensor initialization failure",1);
    screenAlert("Sensor failure, rebooting");
    delay(5000);
    // This error often occurs right after a firmware flash and reset.
    // Hardware deep sleep typically resolves it, so quickly cycle the hardware
    deviceDeepSleep(hardwareErrorSleepTimeμS);
  }

  // initialize sensor value arrays
  for(uint8_t loop=0;loop<graphPoints;loop++) {
    sensorData.ambientCO2[loop] = -1;
    sensorData.vocIndex[loop] = -1;
  }

  #ifndef HARDWARE_SIMULATE
    if (!openWiFiManager()) {
      hardwareData.rssi = 0;  // 0 = no WiFi
    }
    #ifdef MQTT
      mqttConnect();
    #endif

    // Explicit start-up delay because the SEN66 takes 5-6 seconds to return valid
    // CO2 readinggs and 10-11 seconds to return valid NOx index values, and the SEN554
    // takes up to 6 seconds to return valid NOx index values.  The user interface 
    // should display "Initializing" during this delay
    #ifdef SENSOR_SEN66
      delay(12000);
    #endif
    #ifdef SENSOR_SEN54SCD40
      delay(7000);
    #endif
  #else
    // generate truely random numbers
    randomSeed(analogRead(0));
    networkSimulate();
  #endif
}

void loop() {
  static uint8_t numSamples               = 0;  // Number of sensor readings over reporting interval
  static uint32_t timeLastSampleMS        = -(sensorSampleIntervalMS); // forces immediate sample in loop() 
  static uint32_t timeLastInputMS         = millis();  // timestamp for last user input (screensaver)
  static uint32_t timeNextNetworkRetryMS  = 0;
  static uint32_t timeLastOWMUpdateMS     = -(OWMIntervalMS); // forces immediate sample in loop()
  uint16_t calibratedX, calibratedY;

  // is it time to read the sensor?
  if ((millis() - timeLastSampleMS) >= sensorSampleIntervalMS) {
    // Read sensor(s) to obtain all environmental values
    if (sensorRead()) {
      // add to the running totals
      numSamples++;
      totalTemperatureF.include(sensorData.ambientTemperatureF);
      totalHumidity.include(sensorData.ambientHumidity);
      totalCO2.include(sensorData.ambientCO2[graphPoints-1]);
      totalVOCIndex.include(sensorData.vocIndex[graphPoints-1]);
      totalPM25.include(sensorData.pm25);
      totalNOxIndex.include(sensorData.noxIndex);

      debugMessage(String("Sample #") + numSamples + ", running totals: ",2);
      debugMessage(String("TemperatureF total: ") + totalTemperatureF.getTotal(),2);
      debugMessage(String("Humidity total: ") + totalHumidity.getTotal(),2);
      debugMessage(String("CO2 total: ") + totalCO2.getTotal(),2);    
      debugMessage(String("VOC index total: ") + totalVOCIndex.getTotal(),2);
      debugMessage(String("PM25 total: ") + totalPM25.getTotal(),2);
      debugMessage(String("NOx index total: ") + totalNOxIndex.getTotal(),2);

      screenUpdate(screenCurrent);
    }
    else {
      // TODO: what else to do here?; detailed error message comes from sensorRead()
      // debugMessage("Sensor read failed!",1);
    }
    // Save last sample time
    timeLastSampleMS = millis();
  }

  // is there user input to process?
  // cyd 2.8 touchscreen from XPT2046
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
  // cyd 3.2 usb-c touchscreen from TFT_eSPI
  // if (display.getTouch(&calibratedX,&calibratedY)) {
    // If screen saver active, switch to master screen
    switch (screenCurrent) {
      case sSaver :
        // always go to the main screen when returning from screenSaver;
        screenCurrent = sMain;
        break;
      case sMain : {
        // get raw 12bit touchscreen x,y and then calibrate to screen size
        TS_Point p = touchscreen.getPoint();
        uint16_t calibratedX = map(p.x, touchscreenMinX, touchscreenMaxX, 1, display.width());
        uint16_t calibratedY = map(p.y, touchscreenMinY, touchscreenMaxY, 1, display.height());
        // alternate conversion
        // uint16_t calibratedX = (uint16_t)((p.x - touchscreenMinX) * display.width() / (touchscreenMaxX - touchscreenMinX));
        // uint16_t calibratedY = (uint16_t)((p.y - touchscreenMinY) * display.height() / (touchscreenMaxY - touchscreenMinY));
        debugMessage(String("input: touchpoint x=") + calibratedX + ", y=" + calibratedY,2);
        // transition to appropriate component screen
        if ((calibratedX < display.width()/2) && (calibratedY < display.height()/2)) {
          // upper left quandrant
          screenCurrent = sCO2;
        }
        else
          if ((calibratedX < display.width()/2) && (calibratedY > display.height()/2)) {
            // lower left quandrant
            screenCurrent = sVOC;
          }
          else
            if ((calibratedX > display.width()/2) && (calibratedY < display.height()/2)) {
              // upper right quandrant
              screenCurrent = sPM25;
            }
            else
            // lower right quandrant, either temp/humidity (SCD40/SEN55) or NOx Index (SEN66)
            screenCurrent = sNOX;
        break;
      }
      default:
        // switch back to main from component screen
        screenCurrent = sMain;
        break;
    }
    screenUpdate(screenCurrent);
    timeLastInputMS = millis();
  }

  // is it time to enable the screensaver AND we're not in screen saver mode already?
  if ((screenCurrent != sSaver) && ((millis() - timeLastInputMS) > screenSaverIntervalMS)) {
    screenCurrent = sSaver;
    debugMessage(String("Screen saver engaged"),1);
    screenUpdate(screenCurrent);
  }

  #ifndef HARDWARE_SIMULATE
    // is there a long press on the reset button to wipe all configuration data?
    checkResetLongPress();  // Always watching for long-press to wipe

    // is it time to check the WiFi connection before network endpoint write or OWM update?
    if (WiFi.status() != WL_CONNECTED) {
      if ((long)(millis() - timeNextNetworkRetryMS) >= 0) {
        WiFi.reconnect();
        timeNextNetworkRetryMS = millis() + timeNetworkRetryIntervalMS;
      }
    }

    // is it time for MQTT keep alive or reconnect?
    #ifdef MQTT
      if (mqtt.connected()) {
        if (millis() - timeLastMQTTPingMS > timeMQTTKeepAliveIntervalMS) {
          mqtt.loop();
          timeLastMQTTPingMS = millis();
        }
      } 
      else {
        mqttConnect();
      }
    #endif
  #endif

  // is it time to update OWM data?
  if ((millis() - timeLastOWMUpdateMS) >= OWMIntervalMS) {
    // update local weather data
    if (!OWMCurrentWeatherRead()) {
      owmCurrentData.tempF = 10000;
      debugMessage("OWM weather read failed!",1);
    }
    
    // update local air quality data
    if (!OWMAirPollutionRead()) {
      owmAirQuality.aqi = 10000;
      debugMessage("OWM AQI read failed!",1);
    }
    timeLastOWMUpdateMS = millis();
  }

  // is it time to write to the network endpoints?
  if ((millis() - timeLastReportMS) >= reportIntervalMS) {
    processSamples(numSamples);
    numSamples = 0;
    timeLastReportMS = millis();
  }
}

void screenUpdate(uint8_t screenCurrent) 
{
  switch(screenCurrent) {
    case sSaver:
      screenSaver();
      break;
    case sMain:
      screenMain();
      break;
    case sVOC:
      screenVOC();
      break;
    case sCO2:
      screenCO2();
      break;
    case sPM25:
      screenPM25();
      break;
    case sNOX:
      #ifdef SENSOR_SEN66  
        screenNOX();
      #else
        screenTempHumidity();
      #endif
      break;
    default:
      // if on a component screen, go back to main
      screenMain();
      break;
  }
}

void screenTempHumidity() 
// Description: Displays indoor and outdoor temperature and humidity
// Parameters:
// Output: NA (void)
// Improvement: 
{
  uint16_t text1Width;  // For text size calculation

  // screen layout assists in pixels

  debugMessage("screenTempHumidity() start",2);

  // clear screen
  display.fillScreen(TFT_BLACK);

  screenHelperIndoorOutdoorStatusRegion();

  display.setFreeFont(&FreeSans24pt7b);
  display.setTextDatum(MC_DATUM);

  // Indoor
  // Indoor temp
  if ((sensorData.ambientTemperatureF<sensorTempFComfortMin) || (sensorData.ambientTemperatureF>sensorTempFComfortMax))
    display.setTextColor(TFT_YELLOW);
  else
    display.setTextColor(TFT_WHITE);
  display.drawString(String((uint8_t)(sensorData.ambientTemperatureF + .5)), (display.width()/4), (display.height()*3/8));
  display.drawBitmap((display.width()/4 + 30), ((display.height()*3/8) - 14), bitmapTempFSmall, 20, 28, TFT_WHITE);

  // Indoor humidity
  if ((sensorData.ambientHumidity < sensorHumidityComfortMin) || (sensorData.ambientHumidity > sensorHumidityComfortMax))
    display.setTextColor(TFT_YELLOW);
  else
    display.setTextColor(TFT_GREEN);
  display.drawString(String((uint8_t)(sensorData.ambientHumidity + 0.5)), (display.width()/4), (display.height()*5/8));
  display.drawBitmap((display.width()/4 + 30), ((display.height()*5/8) - 14), bitmapHumidityIconSmall, 20, 28, TFT_WHITE);

  // Outside
  // do we have OWM Current data to display?
  if (owmCurrentData.tempF != 10000) {
    // Outside temp
    if ((owmCurrentData.tempF < sensorTempFComfortMin) || (owmCurrentData.tempF > sensorTempFComfortMax))
      display.setTextColor(TFT_YELLOW);
    else
      display.setTextColor(TFT_WHITE);
    display.drawString(String((uint8_t)(owmCurrentData.tempF + 0.5)), (display.width()*3/4), (display.height()*3/8));
    display.drawBitmap(((display.width()*3/4) + 30), ((display.height()*3/8) - 14), bitmapTempFSmall, 20, 28, TFT_WHITE);

    // Outside humidity
    if ((owmCurrentData.humidity < sensorHumidityComfortMin) || (owmCurrentData.humidity > sensorHumidityComfortMax))
      display.setTextColor(TFT_YELLOW);
    else
      display.setTextColor(TFT_WHITE);
    display.drawString(String((uint8_t)(owmCurrentData.humidity + 0.5)), (display.width()*3/4), (display.height()*5/8));
    display.drawBitmap(((display.width()*3/4) + 30), ((display.height()*5/8) - 14), bitmapHumidityIconSmall, 20, 28, TFT_WHITE);
  }

  //weather icon
  char weatherIcon = OWMtoMeteoconIcon(owmCurrentData.icon);
  // if getMeteoIcon doesn't have a matching symbol, skip display
  if (weatherIcon != '?') {
    // display icon
    display.setFreeFont(&meteocons24pt7b);
    display.setTextColor(TFT_WHITE);
    display.drawString(String(weatherIcon), ((display.width()*3/4)-12), (display.height()*7/8));
    // display.setCursor(((display.width()*3/4)-12), (display.height()*7/8));
    // display.print(weatherIcon);
  }
  debugMessage("screenTempHumidity() end", 2);
}

void screenPM25() 
// Description: Displays indoor and outdoor PM25, outdoor air pollution index
// Parameters:
// Output: NA (void)
// Improvement: 
{
  // screen layout assists in pixels
  const uint16_t xOutdoorMargin = ((display.width() / 2) + xMargins);
  // temp & humidity
  const uint16_t yPollution = 210;
  // pm25 rings
  const uint16_t xIndoorPMCircle = (display.width() / 4);
  const uint16_t xOutdoorPMCircle = ((display.width() / 4) * 3);
  const uint16_t yPMCircles = 110;
  const uint16_t circleRadius = 65;
  // inside the pm25 rings
  const uint16_t xIndoorCircleText = (xIndoorPMCircle - 18);
  const uint16_t xOutdoorCircleText = (xOutdoorPMCircle - 18);

  debugMessage("screenPM25() start",2);

  // clear screen
  display.fillScreen(TFT_BLACK);

  display.setTextDatum(TL_DATUM);

  screenHelperIndoorOutdoorStatusRegion();

  // Indoor PM2.5 ring
  display.fillSmoothCircle(xIndoorPMCircle,yPMCircles,circleRadius,warningColor[pm25Range(sensorData.pm25)]);
  display.fillSmoothCircle(xIndoorPMCircle,yPMCircles,circleRadius*0.8,TFT_BLACK);

  // Indoor pm25 value and label inside the circle
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(warningColor[pm25Range(sensorData.pm25)]);  // Use highlight color look-up table
  display.setCursor(xIndoorCircleText,yPMCircles);
  display.print(sensorData.pm25);
  // label
  display.setTextColor(TFT_WHITE);
  display.setCursor(xIndoorCircleText,yPMCircles+23);
  display.setFreeFont(&FreeSans9pt7b);
  display.print("PM25");
  
  // Outside
  // do we have OWM Air Quality data to display?
  if (owmAirQuality.aqi != 10000) {
    // Outside PM2.5
    display.fillSmoothCircle(xOutdoorPMCircle,yPMCircles,circleRadius,warningColor[pm25Range(owmAirQuality.pm25)]);
    display.fillSmoothCircle(xOutdoorPMCircle,yPMCircles,circleRadius*0.8,TFT_BLACK);

    // outdoor pm25 value and label inside the circle
    display.setFreeFont(&FreeSans12pt7b);
    display.setTextColor(warningColor[pm25Range(owmAirQuality.pm25)]);  // Use highlight color look-up table
    display.setCursor(xOutdoorCircleText, yPMCircles);
    display.print(owmAirQuality.pm25);
    //label
    display.setTextColor(TFT_WHITE);
    display.setCursor(xOutdoorCircleText,yPMCircles + 23);
    display.setFreeFont(&FreeSans9pt7b);
    display.print("PM25");
  }

  // outside AQI
  display.setCursor(xOutdoorMargin, yPollution);
  display.print(OWMPollutionLabel[(owmAirQuality.aqi)]);
  display.setCursor(xOutdoorMargin, yPollution + 15);
  display.print("air pollution");
  debugMessage("screenPM25() end", 2);
}

// void screenAggregateData()
// // Displays minimum, average, and maximum values for primary sensor values
// // using a table-style layout (with labels)
// {
//   const uint16_t xValueColumn =  10;
//   const uint16_t xMinColumn   = 115;
//   const uint16_t xAvgColumn   = 185;
//   const uint16_t xMaxColumn   = 255;
//   const uint16_t yHeaderRow   =  10;
//   const uint16_t yPM25Row     =  40;
//   const uint16_t yAQIRow      =  70;
//   const uint16_t yCO2Row      = 100;
//   const uint16_t yVOCRow      = 130;
//   const uint16_t yNOXRow      = 170;
//   const uint16_t yTempFRow    = 200;
//   const uint16_t yHumidityRow = 220;

//   debugMessage("screenAggregateData() start",2);

//   // clear screen and initialize properties
//   display.fillScreen(TFT_BLACK);
//   display.setFreeFont();  // Revert to built-in font
//   display.setTextSize(2);
//   display.setTextColor(TFT_WHITE);

//   // Display column heaings
//   display.setTextColor(TFT_BLUE);
//   display.setCursor(xAvgColumn, yHeaderRow); display.print("Avg");
//   display.drawLine(0,yPM25Row-10,display.width(),yPM25Row-10,TFT_BLUE);
//   display.setTextColor(TFT_WHITE);

//   // Display a unique unit ID based on the high-order 16 bits of the
//   // ESP32 MAC address (as the header for the data name column)
//   display.setCursor(0,yHeaderRow);
//   display.print(deviceGetID("AQ"));

//   // Display column headers
//   display.setCursor(xMinColumn, yHeaderRow); display.print("Min");
//   display.setCursor(xMaxColumn, yHeaderRow); display.print("Max");

//   // Display row headings
//   display.setCursor(xValueColumn, yPM25Row); display.print("PM25");
//   display.setCursor(xValueColumn, yAQIRow); display.print("AQI");
//   display.setCursor(xValueColumn, yCO2Row); display.print("CO2");
//   display.setCursor(xValueColumn, yVOCRow); display.print("VOC");
//   display.setCursor(xValueColumn, yNOXRow); display.print("NOx");
//   display.setCursor(xValueColumn, yTempFRow); display.print(" F");
//   display.setCursor(xValueColumn, yHumidityRow); display.print("%RH");

//   // PM2.5
//   display.setCursor(xMinColumn,yPM25Row); display.print(totalPM25.getMin(),1);
//   display.setCursor(xAvgColumn,yPM25Row); display.print(totalPM25.getAverage(),1);
//   display.setCursor(xMaxColumn,yPM25Row); display.print(totalPM25.getMax(),1);

//   // AQI
//   display.setCursor(xMinColumn,yAQIRow); display.print(pm25toAQI_US(totalPM25.getMin()),1);
//   display.setCursor(xAvgColumn,yAQIRow); display.print(pm25toAQI_US(totalPM25.getAverage()),1);
//   display.setCursor(xMaxColumn,yAQIRow); display.print(pm25toAQI_US(totalPM25.getMax()),1);

//   // CO2 color coded
//   display.setTextColor(warningColor[co2Range(totalCO2.getMin())]);  // Use highlight color look-up table
//   display.setCursor(xMinColumn,yCO2Row); display.print(totalCO2.getMin(),0);
//   display.setTextColor(warningColor[co2Range(totalCO2.getAverage())]);
//   display.setCursor(xAvgColumn,yCO2Row); display.print(totalCO2.getAverage(),0);
//   display.setTextColor(warningColor[co2Range(totalCO2.getMax())]);
//   display.setCursor(xMaxColumn,yCO2Row); display.print(totalCO2.getMax(),0);
//   display.setTextColor(TFT_WHITE);  // Restore text color

//   //VOC index
//   display.setCursor(xMinColumn,yVOCRow); display.print(totalVOCIndex.getMin(),0);
//   display.setCursor(xAvgColumn,yVOCRow); display.print(totalVOCIndex.getAverage(),0);
//   display.setCursor(xMaxColumn,yVOCRow); display.print(totalVOCIndex.getMax(),0);

//   // NOx index
//   display.setCursor(xMinColumn,yNOXRow); display.print(totalNOxIndex.getMin(),1);
//   display.setCursor(xAvgColumn,yNOXRow); display.print(totalNOxIndex.getAverage(),1);
//   display.setCursor(xMaxColumn,yNOXRow); display.print(totalNOxIndex.getMax(),1);

//   // temperature
//   display.setCursor(xMinColumn,yTempFRow); display.print(totalTemperatureF.getMin(),1);
//   display.setCursor(xAvgColumn,yTempFRow); display.print(totalTemperatureF.getAverage(),1);
//   display.setCursor(xMaxColumn,yTempFRow); display.print(totalTemperatureF.getMax(),1);

//   // humidity
//   display.setCursor(xMinColumn,yHumidityRow); display.print(totalHumidity.getMin(),0);
//   display.setCursor(xAvgColumn,yHumidityRow); display.print(totalHumidity.getAverage(),0);
//   display.setCursor(xMaxColumn,yHumidityRow); display.print(totalHumidity.getMax(),0);

//   // return to default text size
//   display.setTextSize(1);

//   debugMessage("screenAggregateData() end",2);
// }

bool screenAlert(String messageText)
// Description: Display error message centered on screen, using different font sizes and/or splitting to fit on screen
// Parameters: String containing error message text
// Output: NA (void)
// Improvement: Break the long font string to word blocks that fit in two lines
{
  bool success = false;

  debugMessage("screenAlert start",1);

  display.setTextColor(TFT_WHITE);
  display.fillScreen(TFT_BLACK);
  display.setTextDatum(MC_DATUM);

  debugMessage(String("screenAlert text is '") + messageText + "'",2);

  // does message fit on one line with large font?
  display.setFreeFont(&FreeSans24pt7b);
  if (display.textWidth(messageText) <= (display.width() + (xMargins*2))) {
    // fits with large font
    display.drawString(messageText, (display.width()/2), (display.height()/2));
    success = true;
  }
  else {
    // does message fit on two lines with large font?
    debugMessage(String("large font is ") + abs(display.width()-display.textWidth(messageText)) + " pixels too long, trying 2 lines", 1);
    // does the string break into two pieces based on a space character?
    uint8_t spaceLocation;
    uint16_t text1Width, text2Width;
    String messageTextPartOne, messageTextPartTwo;

    spaceLocation = messageText.indexOf(' ');
    if (spaceLocation) {
      // has a space character, measure two lines
      messageTextPartOne = messageText.substring(0,spaceLocation);
      messageTextPartTwo = messageText.substring(spaceLocation+1);
      text1Width = display.textWidth(messageTextPartOne);
      text2Width = display.textWidth(messageTextPartTwo);
      debugMessage(String("Message part one with large font is ") + text1Width + " pixels wide",2);
      debugMessage(String("Message part two with large font is ") + text2Width + " pixels wide",2);
    }
    else {
      debugMessage("there is no space in message to break message into 2 lines",2);
    }
    if ((text1Width <= (display.width() + (xMargins*2))) && (text2Width <= (display.width() + (xMargins*2)))) {
        // fits on two lines
        display.drawString(messageTextPartOne, (display.width()/2), ((display.height()/2)-25));
        display.drawString(messageTextPartTwo, (display.width()/2), ((display.height()/2)+25));
        success = true;
    }
    else {
      // does message fit on one line with medium sized text?
      debugMessage("couldn't break text into 2 lines or one line is too long, trying medium text",1);

      display.setFreeFont(&FreeSans18pt7b);
      if (display.textWidth(messageText) <= (display.width() + (xMargins*2))) {
        // fits with small size
        display.drawString(messageText, (display.width()/2), (display.height()/2));
        success = true;
      }
      else {
        // doesn't fit with medium font, display as truncated, small text
        debugMessage(String("medium font is ") + abs(display.width() - display.textWidth(messageText)) + " pixels too long, displaying small, truncated text", 1);
        display.setFreeFont(&FreeSans12pt7b);
        display.drawString(messageText, (display.width()/2), (display.height()/2));
      }
    }
  }
  debugMessage("screenAlert end",1);
  return success;
}

void retainCO2(float co2)
// Description: add new element, FIFO, to CO2 array
// Parameters:  new CO2 value from sensor
// Returns: NA (void)
// Improvement: ?
{
  for(uint8_t loop=1;loop<graphPoints;loop++) {
    sensorData.ambientCO2[loop-1] = sensorData.ambientCO2[loop];
  }
  sensorData.ambientCO2[graphPoints-1] = co2;
}

void retainVOC(float voc)
// Description: add new element, FIFO, to VOC array
// Parameters:  new VOC index value from sensor
// Returns: NA (void)
// Improvement: not merged with retainCO2 because reads are in independent functions
{
  for(uint8_t loop=1;loop<graphPoints;loop++) {
    sensorData.vocIndex[loop-1] = sensorData.vocIndex[loop];
  }
  sensorData.vocIndex[graphPoints-1] = voc;
}

void screenMain()
// Description: Represent CO2, VOC, PM25, and either T/H or NOx as touchscreen input quadrants color coded by current quality
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  // screen assists
  const uint8_t halfBorderWidth = 2;

  debugMessage("screenMain start",1);

  display.setFreeFont(&FreeSans18pt7b);
  display.setTextColor(TFT_BLACK);
  display.setTextDatum(MC_DATUM);

  display.fillScreen(TFT_BLACK);
  // CO2
  display.fillSmoothRoundRect(0, 0, ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, warningColor[co2Range(sensorData.ambientCO2[graphPoints-1])]);
  display.drawString("CO2",display.width()/4,display.height()/4);
  // PM2.5
  display.fillSmoothRoundRect(((display.width()/2)+halfBorderWidth), 0, ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, warningColor[pm25Range(sensorData.pm25)]);
  display.drawString("PM25",display.width()*3/4,display.height()/4);
  // VOC Index
  display.fillSmoothRoundRect(0, ((display.height()/2)+halfBorderWidth), ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, warningColor[vocRange(sensorData.vocIndex[graphPoints-1])]);
  display.drawString("VOC",display.width()/4,display.height()*3/4);
  #ifdef SENSOR_SEN66
    // NOx index
    display.fillSmoothRoundRect(((display.width()/2)+halfBorderWidth), ((display.height()/2)+halfBorderWidth), ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, warningColor[pm25Range(sensorData.pm25)]);
    display.drawString("NOx",display.width()*3/4,display.height()*3/4);
  #else
    // Temperature
    if ((sensorData.ambientTemperatureF<sensorTempFComfortMin) || (sensorData.ambientTemperatureF>sensorTempFComfortMax))
      display.fillSmoothRoundRect(((display.width()/2)+halfBorderWidth),((display.height()/2)+halfBorderWidth),((display.width()/4)-halfBorderWidth),((display.height()/2)-halfBorderWidth),cornerRoundRadius,TFT_YELLOW);
    else
      display.fillSmoothRoundRect(((display.width()/2)+halfBorderWidth),((display.height()/2)+halfBorderWidth),((display.width()/4)-halfBorderWidth),((display.height()/2)-halfBorderWidth),cornerRoundRadius,TFT_GREEN);
    // display.setCursor(((display.width()*5)/8),((display.height()*3)/4));
    display.setFreeFont(&meteocons24pt7b);
    display.drawString("+",display.width()*5/8,display.height()*3/4);
    // display.print("+");
    // Humdity
    if ((sensorData.ambientHumidity < sensorHumidityComfortMin) || (sensorData.ambientHumidity > sensorHumidityComfortMax))
      display.fillSmoothRoundRect((((display.width()*3)/4)+halfBorderWidth),((display.height()/2)+halfBorderWidth),((display.width()/4)-halfBorderWidth),((display.height()/2)-halfBorderWidth),cornerRoundRadius,TFT_YELLOW);
    else
      display.fillSmoothRoundRect((((display.width()*3)/4)+halfBorderWidth),((display.height()/2)+halfBorderWidth),((display.width()/4)-halfBorderWidth),((display.height()/2)-halfBorderWidth),cornerRoundRadius,TFT_GREEN);
    display.drawBitmap(((display.width()*7/8)-10),((display.height()*3/4)-14), bitmapHumidityIconSmall, 20, 28, TFT_BLACK);
  #endif

  debugMessage("screenMain end",1);
}

void screenSaver()
// Description: Display current CO2 reading at a random location (e.g. "screen saver")
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  debugMessage("screenSaver() start",1);

  display.fillScreen(TFT_BLACK);

  display.setFreeFont(&FreeSans24pt7b);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(warningColor[co2Range(sensorData.ambientCO2[graphPoints-1])]);

  uint16_t textWidth = display.textWidth(String(sensorData.ambientCO2[graphPoints-1]));

  // Display CO2 value in random, valid location
  display.drawString(String(uint16_t(sensorData.ambientCO2[graphPoints-1])), random(xMargins,display.width()-xMargins-textWidth), random(yMargins, display.height() - yMargins - display.fontHeight()));
  
  debugMessage("screenSaver() end",1);
}

void screenVOC()
// Description: Display VOC index information (ppm, color grade, graph)
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  // screen layout assists in pixels
  const uint8_t legendHeight = 20;
  const uint8_t legendWidth = 10;
  const uint16_t borderWidth = 15;
  const uint16_t borderHeight = 15;
  const uint16_t xLegend = display.width() - borderWidth - 5 - legendWidth;
  const uint16_t yLegend =  ((display.height()/4) + (uint8_t(3.5*legendHeight)));
  const uint16_t xLabel = display.width()/2;
  const uint16_t yLabel = yMargins + borderHeight + 30;

  debugMessage("screenVOC() start",1);

  display.setFreeFont(&FreeSans18pt7b);
  display.setTextColor(TFT_WHITE);

  // fill screen with VOC value color
  display.fillScreen(warningColor[vocRange(sensorData.vocIndex[graphPoints-1])]);
  display.fillSmoothRoundRect(borderWidth, borderHeight,display.width()-(2*borderWidth),display.height()-(2*borderHeight),cornerRoundRadius,TFT_BLACK);

  // value and label
  display.setCursor(borderWidth + 20,yLabel);
  display.print("VOC");
  display.setTextColor(warningColor[vocRange(sensorData.vocIndex[graphPoints-1])]);  // Use highlight color look-up table
  display.setCursor(xLabel, yLabel);
  display.print(uint16_t(sensorData.vocIndex[graphPoints-1]));

  screenHelperGraph(borderWidth + 5, display.height()/3, (display.width()-(2*borderWidth + 10)),((display.height()*2/3)-(borderHeight + 5)), sensorData.vocIndex, "Recent values");

  // legend for CO2 color wheel
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  debugMessage("screenVOC() end",1);
}

void screenNOX()
// Description: Display NOx index information (ppm, color grade, graph)
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  // screen layout assists in pixels
  const uint8_t legendHeight = 20;
  const uint8_t legendWidth = 10;
  const uint16_t xLegend = (display.width() - xMargins - legendWidth);
  const uint16_t yLegend =  ((display.height()/4) + (uint8_t(3.5*legendHeight)));
  const uint16_t circleRadius = 100;
  const uint16_t xVOCCircle = (display.width() / 2);
  const uint16_t yVOCCircle = (display.height() / 2);
  const uint16_t xVOCLabel = xVOCCircle - 35;
  const uint16_t yVOCLabel = yVOCCircle + 35;

  debugMessage("screenNOX() start",1);

  // clear screen
  display.fillScreen(TFT_BLACK);
  display.setFreeFont(&FreeSans18pt7b);

  // NOx color circle
  display.fillSmoothCircle(xVOCCircle,yVOCCircle,circleRadius,warningColor[noxRange(sensorData.noxIndex)]);
  display.fillSmoothCircle(xVOCCircle,yVOCCircle,circleRadius*0.8,TFT_BLACK);

  // legend for NOx color wheel
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  // NOx value and label (displayed inside circle)
  display.setTextColor(warningColor[vocRange(sensorData.noxIndex)]);  // Use highlight color look-up table
  display.setCursor(xVOCCircle-20,yVOCCircle);
  display.print(int(sensorData.noxIndex+.5));
  display.setTextColor(TFT_WHITE);
  display.setCursor(xVOCLabel,yVOCLabel);
  display.print("NOx");

  debugMessage("screenNOX() end",1);
}

void screenCO2()
// Description: Display CO2 information (ppm, color grade, graph)
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  // screen layout assists in pixels
  const uint8_t legendHeight = 20;
  const uint8_t legendWidth = 10;
  const uint16_t borderWidth = 15;
  const uint16_t borderHeight = 15;
  const uint16_t xLegend = display.width() - borderWidth - 5 - legendWidth;
  const uint16_t yLegend =  ((display.height()/4) + (uint8_t(3.5*legendHeight)));
  const uint16_t xLabel = display.width()/2;
  const uint16_t yLabel = yMargins + borderHeight + 30;

  debugMessage("screenCO2() start",1);

  display.setFreeFont(&FreeSans18pt7b);
  display.setTextColor(TFT_WHITE);

  // fill screen with CO2 value color
  display.fillScreen(warningColor[co2Range(sensorData.ambientCO2[graphPoints-1])]);
  display.fillSmoothRoundRect(borderWidth, borderHeight,display.width()-(2*borderWidth),display.height()-(2*borderHeight),cornerRoundRadius,TFT_BLACK);

  // value and label
  display.setCursor(borderWidth + 20,yLabel);
  display.print("CO2");
  display.setTextColor(warningColor[co2Range(sensorData.ambientCO2[graphPoints-1])]);  // Use highlight color look-up table
  display.setCursor(xLabel, yLabel);
  display.print(uint16_t(sensorData.ambientCO2[graphPoints-1]));

  screenHelperGraph(borderWidth + 5, display.height()/3, (display.width()-(2*borderWidth + 10)),((display.height()*2/3)-(borderHeight + 5)), sensorData.ambientCO2, "Recent values");

  // legend for CO2 color wheel
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  debugMessage("screenCO2() end",1);
}

void screenHelperWiFiStatus(uint16_t initialX, uint16_t initialY, uint8_t barWidth, uint8_t barHeightIncrement, uint8_t barSpacing)
// Description: helper function for screenXXX() routines drawing WiFi RSSI strength
// Parameters: 
// Output : NA
// Improvement : initialX, initialY, and overall width and height bounding for screen edge + x/y margins
//  dedicated icon type for no WiFi?
{
  // convert RSSI values to a 5 bar visual indicator
  // hardware.rssi = 0 or >90 means no signal
  uint8_t barCount = (hardwareData.rssi != 0)  ? constrain((6 - ((hardwareData.rssi / 10) - 3)), 0, 5) : 0;
  if (barCount > 0) {
    // <50 rssi value = draw 5 bars, each +10 rssi draw one less bar
    for (uint8_t loop = 1; loop <= barCount; loop++) {
      display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, TFT_WHITE);
    }
    debugMessage(String("WiFi signal strength on screen as ") + barCount + " bars", 2);
  } 
  else {
    // draw bars in red to represent no WiFi signal
    for (uint8_t loop = 1; loop <= 5; loop++) {
      display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, TFT_RED);
    }
    debugMessage("No WiFi or RSSI too low", 1);
  }
}

void screenHelperReportStatus(uint16_t initialX, uint16_t initialY)
// Description: helper function for screenXXX() routines that displays an icon relaying success of network endpoint writes
// Parameters: initial x and y coordinate to draw from
// Output : NA
// Improvement : initialX, initialY, and overall width and height bounding for screen edge + x/y margins
// 
{
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
    if ((timeLastReportMS == 0) || ((millis() - timeLastReportMS) >= (reportIntervalMS * reportFailureThreshold)))
        // we haven't successfully written to a network endpoint at all or before the reportFailureThreshold
        display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_RED);
      else
        display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_GREEN);
  #endif
}

void screenHelperGraph(uint16_t initialX, uint16_t initialY, uint16_t xWidth, uint16_t yHeight, const float *values, String xLabel)
// Description : Draw a graph of recent (CO2) values from right (most recent) to left. -1 values not graphed.
// Parameters: starting graph position (x,y), width and height of graph, x axis description
// Return : none
// Improvement : This function assumes the use of the default Adafruit GFX font and its rendering direction (down, right)
//  determine minimum size and block width and height smaller than that  
{
  uint8_t loop; // upper bound is graphPoints definition
  uint16_t text1Width, text1Height;
  uint16_t deltaX, x, y, xp, yp;  // graphing positions
  float minValue, maxValue;
  bool firstpoint = true, nodata = true;

  // screen layout assists in pixels
  uint8_t labelSpacer = 2;
  uint16_t graphLineX; // dynamically defined below
  uint16_t graphLineY;

  debugMessage("screenHelperGraph() start",1);

  display.fillRect(initialX,initialY,xWidth,yHeight,TFT_BLACK);
  display.setFreeFont();
  display.setTextColor(TFT_WHITE);

  switch (screenCurrent) {
    case sCO2:
      minValue = sensorCO2Max;
      maxValue = sensorCO2Min;
      break;
    case sVOC:
      minValue = sensorVOCMax;
      maxValue = sensorVOCMin;
      break;  
}
  // scan the array for min/max
  for(loop=0;loop<graphPoints;loop++) {
    if(values[loop] == -1) continue;   // Skip "empty" slots
    nodata = false;  // At least one data point
    if(values[loop] < minValue) minValue = values[loop];
    if(values[loop] > maxValue) maxValue = values[loop];
  }
  debugMessage(String("Min value in samples is ") + minValue + ", max is " + maxValue, 2);

  // do we have data? (e.g., just booted)
  if (nodata)
    xLabel = "Awaiting samples";
  else {
    // since we have data, pad min and max CO2 to add room and be multiples of 50 (for nicer axis labels)
    minValue = (uint16_t(minValue)/50)*50;
    maxValue = ((uint16_t(maxValue)/50)+1)*50;
  }

  // draw the X axis description, if provided, and set the position of the horizontal axis line
  if (strlen(xLabel.c_str())) {
    text1Width = display.textWidth(xLabel);
    text1Height = display.fontHeight();
    graphLineY = initialY + yHeight - text1Height - labelSpacer;
    display.setCursor((((initialX + xWidth)/2) - (text1Width/2)), (initialY + yHeight - text1Height));
    display.print(xLabel);
  }

  // calculate text width and height of longest Y axis label
  text1Width = display.textWidth(String(maxValue));
  text1Height = display.fontHeight(); 
  graphLineX = initialX + text1Width + labelSpacer;
  
  // draw top Y axis label
  display.setCursor(initialX, initialY);
  display.print(uint16_t(maxValue));
  // draw bottom Y axis label
  display.setCursor(initialX, graphLineY-text1Height); 
  display.print(uint16_t(minValue));

  // Draw vertical axis
  display.drawFastVLine(graphLineX,initialY,(graphLineY-initialY), TFT_WHITE);
  // Draw horitzonal axis
  display.drawFastHLine(graphLineX,graphLineY,(xWidth-graphLineX),TFT_WHITE);

  // Plot however many data points we have both with filled circles at each
  // point and lines connecting the points.  Color the filled circles with the
  // appropriate CO2 warning level color.
  deltaX = ((xWidth-graphLineX) - 10) / (graphPoints-1);  // X distance between points, 10 pixel padding for Y axis
  xp = graphLineX;
  yp = graphLineY;
  for(loop=0;loop<graphPoints;loop++) {
    if(values[loop] == -1) continue;
    x = graphLineX + 10 + (loop*deltaX);  // Include 10 pixel padding for Y axis
    y = graphLineY - (((values[loop] - minValue)/(maxValue-minValue)) * (graphLineY-initialY));
    debugMessage(String("Array ") + loop + " y value is " + y,2);
    if (screenCurrent == sCO2)
      display.fillSmoothCircle(x,y,4,warningColor[co2Range(sensorData.ambientCO2[loop])]);
    else
      if (screenCurrent == sVOC)
        display.fillSmoothCircle(x,y,4,warningColor[vocRange(sensorData.vocIndex[loop])]);
    if(firstpoint) {
      // If this is the first drawn point then don't try to draw a line
      firstpoint = false;
    }
    else {
      // Draw line from previous point (if one) to this point
      display.drawLine(xp,yp,x,y,TFT_WHITE);
    }
    // Save x & y of this point to use as previous point for next one.
    xp = x;
    yp = y;
  }
  debugMessage("screenHelperGraph() end",1);
}

void screenHelperIndoorOutdoorStatusRegion()
// Description: helper function for screenXXX() routines to draw the status region frame for indoor/outdoor information
// Parameters: NA
// Output : NA
// Improvement : NA
{
  // screen layout assists in pixels
  const uint8_t   yStatusRegion = display.height()/8;
  const uint16_t  xOutdoorMargin = ((display.width() / 2) + xMargins);
  const uint8_t   yStatusRegionFloor = yStatusRegion - 7;  
  const uint8_t   helperXSpacing = 15;

  display.fillRect(0,0,display.width(),yStatusRegion,TFT_DARKGREY);
  // split indoor v. outside
  display.drawFastVLine((display.width() / 2), yStatusRegion, display.height(), TFT_DARKGREY);
  // screen helpers in status region
  // IMPROVEMENT: Pad the initial X coordinate by the actual # of bars
  // screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), (yMargins + (5*wifiBarHeightIncrement)), wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), yStatusRegionFloor, wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperReportStatus(((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing)))-helperXSpacing), (yStatusRegionFloor-15));
  // labels
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(TFT_WHITE);
  display.setTextDatum(L_BASELINE);
  display.drawString("Indoor", xMargins, yStatusRegionFloor);
  display.drawString("Outside", xOutdoorMargin, yStatusRegionFloor);
}

// Hardware simulation routines
#ifdef HARDWARE_SIMULATE
  void OWMCurrentWeatherSimulate()
  // Description : Simulates Open Weather Map (OWM) Current Weather data
  // Parameters: NA
  // Return : NA
  // Improvement : variable city name and weather condition/icon
  {
    owmCurrentData.cityName = "Pleasantville";
    owmCurrentData.tempF = randomFloatRange(sensorTempMinF, sensorTempMaxF);
    owmCurrentData.humidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
    owmCurrentData.icon = "09d";
    debugMessage(String("SIMULATED OWM Current Weather: ") + owmCurrentData.tempF + "F, " + owmCurrentData.humidity + "%", 1);
  }

  void OWMAirPollutionSimulate()
  // Description : Simulates Open Weather Map (OWM) Air Pollution data
  // Parameters: NA
  // Return : NA
  // Improvement : NA
  {
    owmAirQuality.aqi = random(OWMAQIMin, OWMAQIMax);
    owmAirQuality.pm25 = randomFloatRange(OWMPM25Min, OWMPM25Max);
    debugMessage(String("SIMULATED OWM Air Pollution PM2.5: ") + owmAirQuality.pm25 + ", AQI: " + owmAirQuality.aqi,1);
  }

  void networkSimulate()
  // Description : Simulates successful WiFi connection data
  // Parameters: NA
  // Return : NA
  // Improvement : NA
  { 
    hardwareData.rssi = random(networkRSSIMin, networkRSSIMax);
    debugMessage(String("SIMULATED WiFi RSSI: ") + hardwareData.rssi,1);
  }

  void sensorSEN54Simulate()
  // Description: Simulates sensor reading from SEN54 sensor
  // Parameters: NA
  // Return: NA
  // Improvement: mode 1 from CO2 for VOC
  // Note: tempF and humidity come from SCD4X simulation
  {
    sensorData.pm1 = randomFloatRange(sensorPMMin, sensorPMMax);
    sensorData.pm10 = randomFloatRange(sensorPMMin, sensorPMMax);
    sensorData.pm25 = randomFloatRange(sensorPMMin, sensorPMMax);
    sensorData.pm4 = randomFloatRange(sensorPMMin, sensorPMMax);
    retainVOC(randomFloatRange(sensorVOCMin, sensorVOCMax));

    debugMessage(String("SIMULATED PM2.5: ") + sensorData.pm25 + " ppm, VOC index: " + sensorData.vocIndex[graphPoints-1],1);
  }

  void sensorSCD4xSimulate(uint8_t mode = 0, uint8_t cycles = 0)
  // Description: Simulates temp, humidity, and CO2 values from Sensirion SCD4X sensor
  // Parameters:
  //  mode
  //    default = random values, ignores cycles parameter
  //    1 = random values, slightly +/- per cycle
  //  cycles = If used, determines how many times the current mode executes before resetting
  // Output : NA
  // Improvement : implement edge value mode, rapid CO2 rise mode 
  {
    static uint8_t currentMode = 0;
    static uint8_t cycleCount = 0;
    static float simulatedTempF;
    static float simulatedHumidity;
    static uint16_t simulatedCO2;

    if (mode != currentMode) {
      cycleCount = 0;
      currentMode = mode;
    }
    switch (currentMode) {
    case 0: // 0 = random values, ignores cycles value
      simulatedTempF = randomFloatRange(sensorTempMinF,sensorTempMaxF);
      simulatedHumidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
      simulatedCO2 = random(sensorCO2Min, sensorCO2Max);
      break;    
    case 1: // 1 = random values, slightly +/- per cycle
      if (cycleCount == cycles) {
        cycleCount = 0;
      }
      if (!cycleCount) {
        // create new base values
        simulatedTempF = randomFloatRange(sensorTempMinF,sensorTempMaxF);
        simulatedHumidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
        simulatedCO2 = random(sensorCO2Min, sensorCO2Max);
        cycleCount++;
      }
      else
      {
        // slightly +/- CO2 value
        int8_t sign = random(0, 2) == 0 ? -1 : 1;
        simulatedCO2 = simulatedCO2 + (sign * random(0, sensorCO2VariabilityRange));
        // slightly +/- temp value
        // slightly +/- humidity value
        cycleCount++;
      }
      break;
    default: // should not occur; random values, ignores cycles value
      simulatedTempF = randomFloatRange(sensorTempMinF,sensorTempMaxF);
      simulatedHumidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
      simulatedCO2 = random(sensorCO2Min, sensorCO2Max);
      break;
    }
    sensorData.ambientTemperatureF = simulatedTempF;
    sensorData.ambientHumidity = simulatedHumidity;
    retainCO2(simulatedCO2);

    debugMessage(String("Simulated temp: ") + sensorData.ambientTemperatureF + "F, humidity: " + sensorData.ambientHumidity
      + "%, CO2: " + uint16_t(sensorData.ambientCO2[graphPoints-1]) + "ppm",1);
  }

  void sensorSEN6xSimulate()
  // Description: Simulates sensor reading from SEN66 sensor
  //  leveraging other sensor simulations
  // Parameters: NA
  // Return: NA
  // Improvement: implement mode passthrough for other sensorSimulate APIs
  {
    sensorSCD4xSimulate(1,3); // tempF, humidity, and CO2 values
    sensorSEN54Simulate(); // pm values
    sensorData.noxIndex = randomFloatRange(sensorVOCMin, sensorVOCMax);

    debugMessage(String("SIMULATED SEN66 noxIndex: ") + sensorData.noxIndex,1);
  }
#endif

// hardware routines tied to HARDWARE_SIMULATE
#ifndef HARDWARE_SIMULATE
  // WiFiManager portal functions
  void saveConfigCallback() 
  //callback notifying us of the need to save config from WiFi Manager AP mode
  {
    saveWFMConfig = true;
  }

  bool openWiFiManager()
  // Connect to WiFi network using WiFiManager
  {
    bool connected = false;
    String parameterText;

    debugMessage("openWiFiManager begin",2);

    WiFiManager wfm;

    // try and use stored credentials
    WiFi.begin();

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeNetworkConnectTimeoutMS) {
      delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      hardwareData.rssi = abs(WiFi.RSSI());
      debugMessage(String("openWiFiManager end; connected via stored credentials to " + WiFi.SSID() + ", ") + hardwareData.rssi + " dBm RSSI", 1);
    } 
    else {
      // no stored credentials or failed connect, use WiFiManager
      debugMessage("No stored credentials or failed connect, switching to WiFi Manager",1);
      parameterText = hardwareDeviceType + " setup";

      screenAlert(String("goto WiFi AP '") + parameterText + "'");

      wfm.setSaveConfigCallback(saveConfigCallback);
      wfm.setHostname(endpointPath.deviceID.c_str());
      #ifndef DEBUG
        wfm.setDebugOutput(false);
      #endif
      wfm.setConnectTimeout(180);

      wfm.setTitle("Ola friend!");
      // hint text (optional)
      //WiFiManagerParameter hint_text("<small>*If you want to connect to already connected AP, leave SSID and password fields empty</small>");
      
      //order determines on-screen order
      // wfm.addParameter(&hint_text);

      // collect common parameters in AP portal mode
      WiFiManagerParameter deviceLatitude("deviceLatitude", "device latitude","",9);
      WiFiManagerParameter deviceLongitude("deviceLongitude", "device longitude","",9);
      // String altitude = to_string(defaultAltitude);
      WiFiManagerParameter deviceAltitude("deviceAltitude", "Meters above sea level",defaultAltitude.c_str(),5);

      wfm.addParameter(&deviceLatitude);
      wfm.addParameter(&deviceLongitude);
      wfm.addParameter(&deviceAltitude);

      #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
        // collect network endpoint path in AP portal mode
        WiFiManagerParameter deviceSite("deviceSite", "device site", defaultSite.c_str(), 20);
        WiFiManagerParameter deviceLocation("deviceLocation", "indoor or outdoor", defaultLocation.c_str(), 20);
        WiFiManagerParameter deviceRoom("deviceRoom", "what room is the device in", defaultRoom.c_str(), 20);
        WiFiManagerParameter deviceID("deviceID", "unique name for device", defaultDeviceID.c_str(), 30);

        wfm.addParameter(&deviceSite);
        wfm.addParameter(&deviceLocation);
        wfm.addParameter(&deviceRoom);
        wfm.addParameter(&deviceID);
      #endif

      #ifdef MQTT
         // collect MQTT parameters in AP portal mode
        WiFiManagerParameter mqttBroker("mqttBroker","MQTT broker address",defaultMQTTBroker.c_str(),30);;
        WiFiManagerParameter mqttPort("mqttPort", "MQTT broker port", defaultMQTTPort.c_str(), 5);
        WiFiManagerParameter mqttUser("mqttUser", "MQTT username", defaultMQTTUser.c_str(), 20);
        WiFiManagerParameter mqttPassword("mqttPassword", "MQTT user password", defaultMQTTPassword.c_str(), 20);

        wfm.addParameter(&mqttBroker);
        wfm.addParameter(&mqttPort);
        wfm.addParameter(&mqttUser);
        wfm.addParameter(&mqttPassword);
      #endif

      #ifdef INFLUX
        WiFiManagerParameter influxBroker("influxBroker","influxdb server address",defaultInfluxAddress.c_str(),30);;
        WiFiManagerParameter influxPort("influxPort", "influxdb server port", defaultInfluxPort.c_str(), 5);
        WiFiManagerParameter influxOrg("influxOrg", "influx organization name", defaultInfluxOrg.c_str(),20);
        WiFiManagerParameter influxBucket("influxBucket", "influx bucket name", defaultInfluxBucket.c_str(),20);
        WiFiManagerParameter influxEnvMeasurement("influxEnvMeasurement", "influx environment measurement", defaultInfluxEnvMeasurement.c_str(),20);
        WiFiManagerParameter influxDevMeasurement("influxDevMeasurement", "influx device measurement", defaultInfluxDevMeasurement.c_str(),20);

        wfm.addParameter(&influxBroker);
        wfm.addParameter(&influxPort);
        wfm.addParameter(&influxOrg);
        wfm.addParameter(&influxBucket);
        wfm.addParameter(&influxEnvMeasurement);
        wfm.addParameter(&influxDevMeasurement);
      #endif

      connected = wfm.autoConnect(parameterText.c_str()); // anonymous ap
      // connected = wfm.autoConnect(hardwareDeviceType + " AP","password"); // password protected AP

      if(!connected) {
        debugMessage("WiFi connection failure; local sensor data ONLY", 1);
        // ESP.restart(); // if MQTT support is critical, make failure a stop gate
      } 
      else {
        if (saveWFMConfig) {
          debugMessage("retreiving new parameters from AP portal",2);
          hardwareData.altitude = (uint16_t)strtoul(deviceAltitude.getValue(), nullptr, 10);
          hardwareData.latitude = atof(deviceLatitude.getValue());
          hardwareData.longitude =  atof(deviceLongitude.getValue());

          #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
            endpointPath.site = deviceSite.getValue();
            endpointPath.location = deviceLocation.getValue();
            endpointPath.room = deviceRoom.getValue();
            endpointPath.deviceID = deviceID.getValue();
          #endif

          #ifdef MQTT
            mqttBrokerConfig.host     = mqttBroker.getValue();
            mqttBrokerConfig.port     = (uint16_t)strtoul(mqttPort.getValue(), nullptr, 10);
            mqttBrokerConfig.user     = mqttUser.getValue();
            mqttBrokerConfig.password = mqttPassword.getValue();
          #endif

          #ifdef INFLUX
            influxdbConfig.host       = influxBroker.getValue();
            influxdbConfig.port       = (uint16_t)strtoul(influxPort.getValue(), nullptr, 10);
            influxdbConfig.org        = influxOrg.getValue();
            influxdbConfig.bucket     = influxBucket.getValue();
            influxdbConfig.envMeasurement = influxEnvMeasurement.getValue();
            influxdbConfig.devMeasurement = influxDevMeasurement.getValue();
          #endif

          saveNVConfig();
          saveWFMConfig = false;
        }
        connected = true;
        hardwareData.rssi = abs(WiFi.RSSI());
        debugMessage(String("openWiFiManager end; connected to " + WiFi.SSID() + ", ") + hardwareData.rssi + " dBm RSSI", 1);
      }
    }
    return (connected);
  }

  void checkResetLongPress() {
  uint8_t buttonState = digitalRead(hardwareWipeButton);

  if (buttonState == LOW) {
    debugMessage(String("button pressed for ") + ((millis() - timeResetPressStartMS)/1000) + " seconds",2);
    if (timeResetPressStartMS == 0)
      timeResetPressStartMS = millis();
    if (millis() - timeResetPressStartMS >= timeResetButtonHoldMS)
      wipePrefsAndReboot();
  }
  else {
    if (timeResetPressStartMS != 0) {
      debugMessage("button released",2);
      timeResetPressStartMS = 0; // reset button press timer
    }
  }
  }

  void wipePrefsAndReboot() 
  // Wipes all ESP, WiFiManager preferences and reboots device
  {
    debugMessage("wipePrefsAndReboot begin",2);

    // Clear nv storage
    nvConfig.begin("config", false);
    nvConfig.clear();
    nvConfig.end();

    // disconnect and clear (via true) stored Wi-Fi credentials
    WiFi.disconnect(true);

    // Clear WiFiManager settings (AP config)
    WiFiManager wm;
    wm.resetSettings();

    debugMessage("wipePrefsAndReboot end, rebooting...",2);
    ESP.restart();
  }

  // Preferences helper routines
  void loadNVConfig() {
    debugMessage("loadNVConfig begin",2);
    nvConfig.begin("config", true); // read-only

    hardwareData.altitude = nvConfig.getUShort("altitude", uint16_t(defaultAltitude.toInt()));
    debugMessage(String("Device altitude is ") + hardwareData.altitude + " meters",2);
    hardwareData.latitude = nvConfig.getFloat("latitude");
    debugMessage(String("Device latitude is ") + hardwareData.latitude,2);
    hardwareData.longitude = nvConfig.getFloat("longitude");
    debugMessage(String("Device longitude is ") + hardwareData.longitude,2);

    #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
      endpointPath.site = nvConfig.getString("site", defaultSite);
      debugMessage(String("Device site is ") + endpointPath.site,2);
      endpointPath.location = nvConfig.getString("location", defaultLocation);
      debugMessage(String("Device location is ") + endpointPath.location,2);
      endpointPath.room = nvConfig.getString("room", defaultRoom);
      debugMessage(String("Device room is ") + endpointPath.room,2);
      endpointPath.deviceID = nvConfig.getString("deviceID", defaultDeviceID);
      debugMessage(String("Device ID is ") + endpointPath.deviceID,1);
    #endif

    #ifdef MQTT
      mqttBrokerConfig.host     = nvConfig.getString("mqttHost", defaultMQTTBroker);
      debugMessage(String("MQTT broker is ") + mqttBrokerConfig.host,2);
      mqttBrokerConfig.port     = nvConfig.getUShort("mqttPort", uint16_t(defaultMQTTPort.toInt()));
      debugMessage(String("MQTT broker port is ") + mqttBrokerConfig.port,2);
      mqttBrokerConfig.user     = nvConfig.getString("mqttUser", defaultMQTTUser);
      debugMessage(String("MQTT username is ") + mqttBrokerConfig.user,2);
      mqttBrokerConfig.password = nvConfig.getString("mqttPassword", defaultMQTTPassword);
      debugMessage(String("MQTT user password is ") + mqttBrokerConfig.password,2);
    #endif

    #ifdef INFLUX
      influxdbConfig.host     = nvConfig.getString("influxHost", defaultInfluxAddress);
      debugMessage(String("influxdb server address is ") + influxdbConfig.host,2);
      influxdbConfig.port     = nvConfig.getUShort("influxPort", uint16_t(defaultInfluxPort.toInt()));
      debugMessage(String("influxdb server port is ") + influxdbConfig.port,2);
      influxdbConfig.org     = nvConfig.getString("influxOrg", defaultInfluxOrg);
      debugMessage(String("influxdb org is ") + influxdbConfig.org,2);
      influxdbConfig.bucket = nvConfig.getString("influxBucket", defaultInfluxBucket);
      debugMessage(String("influxdb bucket is ") + influxdbConfig.bucket,2);
      influxdbConfig.envMeasurement = nvConfig.getString("influxEnvMeasure", defaultInfluxEnvMeasurement);
      debugMessage(String("influxdb environment measurement is ") + influxdbConfig.envMeasurement,2);
      influxdbConfig.devMeasurement = nvConfig.getString("influxDevMeasure", defaultInfluxDevMeasurement);
      debugMessage(String("influxdb device measurement is ") + influxdbConfig.devMeasurement,2);
    #endif

    nvConfig.end();
    debugMessage("loadNVConfig end",2);
  }

  void saveNVConfig()
  // copy new config data to non-volatile storage
  {
    debugMessage("saveNVConfig begin",2);
    nvConfig.begin("config", false); // read-write

    nvConfig.putUShort("altitude", hardwareData.altitude);
    nvConfig.putFloat("latitude",hardwareData.latitude);
    nvConfig.putFloat("longitude", hardwareData.longitude);

    #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
      nvConfig.putString("site", endpointPath.site);
      nvConfig.putString("location", endpointPath.location);
      nvConfig.putString("room", endpointPath.room);
      nvConfig.putString("deviceID", endpointPath.deviceID);
    #endif

    #ifdef MQTT
      nvConfig.putString("mqttHost",  mqttBrokerConfig.host);
      nvConfig.putUShort("mqttPort",  mqttBrokerConfig.port);
      nvConfig.putString("mqttUser",  mqttBrokerConfig.user);
      nvConfig.putString("mqttPassword",  mqttBrokerConfig.password);
    #endif

    #ifdef INFLUX
      nvConfig.putString("influxHost",  influxdbConfig.host);
      nvConfig.putUShort("influxPort",  influxdbConfig.port);
      nvConfig.putString("influxOrg",   influxdbConfig.org);
      nvConfig.putString("influxBucket",influxdbConfig.bucket);
      nvConfig.putString("influxEnvMeasure",influxdbConfig.envMeasurement);
      nvConfig.putString("influxDevMeasure", influxdbConfig.devMeasurement);
    #endif

    nvConfig.end();
    debugMessage("saveNVConfig end",2);
  }
#endif  

bool OWMCurrentWeatherRead()
// Gets Open Weather Map Current Weather data
{
  #ifdef HARDWARE_SIMULATE
    OWMCurrentWeatherSimulate();
    return true;
  #else
    // OWM latitude + longitude is "lat=xx.xxx&lon=-yyy.yyyy"
    String serverPath = OWMServer + OWMWeatherPath +
      "lat=" + hardwareData.latitude + "&lon=" + hardwareData.longitude + "&units=imperial&APPID=" + OWMKey;

    HTTPClient http;
    if (!http.begin(serverPath)) {
      debugMessage("OWM Current Weather connection failed",1);
      return false;
    }

    uint16_t httpResponseCode = http.GET();
    if (httpResponseCode != HTTP_CODE_OK) {
      debugMessage("OWM Current Weather HTTP GET error code: " + httpResponseCode,1);
      http.end();
      return false;
    }  

    // Filter: only parse what we need (saves RAM)
    JsonDocument filter;
    filter["main"]["temp"] = true;
    filter["main"]["humidity"] = true;
    filter["name"] = true;
    filter["weather"][0]["icon"] = true;

    JsonDocument doc;
    const DeserializationError error = deserializeJson(
      doc,
      http.getStream(),                      // parse directly from stream
      DeserializationOption::Filter(filter)  // apply filter
    );

    http.end();

    if (error) {
      debugMessage(String("deserializeJson failed with error message: ") + error.c_str(), 1);
      return false;
    }

    // owmCurrentData.lat = doc["coord"]["lat"];
    // owmCurrentData.lon = doc["coord"]["lon"];
    // owmCurrentData.main = (const char*) doc["weather"][0]["main"];
    // owmCurrentData.description = (const char*) doc["weather"][0]["description"];
    const char* iconStr = doc["weather"][0]["icon"] | "";
    strlcpy(owmCurrentData.icon, iconStr, sizeof(owmCurrentData.icon));
    owmCurrentData.cityName = (const char *)(doc["name"] | "");
    // owmCurrentData.visibility = doc["visibility"];
    // owmCurrentData.timezone = (time_t) doc["timezone"];
    // owmCurrentData.country = (const char*) doc["sys"]["country"];
    // owmCurrentData.observationTime = (time_t) doc["dt"];
    // owmCurrentData.sunrise = (time_t) doc["sys"]["sunrise"];
    // owmCurrentData.sunset = (time_t) doc["sys"]["sunset"];
    owmCurrentData.tempF = doc["main"]["temp"] | NAN;
    // owmCurrentData.pressure = (uint16_t) doc["main"]["pressure"];
    owmCurrentData.humidity = doc["main"]["humidity"] | 0;
    // owmCurrentData.tempMin = (float) doc["main"]["temp_min"];
    // owmCurrentData.tempMax = (float) doc["main"]["temp_max"];
    // owmCurrentData.windSpeed = (float) doc["wind"]["speed"];
    // owmCurrentData.windDeg = (float) doc["wind"]["deg"];
    debugMessage(String("OWM Current Weather for ") + owmCurrentData.cityName + " is " + owmCurrentData.tempF + "F, " + owmCurrentData.humidity + "% RH", 1);
    return true;
  #endif
}

bool OWMAirPollutionRead()
// stores local air pollution info from Open Weather Map in environment global
{
  #ifdef HARDWARE_SIMULATE
    OWMAirPollutionSimulate();
    return true;
  #else
    String serverPath = OWMServer + OWMAQMPath +
     "lat=" + hardwareData.latitude + "&lon=" + hardwareData.longitude + "&APPID=" + OWMKey;

    HTTPClient http;
    if (!http.begin(serverPath)) {
      debugMessage("OWM Air Pollution connection failed",1);
      return false;
    }

    uint16_t httpResponseCode = http.GET();
    if (httpResponseCode != HTTP_CODE_OK) {
      debugMessage("OWM AirPollution HTTP GET error code: " + httpResponseCode,1);
      http.end();
      return false;
    }

    // Filter: only parse what we need (saves RAM)
    JsonDocument filter;
    filter["list"][0]["main"]["aqi"] = true;
    filter["list"][0]["components"]["pm2_5"] = true;

    JsonDocument doc;
    const DeserializationError err = deserializeJson(
      doc,
      http.getStream(),
      DeserializationOption::Filter(filter)
    );

    http.end();

    // owmAirQuality.lon = (float) doc["coord"]["lon"];
    // owmAirQuality.lat = (float) doc["coord"]["lat"];
    owmAirQuality.aqi  = doc["list"][0]["main"]["aqi"] | 0;
    // owmAirQuality.co = (float) list_0_components["co"];
    // owmAirQuality.no = (float) list_0_components["no"];
    // owmAirQuality.no2 = (float) list_0_components["no2"];
    // owmAirQuality.o3 = (float) list_0_components["o3"];
    // owmAirQuality.so2 = (float) list_0_components["so2"];
    owmAirQuality.pm25 = doc["list"][0]["components"]["pm2_5"] | NAN;
    // owmAirQuality.pm10 = (float) list_0_components["pm10"];
    // owmAirQuality.nh3 = (float) list_0_components["nh3"];
    debugMessage(String("OWM Air Pollution PM2.5 is ") + owmAirQuality.pm25 + "μg/m3, AQI is " + owmAirQuality.aqi + " of 5",1);
    return true;
  #endif
}

char OWMtoMeteoconIcon(const char* icon)
// Description: Maps OWM icon data to the appropropriate Meteocon font character
// Parameters:  OWM icon string, OWM uses: 01,02,03,04,09,10,11,13,50 plus day/night suffix d/n
// Returns: NA (void)
// Improvement: ?
// Notes: Meteocon fonts: https://demo.alessioatzeni.com/meteocons/
{
  if (!icon || icon[0] == '\0' || icon[1] == '\0' || icon[2] == '\0') {
      debugMessage("OWM icon invalid", 1);
      return ')';
    }

  const char a = icon[0];
  const char b = icon[1];
  const bool night = (icon[2] == 'n');

  if (a == '0') {
    switch (b) {
      case '1': return night ? 'C' : 'B';
      case '2': return night ? '4' : 'H';
      case '3': return night ? '5' : 'N';
      case '4': return night ? '%' : 'Y';
      case '9': return night ? '8' : 'R';
    }
  } else if (a == '1') {
    switch (b) {
      case '0': return night ? '7' : 'Q';
      case '1': return night ? '6' : 'P';
      case '3': return night ? '#' : 'W';
    }
  } else if (a == '5' && b == '0') {
    return 'M';
  }

  debugMessage("OWM icon not matched to Meteocon, why?", 1);
  return '?'; // error handling for calling function
}

void processSamples(uint8_t numSamples)
{
  debugMessage(String("processSamples() start"),2);

  // do we have samples to process?
  if (numSamples) {
    // can we report to network endPoints?
    #ifndef HARDWARE_SIMULATE
      if (WiFi.status() == WL_CONNECTED) {
        // Get averaged sample values from Measure class objects for endPoint reporting
        float avgTemperatureF = totalTemperatureF.getAverage();
        float avgHumidity = totalHumidity.getAverage();
        uint16_t avgCO2 = totalCO2.getAverage();
        float avgVOC = totalVOCIndex.getAverage();
        float avgPM25 = totalPM25.getAverage();
        float avgNOX = totalNOxIndex.getAverage();

        debugMessage(String("Averages for the last ") + (reportIntervalMS/60000) + " minutes for endpoint reporting",1);
        debugMessage(String("PM2.5: ") + avgPM25 + "ppm, CO2: " + avgCO2 + "ppm, VOC index: " + avgVOC + ", NOx index: " + avgNOX + ", " + 
          avgTemperatureF + "F, humidity: " + avgHumidity + "%", 1);

        #ifdef THINGSPEAK
          debugMessage(String("AQI(US): ") + pm25toAQI_US(avgPM25),1);
          if (!post_thingspeak(avgPM25, avgCO2, avgTemperatureF, avgHumidity, avgVOC, avgNOX, pm25toAQI_US(avgPM25)) ) {
            Serial.println("ERROR: Did not write to ThingSpeak");
          }
        #endif

        #ifdef INFLUX
          debugMessage(String("WiFi RSSI: ") + hardwareData.rssi,1);
          if (!post_influx(avgTemperatureF, avgHumidity, avgCO2 , avgPM25, avgVOC, avgNOX, hardwareData.rssi))
            Serial.println("ERROR: Did not write to influxDB");
        #endif

        #ifdef MQTT
          debugMessage(String("WiFi RSSI: ") + hardwareData.rssi,1);

          // publish device data
          const char* topic;

          // publish hardware data
          topic = generateMQTTTopic(VALUE_KEY_RSSI);
          mqttPublish(topic, String(hardwareData.rssi));

          // publish sensor data
          topic = generateMQTTTopic(VALUE_KEY_TEMPERATURE);
          mqttPublish(topic, String(avgTemperatureF));
          topic = generateMQTTTopic(VALUE_KEY_HUMIDITY);
          mqttPublish(topic, String(avgHumidity));
          topic = generateMQTTTopic(VALUE_KEY_PM25);
          mqttPublish(topic, String(avgPM25));
          topic = generateMQTTTopic(VALUE_KEY_VOC);
          mqttPublish(topic, String(avgVOC));
          topic = generateMQTTTopic(VALUE_KEY_CO2);
          mqttPublish(topic, String(avgCO2));
          topic = generateMQTTTopic(VALUE_KEY_NOX);
          mqttPublish(topic, String(avgNOX));

          #ifdef HASSIO_MQTT
            debugMessage("Establishing MQTT for Home Assistant",1);
            // Either configure sensors in Home Assistant's configuration.yaml file
            // directly or attempt to do it via MQTT auto-discovery
            // hassio_mqtt_setup();  // Config for MQTT auto-discovery
            hassio_mqtt_publish(avgPM25, avgTemperatureF, avgVOC, avgHumidity);
          #endif
        #endif
      }
      else {
        debugMessage("No network, endpoint reporting skipped",1);
      }
    #endif
    // Reset sample counters
    totalTemperatureF.clear();
    totalHumidity.clear();
    totalCO2.clear();
    totalVOCIndex.clear();
    totalPM25.clear();
    totalNOxIndex.clear();
  }      
  else {
    debugMessage("No samples to process this cycle",1);
  }
  debugMessage(String("processSamples() end"),2);
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

bool sensorInit()
// Generalized entry point for sensor initialization
{
  // Conditionally compiled based on the sensor configuration as defined in config.h
  #ifdef SENSOR_SEN66
    return(sensorSEN6xInit());
  #endif

  #ifdef SENSOR_SEN54SCD40
    bool success = true;
    // Initialize PM25 sensor (SEN54)
    if (!sensorPMInit()) {
      success = false;
    }

    // Initialize CO2 Sensor (SCD4X)
    if (!sensorCO2Init()) {
      success = false;
    }
    return(success);
  #endif // SENSOR_SEN54SCD40

  debugMessage("Initialization failed: no sensor(s) defined!",1);
  return false;
}

bool sensorRead()
// Generalized entry point for reading sensor values
{
  #ifdef SENSOR_SEN66
    return(sensorSEN6xRead());
  #endif // SENSOR_SEN66

  #ifdef SENSOR_SEN54SCD40
    bool status = true;
    if (!sensorPMRead())
    {
      // TODO: what else to do here, see OWM Reads...
      debugMessage("PM sensor read failed",1);
      status = false;
    }

    if (!sensorCO2Read())
    {
      screenAlert("CO2 read fail");
      debugMessage("CO2 sensor read failed",1);
      status = false;
    }
    return(status);
  #endif // SENSOR_SEN54SCD40

  debugMessage("Read failed: no sensor(s) defined!",1);
  return false;
}

// Functions to be compiled in and used with the SEN66-based configuration
#ifdef SENSOR_SEN66
  // Initialize SEN66 sensor
  bool sensorSEN6xInit()
  {
      static char errorMessage[64];
      static int16_t error;

      Wire.begin(CYD_SDA, CYD_SCL);
      paqSensor.begin(Wire, SEN66_I2C_ADDR_6B);

      error = paqSensor.deviceReset();
      if (error != 0) {
          debugMessage("sensorSEN6xInit(): error msg from deviceReset() is",1);
          errorToString(error, errorMessage, sizeof errorMessage);
          debugMessage(errorMessage,1);
          return false;
      }
      delay(1200);

      error = paqSensor.startContinuousMeasurement();
      if (error != 0) {
          debugMessage("sensorSEN6xInit(): error msg from startContinuousMeasurement() is",1);
          errorToString(error, errorMessage, sizeof errorMessage);
          debugMessage(errorMessage,1);
          return false;
      }

      // TODO: Add support for setting custom temperature offset for SEN66
      return true;
  }

  // Read data from SEN66 sensor
  bool sensorSEN6xRead()
  {
    #ifdef HARDWARE_SIMULATE
      sensorSEN6xSimulate();
      return true;
    #endif
      static char errorMessage[64];
      static int16_t error;
      float ambientTemperatureC, voc;
      uint16_t co2 = 0;

      // TODO: Add support for checking isDataReady flag on SEN66

      error = paqSensor.readMeasuredValues(
        sensorData.pm1, sensorData.pm25, sensorData.pm4,
        sensorData.pm10, sensorData.ambientHumidity, ambientTemperatureC , voc,
        sensorData.noxIndex, co2);

    if (error != 0) {
        debugMessage("Error trying to execute readMeasuredValues(): ",1);
        errorToString(error, errorMessage, sizeof errorMessage);
        debugMessage(errorMessage,1);
        return false;
    }
    // Convert temperature Celsius from sensor into Farenheit
    sensorData.ambientTemperatureF = (1.8*ambientTemperatureC) + 32.0;

    retainCO2(co2);
    retainVOC(voc);
    
    debugMessage(String("SEN6x: PM2.5: ") + sensorData.pm25 + ", CO2: " + uint16_t(sensorData.ambientCO2[graphPoints-1]) +
      "ppm, VOC index: " + sensorData.vocIndex[graphPoints-1] + ", NOx index: " + sensorData.noxIndex + ", temp:" + 
      sensorData.ambientTemperatureF + "F, humidity:" + sensorData.ambientHumidity + "%",1);
    return true;
  }
#endif // SENSOR_SEN66

// Functions to be compiled in and use with the SEN54 + SCD40 configuration
#ifdef SENSOR_SEN54SCD40
  bool sensorPMInit()
  {
    #ifdef HARDWARE_SIMULATE
      return true;
    #else
      uint16_t error;
      char errorMessage[256];

      // Wire.begin();
      // IMPROVEMENT: Do you need another Wire.begin() [see sensorPMInit()]?
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
      sensorSEN54Simulate();
      return true;
    #else
      uint16_t error;
      char errorMessage[256];
      // we'll use the SCD4X values for these
      // IMPROVEMENT: Compare to SCD4X values?
      float sen5xTempF, sen5xHumidity, sen5xvoc;


      debugMessage("SEN5X read initiated",1);

      error = pmSensor.readMeasuredValues(
        sensorData.pm1, sensorData.pm25, sensorData.pm4,
        sensorData.pm10, sen5xHumidity, sen5xTempF, sen5xvoc,
        sensorData.noxIndex);
      if (error) {
        errorToString(error, errorMessage, 256);
        debugMessage(String(errorMessage) + " error during SEN5x read",1);
        return false;
      }

      retainVOC(sen5xvoc);
      debugMessage(String("SEN5X PM2.5: ") + sensorData.pm25 + "ppm, VOC Index: " + sensorData.vocIndex[graphPoints-1] + ", NOx Index: " + sensorData.noxIndex, 1);
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

      // IMPROVEMENT: Do you need another Wire.begin() [see sensorPMInit()]?
      Wire.begin(CYD_SDA, CYD_SCL);
      co2Sensor.begin(Wire, SCD41_I2C_ADDR_62);

      // stop potentially previously started measurement
      error = co2Sensor.stopPeriodicMeasurement();
      if (error) {
        errorToString(error, errorMessage, 256);
        debugMessage(String(errorMessage) + " executing SCD4X stopPeriodicMeasurement()",1);
        return false;
      }

      // modify configuration settings while not in active measurement mode
      error = co2Sensor.setSensorAltitude(hardwareData.altitude);  // optimizes CO2 reading
      if (!error)
        debugMessage(String("SCD4X altitude set to ") + hardwareData.altitude + " meters",2);
      else {
        errorToString(error, errorMessage, 256);
        debugMessage(String(errorMessage) + " executing SCD4X setSensorAltitude()",1);
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
    bool success = false;

    #ifdef HARDWARE_SIMULATE
      success = true;
      sensorSCD4xSimulate(1,3);
    #else
      char errorMessage[256];
      uint16_t co2 = 0;
      float temperatureC = 0.0f;
      float humidity = 0.0f;
      uint16_t error;
      uint8_t errorCount = 0;

      // Loop attempting to read Measurement
      debugMessage("CO2 sensor read initiated",1);
      while(!success) {
        delay(100);
        errorCount++;
        if (errorCount > co2SensorReadFailureLimit) {
          debugMessage(String("SCD40 failed to read after ") + errorCount + " attempts",1);
          break;
        }
        // Is data ready to be read?
        bool isDataReady = false;
        error = co2Sensor.getDataReadyStatus(isDataReady);
        if (error) {
            errorToString(error, errorMessage, 256);
            debugMessage(String("Error trying to execute getDataReadyStatus(): ") + errorMessage,1);
            continue; // Back to the top of the loop
        }
        if (!isDataReady) {
            continue; // Back to the top of the loop
        }
        debugMessage("SCD4X data available",2);

        error = co2Sensor.readMeasurement(co2, temperatureC, humidity);
        if (error) {
            errorToString(error, errorMessage, 256);
            debugMessage(String("SCD40 executing readMeasurement(): ") + errorMessage,1);
            // Implicitly continues back to the top of the loop
        }
        else if (co2 < sensorCO2Min || co2 > sensorCO2Max)
        {
          debugMessage(String("SCD40 CO2 reading: ") + co2 + " is out of expected range",1);
          //(sensorData.ambientCO2 < sensorCO2Min) ? sensorData.ambientCO2 = sensorCO2Min : sensorData.ambientCO2 = sensorCO2Max;
          // Implicitly continues back to the top of the loop
        }
        else
        {
          // Valid measurement available, update globals
          sensorData.ambientTemperatureF = (temperatureC*1.8)+32.0;
          sensorData.ambientHumidity = humidity;
          retainCO2(co2);
          debugMessage(String("SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + uint16_t(sensorData.ambientCO2[graphPoints-1]) + " ppm",1);
          // Update global sensor readings
          success = true;
          break;
        }
        delay(100); // reduces readMeasurement() "Not enough data received" errors
      }
    #endif
    return(success);
  }
#endif // SENSOR_SEN54SCD40

String deviceGetID(String prefix)
// Returns a unique device identifier based on ESP32 MAC address along with a specified prefix
{
  uint16_t shortid = (uint16_t) ((ESP.getEfuseMac() >> 32) & 0xFFFF ) ;
  if( shortid < 0x1000) {
    return(prefix + "-0" + String(shortid,HEX));
  }
  else {
    return(prefix + "-" + String(shortid,HEX));
  }
}

void deviceDeepSleep(uint32_t deepSleepTime)
// turns off component hardware then puts ESP32 into deep sleep mode for specified seconds
{
  debugMessage("deviceDeepSleep() start",1);

  // power down SCD4X by stopping potentially started measurement then power down SCD4X
  #ifndef HARDWARE_SIMULATE
    /* DJB-DEV
    uint16_t error = co2Sensor.stopPeriodicMeasurement();
    if (error) {
      char errorMessage[256];
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD4X stopPeriodicMeasurement()",1);
    }
    co2Sensor.powerDown();
    debugMessage("power off: SCD4X",2);
    */
  #endif

  networkDisconnect();

  esp_sleep_enable_timer_wakeup(deepSleepTime);
  debugMessage(String("deviceDeepSleep() end: ESP32 deep sleep for ") + (deepSleepTime/1000000) + " seconds",1);
  esp_deep_sleep_start();
}

// Range and math functions
uint8_t co2Range(float co2) 
// converts co2 value to index value for labeling and color
{
  uint8_t co2Range = 
    (co2 <= sensorCO2Fair) ? 0 :
    (co2 <= sensorCO2Poor) ? 1 :
    (co2 <= sensorCO2Bad)  ? 2 : 3;

  debugMessage(String("CO2 input of ") + co2 + " yields " + co2Range + " CO2 band", 2);
  return co2Range;
}

uint8_t pm25Range(float pm25)
// converts pm25 value to index value for labeling and color
{
  uint8_t aqi =
  (pm25 <= sensorPMFair) ? 0 :
  (pm25 <= sensorPMPoor) ? 1 :
  (pm25 <= sensorPMBad) ? 2 : 3;

  debugMessage(String("PM2.5 input of ") + pm25 + " yields " + aqi + " aqi",2);
  return aqi;
}

uint8_t vocRange(float vocIndex)
// converts vocIndex value to index value for labeling and color
{
  uint8_t vocRange =
  (vocIndex <= sensorVOCFair) ? 0 :
  (vocIndex <= sensorVOCPoor) ? 1 :
  (vocIndex <= sensorVOCBad)  ? 2 : 3;

  debugMessage(String("VOC index input of ") + vocIndex + " yields " + vocRange + " VOC band",2);
  return vocRange;
}

uint8_t noxRange(float noxIndex)
// converts noxIndex value to index value for labeling and color
{
  uint8_t noxRange =
  (noxIndex <= noxFair) ? 0 :
  (noxIndex <= noxPoor) ? 1 :
  (noxIndex <= noxBad)  ? 2 : 3;

  debugMessage(String("NOx index input of ") + noxIndex + " yields " + noxRange + " NOx band",2);
  return noxRange;
}

float pm25toAQI_US(float pm25)
// Converts pm25 reading to AQI using the US EPA standard (revised Feb 7, 2024) and detailed
// here: https://www.epa.gov/system/files/documents/2024-02/pm-naaqs-air-quality-index-fact-sheet.pdf.
{  
  float aqiValue;
  if(pm25 <= 9.0)        aqiValue = (fmap(pm25,  0.0,  9.0,  0.0, 50.0)); // "Good"
  else if(pm25 <= 35.4)  aqiValue = (fmap(pm25, 12.1, 35.4, 51.0,100.0)); // "Moderate"
  else if(pm25 <= 55.4)  aqiValue = (fmap(pm25, 35.5, 55.4,101.0,150.0)); // "Unhealthy for Sensitive Groups"
  else if(pm25 <= 125.4) aqiValue = (fmap(pm25, 55.5,125.4,151.0,200.0)); // "Unhnealthy"
  else if(pm25 <= 225.4) aqiValue = (fmap(pm25,125.5,225.4,201.0,300.0)); // "Very Unhealthy"
  else if(pm25 <= 500.0) aqiValue = (fmap(pm25,225.5,500.0,301.0,500.0)); // "Hazardous"
  else aqiValue = (501.0); // AQI above 500 not recognized
  debugMessage(String("PM2.5 value of ") + pm25 + " converts to US AQI value of " + aqiValue, 2);

  return aqiValue;
}

float fmap(float x, float xmin, float xmax, float ymin, float ymax)
{
  return( ymin + ((x - xmin)*(ymax-ymin)/(xmax - xmin)));
}

float randomFloatRange(uint16_t min, uint16_t max) {
  uint16_t randomFixed = random((max-min) *100 + 1);
  // return float with 2 decimal precision
  return randomFixed / 100.0f;
}

void debugMessage(String messageText, uint8_t messageLevel)
// wraps Serial.println as #define conditional
{
  #ifdef DEBUG
    if (messageLevel <= DEBUG) {
      Serial.println(messageText);
      Serial.flush();      // Make sure the message gets output (before any sleeping...)
    }
  #endif
}