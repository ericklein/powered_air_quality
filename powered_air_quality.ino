/*
  Project:        Powered Air Quality
  Description:    Sample and log indoor air quality via AC powered device

  See README.md for target information
*/

#include "config.h"               // hardware and internet configuration parameters
#include "powered_air_quality.h"  // global data structures
#include "secrets.h"              // private credentials for network, MQTT
#include "data.h"

#include <math.h>
#include <HTTPClient.h>           // used to access Open Weather Map
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <Measure.hpp>            // https://github.com/disquisitioner/Measure, utility class for collecting, processing, and reporting periodic data
#include <Preferences.h>          // read-write to ESP32 persistent storage
#include <TimeLib.h>              // https://github.com/PaulStoffregen/Time, used to process OWM Forecast
#include <TFT_eSPI.h>             // https://github.com/Bodmer/TFT_eSPI
#include <XPT2046_Touchscreen.h>  // https://github.com/PaulStoffregen/XPT2046_Touchscreen
#include <ArduinoJson.h>          // https://github.com/bblanchon/ArduinoJson, used by OWM retrieval routines

#include "ui/fonts/Roboto_Regular_18.h"
#include "ui/fonts/Roboto_Regular_24.h"
#include "ui/fonts/Roboto_Regular_36.h"

// instanstiate SEN5X hardware object
#include <SensirionI2CSen5x.h>
SensirionI2CSen5x pmSensor;

// instanstiate SCD4X hardware object
#include <SensirionI2cScd4x.h>
SensirionI2cScd4x co2Sensor;

Preferences nvConfig;

// WiFiManager global configuration
WiFiClient client;   // WiFiManager loads WiFi.h, which is used by OWM and MQTT
WiFiManager wfm;

// WiFiManager parameter backing buffers
char wfmLatitudeStr[20];
char wfmLongitudeStr[20];
char wfmAltitudeStr[6];
char wfmMqttPortStr[6];
char wfmInfluxPortStr[6];

// Persistent WiFiManagerParameter pointers
WiFiManagerParameter* pHintText = nullptr;
WiFiManagerParameter* pSeparator = nullptr;

WiFiManagerParameter* pDeviceLatitude = nullptr;
WiFiManagerParameter* pDeviceLongitude = nullptr;
WiFiManagerParameter* pDeviceAltitude = nullptr;
WiFiManagerParameter* pDeviceID = nullptr;

#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
  WiFiManagerParameter* pDeviceSite = nullptr;
  WiFiManagerParameter* pDeviceLocation = nullptr;
  WiFiManagerParameter* pDeviceRoom = nullptr;
#endif

#ifdef MQTT
  WiFiManagerParameter* pMQTTHeader = nullptr;
  WiFiManagerParameter* pMqttBroker = nullptr;
  WiFiManagerParameter* pMqttPort = nullptr;
  WiFiManagerParameter* pMqttUser = nullptr;
  WiFiManagerParameter* pMqttPassword = nullptr;
#endif

#ifdef INFLUX
  WiFiManagerParameter* pInfluxHeader = nullptr;
  WiFiManagerParameter* pInfluxBroker = nullptr;
  WiFiManagerParameter* pInfluxPort = nullptr;
  WiFiManagerParameter* pInfluxOrg = nullptr;
  WiFiManagerParameter* pInfluxBucket = nullptr;
  WiFiManagerParameter* pInfluxEnvMeasurement = nullptr;
  WiFiManagerParameter* pInfluxDevMeasurement = nullptr;
#endif

bool wfmParametersAdded = false;
bool wfmPortalRunning = false;
bool saveWFMConfig = false;
uint32_t wfmPortalStartMS = 0;

// 2.8″ 320x240 color TFT
TFT_eSPI display = TFT_eSPI();
enum screenNames screenCurrent = sMain; // Initial screen to display (on startup)

// Screen specific functions that reside separately in screens.cpp
extern void screenMain();
extern void screenVOC();
extern void screenCO2();
extern void screenPM25();
extern void screenForecast();
// other functions residing in screens.cpp
extern uint8_t co2Range(float);
extern uint8_t pm25Range(float);
extern uint8_t vocRange(float);

// CYD 2432S028R -> XPT2046
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(pinTouchCS,pinTouchIRQ);

#ifdef THINGSPEAK
  extern bool post_thingspeak(float pm25, float co2, float temperatureF, float humidity, 
    float vocIndex, float aqi);
#endif

#ifdef INFLUX
  extern bool post_influx(float temperatureF, float humidity, uint16_t co2, float pm25, float vocIndex, uint8_t rssi);
#endif

#ifdef MQTT
  #include <PubSubClient.h>     // https://github.com/knolleary/pubsubclient
  PubSubClient mqtt(client);

  extern bool mqttConnect();
  extern void mqttPublish(const char* topic, const String& payload);
  extern const char* generateMQTTTopic(String key);
  extern bool mqttPublishValue(String key, const String& payload);

  #ifdef HASSIO_MQTT
    extern bool hassio_mqtt_publish(float pm25, float co2, float temperatureF, float humidity, float aqi);
  #endif
#endif

// data structures defined in powered_air_quality.h
networkEndpointConfig endpointPath;
hdweData hardwareData;
OpenWeatherMapAirQuality owmAirQuality;
SiteForecast owmSiteForecast;
influxConfig influxdbConfig; // available globally for nvconfig use
MqttConfig mqttBrokerConfig; // available globally for nvconfig use

// Utility class used to streamline accumulating sensor values, averages, min/max &c.  Each
// instance contains storage to retain points for subsequent processing, which are used
// here to graph recent data. The size of that retatined data is based on the
// kSampleCapacity value defined in config.h.
Measure<kSampleCapacity> totalTemperatureF, totalHumidity, totalCO2, totalVOCIndex, totalPM25;

uint32_t timeLastReportMS = 0;  // timestamp for last report to network endpoints

// alert management
uint32_t alertStartMS = 0;
uint32_t alertLengthMS = 0;
bool alertScreen = false;
bool alertLED = false;
bool alertSound = false;

void setup() {
  // config Serial first for debugMessage()
  #ifdef DEBUG
    Serial.begin(115200);
    // wait for serial port connection
    while (!Serial);
    // Display key configuration parameters
    debugMessage(String("Starting Powered Air Quality with ") + (timeSensorSampleMS/1000) + String(" second sample interval"),1);
    #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
      debugMessage(String("Report interval is ") + (timeReportMS/60000) + " minutes",1);
    #endif
  #endif

  // generate truely random numbers
  randomSeed(esp_random());

  display.begin();
  display.setRotation(screenRotation);
  display.setTextWrap(false);
  display.fillScreen(TFT_BLACK);
  // set LED backlight
  ledcAttach(TFT_BL, 5000, 8); // 5000 = pwm frequency, 8 = bit resolution
  ledcWrite(TFT_BL, screenBLMax);

  display.loadFont(Roboto_Regular_36);
  screenHelperAlert("Initializing",TFT_WHITE,TFT_BLACK,TFT_BLUE);
  display.unloadFont();

  // CYD 2432S028R -> XPT2046
  Wire.begin(pinSensorSDA, pinSensorSCL);
  touchscreenSPI.begin(pinTouchCLK, pinTouchMISO, pinTouchMOSI, pinTouchCS); // setup the VSPI to use CYD touchscreen pins
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(screenRotation);

  // initialize GPIO
  pinMode(pinButton, INPUT_PULLUP);

  ledcAttach(pinAudio, audioFrequency, audioResolution);

  // get configuration data before calling sensorInit() to load altitude value
  if(!nvconfigRead()) {
    // no configuration parameters in non-volatile storage, so write defaults
    nvconfigDefaultsLoad();
    nvconfigWrite();
  }   

  // initialize sensor(s)
  if( !sensorInit()) {
    // error often occurs after firmware flash/reset
    display.loadFont(Roboto_Regular_24);
    deviceReboot("Sensor failure, rebooting", 5000);
    display.unloadFont();
  }
  networkWiFiManagerOpen();
}

