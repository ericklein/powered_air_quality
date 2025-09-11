/*
  Project:        Powered Air Quality
  Description:    Sample and log indoor air quality via AC powered device

  See README.md for target information
*/

#include "powered_air_quality.h"
#include "config.h"           // hardware and internet configuration parameters
#include "secrets.h"          // private credentials for network, MQTT
#include "measure.h"          // Utility class for easy handling of aggregate sensor data
#include "data.h"
#include <HTTPClient.h>
#include <Preferences.h>      // read-write to ESP32 persistent storage
#include <ArduinoJson.h>      // Needed by OWM retrieval routines
#include <WiFiManager.h>      // https://github.com/tzapu/WiFiManager

WiFiClient client;   // WiFi Managers loads WiFi.h, which is used by OWM and MQTT
Preferences nvConfig;

#include <SPI.h>
// ESP32 has 2 SPI ports; for CYD to work with the TFT and touchscreen on different SPI ports
// each needs to be defined and passed to the library
SPIClass hspi = SPIClass(HSPI);
SPIClass vspi = SPIClass(VSPI);

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

// 3.2″ 320x240 color TFT w/resistive touch screen
#include <Adafruit_ILI9341.h>
Adafruit_ILI9341 display = Adafruit_ILI9341(&hspi, TFT_DC, TFT_CS, TFT_RST);

// touchscreen
#include <XPT2046_Touchscreen.h>
XPT2046_Touchscreen touchscreen(XPT2046_CS,XPT2046_IRQ);

// fonts and glyphs
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>
#include "Fonts/meteocons16pt7b.h"
#include "Fonts/meteocons12pt7b.h"
#include "glyphs.h"

// external function dependencies
#ifdef THINGSPEAK
  extern bool post_thingspeak(float pm25, float co2, float temperatureF, float humidity, 
    float vocIndex, float noxIndex, float aqi);
#endif

#ifdef INFLUX
  extern bool post_influx(float temperatureF, float humidity, uint16_t co2, float pm25, float vocIndex, float noxIndex, uint8_t rssi);
#endif

#ifdef MQTT
  #include <PubSubClient.h>     // https://github.com/knolleary/pubsubclient
  PubSubClient mqtt(client);

  extern bool mqttConnect();
  extern void mqttPublish(const char* topic, const String& payload);
  extern const char* generateMQTTTopic(String key);

  #ifdef HASSIO_MQTT
    extern void hassio_mqtt_publish(float pm25, float temperatureF, float vocIndex, float humidity, uint16_t co2);
  #endif
#endif

// global variables

// data structures defined in powered_air_quality.h
MqttConfig mqttBrokerConfig;
influxConfig influxdbConfig;
networkEndpointConfig endpointPath;
envData sensorData;
hdweData hardwareData;
OpenWeatherMapCurrentData owmCurrentData;
OpenWeatherMapAirQuality owmAirQuality; 

// Utility class used to streamline accumulating sensor values, averages, min/max &c.
Measure totalTemperatureF, totalHumidity, totalCO2, totalVOC, totalPM25, totalNOX;

int16_t co2data[GRAPH_POINTS];

uint32_t timeLastReportMS       = 0;  // timestamp for last report to network endpoints
uint32_t timeResetPressStartMS = 0; // IMPROVEMENT: Move this as static to CheckResetLongPress()
bool saveWFMConfig = false;

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

  // initialize screen first to display (initialization) messages
  pinMode(TFT_BACKLIGHT, OUTPUT);
  digitalWrite(TFT_BACKLIGHT, HIGH);

  display.begin();
  display.setRotation(screenRotation);
  display.setTextWrap(false);
  display.fillScreen(ILI9341_BLACK);
  screenAlert("Initializing");

  // Setup the VSPI to use CYD touchscreen pins
  vspi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(vspi);

  // truly random numbers for every boot cycle
  randomSeed(analogRead(0));

  // initialize button
  pinMode(hardwareWipeButton, INPUT_PULLUP);

  // load before sensorInit() to get altitude data
  loadNVConfig();

  // *** Initialize sensors and other connected/onboard devices ***
  if( !sensorInit()) {
    debugMessage("Sensor initialization failure",1);
    screenAlert("Sensor failure, rebooting");
    delay(5000);
    // This error often occurs right after a firmware flash and reset.
    // Hardware deep sleep typically resolves it, so quickly cycle the hardware
    powerDisable(hardwareErrorSleepTimeμS);
  }

  // initialize retained CO2 data values array for graphing
  for(uint8_t loop=0;loop<GRAPH_POINTS;loop++) {
    co2data[loop] = -1;
  }

  if (!openWiFiManager()) {
    hardwareData.rssi = 0;  // 0 = no WiFi
  }
  #ifdef MQTT
    mqttConnect();
  #endif
}

