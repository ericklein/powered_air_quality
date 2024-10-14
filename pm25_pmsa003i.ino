/*
  Project:        Powered Air Quality
  Description:    Sample and log indoor air quality via AC powered device

  See README.md for target information
*/

// hardware and internet configuration parameters
#include "config.h"
// private credentials for network, MQTT
#include "secrets.h"

// sensor support
#ifndef SIMULATE_SENSOR
  // initialize pm25 sensor
  #include "Adafruit_PM25AQI.h"
  Adafruit_PM25AQI aqi = Adafruit_PM25AQI();

  // initialize scd40 environment sensor
  #include <SensirionI2CScd4x.h>
  SensirionI2CScd4x envSensor;
#endif

// WiFi support if needed
#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT)
  #if defined(ESP8266)
    #include <ESP8266WiFi.h>
  #elif defined(ESP32)
    #include <WiFi.h>
  #else
    #include <WiFiNINA.h> // PyPortal
  #endif
#endif

// button support
// #include <ezButton.h>
// ezButton buttonOne(buttonD1Pin);

// external function dependencies
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

// screen support
// 3.2″ ILI9341 320 x 240 color TFT with resistive touch screen
#include "Adafruit_ILI9341.h"
Adafruit_ILI9341 display = Adafruit_ILI9341(tft8bitbus, TFT_D0, TFT_WR, TFT_DC, TFT_CS, TFT_RST, TFT_RD);

#include <Fonts/FreeSans9pt7b.h>
// #include <Fonts/FreeSans12pt7b.h>
// #include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

// Special glyphs for the UI
#include "glyphs.h"

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
  uint8_t rssi; // WiFi RSSI value
} hdweData;
hdweData hardwareData;

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
    debugMessage("PM2.5 monitor started",1);
    debugMessage(String("Sample interval is ") + sensorSampleInterval + " seconds",1);
    debugMessage(String("Report interval is ") + sensorReportInterval + " minutes",1);
    debugMessage(String("Internet service reconnect delay is ") + networkConnectAttemptInterval + " seconds",1);
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

  hardwareData.rssi = 0;            // 0 = no WiFi

  // Initialize PM25 sensor
  if (!sensorPM25Init()) {
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
    if (!sensorPM25Read())
    {
      debugMessage("Could not read PMSA003I sensor data",1);
      // TODO: what else to do here
    }
    sensorCO2Read();
    screenPM();
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

void screenPM() 
// 
{
  const int yTemperature = 23;
  const int yLegend = 95;
  const int legendHeight = 10;
  const int legendWidth = 5; 

  debugMessage("Starting screenPM refresh", 1);

  // clear screen
  display.fillScreen(ILI9341_BLACK);

  // screen helper routines
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), (yMargins + (5*wifiBarHeightIncrement)), wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);

  // temperature and humidity
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(ILI9341_WHITE);
  display.setCursor(xMargins, yTemperature);
  display.print(sensorData.ambientTemperatureF,1);
  display.print("F ");
  if ((sensorData.ambientHumidity<40) || (sensorData.ambientHumidity>60))
    display.setTextColor(ILI9341_RED);
  else
    display.setTextColor(ILI9341_GREEN);
  display.print(sensorData.ambientHumidity,1);
  display.print("%");

  // pm25 level circle
  switch (int(sensorData.massConcentrationPm2p5/50))
  {
    case 0: // good
      display.fillCircle(46,75,31,ILI9341_BLUE);
      break;
    case 1: // moderate
      display.fillCircle(46,75,31,ILI9341_GREEN);
      break;
    case 2: // unhealthy for sensitive groups
      display.fillCircle(46,75,31,ILI9341_YELLOW);
      break;
    case 3: // unhealthy
      display.fillCircle(46,75,31,ILI9341_ORANGE);
      break;
    case 4: // very unhealthy
      display.fillCircle(46,75,31,ILI9341_RED);
      break;
    case 5: // very unhealthy
      display.fillCircle(46,75,31,ILI9341_RED);
      break;
    default: // >=6 is hazardous
      display.fillCircle(46,75,31,ILI9341_MAGENTA);
      break;
  }

  // pm25 legend
  display.fillRect(xMargins,yLegend,legendWidth,legendHeight,ILI9341_BLUE);
  display.fillRect(xMargins,yLegend-legendHeight,legendWidth,legendHeight,ILI9341_GREEN);
  display.fillRect(xMargins,(yLegend-(2*legendHeight)),legendWidth,legendHeight,ILI9341_YELLOW);
  display.fillRect(xMargins,(yLegend-(3*legendHeight)),legendWidth,legendHeight,ILI9341_ORANGE);
  display.fillRect(xMargins,(yLegend-(4*legendHeight)),legendWidth,legendHeight,ILI9341_RED);
  display.fillRect(xMargins,(yLegend-(5*legendHeight)),legendWidth,legendHeight,ILI9341_MAGENTA);


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
  display.setCursor(33,110);
  display.print("PM2.5");
  // display.setCursor(106,110);
  // display.print("VoC"); 

  // pm25 level
  display.setFont(&FreeSans9pt7b);
  display.setCursor(40,80);
  display.print(int(sensorData.massConcentrationPm2p5));

  // VoC level
  // display.setCursor(100,80);
  // display.print(int(sensorData.vocIndex));
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
        display.fillRect((initialX + (b * barSpacing)), (initialY - (b * barHeightIncrement)), barWidth, b * barHeightIncrement, ILI9341_WHITE);
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

    for (int tries = 1; tries <= networkConnectAttemptLimit; tries++)
    // Attempts WiFi connection, and if unsuccessful, re-attempts after networkConnectAttemptInterval second delay for networkConnectAttemptLimit times
    {
      if (WiFi.status() == WL_CONNECTED) {
        hardwareData.rssi = abs(WiFi.RSSI());
        debugMessage(String("WiFi IP address lease from ") + WIFI_SSID + " is " + WiFi.localIP().toString(), 1);
        debugMessage(String("WiFi RSSI is: ") + hardwareData.rssi + " dBm", 1);
        return true;
      }
      debugMessage(String("Connection attempt ") + tries + " of " + networkConnectAttemptLimit + " to " + WIFI_SSID + " failed", 1);
      // use of delay() OK as this is initialization code
      delay(networkConnectAttemptInterval * 1000);  // converted into milliseconds
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
  // sensorData.vocIndex = random(0, 500) / 10.0;
  // sensorData.noxIndex = random(0, 2500) / 10.0;  
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
  debugMessage(String("PM2.5 reading is ") + sensorData.massConcentrationPm2p5 + " or AQI " + pm25toAQI(sensorData.massConcentrationPm2p5),1);
  // debugMessage(String("Particles > 0.3um / 0.1L air:") + sensorData.particles_03um,2);
  // debugMessage(String("Particles > 0.5um / 0.1L air:") + sensorData.particles_05um,2);
  // debugMessage(String("Particles > 1.0um / 0.1L air:") + sensorData.particles_10um,2);
  // debugMessage(String("Particles > 2.5um / 0.1L air:") + sensorData.particles_25um,2);
  // debugMessage(String("Particles > 5.0um / 0.1L air:") + sensorData.particles_50um,2);
  // debugMessage(String("Particles > 10 um / 0.1L air:") + sensorData.particles_100um,2);
  return true;
}