void loop() {
  static uint8_t numSamples               = 0;  // Number of sensor readings over reporting interval
  static uint32_t timeLastSampleMS        = -(timeSensorSampleMS); // forces immediate sample in loop() 
  static uint32_t timeLastInputMS         = millis();  // timestamp for last user input (screensaver)
  uint16_t calibratedX, calibratedY;

  // order of operation
  // 0 - update current alerts
  // 1 - feed cycles to LEDControl
  // 2 - feed cycles to web portal
  // 3 - handle touchscreen input
  // 4 - handle button press
  // ------------------------- interupts and cycles fed
  // 5 - read sensor
  // 6 - update screen saver
  // 7 - network endpoint(s) write?

  // update current alerts
  alertHandle();

  // feed processor cycles to the web portal if needed
  if (wfm.getWebPortalActive()) {
    wfm.process();

    if (saveWFMConfig) {
      networkWiFiManagerSaveParameterValues();
      saveWFMConfig = false;
      networkWiFiManagerRefreshParameterValues();
    }

    if (wfmPortalRunning &&
        millis() - wfmPortalStartMS > timeWebPortalTimeOutMS) {
      wfm.stopWebPortal();
      wfmPortalRunning = false;
    }
  }

  // is there user input to process?
  bool touchEvent = false;
  // CYD 2432S028R -> XPT2046
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    // get raw 12bit touchscreen x,y and then calibrate to screen size
    TS_Point p = touchscreen.getPoint();
    calibratedX = map(p.x, touchscreenMinX, touchscreenMaxX, 1, display.width());
    calibratedY = map(p.y, touchscreenMinY, touchscreenMaxY, 1, display.height());
    // alternate conversion
    // uint16_t calibratedX = (uint16_t)((p.x - touchscreenMinX) * display.width() / (touchscreenMaxX - touchscreenMinX));
    // uint16_t calibratedY = (uint16_t)((p.y - touchscreenMinY) * display.height() / (touchscreenMaxY - touchscreenMinY));
    touchEvent = true;
  }
  if (touchEvent) {
    debugMessage(String("touch input x=") + calibratedX + ", y=" + calibratedY,2);
    ledcWrite(TFT_BL, screenBLMax);
    if (screenCurrent == sMain) {
      if (calibratedY < 122) { // top components
        if (calibratedX < 107) {
          screenCurrent = sForecast;
        }
        else {
          screenCurrent = sCO2;
        }
      }
      else {
        if (calibratedX < 107) {
          screenCurrent = sVOC;
        }
        else {
            // only PM25 guage displayed
            if (calibratedX < 214) {
              screenCurrent = sPM25;
            }
        }
      }
    }
    else {
      screenCurrent = sMain;
    }
    screenUpdate(screenCurrent);
    timeLastInputMS = millis();
  }

  // (reset) button press that needs to be handled?
  checkButtonPress();

    // is it time to read the sensor?
  if ((millis() - timeLastSampleMS) >= timeSensorSampleMS) {
    // Read sensor(s)
    if (sensorRead()) {
      numSamples++;
      // IMPROVEMENT: evaluate whether the screen actually needs updated based on changed data
      screenUpdate(screenCurrent);
      if (sampleEvaluate()) {
        // ALERT: 5 second, sound, LED, and screen
        alertLengthMS = 5000;
        alertStartMS = millis();
        alertScreen = true;
        alertSound = true;
        ledcWriteTone(pinAudio, audioFrequency);
        display.loadFont(Roboto_Regular_24);
        screenHelperAlert("CO2 rising rapidly", TFT_WHITE,TFT_BLACK,TFT_RED);
        display.unloadFont();
      }
    }
    else {
      // ALERT: 5 second screen alert, no sound or LEDs
      alertScreen = true;
      alertLengthMS = 5000;
      alertStartMS = millis();
      display.loadFont(Roboto_Regular_24);
      screenHelperAlert("Sensor read fail", TFT_WHITE,TFT_BLACK,TFT_YELLOW);
      display.unloadFont();
    }
    // Save last sample time
    timeLastSampleMS = millis();
  }

  // is it time to enable the screensaver?
  if ((millis() - timeLastInputMS) > timeScreenSaverStartMS) {
    ledcWrite(TFT_BL, screenBLLow);
  }

  // is it time to write to the network endpoints?
  if ((millis() - timeLastReportMS) >= timeReportMS) {
    samplePost(numSamples);
    timeLastReportMS = millis();
  }
}

void screenUpdate(uint8_t screenCurrent) 
{
  switch(screenCurrent) {
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
    case sForecast:
      screenForecast();
      break;
  }
}

/**
 * @brief Draw a centered rounded-rectangle "bubble" containing a one- or two-line message.
 *
 * The message is split using pixel width measurements and word boundaries. If the message
 * is too long, truncation is applied at the end of the overall message (ellipsis on line 2
 * only, or on line 1 if it must be single-line). The rounded rectangle and text are constrained
 * to stay within the display and within a horizontal safe region defined by @p kXMargins.
 *
 * Vertical centering behavior:
 * - If two lines are drawn, the vertical gap between the lines is centered on the screen.
 * - If one line is drawn, the text itself is centered on the screen.
 *
 * @param messageText Message to render inside the bubble.
 * @param fgColor   Text color.
 * @param bgColor   Bubble fill color (also used as text background color).
 * @param borderColor Bubble outline color.
 * @param kXMargins    Horizontal safe margin in pixels applied to both left and right edges.
 *
 * @note Set the desired font and text size on @p display before calling this function.
 */
void screenHelperAlert( const String &messageText, uint16_t fgColor, uint16_t bgColor, uint16_t borderColor) {
  debugMessage(String("screenHelperAlert start()"),1);

  display.setTextColor(fgColor, bgColor, true);
  display.setTextPadding(0);

  const int16_t screenW = (int16_t)display.width();
  const int16_t screenH = (int16_t)display.height();
  const int16_t centerY = screenH / 2;

  const int16_t safeLeft  = (int16_t)kXMargins;
  const int16_t safeRight = (int16_t)(screenW - 1 - (int16_t)kXMargins);
  const int16_t safeW     = safeRight - safeLeft + 1;
  if (safeW <= 0) return;

  const uint16_t lineHeight = (uint16_t)display.fontHeight();

  uint8_t lineSpacing = (uint8_t)(lineHeight / 4);
  if (lineSpacing < 2)  lineSpacing = 2;
  if (lineSpacing > 10) lineSpacing = 10;

  uint8_t padX = (uint8_t)(lineHeight / 3);
  uint8_t padY = (uint8_t)(lineHeight / 4);
  if (padX < 6) padX = 6;
  if (padY < 4) padY = 4;

  uint8_t radius = (uint8_t)(lineHeight / 3);
  if (radius < 6)  radius = 6;
  if (radius > 18) radius = 18;

  const int16_t innerW_signed = safeW - (int16_t)(2 * padX);
  const uint16_t innerW = (innerW_signed > 0) ? (uint16_t)innerW_signed : 0;
  if (innerW == 0) return;

  String line1, line2;
  textSplitTwoLines(messageText, line1, line2, innerW);

  const bool twoLines = (line2.length() > 0);

  const int16_t w1 = (int16_t)display.textWidth(line1);
  const int16_t w2 = twoLines ? (int16_t)display.textWidth(line2) : 0;
  const int16_t textW = (w2 > w1) ? w2 : w1;

  const int16_t textH = twoLines
    ? (int16_t)(2 * (int16_t)lineHeight + (int16_t)lineSpacing)
    : (int16_t)lineHeight;

  int16_t rectW = textW + (int16_t)padX * 2;
  int16_t rectH = textH + (int16_t)padY * 2;

  if (rectW > safeW)   rectW = safeW;
  if (rectH > screenH) rectH = screenH;

  int16_t yTextTop;
  if (!twoLines) {
    yTextTop = centerY - (textH / 2);
  } else {
    const int16_t halfGapTop = (int16_t)(lineSpacing / 2);
    yTextTop = centerY - halfGapTop - (int16_t)lineHeight;
  }

  int16_t rectX = safeLeft + (safeW - rectW) / 2;
  int16_t rectY = yTextTop - (int16_t)padY;

  const int16_t maxRectX = safeLeft + safeW - rectW;
  if (rectX < safeLeft) rectX = safeLeft;
  if (rectX > maxRectX) rectX = maxRectX;

  if (rectY < 0) rectY = 0;
  if (rectY + rectH > screenH) rectY = screenH - rectH;

  const int16_t rectCenterX = rectX + rectW / 2;

  display.fillRoundRect(rectX, rectY, rectW, rectH, radius, bgColor);
  display.drawRoundRect(rectX, rectY, rectW, rectH, radius, borderColor);

  display.setTextDatum(TC_DATUM);
  const int16_t textTopY = rectY + (int16_t)padY;

  display.drawString(line1, rectCenterX, textTopY);
  if (twoLines) {
    display.drawString(line2, rectCenterX,
                   textTopY + (int16_t)lineHeight + (int16_t)lineSpacing);
  }

  debugMessage(String("screenHelperAlert end()"), 1);
}

bool sampleEvaluate()
{
  debugMessage(String("sampleEvaluate() start"), 1);

  static bool trendAlreadyReported = false;

  const uint16_t stored = totalCO2.getStored();

  if (stored < (kRequiredRisingDeltas + 1))
  {
    trendAlreadyReported = false;
    return false;
  }

  const uint16_t startIndex = stored - (kRequiredRisingDeltas + 1);

  float deltas[kRequiredRisingDeltas];

  for (uint8_t i = 0; i < kRequiredRisingDeltas; ++i)
  {
    const uint16_t sampleIndex = startIndex + i;

    deltas[i] = totalCO2.getMember(sampleIndex + 1)
              - totalCO2.getMember(sampleIndex);
  }

  float meanDelta = 0.0f;
  for (uint8_t i = 0; i < kRequiredRisingDeltas; ++i)
  {
    meanDelta += deltas[i];
  }
  meanDelta /= kRequiredRisingDeltas;

  float variance = 0.0f;
  for (uint8_t i = 0; i < kRequiredRisingDeltas; ++i)
  {
    const float diff = deltas[i] - meanDelta;
    variance += diff * diff;
  }
  variance /= kRequiredRisingDeltas;

  const float stdDelta = sqrtf(variance);
  const float threshold = fmaxf(kSigmaMultiplier * stdDelta, kMinSigmaFloor);

  bool rapidRisingTrend = true;

  for (uint8_t i = 0; i < kRequiredRisingDeltas; ++i)
  {
    if (deltas[i] < threshold)
    {
      rapidRisingTrend = false;
      break;
    }
  }

  if (rapidRisingTrend && !trendAlreadyReported)
  {
    trendAlreadyReported = true;

    debugMessage(
      String("Rapid CO2 rise detected across last ")
      + (kRequiredRisingDeltas + 1)
      + " samples",
      1
    );

    return true;
  }

  if (!rapidRisingTrend)
  {
    trendAlreadyReported = false;
  }

  debugMessage(String("sampleEvaluate(): no sustained trend detected"), 2);
  debugMessage(String("sampleEvaluate() end"),1);
  return false;
}

/**
 * @brief Process and report accumulated sensor samples.
 *
 * Computes averaged sensor values from accumulated sample buffers and reports
 * them to enabled network endpoints (ThingSpeak, InfluxDB, MQTT, Home Assistant),
 * depending on compile-time configuration and network availability.
 *
 * If no samples are available, the function exits without performing any
 * reporting.
 *
 * After processing, all accumulated sample buffers are cleared and the
 * sample counter is reset to zero to prepare for the next sampling interval.
 *
 * @param[in,out] numSamples Reference to the number of samples accumulated
 *                           during the current reporting interval. A value
 *                           greater than zero indicates that samples are
 *                           available for processing. This value is reset
 *                           to zero after processing.
 *
 * @note Network reporting is skipped if WiFi is not connected.
 * @note Endpoint support is controlled via compile-time flags
 *       (THINGSPEAK, INFLUX, MQTT, HASSIO_MQTT).
 * @note The sample counter is reset regardless of whether reporting succeeds.
 */