void loop() {
  static uint8_t numSamples               = 0;  // Number of sensor readings over reporting interval
  static uint32_t timeLastSampleMS        = -(sensorSampleIntervalMS); // forces immediate sample in loop() 
  static uint32_t timeLastInputMS         = millis();  // timestamp for last user input (screensaver)
  static uint32_t timeNextNetworkRetryMS  = 0;
  static uint32_t timeLastOWMUpdateMS     = -(OWMIntervalMS); // forces immediate sample in loop()
  static uint32_t timeLastMQTTPingMS      = 0;

  // Set first screen to display.  If that first screen is the screen saver then we need to
  // have the saved screen be something else so it'll get switched to on the first touch
  static uint8_t screenSaved   = SCREEN_INFO; // Saved when screen saver engages so it can be restored
  static uint8_t screenCurrent = SCREEN_SAVER; // Initial screen to display (on startup)

  // is there a long press on the reset button to wipe all configuration data?
  checkResetLongPress();  // Always watching for long-press to wipe

  // is it time to read the sensor?
  if ((millis() - timeLastSampleMS) >= sensorSampleIntervalMS) {
    // Read sensor(s) to obtain all environmental values
    if (sensorRead()) {
      // add to the running totals
      numSamples++;
      totalTemperatureF.include(sensorData.ambientTemperatureF);
      totalHumidity.include(sensorData.ambientHumidity);
      totalCO2.include(sensorData.ambientCO2);
      totalVOC.include(sensorData.vocIndex);
      totalPM25.include(sensorData.pm25);
      totalNOX.include(sensorData.noxIndex);  // TODO: Skip invalid values immediately after initialization

      debugMessage(String("Sample #") + numSamples + ", running totals: ",2);
      debugMessage(String("TemperatureF total: ") + totalTemperatureF.getTotal(),2);
      debugMessage(String("Humidity total: ") + totalHumidity.getTotal(),2);
      debugMessage(String("CO2 total: ") + totalCO2.getTotal(),2);    
      debugMessage(String("VOC total: ") + totalVOC.getTotal(),2);
      debugMessage(String("PM25 total: ") + totalPM25.getTotal(),2);
      debugMessage(String("NOX total: ") + totalNOX.getTotal(),2);

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
  if (touchscreen.touched()) {
    // If screen saver was active, switch to the previous active screen
    if( screenCurrent == SCREEN_SAVER) {
        screenCurrent = screenSaved;
        debugMessage(String("touchscreen pressed, screen saver off => ") + screenCurrent,1);
        screenUpdate(screenCurrent);
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
      screenUpdate(screenCurrent);
    }
    // Save time touch input occurred
    timeLastInputMS = millis();
  }

  // is it time to enable the screensaver AND we're not in screen saver mode already?
  if ((screenCurrent != SCREEN_SAVER) && ((millis() - timeLastInputMS) > screenSaverIntervalMS)) {
    // Activate screen saver, retaining current screen for easy return
    screenSaved = screenCurrent;
    screenCurrent = SCREEN_SAVER;
    debugMessage(String("Screen saver engaged, will restore to ") + screenSaved,1);
    screenUpdate(screenCurrent);
  }

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
    endPointWrite(numSamples);
    numSamples = 0;
    timeLastReportMS = millis();
  }
}

void screenUpdate(uint8_t screenCurrent) 
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
  int16_t x1, y1; // For (x,y) calculations
  uint16_t w1, h1;  // For text size calculations

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

  debugMessage("screenCurrentInfo() start",2);

  // clear screen
  display.fillScreen(ILI9341_BLACK);

  // status region
  display.fillRect(0,0,display.width(),yStatusRegion,ILI9341_DARKGREY);
  // split indoor v. outside
  display.drawFastVLine((display.width() / 2), yStatusRegion, display.height(), ILI9341_WHITE);
  // screen helpers in status region
  // IMPROVEMENT: Pad the initial X coordinate by the actual # of bars
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), (yMargins + (5*wifiBarHeightIncrement)), wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperReportStatus(((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing)))-20), yMargins);

  // Indoor
  // Indoor temp
  display.setFont(&FreeSans12pt7b);
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(xMargins + xTempModifier, yTempHumdidity);
  display.print(String((uint8_t)(sensorData.ambientTemperatureF + .5)));
  //display.drawBitmap(xMargins + xTempModifier + 35, yTempHumdidity, bitmapTempFSmall, 20, 28, ILI9341_WHITE);
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
  display.drawBitmap(xMargins + xTempModifier + xHumidityModifier + 35, yTempHumdidity - 21, bitmapHumidityIconSmall, 20, 28, ILI9341_WHITE);

  // Indoor PM2.5 ring
  display.fillCircle(xIndoorPMCircle,yPMCircles,circleRadius,warningColor[aqiRange(sensorData.pm25)]);
  display.fillCircle(xIndoorPMCircle,yPMCircles,circleRadius*0.8,ILI9341_BLACK);

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

    // Outside air quality index (AQI)
    display.setFont(&FreeSans9pt7b);
    display.setTextColor(ILI9341_WHITE);
    // IMPROVEMENT: Dynamic x coordinate based on text length
    // display.setCursor((xOutdoorPMCircle - ((circleRadius*0.8)-10)), yPMCircles+10);
    display.getTextBounds(OWMAQILabels[(owmAirQuality.aqi)],(xOutdoorPMCircle - ((circleRadius*0.8)-10)), yPMCircles+10, &x1, &y1, &w1, &h1);
    display.setCursor((xOutdoorPMCircle - (w1/2)),yPMCircles+10);

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
    display.drawBitmap(xOutdoorMargin + xTempModifier + xHumidityModifier + 35, yTempHumdidity - 21, bitmapHumidityIconSmall, 20, 28, ILI9341_WHITE);

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
  debugMessage("screenCurrentInfo() end", 2);  // DJB-DEV: was 1
}