bool sensorCO2Init()
// initializes CO2 sensor to read
{
  #ifdef SENSOR_SIMULATE
    return true;
 #else
    char errorMessage[256];
    uint16_t error;

    Wire.begin();
    envSensor.begin(Wire);

    // stop potentially previously started measurement.
    error = envSensor.stopPeriodicMeasurement();
    if (error) {
      errorToString(error, errorMessage, 256);
      debugMessage(String(errorMessage) + " executing SCD40 stopPeriodicMeasurement()",1);
      return false;
    }

    // Check onboard configuration settings while not in active measurement mode
    float offset;
    error = envSensor.getTemperatureOffset(offset);
    if (error == 0){
        error = envSensor.setTemperatureOffset(sensorTempCOffset);
        if (error == 0)
          debugMessage(String("Initial SCD40 temperature offset ") + offset + " ,set to " + sensorTempCOffset,2);
    }

    uint16_t sensor_altitude;
    error = envSensor.getSensorAltitude(sensor_altitude);
    if (error == 0){
      error = envSensor.setSensorAltitude(SITE_ALTITUDE);  // optimizes CO2 reading
      if (error == 0)
        debugMessage(String("Initial SCD40 altitude ") + sensor_altitude + " meters, set to " + SITE_ALTITUDE,2);
    }

    // Start Measurement.  For high power mode, with a fixed update interval of 5 seconds
    // (the typical usage mode), use startPeriodicMeasurement().  For low power mode, with
    // a longer fixed sample interval of 30 seconds, use startLowPowerPeriodicMeasurement()
    // uint16_t error = envSensor.startPeriodicMeasurement();
    error = envSensor.startLowPowerPeriodicMeasurement();
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
  #ifdef SENSOR_SIMULATE
    sensorCO2Simulate();
    debugMessage(String("SIMULATED SCD40: ") + sensorData.ambientTemperatureF + "F, " + sensorData.ambientHumidity + "%, " + sensorData.ambientCO2 + " ppm",1);
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
      error = envSensor.getDataReadyFlag(isDataReady);
      if (error) {
          errorToString(error, errorMessage, 256);
          debugMessage(String("Error trying to execute getDataReadyFlag(): ") + errorMessage,1);
          continue; // Back to the top of the loop
      }
      if (!isDataReady) {
          continue; // Back to the top of the loop
      }
      debugMessage("CO2 sensor data available",2);

      error = envSensor.readMeasurement(co2, temperature, humidity);
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