void samplePost(uint8_t& numSamples)
{
  debugMessage(String("samplePost() start"),1);

  // do we have samples to process?
  if (numSamples) {
    // can we report to network endPoints?
    #ifndef HARDWARE_SIMULATE
      // attemot to reconnect to WiFi if needed
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
      }

      if (WiFi.status() == WL_CONNECTED) {
        // Get averaged sample values from Measure class objects for endPoint reporting
        float avgTemperatureF = totalTemperatureF.getAverage();
        float avgHumidity = totalHumidity.getAverage();
        uint16_t avgCO2 = totalCO2.getAverage();
        float avgVOC = totalVOCIndex.getAverage();
        float avgPM25 = totalPM25.getAverage();
        float aqi = pm25toAQI_US(avgPM25);

        debugMessage(String("Averages being sent to endpoints for the last ") + (timeReportMS/60000) + " minutes",2);
        debugMessage(String("PM2.5: ") + avgPM25 + "ppm, CO2: " + avgCO2 + "ppm, " + avgTemperatureF + "F, humidity: " + avgHumidity + "%", 2);

        // update RSSI before publishing
        hardwareData.rssi = networkRSSIRead();

        #ifdef THINGSPEAK
          if (!post_thingspeak(avgPM25, avgCO2, avgTemperatureF, avgHumidity, avgVOC, pm25toAQI_US(avgPM25)) ) {
            debugMessage(String("ERROR: Did not write to ThingSpeak"),1);
          }
        #endif

        #ifdef INFLUX
          if (!post_influx(avgTemperatureF, avgHumidity, avgCO2 , avgPM25, avgVOC, hardwareData.rssi))
            debugMessage(String("ERROR: Did not write to InfluxDB"),1);
        #endif

        #ifdef MQTT
          if(mqttConnect()) {
            // publish device data
            const char* topic;

            // publish hardware data
            mqttPublishValue(VALUE_KEY_RSSI, String(hardwareData.rssi));

            // publish sensor data
            mqttPublishValue(VALUE_KEY_TEMPERATURE, String(avgTemperatureF));
            mqttPublishValue(VALUE_KEY_HUMIDITY, String(avgHumidity));
            mqttPublishValue(VALUE_KEY_PM25, String(avgPM25));
            mqttPublishValue(VALUE_KEY_VOC, String(avgVOC));
            mqttPublishValue(VALUE_KEY_CO2, String(avgCO2));

            #ifdef HASSIO_MQTT
              debugMessage("Establishing MQTT for Home Assistant",1);
              // Either configure sensors in Home Assistant's configuration.yaml file
              // directly or attempt to do it via MQTT auto-discovery
              // hassio_mqtt_setup();  // Config for MQTT auto-discovery
              hassio_mqtt_publish(avgPM25, avgCO2, avgTemperatureF, avgHumidity, avgVOC, aqi);
            #endif

            mqtt.disconnect();
          }
        #endif
      }
      else {
        debugMessage("No network, endpoint reporting skipped",1);
      }
    #endif
  }      
  else {
    // ALERT: 5 second, sound, LED, and screen
    alertLengthMS = 5000;
    alertStartMS = millis();
    alertScreen = true;
    alertSound = true;
    ledcWriteTone(pinAudio, audioFrequency);
    display.loadFont(Roboto_Regular_24);
    screenHelperAlert("No samples available", TFT_WHITE,TFT_BLACK,TFT_RED);
    display.unloadFont();
    debugMessage(String("samplePost() no samples to process this cycle"),1);
  }
  // Reset sample counters
  numSamples = 0;
  totalTemperatureF.clear();
  totalHumidity.clear();
  totalCO2.clear();
  totalVOCIndex.clear();
  totalPM25.clear();
  debugMessage(String("samplePost() end"), 1);
}

uint8_t networkRSSISimulate()
// Description : returns simulated WiFi RSSI value from hardware or simulation value
// Parameters: NA
// Return : NA
// Improvement : NA
{ 
  uint8_t rssi = random(networkRSSIMin, networkRSSIMax);
  debugMessage(String("returning simulated WiFi RSSI: -") + rssi + "db",1);
  return(rssi);
}

void networkWiFiManagerBuildParameters()
{
  if (wfmParametersAdded) {
    return;
  }

  dtostrf(hardwareData.latitude, 0, 5, wfmLatitudeStr);
  dtostrf(hardwareData.longitude, 0, 5, wfmLongitudeStr);
  utoa(hardwareData.altitude, wfmAltitudeStr, 10);

  pHintText = new WiFiManagerParameter(
    "<small>*If you want to connect to already connected AP, leave SSID and password fields empty</small>"
  );

  pSeparator = new WiFiManagerParameter(
    "<hr style='margin:20px 0; border:0; border-top:1px solid #888;'>"
  );

  pDeviceLatitude = new WiFiManagerParameter(
    "deviceLatitude",
    "What is the latitude where this device is located?",
    wfmLatitudeStr,
    16
  );

  pDeviceLongitude = new WiFiManagerParameter(
    "deviceLongitude",
    "What is the longitude where this device is located?",
    wfmLongitudeStr,
    16
  );

  pDeviceAltitude = new WiFiManagerParameter(
    "deviceAltitude",
    "What is the altitude where this device is located?",
    wfmAltitudeStr,
    sizeof(wfmAltitudeStr)
  );

  pDeviceID = new WiFiManagerParameter(
    "deviceID",
    "Optional: Give this device a unique name",
    endpointPath.deviceID.c_str(),
    30
  );

  wfm.addParameter(pHintText);
  wfm.addParameter(pSeparator);
  wfm.addParameter(pDeviceLatitude);
  wfm.addParameter(pDeviceLongitude);
  wfm.addParameter(pDeviceAltitude);
  wfm.addParameter(pDeviceID);

#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
  pDeviceSite = new WiFiManagerParameter(
    "deviceSite",
    "What is a single number or word to describe the building this device is in?",
    endpointPath.site.c_str(),
    20
  );

  pDeviceLocation = new WiFiManagerParameter(
    "deviceLocation",
    "Is the device indoors or outdoors",
    endpointPath.location.c_str(),
    20
  );

  pDeviceRoom = new WiFiManagerParameter(
    "deviceRoom",
    "What is a good name for the room this device is in?",
    endpointPath.room.c_str(),
    20
  );

  wfm.addParameter(pDeviceSite);
  wfm.addParameter(pDeviceLocation);
  wfm.addParameter(pDeviceRoom);
#endif

#ifdef MQTT
  utoa(mqttBrokerConfig.port, wfmMqttPortStr, 10);

  pMQTTHeader = new WiFiManagerParameter(
    "<h3 style='margin-top:20px;'>MQTT parameters</h3><hr>"
  );

  pMqttBroker = new WiFiManagerParameter(
    "mqttBroker",
    "MQTT broker address",
    mqttBrokerConfig.host.c_str(),
    30
  );

  pMqttPort = new WiFiManagerParameter(
    "mqttPort",
    "MQTT broker port",
    wfmMqttPortStr,
    sizeof(wfmMqttPortStr)
  );

  pMqttUser = new WiFiManagerParameter(
    "mqttUser",
    "MQTT username",
    mqttBrokerConfig.user.c_str(),
    20
  );

  pMqttPassword = new WiFiManagerParameter(
    "mqttPassword",
    "MQTT password for username",
    mqttBrokerConfig.password.c_str(),
    20
  );

  wfm.addParameter(pMQTTHeader);
  wfm.addParameter(pMqttBroker);
  wfm.addParameter(pMqttPort);
  wfm.addParameter(pMqttUser);
  wfm.addParameter(pMqttPassword);
#endif

#ifdef INFLUX
  utoa(influxdbConfig.port, wfmInfluxPortStr, 10);

  pInfluxHeader = new WiFiManagerParameter(
    "<h3 style='margin-top:20px;'>Influxdb parameters</h3><hr>"
  );

  pInfluxBroker = new WiFiManagerParameter(
    "influxBroker",
    "influxdb server address",
    influxdbConfig.host.c_str(),
    30
  );

  pInfluxPort = new WiFiManagerParameter(
    "influxPort",
    "influxdb server port",
    wfmInfluxPortStr,
    sizeof(wfmInfluxPortStr)
  );

  pInfluxOrg = new WiFiManagerParameter(
    "influxOrg",
    "influx organization name",
    influxdbConfig.org.c_str(),
    20
  );

  pInfluxBucket = new WiFiManagerParameter(
    "influxBucket",
    "influx bucket name",
    influxdbConfig.bucket.c_str(),
    20
  );

  pInfluxEnvMeasurement = new WiFiManagerParameter(
    "influxEnvment",
    "influx environment measurement",
    influxdbConfig.envMeasurement.c_str(),
    20
  );

  pInfluxDevMeasurement = new WiFiManagerParameter(
    "influxDevment",
    "influx device measurement",
    influxdbConfig.devMeasurement.c_str(),
    20
  );

  wfm.addParameter(pInfluxHeader);
  wfm.addParameter(pInfluxBroker);
  wfm.addParameter(pInfluxPort);
  wfm.addParameter(pInfluxOrg);
  wfm.addParameter(pInfluxBucket);
  wfm.addParameter(pInfluxEnvMeasurement);
  wfm.addParameter(pInfluxDevMeasurement);
#endif

  wfmParametersAdded = true;
}