void screenAggregateData()
// Displays minimum, average, and maximum values for primary sensor values
// using a table-style layout (with labels)
{
  const uint16_t xValueColumn =  10;
  const uint16_t xMinColumn   = 115;
  const uint16_t xAvgColumn   = 185;
  const uint16_t xMaxColumn   = 255;
  const uint16_t yHeaderRow   =  10;
  const uint16_t yPM25Row     =  40;
  const uint16_t yCO2Row      =  70;
  const uint16_t yVOCRow      = 100;
  const uint16_t yNOXRow      = 130;
  const uint16_t yTempFRow    = 160;
  const uint16_t yHumidityRow = 190;
  const uint16_t yAQIRow      = 220;

  // clear screen and initialize properties
  display.fillScreen(ILI9341_BLACK);
  display.setFont();  // Revert to built-in font
  display.setTextSize(2);
  display.setTextColor(ILI9341_WHITE);

  // Display column headings
  display.setTextColor(ILI9341_BLUE);
  display.setCursor(xAvgColumn, yHeaderRow); display.print("Avg");
  display.drawLine(0,yPM25Row-10,display.width(),yPM25Row-10,ILI9341_BLUE);
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(0,yHeaderRow); display.print("REPORT:");
  display.setCursor(xMinColumn, yHeaderRow); display.print("Min");
  display.setCursor(xMaxColumn, yHeaderRow); display.print("Max");

  // Display row headings
  display.setCursor(xValueColumn, yPM25Row); display.print("PM25");
  display.setCursor(xValueColumn, yCO2Row); display.print("CO2");
  display.setCursor(xValueColumn, yVOCRow); display.print("VOC");
  display.setCursor(xValueColumn, yNOXRow); display.print("NOX");
  display.setCursor(xValueColumn, yTempFRow); display.print(" F");
  display.setCursor(xValueColumn, yHumidityRow); display.print("%RH");
  display.setCursor(xValueColumn, yAQIRow); display.print("AQI");

  // Fill in the data row by row
  display.setCursor(xMinColumn,yPM25Row); display.print(totalPM25.getMin(),1);
  display.setCursor(xAvgColumn,yPM25Row); display.print(totalPM25.getAverage(),1);
  display.setCursor(xMaxColumn,yPM25Row); display.print(totalPM25.getMax(),1);

  // Color code the CO2 row
  display.setTextColor(warningColor[co2Range(totalCO2.getMin())]);  // Use highlight color look-up table
  display.setCursor(xMinColumn,yCO2Row); display.print(totalCO2.getMin(),0);
  display.setTextColor(warningColor[co2Range(totalCO2.getAverage())]);  // Use highlight color look-up table
  display.setCursor(xAvgColumn,yCO2Row); display.print(totalCO2.getAverage(),0);
  display.setTextColor(warningColor[co2Range(totalCO2.getMax())]);  // Use highlight color look-up table
  display.setCursor(xMaxColumn,yCO2Row); display.print(totalCO2.getMax(),0);
  display.setTextColor(ILI9341_WHITE);  // Restore text color

  display.setCursor(xMinColumn,yVOCRow); display.print(totalVOC.getMin(),0);
  display.setCursor(xAvgColumn,yVOCRow); display.print(totalVOC.getAverage(),0);
  display.setCursor(xMaxColumn,yVOCRow); display.print(totalVOC.getMax(),0);

  display.setCursor(xMinColumn,yNOXRow); display.print(totalNOX.getMin(),1);
  display.setCursor(xAvgColumn,yNOXRow); display.print(totalNOX.getAverage(),1);
  display.setCursor(xMaxColumn,yNOXRow); display.print(totalNOX.getMax(),1);

  display.setCursor(xMinColumn,yTempFRow); display.print(totalTemperatureF.getMin(),1);
  display.setCursor(xAvgColumn,yTempFRow); display.print(totalTemperatureF.getAverage(),1);
  display.setCursor(xMaxColumn,yTempFRow); display.print(totalTemperatureF.getMax(),1);

  display.setCursor(xMinColumn,yHumidityRow); display.print(totalHumidity.getMin(),0);
  display.setCursor(xAvgColumn,yHumidityRow); display.print(totalHumidity.getAverage(),0);
  display.setCursor(xMaxColumn,yHumidityRow); display.print(totalHumidity.getMax(),0);

  display.setCursor(xMinColumn,yAQIRow); display.print(pm25toAQI_US(totalPM25.getMin()),1);
  display.setCursor(xAvgColumn,yAQIRow); display.print(pm25toAQI_US(totalPM25.getAverage()),1);
  display.setCursor(xMaxColumn,yAQIRow); display.print(pm25toAQI_US(totalPM25.getMax()),1);

}