void networkWiFiManagerRefreshParameterValues()
{
  dtostrf(hardwareData.latitude, 0, 5, wfmLatitudeStr);
  dtostrf(hardwareData.longitude, 0, 5, wfmLongitudeStr);
  utoa(hardwareData.altitude, wfmAltitudeStr, 10);

  if (pDeviceLatitude) {
    pDeviceLatitude->setValue(wfmLatitudeStr, 16);
  }

  if (pDeviceLongitude) {
    pDeviceLongitude->setValue(wfmLongitudeStr, 16);
  }

  if (pDeviceAltitude) {
    pDeviceAltitude->setValue(wfmAltitudeStr, sizeof(wfmAltitudeStr));
  }

  if (pDeviceID) {
    pDeviceID->setValue(endpointPath.deviceID.c_str(), 30);
  }

#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
  if (pDeviceSite) {
    pDeviceSite->setValue(endpointPath.site.c_str(), 20);
  }

  if (pDeviceLocation) {
    pDeviceLocation->setValue(endpointPath.location.c_str(), 20);
  }

  if (pDeviceRoom) {
    pDeviceRoom->setValue(endpointPath.room.c_str(), 20);
  }
#endif

#ifdef MQTT
  utoa(mqttBrokerConfig.port, wfmMqttPortStr, 10);

  if (pMqttBroker) {
    pMqttBroker->setValue(mqttBrokerConfig.host.c_str(), 30);
  }

  if (pMqttPort) {
    pMqttPort->setValue(wfmMqttPortStr, sizeof(wfmMqttPortStr));
  }

  if (pMqttUser) {
    pMqttUser->setValue(mqttBrokerConfig.user.c_str(), 20);
  }

  if (pMqttPassword) {
    pMqttPassword->setValue(mqttBrokerConfig.password.c_str(), 20);
  }
#endif

#ifdef INFLUX
  utoa(influxdbConfig.port, wfmInfluxPortStr, 10);

  if (pInfluxBroker) {
    pInfluxBroker->setValue(influxdbConfig.host.c_str(), 30);
  }

  if (pInfluxPort) {
    pInfluxPort->setValue(wfmInfluxPortStr, sizeof(wfmInfluxPortStr));
  }

  if (pInfluxOrg) {
    pInfluxOrg->setValue(influxdbConfig.org.c_str(), 20);
  }

  if (pInfluxBucket) {
    pInfluxBucket->setValue(influxdbConfig.bucket.c_str(), 20);
  }

  if (pInfluxEnvMeasurement) {
    pInfluxEnvMeasurement->setValue(influxdbConfig.envMeasurement.c_str(), 20);
  }

  if (pInfluxDevMeasurement) {
    pInfluxDevMeasurement->setValue(influxdbConfig.devMeasurement.c_str(), 20);
  }
#endif
}

// callback notifying us to save config from web configuration portal
void networkWiFiMgrSaveParamsCallback() {
  saveWFMConfig = true;
  debugMessage(String("networkWiFiMgrPortalCallback() sets saveWFMConfig to true"),1);
}

// callback notifying us WiFiManager did not connect to a (stored) WiFi AP
void networkWiFiMgrAPCallback(WiFiManager *myWiFiManager) {
  debugMessage(String("networkWiFiMgrAPCallback() start"),1);
  // This alert is intentionally a UI blocker, handled by WiFiManager, not alertHandle()
  display.fillScreen(TFT_BLACK);
  display.loadFont(Roboto_Regular_18);
  screenHelperAlert(String("Setup device at http://") + WiFi.softAPIP().toString(),TFT_WHITE,TFT_BLACK,TFT_BLUE);
  display.unloadFont();
  debugMessage(String("Did not connect to (stored) AP, WiFiManager web config portal should start"),1);
  debugMessage(String("networkWiFiMgrAPCallback() end"),1);
}

bool networkWiFiManagerOpen()
// Connect to WiFi network using WiFiManager library
{
  debugMessage("networkWiFiManagerOpen() start",1);
  // make sure Wi-Fi is fully stopped before setting hostname
  WiFi.mode(WIFI_MODE_NULL);
  WiFi.setHostname(endpointPath.deviceID.c_str());

  // set WiFiManager parameters
  wfm.setAPCallback(networkWiFiMgrAPCallback);
  wfm.setSaveParamsCallback(networkWiFiMgrSaveParamsCallback);
  wfm.setBreakAfterConfig(true);
  wfm.setConnectTimeout(timeConnectTimeoutSeconds); // how long to try connecting before continuing
  wfm.setConfigPortalTimeout(timeWebPortalTimeOutMS/1000); // auto close configportal after n seconds
  // wm.setRemoveDuplicateAPs(false); // do not remove duplicate ap names (true)
  // wm.setMinimumSignalQuality(20);  // set min RSSI (percentage) to show in scans, null = 8%
  // wm.setShowInfoErase(false);      // do not show erase button on info page
  // wm.setScanDispPerc(true);       // show RSSI as percentage not graph icons

  // Enable WiFiManager debug outyput based on DEBUG definition
  #if defined(DEBUG) && (DEBUG >= 2)
      wfm.setDebugOutput(true);
  #else
      wfm.setDebugOutput(false);
  #endif

  wfm.setTitle("Climatron Configurator");
  networkWiFiManagerBuildParameters();
  networkWiFiManagerRefreshParameterValues();

  saveWFMConfig = false;

  String parameterText = hardwareDeviceType + " setup";
  bool connected = wfm.autoConnect(parameterText.c_str()); // anonymous ap
  // connected = wfm.autoConnect(hardwareDeviceType + " AP","password"); // password protected AP

  if (saveWFMConfig) {
    debugMessage("getting (new) config parameters from web configuration portal",2);
    networkWiFiManagerSaveParameterValues();
    saveWFMConfig = false;
  }

  if(connected) {
    hardwareData.rssi = networkRSSIRead();
    debugMessage(endpointPath.deviceID + " connected to " + WiFi.SSID() + ", " + WiFi.localIP().toString() + ", " + hardwareData.rssi + "dBm RSSI", 2);
  } 
  else {
    debugMessage("WiFi connection failure; local sensor data ONLY", 1);
    hardwareData.rssi = 255; // 255 indicates no WiFi connection 
    #ifdef HARDWARE_SIMULATE
      networkRSSISimulate();
    #endif
  }
  debugMessage("networkWiFiManagerOpen() end", 1);
  return (connected);
}

void networkWiFiManagerSaveParameterValues()
{
  // IMPROVEMENT: Need to implement range checking
  debugMessage("getting (new) config parameters from web configuration portal", 2);

  hardwareData.altitude =
    static_cast<uint16_t>(strtoul(pDeviceAltitude->getValue(), nullptr, 10));

  hardwareData.latitude =
    strtof(pDeviceLatitude->getValue(), nullptr);

  hardwareData.longitude =
    strtof(pDeviceLongitude->getValue(), nullptr);

  endpointPath.deviceID = pDeviceID->getValue();

  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
    endpointPath.site = pDeviceSite->getValue();
    endpointPath.location = pDeviceLocation->getValue();
    endpointPath.room = pDeviceRoom->getValue();
  #endif

  #ifdef MQTT
    mqttBrokerConfig.host = pMqttBroker->getValue();
    mqttBrokerConfig.port =
      static_cast<uint16_t>(strtoul(pMqttPort->getValue(), nullptr, 10));
    mqttBrokerConfig.user = pMqttUser->getValue();
    mqttBrokerConfig.password = pMqttPassword->getValue();
  #endif

  #ifdef INFLUX
    influxdbConfig.host = pInfluxBroker->getValue();
    influxdbConfig.port =
      static_cast<uint16_t>(strtoul(pInfluxPort->getValue(), nullptr, 10));
    influxdbConfig.org = pInfluxOrg->getValue();
    influxdbConfig.bucket = pInfluxBucket->getValue();
    influxdbConfig.envMeasurement = pInfluxEnvMeasurement->getValue();
    influxdbConfig.devMeasurement = pInfluxDevMeasurement->getValue();
  #endif

  nvconfigWrite();
}

void networkStartWiFiMgrPortal()
{
  debugMessage(String("networkStartWiFiMgrPortal begin()"), 1);

  if (wfm.getWebPortalActive()) {
    debugMessage("WiFiManager web portal already active", 2);
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    debugMessage("Cannot start WiFiManager Web Portal; WiFi not connected", 1);
    return;
  }

  alertLengthMS = 5000;
  alertStartMS = millis();
  alertScreen = true;

  display.loadFont(Roboto_Regular_24);
  screenHelperAlert(String("goto http://") + WiFi.localIP().toString() + " for device configuration",TFT_WHITE,TFT_BLACK,TFT_BLUE);
  display.unloadFont();

  wfm.setTitle("Climatron Configurator");

  std::vector<const char*> menu = {
    "wifi",
    "param",
    "info",
    "update",
    "restart",
    "exit"
  };

  wfm.setMenu(menu);
  wfm.setSaveParamsCallback(networkWiFiMgrSaveParamsCallback);

  networkWiFiManagerBuildParameters();
  networkWiFiManagerRefreshParameterValues();

  wfm.setConfigPortalBlocking(false);
  saveWFMConfig = false;
  wfm.startWebPortal();
  wfmPortalRunning = true;
  wfmPortalStartMS = millis();

  debugMessage(String("web portal active at ") + WiFi.localIP().toString(), 2);
  debugMessage(String("networkStartWiFiMgrPortal end()"), 1);
}

uint8_t networkRSSIRead()
{
  uint8_t rssi;

  #ifdef HARDWARE_SIMULATE
    rssi = networkRSSISimulate();
  #else
    // attemot to reconnect to WiFi if needed
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }

    if (WiFi.status() == WL_CONNECTED) {
      rssi = abs(WiFi.RSSI());
      debugMessage(String("WiFi RSSI: -") + rssi + "db",2);
    }
    else
      rssi = 255; // no network connection
  #endif
  return (rssi);
}

void networkDisconnect()
// Disconnect from WiFi network
{
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

// Preferences helper routines
bool nvconfigRead() {
  bool success = false;

  debugMessage("nvconfigRead() start",1);
  // open config read-only
  if (nvConfig.begin("config", true)) {
    // check to see if there are pre-existing parameters
    if (nvConfig.isKey("altitude")) {
      // config data exists
      hardwareData.altitude = nvConfig.getUShort("altitude");
      debugMessage(String("Altitude from nvconfig is ") + hardwareData.altitude + " meters",2);
      hardwareData.latitude = nvConfig.getFloat("latitude");
      debugMessage(String("Latitude from nvconfig is ") + hardwareData.latitude,2);
      hardwareData.longitude = nvConfig.getFloat("longitude");
      debugMessage(String("Longitude from nvconfig is ") + hardwareData.longitude,2);
      // generate default unique device identifier based on ESP32 MAC address and hardware device type specified in config.h.
      endpointPath.deviceID = nvConfig.getString("deviceID");
      debugMessage(String("Device ID from nvconfig is ") + endpointPath.deviceID,1);

      endpointPath.site = nvConfig.getString("site");
      debugMessage(String("Device site from nvconfig is ") + endpointPath.site,2);
      endpointPath.location = nvConfig.getString("location");
      debugMessage(String("Device location from nvconfig is ") + endpointPath.location,2);
      endpointPath.room = nvConfig.getString("room", kDefaultRoom);
      debugMessage(String("Device room from nvconfig is ") + endpointPath.room,2);

      mqttBrokerConfig.host = nvConfig.getString("mqttHost");
      debugMessage(String("MQTT broker address from nvconfig is ") + mqttBrokerConfig.host,2);
      mqttBrokerConfig.port = nvConfig.getUShort("mqttPort");
      debugMessage(String("MQTT broker port from nvconfig is ") + mqttBrokerConfig.port,2);
      mqttBrokerConfig.user = nvConfig.getString("mqttUser");
      debugMessage(String("MQTT username from nvconfig is ") + mqttBrokerConfig.user,2);
      mqttBrokerConfig.password = nvConfig.getString("mqttPassword");
      debugMessage(String("MQTT user password from nvconfig is ") + mqttBrokerConfig.password,2);

      influxdbConfig.host = nvConfig.getString("influxHost");
      debugMessage(String("influxdb server address from nvconfig is ") + influxdbConfig.host,2);
      influxdbConfig.port = nvConfig.getUShort("influxPort");
      debugMessage(String("influxdb server port from nvconfig is ") + influxdbConfig.port,2);
      influxdbConfig.org = nvConfig.getString("influxOrg");
      debugMessage(String("influxdb org from nvconfig is ") + influxdbConfig.org,2);
      influxdbConfig.bucket = nvConfig.getString("influxBucket");
      debugMessage(String("influxdb bucket from nvconfig is ") + influxdbConfig.bucket,2);
      influxdbConfig.envMeasurement = nvConfig.getString("influxEnv");
      debugMessage(String("influxdb environment measurement from nvconfig is ") + influxdbConfig.envMeasurement,2);
      influxdbConfig.devMeasurement = nvConfig.getString("influxDev");
      debugMessage(String("influxdb device measurement from nvconfig is ") + influxdbConfig.devMeasurement,2);
      success = true;
    }
    else {
      // there is no existing data, causing function to return false
    }
    nvConfig.end();
  }
  else {
    // non-volatile storage issue, causing function to return false
    // TODO : handle this error condition
    debugMessage(String("Non-volatile storage issue"),1);
  }
  debugMessage("nvconfigRead() end",1);
  return success;
}

void nvconfigDefaultsLoad()
// load ALL default data values into appropriate locations so we can write defaults to nvconfig
{
  debugMessage("nvconfigDefaultsLoad() start",1);

  hardwareData.altitude = uint16_t(kDefaultAltitude.toInt());
  debugMessage(String("Altitude not in nvconfig, using default; ") + hardwareData.altitude + " meters",2);
  hardwareData.latitude = kDefaultLatitude.toFloat();
  debugMessage(String("Latitude not in nvconfig, using default; ") + hardwareData.latitude,2);
  hardwareData.longitude = kDefaultLongitude.toFloat();
  debugMessage(String("Longitude not in nvconfig, using default; ") + hardwareData.longitude,2);
  // generate default unique device identifier based on ESP32 MAC address and hardware device type specified in config.h.
  endpointPath.deviceID = deviceGetID(hardwareDeviceType);
  debugMessage(String("Device ID not in nvconfig, using default; ") + endpointPath.deviceID,1);
  endpointPath.site = kDefaultSite;
  debugMessage(String("Device site not in nvconfig, using default; ") + endpointPath.site,2);
  endpointPath.location = kDefaultLocation;
  debugMessage(String("Device location not in nvconfig, using default; ") + endpointPath.location,2);
  endpointPath.room = kDefaultRoom;
  debugMessage(String("Device room not in nvconfig, using default; ") + endpointPath.room,2);
  mqttBrokerConfig.host = kDefaultMQTTBroker;
  debugMessage(String("MQTT broker address not in nvconfig, using default; ") + mqttBrokerConfig.host,2);
  mqttBrokerConfig.port = uint16_t(kDefaultMQTTPort.toInt());
  debugMessage(String("MQTT broker port not in nvconfig, using default; ") + mqttBrokerConfig.port,2);
  mqttBrokerConfig.user = kDefaultMQTTUser;
  debugMessage(String("MQTT username not in nvconfig, using default; ") + mqttBrokerConfig.user,2);
  mqttBrokerConfig.password = kDefaultMQTTPassword;
  debugMessage(String("MQTT user password not in nvconfig, using default; ") + mqttBrokerConfig.password,2);
  influxdbConfig.host = kDefaultInfluxAddress;
  debugMessage(String("influxdb server address not in nvconfig, using default; ") + influxdbConfig.host,2);
  influxdbConfig.port = uint16_t(kDefaultInfluxPort.toInt());
  debugMessage(String("influxdb server port not in nvconfig, using default; ") + influxdbConfig.port,2);
  influxdbConfig.org = kDefaultInfluxOrg;
  debugMessage(String("influxdb org not in nvconfig, using default; ") + influxdbConfig.org,2);
  influxdbConfig.bucket = kDefaultInfluxBucket;
  debugMessage(String("influxdb bucket not in nvconfig, using default; ") + influxdbConfig.bucket,2);
  influxdbConfig.envMeasurement = kDefaultInfluxEnvMeasurement;
  debugMessage(String("influxdb environment measurement not in nvconfig, using default; ") + influxdbConfig.envMeasurement,2);
  influxdbConfig.devMeasurement = kDefaultInfluxDevMeasurement;
  debugMessage(String("influxdb device measurement not in nvconfig, using default; ") + influxdbConfig.devMeasurement,2);

  debugMessage("nvconfigDefaultsLoad() end",1);  
}

void nvconfigWrite()
// write configuration parameters to non-volatile storage
{
  debugMessage("nvconfigWrite() start",1);
  nvConfig.begin("config", false); // read-write

  // general parameters
  nvConfig.putUShort("altitude", hardwareData.altitude);
  nvConfig.putFloat("latitude",hardwareData.latitude);
  nvConfig.putFloat("longitude", hardwareData.longitude);
  nvConfig.putString("deviceID", endpointPath.deviceID);

  // general endpoint parameters
  nvConfig.putString("site", endpointPath.site);
  nvConfig.putString("location", endpointPath.location);
  nvConfig.putString("room", endpointPath.room);

  // MQTT parameters
  nvConfig.putString("mqttHost",  mqttBrokerConfig.host);
  nvConfig.putUShort("mqttPort",  mqttBrokerConfig.port);
  nvConfig.putString("mqttUser",  mqttBrokerConfig.user);
  nvConfig.putString("mqttPassword",  mqttBrokerConfig.password);

  // Influx parameters
  nvConfig.putString("influxHost",  influxdbConfig.host);
  nvConfig.putUShort("influxPort",  influxdbConfig.port);
  nvConfig.putString("influxOrg",   influxdbConfig.org);
  nvConfig.putString("influxBucket",influxdbConfig.bucket);
  nvConfig.putString("influxEnv",influxdbConfig.envMeasurement);
  nvConfig.putString("influxDev", influxdbConfig.devMeasurement);

  nvConfig.end();
  debugMessage("nvconfigWrite() end",1);
}

  void deviceErasePrefsAndReboot() 
  // Wipes all ESP, WiFiManager preferences and reboots device
  {
    debugMessage("deviceErasePrefsAndReboot() start",1);

    // Clear nv storage
    nvConfig.begin("config", false);
    nvConfig.clear();
    nvConfig.end();

    // disconnect and clear (via true) stored Wi-Fi credentials
    WiFi.disconnect(true);

    // Clear WiFiManager settings (AP config)
    wfm.resetSettings();

    debugMessage("deviceErasePrefsAndReboot() end, rebooting...",2);
    ESP.restart();
  }

void checkButtonPress() {
  static uint32_t pressStartMS = 0;

  const uint8_t buttonState = digitalRead(pinButton);
  uint32_t heldMS;
  const uint32_t now = millis();

  // Hardware button is low when pressed, so check for that
  if (buttonState == LOW) {
    // Pressed. If the first detected press instance start timing now
    if (pressStartMS == 0) {
      pressStartMS = now;
    }
    else {
      // Not the first press, so how long has the button been down?
      heldMS = now - pressStartMS;
      debugMessage(String("checkButtonPress(): button pressed for ") + (heldMS / 1000) + " seconds", 2);
    }
  } 
  else {
    // Button currently not pressed, but has it been? If so figure out whether
    // any reset operation is called for.
    if (pressStartMS != 0) {
      heldMS = now - pressStartMS;
      debugMessage(String("checkButtonPress(): button released after ") + (heldMS / 1000) + " seconds", 2);
      pressStartMS = 0;  // Reset for next time

      // Is a reset needed? If so, launch it.  Check for the longer one first
      if(heldMS >= timeDeviceResetHoldMS) {
        debugMessage("Initiating full device reset...",2);
        deviceErasePrefsAndReboot(); // typically does not return
        return;
      }
      else {
        // Not the longer one, but long enough to be the shorter one?
        if(heldMS >= timeStartPortalHoldMS) {
          debugMessage("checkButtonPress(): starting WiFiManager web portal",2);
          networkStartWiFiMgrPortal();
          return;
        }
      }
    }
  }
}

void OWMForecastSimulate()
// Description : Simulates Open Weather Map (OWM) Current Weather data
// Parameters: NA
// Return : NA
// Improvement : variable city name and days of the week
{
  int i;
  float midpoint;

  midpoint = (sensorTempFMin + sensorTempFMax)/2.0;
  owmSiteForecast.cityName = String("Pleasantville (US)");
  for(i=0;i<5;i++) {
    owmSiteForecast.forecastData[i].maxTempF = randomFloatRange(midpoint,sensorTempFMax);
    owmSiteForecast.forecastData[i].minTempF = randomFloatRange(sensorTempFMin,midpoint);
    owmSiteForecast.forecastData[i].humidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
    owmSiteForecast.forecastData[i].wxFcst = random(1,6);  // Confirm consistent with forecast defines FCST_*
    owmSiteForecast.forecastData[i].count = 40;
    owmSiteForecast.forecastData[i].wday = i;
  }
  debugMessage(String("SIMULATED OWM Forecast for ") + owmSiteForecast.cityName, 1);
}

/**
 * @brief Retreives weather forecast data from Open Weather Maps.
 *
 * Fetch weather forecast information from OWM using the 3-hour API. Extract
 * overall daily forecast info from the 3-hour elements as apppropriate and store
 * those daily aggregates in the global data structure used separately to display
 * a 5-day forecast information in the UI.
 *
 * If device is in hardware simulation mode, calls OWMCurrentWeatherSimulate() and returns.
 *
 * @param 
 *
 * @return BOOL true if the function has successfully retrieved weather forecast data
 *
 * @note 
 *
 * @warning 
 */
boolean OWMForecastRead()
{
  uint16_t httpResponseCode, wxcond, wxrange;
  uint32_t dt, lt;
  int32_t i, count, tzoffset;
  uint8_t wd, today, condflags, forecastday;
  float maxtemp, mintemp, curtemp, humidity;
  String wxcondname;
  JsonDocument doc;
  Measure fcstTemperatureF, fcstHumidity;

  #ifdef HARDWARE_SIMULATE
    OWMForecastSimulate();
    return true;
  #else
    HTTPClient http;
    // Attempt to connect to OWM service for 3-hour forecast data
    static String serverPath = kOWMServer + kOWMForecastPath + 
      "lat=" + hardwareData.latitude + "&lon=" + hardwareData.longitude + "&units=imperial&APPID=" + OWMKey;
    debugMessage("OWM Forecast fetch: " + String(serverPath),1);
    if(!http.begin(serverPath)) {
          debugMessage("OWM weather forecast connection failed",1);
      return false;
    }

    // Successfully connected, see if forecast data was returned
    httpResponseCode = http.GET();
    if(httpResponseCode != HTTP_CODE_OK) {
      debugMessage(String("OWM Forecast HTTP GET error code: ") + httpResponseCode,1);
      http.end();
      return false;
    }

    // Retrieved forecast data, so process it
    debugMessage("OWM HTTP GET success",2);

    // Obtain the HTTP GET result payload and convert to a JSON doc
    DeserializationError error = deserializeJson(doc,http.getStream());
    http.end();   

    /*
    * Print the full payload (to assist in development)
    Serial.println("**** OWM payload returned *****");
    serializeJsonPretty(doc,Serial);
    Serial.println("***** End of Payload *****");
    */
    
    // Extract overall info from OWM forecast payload

    count = doc["cnt"];  // Number of forecasts in the payload list
    tzoffset = doc["city"]["timezone"];  // Used to adjust UTC forecasts to local time

    condflags = 0;  // Clear out wx condition accumulator
    forecastday = 0;  // Start storing forecast data on the zeroth day
    // Clear temperature and humidity accumulation so we start fresh
    fcstTemperatureF.clear();
    fcstHumidity.clear();

    // Process all forecasts returned, which are in the "list" element of the JSON doc
    for(i=0;i<count;i++) {
      dt = doc["list"][i]["dt"];  // UTC of forecast
      lt = dt + tzoffset;         // Convert to local time
      wd = weekday(lt);           // Use Time library to determine day of week

      // Fetch key forecast elements from the payload
      curtemp = doc["list"][i]["main"]["temp"];
      humidity = doc["list"][i]["main"]["humidity"];
      wxcond = doc["list"][i]["weather"][0]["id"];
      wxcondname = String(doc["list"][i]["weather"][0]["main"]);
      owmSiteForecast.cityName = String(doc["city"]["name"]);

      // Aggregate daily data
      if(i == 0) {
        today = wd;  // Set today based on the first forecast element
      }
      // If the forecast info is not for today then we need wrap up today's
      // info, store it for future use, and reset for this new day.
      if(wd != today) {
        // Retain daily forecast elements in the global data structure ***
        owmSiteForecast.forecastData[forecastday].maxTempF = fcstTemperatureF.getMax();
        owmSiteForecast.forecastData[forecastday].minTempF = fcstTemperatureF.getMin();
        owmSiteForecast.forecastData[forecastday].humidity = fcstHumidity.getAverage();
        owmSiteForecast.forecastData[forecastday].wxFcst   = forecastMap[condflags];
        owmSiteForecast.forecastData[forecastday].count    = fcstTemperatureF.getCount();
        owmSiteForecast.forecastData[forecastday].wday     = today-1;

        //Reset things for the new day, including clearing min/max/avg accumulation
        today = wd;
        condflags = 0;
        forecastday++;
        fcstTemperatureF.clear();
        fcstHumidity.clear();
      }
      // Now process this daily forecast element
      fcstTemperatureF.include(curtemp);
      fcstHumidity.include(humidity);
      // Aggregate conditions across wide range of possible values returned by OWM
      // See https://openweathermap.org/api/weather-conditions#Weather-Condition-Codes-2 for details
      wxrange = wxcond / 100; // Identify OWM condition group (hundreds digit)
      if(wxcond == 800) {
        condflags |= WX_CLEAR;
      }
      // Any 2xx, 3xx, or 5xx code translates to Rainy
      if(wxrange == 2 || wxrange == 3 || wxrange == 5) {
        condflags |= WX_RAINY;
      }
      // Recognize various cloud cover conditions, and treat Atmospheric as cloudy
      if(wxcond == 801 || wxcond == 802 || wxcond == 803 || wxcond == 804 || wxrange == 7) {
        condflags |= WX_CLOUDY;
      }
      if(wxrange == 6) {
        condflags |= WX_SNOWY;
      }
    }  
    // Summarize what we have for the last day
    // Retain daily forecast elements in the global data structure ***
    owmSiteForecast.forecastData[forecastday].maxTempF = fcstTemperatureF.getMax();
    owmSiteForecast.forecastData[forecastday].minTempF = fcstTemperatureF.getMin();
    owmSiteForecast.forecastData[forecastday].humidity = fcstHumidity.getAverage();
    owmSiteForecast.forecastData[forecastday].wxFcst   = forecastMap[condflags];
    owmSiteForecast.forecastData[forecastday].count    = fcstTemperatureF.getCount();
    owmSiteForecast.forecastData[forecastday].wday     = today-1;

    return true;
  #endif
  return true;
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

bool OWMAirPollutionRead()
// stores local air pollution info from Open Weather Map in environment global
{
  debugMessage(String("OWMAirPollutionRead() start"), 1);
  #ifdef HARDWARE_SIMULATE
    OWMAirPollutionSimulate();
    return true;
  #else
    static int32_t timeLastOWMUpdateMS = -(timeOWMRenewMS); // forces immediate sample at first run
    
    debugMessage(String("OWMAirPollutionRead() start"),1);
    // is it time for new OWM data?
    if (millis() - timeLastOWMUpdateMS > timeOWMRenewMS)
    {
      // attemot to reconnect to WiFi if needed
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.reconnect();
      }

    // http://api.openweathermap.org/data/2.5/air_pollution?lat={lat}&lon={lon}&appid={API key}
    String serverPath = kOWMServer + kOWMAQMPath +
     "lat=" + hardwareData.latitude + "&lon=" + hardwareData.longitude + "&APPID=" + OWMKey;

      HTTPClient http;
      if (!http.begin(serverPath)) {
        debugMessage("OWM AirPollution URL malformed or HTTP client didn't initialize",1);
        return false;
      }

      int httpResponseCode = http.GET();
      if (httpResponseCode != HTTP_CODE_OK) {
        debugMessage(String("OWM AirPollution HTTP GET error: ") + HTTPClient::errorToString(httpResponseCode),1);
        http.end();
        return false;
      }

      // Filter: only parse what we need (saves RAM)
      JsonDocument filter;
      filter["list"][0]["main"]["aqi"] = true;
      filter["list"][0]["components"]["pm2_5"] = true;

      JsonDocument doc;
      const DeserializationError error = deserializeJson(
        doc,
        http.getStream(),
        DeserializationOption::Filter(filter)
      );

      http.end();

      if (error) {
        debugMessage(String("OWM AirPollution deserializeJson error message: ") + error.c_str(), 1);
        return false;
      }

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

      timeLastOWMUpdateMS = millis();
      debugMessage(String("OWMAirPollutionRead() end"),1);
      return true;
    }
  #endif

  debugMessage(String("OWMAirPollutionRead() end"), 1);
  return true;
}

bool sensorInit()
// Generalized entry point for sensor initialization
{
  // Conditionally compiled based on the sensor configuration as defined in config.h
  bool success = false;

  bool pmSuccess = sensorSEN54Init();
  success = sensorSCD4xInit();
  if (!success) {
    debugMessage("SCD4x init failed",1);
  }
  if (!pmSuccess) {
    debugMessage("PM sensor init failed",1);
    success = false;
  }
  if (success) {
    #ifndef HARDWARE_SIMULATE
      // Explicit delay as SEN54 takes 6-7 seconds for valid VOC index values
      delay(7000);
    #endif
  }

  return success;
}

bool sensorRead()
// Generalized entry point for reading sensor values
{
  bool success = false;  // default setting for the final #ifndef

  bool pmSuccess = sensorSEN554Read();
  if (!pmSuccess)
    debugMessage("SEN54 read failed",1);

  success = sensorSCD4xRead();
  if (!success)
    debugMessage("SCD40 read failed",1);
  if (!pmSuccess)
    success = false;

  return success;
}

bool sensorSEN54Init()
{
  bool success = false;

  debugMessage("sensorSEN54Init() start",1);

  #ifdef HARDWARE_SIMULATE
    success = true;
  #else
    uint16_t error;
    char errorMessage[256];

    // CYD 2432S028R 
    pmSensor.begin(Wire);

    error = pmSensor.deviceReset();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " error during SEN5x reset", 1);
    }
    else {
      // start measurement
      error = pmSensor.startMeasurement();
      if (error) {
        errorToString(error, errorMessage, 256);
        debugMessage(String(errorMessage) + " error during SEN5x startMeasurement", 2);
      }
      else {
        debugMessage("SEN5X starting periodic measurements",2);
        success = true;
      }
    }
  #endif
  debugMessage("sensorSEN54Init() end",1);
  return success;
}