bool screenAlert(String messageText)
// Description: Display error message centered on screen, using different font sizes and/or splitting to fit on screen
// Parameters: String containing error message text
// Output: NA (void)
// Improvement: ?
{
  bool success = false;
  int16_t x1, y1;
  uint16_t largeFontPhraseOneWidth, largeFontPhraseOneHeight;

  debugMessage("screenAlert start",1);

  display.setTextColor(ILI9341_WHITE);
  display.fillScreen(ILI9341_BLACK);

  debugMessage(String("screenAlert text is '") + messageText + "'",2);

  // does message fit on one line?
  display.setFont(&FreeSans24pt7b);
  display.getTextBounds(messageText.c_str(), 0, 0, &x1, &y1, &largeFontPhraseOneWidth, &largeFontPhraseOneHeight);
  if (largeFontPhraseOneWidth <= (display.width()-(display.width()/2-(largeFontPhraseOneWidth/2)))) {
    // fits with large font, display
    display.setCursor(((display.width()/2)-(largeFontPhraseOneWidth/2)),((display.height()/2)+(largeFontPhraseOneHeight/2)));
    display.print(messageText);
    success = true;
  }
  else {
    // does message fit on two lines?
    debugMessage(String("text with large font is ") + abs(largeFontPhraseOneWidth - (display.width()-(display.width()/2-(largeFontPhraseOneWidth/2)))) + " pixels too long, trying 2 lines", 1);
    // does the string break into two pieces based on a space character?
    uint8_t spaceLocation;
    String messageTextPartOne, messageTextPartTwo;
    uint16_t largeFontPhraseTwoWidth, largeFontPhraseTwoHeight;

    spaceLocation = messageText.indexOf(' ');
    if (spaceLocation) {
      // has a space character, measure two lines
      messageTextPartOne = messageText.substring(0,spaceLocation);
      messageTextPartTwo = messageText.substring(spaceLocation+1);
      display.getTextBounds(messageTextPartOne.c_str(), 0, 0, &x1, &y1, &largeFontPhraseOneWidth, &largeFontPhraseOneHeight);
      display.getTextBounds(messageTextPartTwo.c_str(), 0, 0, &x1, &y1, &largeFontPhraseTwoWidth, &largeFontPhraseTwoHeight);
      debugMessage(String("Message part one with large font is ") + largeFontPhraseOneWidth + " pixels wide",2);
      debugMessage(String("Message part two with large font is ") + largeFontPhraseTwoWidth + " pixels wide",2);
    }
    else {
      debugMessage("there is no space in message to break message into 2 lines",2);
    }
    if (spaceLocation && (largeFontPhraseOneWidth <= (display.width()-(display.width()/2-(largeFontPhraseOneWidth/2)))) && (largeFontPhraseTwoWidth <= (display.width()-(display.width()/2-(largeFontPhraseTwoWidth/2))))) {
        // fits on two lines, display
        display.setCursor(((display.width()/2)-(largeFontPhraseOneWidth/2)),(display.height()/2+largeFontPhraseOneHeight/2)-25);
        display.print(messageTextPartOne);
        display.setCursor(((display.width()/2)-(largeFontPhraseTwoWidth/2)),(display.height()/2+largeFontPhraseTwoHeight/2)+25);
        display.print(messageTextPartTwo);
        success = true;
    }
    else {
      // does message fit on one line with small text?
      debugMessage("couldn't break text into 2 lines or one line is too long, trying small text",1);
      uint16_t smallFontWidth, smallFontHeight;

      display.setFont(&FreeSans18pt7b);
      display.getTextBounds(messageText.c_str(), 0, 0, &x1, &y1, &smallFontWidth, &smallFontHeight);
      if (smallFontWidth <= (display.width()-(display.width()/2-(smallFontWidth/2)))) {
        // fits with small size
        display.setCursor(display.width()/2-smallFontWidth/2,display.height()/2+smallFontHeight/2);
        display.print(messageText);
        success = true;
      }
      else {
        // doesn't fit at any size/line split configuration, display as truncated, large text
        debugMessage(String("text with small font is ") + abs(smallFontWidth - (display.width()-(display.width()/2-(smallFontWidth/2)))) + " pixels too long, displaying truncated", 1);
        display.setFont(&FreeSans12pt7b);
        display.getTextBounds(messageText.c_str(), 0, 0, &x1, &y1, &largeFontPhraseOneWidth, &largeFontPhraseOneHeight);
        display.setCursor(display.width()/2-largeFontPhraseOneWidth/2,display.height()/2+largeFontPhraseOneHeight/2);
        display.print(messageText);
      }
    }
  }
  debugMessage("screenAlert end",1);
  return success;
}