void sensorSEN54Simulate(float& simulatedPM25, float& simulatedVOCIndex)
// Description: Simulates sensor reading from SEN54 sensor
// Parameters: NA
// Return: NA
// Improvement: mode 1 from CO2 for VOC
// Note: tempF and humidity come from SCD4X simulation
{
  //float pm1, pm10, pm4 = 0.0f;

  debugMessage("sensorSEN54Simulate() start",1);

  simulatedPM25 = randomFloatRange(sensorPMMin, sensorPMMax);
  // pm1 = randomFloatRange(sensorPMMin, sensorPMMax);
  // pm10 = randomFloatRange(sensorPMMin, sensorPMMax);
  // pm4 = randomFloatRange(sensorPMMin, sensorPMMax);
  simulatedVOCIndex = randomFloatRange(sensorVOCMin, sensorVOCMax);

  debugMessage(String("returning simulated PM2.5: ") + simulatedPM25 + " ppm, VOC index: " + simulatedVOCIndex,1);
  debugMessage("sensorSEN54Simulate() end",1);
}

bool sensorSEN554Read() 
// Description: Retrieves values from SEN54 sensor
// Parameters: none
// Output : range validated pm25 and VOCIndex values, NAN NOxIndex value from SEN54
// Improvement : Add support for checking isDataReady flag (see SCD40 read) 
{
  bool success = false;
  float pm25 = 0.0f;
  float VOCIndex = 0.0f;
  float NOxIndex = 0.0f;

  debugMessage("sensorSEN554Read() start",1);

  #ifdef HARDWARE_SIMULATE
    sensorSEN54Simulate(pm25, VOCIndex);
    success = true;
  #else
    uint16_t error;
    char errorMessage[256];
    float pm1, pm4, pm10, temperatureC, humidity = 0.0f; // read and discard

    error = pmSensor.readMeasuredValues(pm1, pm25, pm4, pm10, humidity, temperatureC, VOCIndex, NOxIndex);
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " error during SEN5x read",2);
    }
    else
      success = true;
  #endif

  // range valid returned sensor values, even simulation values can be OOB
  if (pm25 < sensorPMMin || pm25 > sensorPMMax) {
    success = false;
    debugMessage(String("SEN5x PM2.5 reading: ") + pm25 + " is out of datasheet range",2);
  }

  if (VOCIndex < sensorVOCMin || VOCIndex > sensorVOCMax) {
    success = false;
    debugMessage(String("SEN5x VOC index reading: ") + VOCIndex + " is out of datasheet range",2);
  }

  // valid measurement, update globals
  if (success) {
    totalPM25.include(pm25);
    totalVOCIndex.include(VOCIndex);

    debugMessage(String("sensorSEN554Read() updating pm25: ") + totalPM25.getCurrent() + "ppm, total: " + totalPM25.getTotal(),2);
    debugMessage(String("sensorSEN554Read() updating vocIndex: ") + totalVOCIndex.getCurrent() + ", total: " + totalVOCIndex.getTotal(),2);
    debugMessage(String("sensorSEN554Read() NOxIndex is NAN"),2);
  }

  debugMessage("sensorSEN554Read() end",1);
  return(success);
}

bool sensorSCD4xInit()
// initializes SCD4X to read
{
  bool success = false;

  debugMessage("sensorSCD4xInit() start",1);

  #ifdef HARDWARE_SIMULATE
    success = true;
  #else
    uint16_t error;
    char errorMessage[256];

    // CYD 2432S028R 
    co2Sensor.begin(Wire, SCD41_I2C_ADDR_62);

    // stop potentially previously started measurement
    error = co2Sensor.stopPeriodicMeasurement();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD4X stopPeriodicMeasurement()",1);
    }
    else {
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
        debugMessage(String(errorMessage) + " executing SCD4X startLowPowerPeriodicMeasurement()",2);
      }
      else
      {
        debugMessage("SCD4X starting low power periodic measurements",2);
        success = true;
      }
    }
  #endif

  debugMessage("sensorSCD4xInit() end",1);
  return success;
}

// Description: Simulates temp, humidity, and CO2 values from Sensirion SCD4X sensor
// Parameters:
//  mode
//    default = random values, ignores cycles parameter
//    1 = random values, slightly +/- per cycle
//    2 = out of bounds, "bad" values designed to activate alert modes
//    3 = rapidly rising values designed to activate sampleEvaluate()
//  cycles = If used, determines how many times the current mode executes before resetting
// Output : NA
// Improvement : rapid CO2 rise mode to test sampleEvaluate()
void sensorSCD4xSimulate(
  uint8_t mode,
  uint8_t cycles,
  float& simulatedTempF,
  float& simulatedHumidity,
  uint16_t& simulatedCO2)
{
  static uint8_t currentMode = 0;
  static uint8_t cycleCount = 0;
  static float tempF, humidity = 0.0f;
  static uint16_t co2 = 0;

  debugMessage("sensorSCD4xSimulate() start",1);

  if (mode != currentMode) {
    cycleCount = 0;
    currentMode = mode;
  }

  // random sign used in some modes
  int8_t sign = random(0, 2) == 0 ? -1 : 1;

  switch (currentMode) {
  case 0: // 0 = random values, ignores cycles value
    tempF = randomFloatRange(sensorTempFMin,sensorTempFMax);
    humidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
    co2 = random(sensorCO2Min, sensorCO2Max);
    break;    
  case 1: // 1 = random values, slightly +/- per cycle
    if (cycleCount == cycles) {
      cycleCount = 0;
    }
    if (!cycleCount) {
      // create new base values
      tempF = randomFloatRange(sensorTempFMin,sensorTempFMax);
      humidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
      co2 = random(sensorCO2Min, sensorCO2Bad); // starts values in highly likely scenarios
      cycleCount++;
    }
    else
    {
      // slightly +/- CO2 value
      co2 += (sign * random(0, sensorCO2VariabilityRange));
      tempF += (sign * random(0, 3));
      humidity += (-sign * random(0,3));
      cycleCount++;
    }
    break;
  case 2: // 2 = out of bounds, "bad" values designed to activate alert modes
    tempF = (random(0,2)) ? sensorTempFMin-2 : sensorTempFMax+2;
    humidity = (random(0,2)) ? sensorHumidityMin-2 : sensorHumidityMax+2;
    co2 = (random(0,2)) ? sensorCO2Min-2 : sensorCO2Max+2;
    break;
  case 3: // rapidly rising values designed to activate sampleEvaluate()
    if (cycleCount == cycles) {
      cycleCount = 0;
    }
    if (!cycleCount) {
      // clear the retained CO2 values so they don't affect std dev calculation
      totalCO2.deleteRetained();
      // create new base values
      tempF = randomFloatRange((sensorTempFMin + (3 * cycles)),(sensorTempFMax - (3 * cycles))); // crude buffer for potential cycle movement
      humidity = randomFloatRange((sensorHumidityMin + (3* cycles)),(sensorHumidityMax - (3 * cycles)));
      co2 = random(sensorCO2Min, sensorCO2Bad); // vs. sensorCO2Max while produces unrealistic values
      cycleCount++;
    }
    else
    {
      // rapidly spike CO2 value
      co2 += random(kMinSigmaFloor * 2, kMinSigmaFloor * 4);
      tempF += (sign * random(0, 3));
      humidity += (-sign * random(0,3));
      cycleCount++;
    }
    break;
  default: // should not occur; random values, ignores cycles value
    tempF = randomFloatRange(sensorTempFMin,sensorTempFMax);
    humidity = randomFloatRange(sensorHumidityMin,sensorHumidityMax);
    co2 = random(sensorCO2Min, sensorCO2Max);
    break;
  }
  simulatedTempF = tempF;
  simulatedHumidity = humidity;
  simulatedCO2 = co2;
  debugMessage(String("returning simulated temp: ") + simulatedTempF + "F, humidity: " + simulatedHumidity
    + "%, CO2: " + simulatedCO2 + "ppm",1);

  debugMessage("sensorSCD4xSimulate() end",1);
}

void sensorSCD4xSimulate(
float& simulatedTempF,
float& simulatedHumidity,
uint16_t& simulatedCO2)
{
sensorSCD4xSimulate(0, 0, simulatedTempF, simulatedHumidity, simulatedCO2);
}

bool sensorSCD4xRead()
// Description: Retrieves values from SCD4x sensor
// Parameters: none
// Output : range validated tempF, humidity, and CO2 values from SCD4x
// Improvement : NA  
{
  bool success = false;
  float temperatureF = 0.0f;
  float humidity = 0.0f;
  uint16_t co2 = 0;

  debugMessage("sensorSCD4xRead() start",1);

  #ifdef HARDWARE_SIMULATE
    success = true;
    sensorSCD4xSimulate(1, 10, temperatureF, humidity, co2);
  #else
    uint16_t error;
    uint8_t errorCount = 0;
    char errorMessage[256];
    float temperatureC = 0.0f;

    // Loop attempting to read Measurement
    while((errorCount < co2SensorReadFailureLimit) && (!success)) {
      delay(100);
      errorCount++;
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

      error = co2Sensor.readMeasurement(co2, temperatureC, humidity);
      if (error) {
          errorToString(error, errorMessage, 256);
          debugMessage(String("SCD40 executing readMeasurement(): ") + errorMessage,1);
      }
      else {
        success = true;
        temperatureF = (temperatureC*1.8)+32;
      }
    }
  #endif

  // validate returned sensor values, even simulation can generate OOB values

  if (co2 < sensorCO2Min || co2 > sensorCO2Max) {
    success = false;
    debugMessage(String("SCD4x CO2 reading: ") + co2 + " is out of datasheet range",2);
  }

  if (temperatureF < sensorTempFMin || temperatureF > sensorTempFMax) {
    success = false;
    debugMessage(String("SCD4x temperatureF reading: ") + temperatureF + " is out of datasheet range",2);
  }

  if (humidity < sensorHumidityMin || humidity > sensorHumidityMax) {
    success = false;
    debugMessage(String("SCD4x humidity reading: ") + humidity + " is out of datasheet range",2);
  }

  // valid measurement, update globals
  if (success) {
    totalTemperatureF.include(temperatureF);
    totalHumidity.include(humidity);
    totalCO2.include(co2);

    debugMessage(String("SCD4x temp ") + totalTemperatureF.getCurrent() + "F, total across samples: " + totalTemperatureF.getTotal(),2);
    debugMessage(String("SCD4x humidity ") + totalHumidity.getCurrent() + ", total across samples: " + totalHumidity.getTotal(),2);
    debugMessage(String("SCD4x CO2 ") + totalCO2.getCurrent() + "ppm, total: " + totalCO2.getTotal(),2);
  }
  debugMessage("sensorSCD4xRead() end",1);
  return(success);
}

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