// Draw a simple graph of recent CO2 values. Time-ordered data to be plottted is stored in an array with the
// most recent point last.  Values of -1 are to be skipped in the plotting, allowing the line of points to
// always have the most recent value at the right edge of the graph but still work if not enough data has yet
// been reported to fully cover the plot area.
void screenGraph()
// Displays CO2 values over time as a graph
{
  int16_t i, x1, y1;
  uint16_t width, height, deltax, w1, h1, x, y, xp, yp;
  uint16_t gx0, gy0, gx1, gy1;  // Drawing area bouding box
  String minlabel, maxlabel, xlabel;
  float c, minvalue, maxvalue;
  bool firstpoint = true, nodata = true;

  debugMessage("screenGraph start",1);
  width = display.width();
  height = display.height();

  // Set drawing area bounding box values
  gx0 = 50;
  gy0 = 10;
  gx1 = width-30;
  gy1 = height-30;  // Room at the bottom for the graph label

  display.fillScreen(ILI9341_BLACK);
  display.setFont(&FreeSans9pt7b);

  // Scan the retained CO2 data for max & min to scale the plot
  minvalue = 5000;
  maxvalue = 0;
  for(i=0;i<GRAPH_POINTS;i++) {
    if(co2data[i] == -1) continue;   // Skip "empty" slots
    nodata = false;  // At least one data point
    if(co2data[i] < minvalue) minvalue = co2data[i];
    if(co2data[i] > maxvalue) maxvalue = co2data[i];
  }
  // Deal with no data condition (e.g., just booted)
  if(nodata) {
    // Label plot with "awating" message
    display.setTextColor(ILI9341_WHITE);
    xlabel = String("Awaiting CO2 Values");  // Center overall graph label below the drawing area
    display.getTextBounds(xlabel.c_str(),0,0,&x1,&y1,&w1,&h1);
    display.setCursor( ((width-w1)/2),(height-(h1/2)) );
    display.print(xlabel);
    // Set reasonable bounds for the (empty) plot
    minvalue = 400;
    maxvalue = 1200;
  }
  else {
    // We have data to plot, so say so
    display.setTextColor(ILI9341_WHITE);
    xlabel = String("Recent CO2 Values");  // Center overall graph label below the drawing area
    display.getTextBounds(xlabel.c_str(),0,0,&x1,&y1,&w1,&h1);
    display.setCursor( ((width-w1)/2),(height-(h1/2)) );
    display.print(xlabel);

    // Pad min and max CO2 to add room and be multiples of 50 (for nicer axis labels)
    minvalue = (int(minvalue)/50)*50;
    maxvalue = ((int(maxvalue)/50)+1)*50;
  }
  
  display.setTextColor(ILI9341_LIGHTGREY);
  // Draw Y axis labels
  minlabel = String(int(minvalue));
  maxlabel = String(int(maxvalue));
  display.getTextBounds(maxlabel.c_str(),0,0,&x1,&y1,&w1,&h1);
  display.setCursor(gx0-5-w1,h1+5); display.print(maxlabel);
  display.getTextBounds(minlabel.c_str(),0,0,&x1,&y1,&w1,&h1);
  display.setCursor(gx0-5-w1,gy1); display.print(minlabel);

  // Draw axis lines
  display.drawLine(gx0,gy0,gx0,gy1,ILI9341_LIGHTGREY);
  display.drawLine(gx0,gy1,gx1,gy1,ILI9341_LIGHTGREY);
  display.setTextColor(ILI9341_WHITE);

  // Plot however many data points we have both with filled circles at each
  // point and lines connecting the points.  Color the filled circles with the
  // appropriate CO2 warning level color.
  deltax = (gx1 - gx0 - 10)/ (GRAPH_POINTS-1);  // 10 pixel padding for Y axis
  xp = gx0;
  yp = gy1;
  for(i=0;i<GRAPH_POINTS;i++) {
    if(co2data[i] == -1) continue;
    c = co2data[i];
    x = gx0 + 10 + (i*deltax);  // Include 10 pixel padding for Y axis
    y = gy1 - (((c - minvalue)/(maxvalue-minvalue)) * (gy1-gy0));
    display.fillCircle(x,y,4,warningColor[co2Range(c)]);
    if(firstpoint) {
      // If this is the first drawn point then don't try to draw a line
      firstpoint = false;
    }
    else {
      // Draw line from previous point (if one) to this point
      display.drawLine(xp,yp,x,y,ILI9341_WHITE);
    }
    // Save x & y of this point to use as previous point for next one.
    xp = x;
    yp = y;
  }

  debugMessage("screenGraph end",1);
}
// Accumulate a CO2 value into the data array used by the screen graphing function above
void retainCO2(uint16_t co2)
{
  for(uint8_t loop=1;loop<GRAPH_POINTS;loop++) {
    co2data[loop-1] = co2data[loop];
  }
  co2data[GRAPH_POINTS-1] = co2;
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
  int16_t x = random(xMargins,display.width()-xMargins-100);  // 90 pixels leaves room for 4 digit CO2 value
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
  const uint16_t yLegend = ((display.height() / 2 ) - (legendHeight * 2));
  const uint16_t circleRadius = 100;
  const uint16_t xVOCCircle = (display.width() / 2);
  const uint16_t yVOCCircle = (display.height() / 2);
  const uint16_t xVOCLabel = xVOCCircle - 35;
  const uint16_t yVOCLabel = yVOCCircle + 35;

  debugMessage("screenVOC start",1);

  // clear screen
  display.fillScreen(ILI9341_BLACK);

  // VOC color circle
  display.fillCircle(xVOCCircle,yVOCCircle,circleRadius,warningColor[vocRange(sensorData.vocIndex)]);
  display.fillCircle(xVOCCircle,yVOCCircle,circleRadius*0.8,ILI9341_BLACK);

  // VOC color legend
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  // VOC value and label (displayed inside circle)
  display.setFont(&FreeSans18pt7b);
  display.setTextColor(warningColor[vocRange(sensorData.vocIndex)]);  // Use highlight color look-up table
  display.setCursor(xVOCCircle-20,yVOCCircle);
  display.print(int(sensorData.vocIndex+.5));
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(xVOCLabel,yVOCLabel);
  display.print("VOC");

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

void screenHelperReportStatus(uint16_t initialX, uint16_t initialY)
// Description: helper function for screenXXX() routines that displays an icon relaying success of network endpoint writes
// Parameters: initial x and y coordinate to draw from
// Output : NA
// Improvement : NA
// 
{
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
    if ((timeLastReportMS == 0) || ((millis() - timeLastReportMS) >= (reportIntervalMS * reportFailureThreshold)))
        // we haven't successfully written to a network endpoint at all or before the reportFailureThreshold
        display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, ILI9341_RED);
      else
        display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, ILI9341_GREEN);
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
    debugMessage(String("SIMULATED SEN5x PM2.5: ")+sensorData.pm25+" ppm, VOC index: " + sensorData.vocIndex,1);
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
    if (WiFi.status() == WL_CONNECTED) 
    {
      String jsonBuffer;

      // OWM latitude + longitude is "lat=xx.xxx&lon=-yyy.yyyy"
      String serverPath = OWMServer + OWMWeatherPath +
       "lat=" + hardwareData.latitude + "&lon=" + hardwareData.longitude + "&units=imperial&APPID=" + OWMKey;

      jsonBuffer = networkHTTPGETRequest(serverPath.c_str());
      debugMessage("Raw JSON from OWM Current Weather feed", 2);
      debugMessage(jsonBuffer, 2);
      if (jsonBuffer == "HTTP GET error") {
        debugMessage("OWM Weather: HTTP Get error",1);
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
    else {
      debugMessage("No network for OWM Weather update connection",1);
      return false;
    }
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
    if (WiFi.status() == WL_CONNECTED)
    {
      String jsonBuffer;

      // Get local AQI
      String serverPath = OWMServer + OWMAQMPath +
       "lat=" + hardwareData.latitude + "&lon=" + hardwareData.longitude + "&APPID=" + OWMKey;

      jsonBuffer = networkHTTPGETRequest(serverPath.c_str());
      debugMessage("Raw JSON from OWM AQI feed", 2);
      debugMessage(jsonBuffer, 2);
      if (jsonBuffer == "HTTP GET error") {
        debugMessage("OWM AQI: HTTP Get error",1);
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
    else {
      debugMessage("No network for OWM AQI update connection",1);
      return false;
    }
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

void endPointWrite(uint8_t numSamples)
{
  #if !defined (HARDWARE_SIMULATE) && (defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK))
      // do we have samples to report?
    if (numSamples) {
      if (WiFi.status() == WL_CONNECTED) {
        // Get averaged sample values from the Measure utliity class objects
        float avgTemperatureF = totalTemperatureF.getAverage();
        float avgHumidity = totalHumidity.getAverage();
        uint16_t avgCO2 = totalCO2.getAverage();
        float avgVOC = totalVOC.getAverage();
        float avgPM25 = totalPM25.getAverage();
        float maxPM25 = totalPM25.getMax();
        float minPM25 = totalPM25.getMin();
        float avgNOX = totalNOX.getAverage();

        debugMessage(String("** ----- Reporting averages (") + (reportIntervalMS/60000) + " minutes) ----- ",1);

        debugMessage(String("** PM2.5: ") + avgPM25 + String(", CO2: ") + avgCO2 + " ppm" + 
          String(", VOC: ") + avgVOC+ String(", NOX: ") + avgNOX + String(", ") + 
          avgTemperatureF + "F, " + avgHumidity + String("%, ") + String("AQI(US): ") + pm25toAQI_US(avgPM25),1);

        #ifdef THINGSPEAK
        // IMPROVEMENT : Post the AQI sensor data to ThingSpeak. Make sure to use the PM25 to AQI conversion formula for the
        // desired country as there is no global standard.
          if (!post_thingspeak(avgPM25, avgCO2, avgTemperatureF, avgHumidity, avgVOC, avgNOX, pm25toAQI_US(avgPM25)) ) {
            Serial.println("ERROR: Did not write to ThingSpeak");
          }
        #endif

        #ifdef INFLUX
          // IMPROVEMENT: Modify to include NOX readings
          if (!post_influx(avgTemperatureF, avgHumidity, avgCO2 , avgPM25, avgVOC, avgNOX, hardwareData.rssi))
            Serial.println("ERROR: Did not write to influxDB");
        #endif

        #ifdef MQTT
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

        // Retain CO2 data for graphing (see below)
        retainCO2(avgCO2);

        // Reset sample counters
        totalTemperatureF.clear();
        totalHumidity.clear();
        totalCO2.clear();
        totalVOC.clear();
        totalPM25.clear();
        totalNOX.clear();
      }
      else {
        debugMessage("No network for endpoint reporting",1);
      }
    }      
    else {
      debugMessage("No samples for endpoint reporting",1);
    }
  #endif
}

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
#endif // HARDWARE_SIMULATE

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

/**************************************************************************************
 *                 SENSOR SPECIFIC ROUTINES AND CONVENIENCE FUNCTIONS                 *
 *************************************************************************************/

// Generalized entry point for sensor initialization
bool sensorInit()
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

// Generalized entry point for reading sensor values
bool sensorRead()
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
          debugMessage("Error trying to execute deviceReset(): ",1);
          errorToString(error, errorMessage, sizeof errorMessage);
          debugMessage(errorMessage,1);
          return false;
      }
      delay(1200);
      int8_t serialNumber[32] = {0};
      error = paqSensor.getSerialNumber(serialNumber, 32);
      if (error != 0) {
          debugMessage("Error trying to execute SEN6x getSerialNumber(): ",1);
          errorToString(error, errorMessage, sizeof errorMessage);
          debugMessage(errorMessage,1);
          return false;
      }
      debugMessage("SEN6x serial number: ",2);
      debugMessage((const char *)serialNumber,2);
      error = paqSensor.startContinuousMeasurement();
      if (error != 0) {
          debugMessage("Error trying to execute SEN6x startContinuousMeasurement(): ",1);
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
      float ambientTemperatureC;

      // TODO: Add support for checking isDataReady flag on SEN66

      error = paqSensor.readMeasuredValues(
        sensorData.pm1, sensorData.pm25, sensorData.pm4,
        sensorData.pm10, sensorData.ambientHumidity, ambientTemperatureC , sensorData.vocIndex,
        sensorData.noxIndex, sensorData.ambientCO2);

    if (error != 0) {
        debugMessage("Error trying to execute readMeasuredValues(): ",1);
        errorToString(error, errorMessage, sizeof errorMessage);
        debugMessage(errorMessage,1);
        return false;
    }
    // Convert temperature Celsius from sensor into Farenheit
    sensorData.ambientTemperatureF = (1.8*ambientTemperatureC) + 32.0;
    
    debugMessage(String("SEN6x: PM2.5: ") + sensorData.pm25 + String(", CO2: ") + sensorData.ambientCO2 +
      String(", VOC: ") + sensorData.vocIndex + String(", NOX: ") + sensorData.noxIndex + String(", ") + 
      sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + String("%, "),1);
    return true;
  }

  #ifdef HARDWARE_SIMULATE
    // Simulates sensor reading from SEN66 sensor
    void sensorSEN6xSimulate()
    {
      sensorData.pm1 = random(sensorPMMin, sensorPMMax) / 100.0;
      sensorData.pm10 = random(sensorPMMin, sensorPMMax) / 100.0;
      sensorData.pm25 = random(sensorPMMin, sensorPMMax) / 100.0;
      sensorData.pm4 = random(sensorPMMin, sensorPMMax) / 100.0;
      sensorData.vocIndex = random(sensorVOCMin, sensorVOCMax) / 100.0;
      sensorData.noxIndex = random(sensorVOCMin, sensorVOCMax) / 10.0;
      sensorData.ambientTemperatureF = (random(sensorTempMinF,sensorTempMaxF) / 100.0);
      sensorData.ambientHumidity = random(sensorHumidityMin,sensorHumidityMax) / 100.0;
      sensorData.ambientCO2 = random(sensorCO2Min, sensorCO2Max);

      debugMessage(String("SIMULATED SEN66 PM2.5: ")+sensorData.pm25+" ppm, VOC index: " + sensorData.vocIndex +
        sensorData.pm25+" ppm, VOC index: " + sensorData.vocIndex + ",NOX index: " + sensorData.noxIndex,1);

    }
  #endif // HARDWARE_SIMULATE
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
      sensorCO2Simulate();
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
          sensorData.ambientCO2 = co2;
          debugMessage(String("SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
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

uint8_t co2Range(uint16_t co2) 
// converts co2 value to index value for labeling and color
{
  uint8_t co2Range = 
    (co2 <= co2Fair) ? 0 :
    (co2 <= co2Poor) ? 1 :
    (co2 <= co2Bad)  ? 2 : 3;

  debugMessage(String("CO2 input of ") + co2 + " yields co2Range of " + co2Range, 2);
  return co2Range;
}

uint8_t aqiRange(float pm25)
// converts pm25 value to index value for labeling and color
{
  uint8_t aqi =
  (pm25 <= pmFair) ? 0 :
  (pm25 <= pmPoor) ? 1 :
  (pm25 <= pm2Bad) ? 2 : 3;

  debugMessage(String("PM2.5 input of ") + pm25 + " yields " + aqi + " aqi",2);
  return aqi;
}

uint8_t vocRange(float vocIndex)
// converts vocIndex value to index value for labeling and color
{
  uint8_t vocRange =
  (vocIndex <= vocFair) ? 0 :
  (vocIndex <= vocPoor) ? 1 :
  (vocIndex <= vocBad)  ? 2 : 3;

  debugMessage(String("VOC index input of ") + vocIndex + " yields " + vocRange + " vocRange",2);
  return vocRange;
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

void powerDisable(uint32_t deepSleepTime)
// turns off component hardware then puts ESP32 into deep sleep mode for specified seconds
{
  debugMessage("powerDisable start",1);

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
  
  // turn off TFT backlight
  digitalWrite(TFT_BACKLIGHT, LOW);

  networkDisconnect();

  esp_sleep_enable_timer_wakeup(deepSleepTime);
  debugMessage(String("powerDisable complete: ESP32 deep sleep for ") + (deepSleepTime/1000000) + " seconds",1);
  esp_deep_sleep_start();
}