void deviceReboot(String messageText, uint16_t timeAlertMS)
{
  debugMessage("deviceReboot() start",1);
  display.loadFont(Roboto_Regular_18);
  screenHelperAlert(messageText,TFT_WHITE,TFT_BLACK,TFT_RED);
  display.unloadFont();
  networkDisconnect();

  uint32_t timeRebootStartMS = millis();

  while (millis() - timeRebootStartMS < timeAlertMS)
  {
    #ifndef HARDWARE_SIMULATE
      ledcWriteTone(pinAudio, audioFrequency);
      delay(500);
      ledcWriteTone(pinAudio,0);
      delay(500);
    #endif
  }
  debugMessage("deviceReboot() end",1);
  ESP.restart();
}

/**
 * @brief Truncate a string to fit within a maximum pixel width by appending an ellipsis.
 *
 * Uses the currently active TFT_eSPI font/text settings to measure rendered pixel width.
 * If the string exceeds @p maxWidthPixels, it is truncated and "..." is appended.
 *
 * @param s    Input string to be truncated if necessary.
 * @param maxWidthPixels Maximum allowed rendered pixel width (pixels).
 *
 * @return A string guaranteed to render at <= @p maxWidthPixels pixels (or empty if even "..." won't fit).
 */
static String ellipsizeToWidth(const String &s, uint16_t maxWidthPixels) {
  if ((uint16_t)display.textWidth(s) <= maxWidthPixels) return s;

  String text = s;
  text.trim();

  const String ell = "...";
  const uint16_t ellWidth = (uint16_t)display.textWidth(ell);
  if (ellWidth > maxWidthPixels) return "";

  int16_t lo = 0;
  int16_t hi = (int16_t)text.length();
  int16_t best = 0;

  while (lo <= hi) {
    int16_t mid = (int16_t)((lo + hi) / 2);
    String candidate = text.substring(0, mid);
    candidate.trim();

    uint16_t w = (uint16_t)display.textWidth(candidate);
    if ((uint16_t)(w + ellWidth) <= maxWidthPixels) {
      best = mid;
      lo = (int16_t)(mid + 1);
    } 
    else {
        hi = (int16_t)(mid - 1);
    }
  }

  String result = text.substring(0, best);
  result.trim();

  // Avoid awkward trailing punctuation before "..."
  while (result.length() > 0) {
    char c = result[result.length() - 1];
    if (c == ' ' || c == '.' || c == ',' || c == ':' || c == ';' || c == '-') 
      result.remove(result.length() - 1);
    else 
      break;
  }

  return result + ell;
}

/**
 * @brief Split a message into one or two lines using pixel width and word boundaries.
 *
 * Attempts to split on a single space such that line 1 fits without truncation and
 * any truncation (ellipsis) represents the end of the overall message (i.e., is applied
 * to line 2 only). If no word-boundary split can produce a non-truncated line 1, the
 * function falls back to a single-line ellipsized result.
 *
 * @param s           Input message to split.
 * @param line1       Output: first line of text.
 * @param line2       Output: second line of text (empty if not used).
 * @param maxWidthPixels    Maximum allowed rendered pixel width for each line (pixels).
 *
 * @note This function relies on the current TFT_eSPI font/text settings for measurements.
 */
void textSplitTwoLines(
  const String &s,
  String &line1,
  String &line2,
  uint16_t maxWidthPixels
) {

  if (s.length() == 0 || maxWidthPixels == 0) {
    line1 = "";
    line2 = "";
    return;
  }

  if ((uint16_t)display.textWidth(s) <= maxWidthPixels) {
    line1 = s;
    line2 = "";
    return;
  }

  String text = s;
  text.trim();

  const uint16_t len = (uint16_t)text.length();

  int16_t bestSplit = -1;
  uint16_t bestLine1Width = 0;
  uint32_t bestOverflowScore = 0xFFFFFFFFUL; // initally guaranteed to be > than any real overflow score 

  for (uint16_t loop = 1; loop + 1 < len; loop++) {
    if (text[loop] != ' ') continue;
    if (text[loop - 1] == ' ' || text[loop + 1] == ' ') continue;

    String a = text.substring(0, loop);   a.trim();
    String b = text.substring(loop + 1);  b.trim();

    const uint16_t widthA = (uint16_t)display.textWidth(a);
    if (widthA > maxWidthPixels) continue; // line1 must fit WITHOUT ellipsis

    const uint16_t widthB = (uint16_t)display.textWidth(b);
    const uint16_t overflow2 = (widthB > maxWidthPixels) ? (uint16_t)(widthB - maxWidthPixels) : 0;

    const uint32_t score = (uint32_t)overflow2;

    if (widthA > bestLine1Width || (widthA == bestLine1Width && score < bestOverflowScore)) {
      bestLine1Width = widthA;
      bestOverflowScore = score;
      bestSplit = (int16_t)loop;
    }
  }

  if (bestSplit >= 0) {
    line1 = text.substring(0, (uint16_t)bestSplit);        line1.trim();
    line2 = text.substring((uint16_t)bestSplit + 1);       line2.trim();

    if ((uint16_t)display.textWidth(line2) > maxWidthPixels) {
      line2 = ellipsizeToWidth(line2, maxWidthPixels);
    }
    return;
  }

  line1 = ellipsizeToWidth(text, maxWidthPixels);
  line2 = "";
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
  debugMessage(String("PM2.5 value of ") + pm25 + " converts to US AQI value " + aqiValue, 2);

  return aqiValue;
}

float fmap(float x, float xmin, float xmax, float ymin, float ymax)
{
  return( ymin + ((x - xmin)*(ymax-ymin)/(xmax - xmin)));
}

float randomFloatRange(uint16_t min, uint16_t max) {
  uint16_t randomFixed = random((max-min) * 100 + 1);
  // return float with 2 decimal precision
  return min + (randomFixed / 100.0f);
}

void alertHandle() {
  // is there an alert to handle?
  if (alertLengthMS) {
    // has the alert ended
    if (millis() - alertStartMS > alertLengthMS) {
      // clear the alert and reset alert variables
      if ((alertScreen) || (alertLED)) {
        debugMessage("alertHandle() : alert being cleared",2);
        // return screen to previous state
        screenUpdate(screenCurrent);
        alertScreen = false;
        // this only works because screenUpdate also changes LED status
        alertLED = false;
      }
      if (alertSound) {
        ledcWriteTone(pinAudio, 0);
        alertSound = false;
      }
      alertLengthMS = 0;
      alertStartMS = 0;
    }
  }
}

// Determine the right warning color to use for an arbitrary sensor data value given
// the type of data in question.  This utility is used heavily in various screen drawing
// routines (see screens.cpp) but also for managing the notification LEDs in Climatron.
uint16_t getWarningColor(uint8_t datatype, float datavalue)
{
  switch(datatype) {
    case CO2_DATA:
      return(warningColor[co2Range(datavalue)]);
    case VOC_DATA:
      return(warningColor[vocRange(datavalue)]);
    case PM_DATA:
      return(warningColor[pm25Range(datavalue)]);
    case TEMP_DATA:
      // Alternatively could explicitly return TFT_GREEN & TFT_YELLOW for temperature 
      // & humidity comfort zones but using warningColor[0] and warningColor[1] provides 
      // configurable consistency with other warning/comfort coloration
      if( (datavalue < sensorTempFComfortMin) || (datavalue > sensorTempFComfortMax) ) return(warningColor[1]); // "Fair"
      else return(warningColor[0]);  // "Good"
    case HUM_DATA:
      if( (datavalue < sensorHumidityComfortMin) || (datavalue > sensorHumidityComfortMax) ) return(warningColor[1]); // "Fair"
      else return(warningColor[0]); // "Good"
    default:
      return(TFT_WHITE);
  }
}

// Determine the right text color to use in writing on a region colored according to
// warning level of the type of data in question.  
uint16_t getWarningTextColor(uint8_t datatype, float datavalue)
{
  uint16_t windex;

  switch(datatype) {
    case CO2_DATA:
      windex = co2Range(datavalue);
      break;
    case VOC_DATA:
      windex = vocRange(datavalue);
      break;
    case PM_DATA:
      windex = pm25Range(datavalue);
      break;
    case TEMP_DATA:
      if( (datavalue < sensorTempFComfortMin) || (datavalue > sensorTempFComfortMax) ) windex = 1; // "Fair"
      else windex = 0;  // "Good"
      break;
    case HUM_DATA:
      if( (datavalue < sensorHumidityComfortMin) || (datavalue > sensorHumidityComfortMax) ) windex = 1; // "Fair"
      else windex = 0; // "Good"
      break;
    default:
      // Don't know what to do, so just return white
      return(TFT_WHITE);
  }
  // For warning levels 0 (Green) and 1 (Yellow) use black text
  // For warning levels 2 (Orange) and 3 (Red) use white text
  if( (windex == 0) || (windex == 1)) {
    return(TFT_BLACK);
  }
  else return(TFT_WHITE);
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