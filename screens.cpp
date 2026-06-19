/*
  Project:      Powered Air Quality
  Description:  screen related routines
*/

#include <Arduino.h>
#include <Measure.hpp>
#include "config.h"
#include "powered_air_quality.h"
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI

// fonts and glyphs
#include "ui/meteocons24pt7b.h"
#include "ui/glyphs.h"

// https://fonts.google.com/specimen/Roboto
#include "ui/fonts/Roboto_Regular_12.h"
#include "ui/fonts/Roboto_Regular_18.h"
#include "ui/fonts/Roboto_Regular_24.h"
#include "ui/fonts/Roboto_Regular_36.h"

#include "ui/fonts/Roboto_Bold_36.h"
#include "ui/fonts/Roboto_Bold_60.h"

// Shared helper function(s) and globals
extern uint8_t networkRSSIRead();
extern bool OWMAirPollutionRead();
extern bool OWMCurrentWeatherRead();
extern void debugMessage(String messageText, uint8_t messageLevel);
extern uint16_t getWarningColor(uint8_t, float);
extern TFT_eSPI display;
extern uint32_t timeLastReportMS;
extern Measure<kSampleCapacity> totalTemperatureF, totalHumidity, totalCO2, totalVOCIndex, totalPM25, totalNOxIndex;

// Forward declarations for local functions to help make ordering in this file easier
void screenHelperGraph(uint16_t, uint16_t, uint16_t, uint16_t, Measure<kSampleCapacity>, uint8_t, String);
void screenHelperHeaderBar(Measure<kSampleCapacity> measure, uint8_t datatype, String header);
String getWarningLabel(uint8_t, float);
void screenHelperWiFiStatus(uint16_t, uint16_t, uint16_t);
void screenHelperPostStatus(uint16_t, uint16_t, uint16_t, uint16_t);
uint8_t co2Range(float); 
uint8_t pm25Range(float);
uint8_t vocRange(float);
uint8_t noxRange(float);
char OWMtoMeteoconIcon(const char*);
void arcMeter(uint16_t, uint16_t, uint16_t, uint16_t);
void arcGauge(uint16_t, uint16_t, uint16_t, uint16_t);
uint16_t arcGaugeHeight(uint16_t);
uint16_t arcGaugeWidth(uint16_t);
void fillSmoothRoundRectWithBorder(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t fillColor, uint16_t borderColor, int32_t borderWidth = 2);

// ***** Screen display routines, typically one per major screen ***** //
void screenSaver()
{
  // screen assist in pixels
  constexpr uint8_t cornerRoundRadius = 4;

  debugMessage("screenSaver() start",1);

  display.fillScreen(TFT_BLACK);
  display.setTextDatum(TL_DATUM);

  // If no data available, display "Not available"
  if (totalCO2.getStored() == 0) {
    display.loadFont(Roboto_Regular_24);
    // display.setFreeFont(&FreeSans18pt7b);
    display.setTextColor(TFT_RED, TFT_BLACK, true);
    uint16_t textWidth = display.textWidth("Not available");
    display.drawString("Not available", random(kXMargins,display.width()-kXMargins-textWidth), random(kYMargins, display.height() - kYMargins - display.fontHeight()));
  }
  else {
    // Otherwise display the latest CO2 reading
    display.loadFont(Roboto_Bold_60);
    // display.setFreeFont(&FreeSans24pt7b);
    display.setTextColor(getWarningColor(CO2_DATA,totalCO2.getCurrent()), TFT_BLACK, true);
    uint16_t textWidth = display.textWidth(String(totalCO2.getCurrent()));
    // Display CO2 value in random, valid location
    display.drawString(String(uint16_t(totalCO2.getCurrent())), random(kXMargins,display.width()-kXMargins-textWidth), random(kYMargins, display.height() - kYMargins - display.fontHeight()));
  }
  display.unloadFont();
  debugMessage("screenSaver() end",1);
}

void screenMain2()
{
  // screen assists
  constexpr uint8_t halfBorderWidth = 2;
  constexpr uint8_t cornerRoundRadius = 4;

  debugMessage("screenMain() start",1);

  display.loadFont(Roboto_Regular_24);
  //display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(TFT_BLACK);
  display.setTextDatum(MC_DATUM);

display.fillScreen(TFT_BLACK);
  // temp/humdity
  display.drawSmoothRoundRect(10,11,4,2,90,95,TFT_WHITE,TFT_BLACK);
  // CO2
  display.drawSmoothRoundRect(110,11,4,2,200,95,TFT_WHITE,TFT_BLACK);
  // VOC
  fillSmoothRoundRectWithBorder(10,117,91,112,cornerRoundRadius,getWarningColor(VOC_DATA,totalVOCIndex.getCurrent()),TFT_WHITE);
  display.setTextColor(TFT_BLACK,getWarningColor(VOC_DATA,totalVOCIndex.getCurrent()), true);
  display.drawString("VOC",55,199);
  // PM2.5
  fillSmoothRoundRectWithBorder(111,117,97,112,cornerRoundRadius,getWarningColor(PM_DATA,totalPM25.getCurrent()),TFT_WHITE);
  display.setTextColor(TFT_BLACK,getWarningColor(PM_DATA,totalPM25.getCurrent()), true);
  display.drawString("PM25",155,199);
  #ifdef CLIMATRON
    // NOX
    fillSmoothRoundRectWithBorder(218,117,96,112,cornerRoundRadius,getWarningColor(NOX_DATA,totalNOxIndex.getCurrent()),TFT_WHITE);
    display.setTextColor(TFT_BLACK,getWarningColor(NOX_DATA,totalNOxIndex.getCurrent()), true);
    display.drawString("NOx",255,199);
  #endif

  debugMessage("screenMain() end",1);
}

void screenTempHumidity() 
// Description: Displays indoor and outdoor temperature and humidity
// Parameters:
// Output: NA (void)
// Improvement: 
{
  debugMessage("screenTempHumidity() start",1);

  screenHelperHeaderBar(totalTemperatureF, UNK_DATA, "Temp/Humidity");

  display.loadFont(Roboto_Bold_36);
  //display.setFreeFont(&FreeSans24pt7b);
  display.setTextDatum(MC_DATUM);

  // Indoor
  // Indoor temp
  display.setTextColor(getWarningColor(TEMP_DATA,totalTemperatureF.getCurrent()),TFT_BLACK, true);
  display.drawString(String((uint8_t)(totalTemperatureF.getCurrent() + .5)), (display.width()/4), (display.height()*3/8));
  display.drawBitmap((display.width()/4 + 30), ((display.height()*3/8) - 14), bitmapTempFSmall, 20, 28, TFT_WHITE);

  // Indoor humidity
  display.setTextColor(getWarningColor(HUM_DATA,totalHumidity.getCurrent()), TFT_BLACK, true);
  display.drawString(String((uint8_t)(totalHumidity.getCurrent() + 0.5)), (display.width()/4), (display.height()*5/8));
  display.drawBitmap((display.width()/4 + 30), ((display.height()*5/8) - 14), bitmapHumidityIconSmall, 20, 28, TFT_WHITE);

  // Outside
  // do we have OWM Current data to display?
  if ((OWMCurrentWeatherRead()) && (owmCurrentData.tempF != 255)) {
    // Outside temp
    display.setTextColor(getWarningColor(TEMP_DATA,owmCurrentData.tempF), TFT_BLACK, true);
    display.drawString(String((uint8_t)(owmCurrentData.tempF + 0.5)), (display.width()*3/4), (display.height()*3/8));
    display.drawBitmap(((display.width()*3/4) + 30), ((display.height()*3/8) - 14), bitmapTempFSmall, 20, 28, TFT_WHITE);

    // Outside humidity
    display.setTextColor(getWarningColor(HUM_DATA,owmCurrentData.humidity), TFT_BLACK, true);
    display.drawString(String((uint8_t)(owmCurrentData.humidity + 0.5)), (display.width()*3/4), (display.height()*5/8));
    display.drawBitmap(((display.width()*3/4) + 30), ((display.height()*5/8) - 14), bitmapHumidityIconSmall, 20, 28, TFT_WHITE);
  }

  display.unloadFont();

  //weather icon
  char weatherIcon = OWMtoMeteoconIcon(owmCurrentData.icon);
  // if getMeteoIcon doesn't have a matching symbol, skip display
  if (weatherIcon != '?') {
    // display icon
    display.setFreeFont(&meteocons24pt7b);
    display.setTextColor(TFT_WHITE);
    display.drawString(String(weatherIcon), ((display.width()*3/4)-12), (display.height()*7/8));
  }
  debugMessage("screenTempHumidity() end", 1);
}

void screenPM25() 
{
  // screen layout assists in pixels
  const uint16_t  xOutdoorMargin = ((display.width() / 2) + kXMargins);
  const uint16_t  xIndoorPMCircle = (display.width() / 4);
  const uint16_t  xOutdoorPMCircle = (display.width()*3/4);
  constexpr uint16_t  yPMCircles = 150;
  constexpr uint16_t  circleRadius = 65;
  constexpr uint16_t circleInnerRadius = circleRadius * 8 / 10;

  debugMessage("screenPM25() start",1);

  screenHelperHeaderBar(totalPM25, PM_DATA, "PM 2.5");

  display.setTextDatum(MC_DATUM);

  // Indoor PM2.5 ring
  display.drawSmoothArc(xIndoorPMCircle, yPMCircles, circleRadius, circleInnerRadius, 0, 360, getWarningColor(PM_DATA, totalPM25.getCurrent()), TFT_BLACK);

  // Indoor pm25 value and label inside the circle
  // display.setFreeFont(&FreeSans18pt7b);
  display.loadFont(Roboto_Bold_36);
  display.setTextColor(getWarningColor(PM_DATA,totalPM25.getCurrent()), TFT_BLACK, true);  // Use highlight color look-up
  display.drawFloat(totalPM25.getCurrent(), 1, xIndoorPMCircle, yPMCircles);
  // label
  // display.setTextColor(TFT_WHITE);
  // display.setFreeFont(&FreeSans9pt7b);
  // display.drawString("PM25", xIndoorCircleText,yPMCircles+23);
  
  // Outside
  // do we have OWM Air Quality data to display?
  if ((OWMAirPollutionRead()) && (owmAirQuality.aqi != 255)) {
    // Outside PM2.5 ring
    display.drawSmoothArc(xOutdoorPMCircle, yPMCircles, circleRadius, circleInnerRadius, 0, 360, getWarningColor(PM_DATA,owmAirQuality.pm25), TFT_BLACK);

    // outdoor pm25 value and label inside the circle
    //display.setFreeFont(&FreeSans18pt7b);
    display.setTextColor(getWarningColor(PM_DATA,owmAirQuality.pm25), TFT_BLACK, true); // Use highlight color look-up 
    display.drawFloat(owmAirQuality.pm25, 1, xOutdoorPMCircle, yPMCircles);
    //label
    // display.setTextColor(TFT_WHITE);
    // display.setFreeFont(&FreeSans9pt7b);
    // display.drawString("PM25", xOutdoorCircleText,yPMCircles + 23);
  }
  else
  {
    // handle this case with an error message
  }
  display.unloadFont();
  debugMessage("screenPM25() end", 1);
}

void screenVOC()
{
  // screen layout assists in pixels
  const uint16_t xCircle = (display.width()/2);
  const uint16_t yCircle = (display.height()*4/5);
  const uint16_t xValue = xCircle;
  const uint16_t yValue = yCircle - 50;

  debugMessage("screenVOC() start",1);

  screenHelperHeaderBar(totalVOCIndex, VOC_DATA, "VOC Level");

  display.setTextDatum(MC_DATUM);

  // If VOCIndex has no values, alert the user
  if (totalVOCIndex.getStored() == 0) {
    display.setFreeFont(&FreeSans18pt7b);
    display.setTextColor(TFT_RED, TFT_BLACK, true);
    display.drawString("No data", (display.width() / 2), (display.height() / 2));
  }
  else {
    // Draw segmented arc showing color range and current VOCIndex in that range
    arcMeter(xCircle,yCircle,display.width(),vocRange(totalVOCIndex.getCurrent()));

    // Display VOCIndex value and label inside the arc
    display.loadFont(Roboto_Bold_60);
    //display.setFreeFont(&FreeSans24pt7b);
    display.setTextColor(getWarningColor(VOC_DATA,totalVOCIndex.getCurrent()), TFT_BLACK, true);  // Use highlight color look-up 
    display.drawFloat((totalVOCIndex.getCurrent() +.5), 0, xValue, yValue);
    display.loadFont(Roboto_Regular_24);
    //display.setFreeFont(&FreeSans18pt7b);
    display.setTextColor(TFT_WHITE, TFT_BLACK, true);
    display.drawString(getWarningLabel(VOC_DATA,totalVOCIndex.getCurrent()), xValue, yCircle);
  }

  display.unloadFont();
  debugMessage("screenVOC() end",1);
}

void screenCO2()
{
  // screen layout assist(s) in pixels
  const uint16_t yValue = (display.height()*2/5);

  debugMessage("screenCO2() start",1);

  screenHelperHeaderBar(totalCO2,CO2_DATA,"Recent CO2 Values");

  display.loadFont(Roboto_Regular_36);
  //display.setFreeFont(&FreeSans24pt7b);

  // if CO2 values are not yet available, display "NA"
  if (totalCO2.getStored() == 0) {
    display.setTextColor(TFT_RED, TFT_BLACK, true);
    display.setTextDatum(MC_DATUM);
    display.drawString("NA", (display.width() / 2), (display.height() / 2));
  }
  else {
    // display generalized CO₂ level
    display.setTextDatum(BL_DATUM);
    display.setTextColor(getWarningColor(CO2_DATA,totalCO2.getCurrent()), TFT_BLACK, true);
    display.drawString(getWarningLabel(CO2_DATA,totalCO2.getCurrent()),kXMargins, yValue - 3);

    // display current CO₂ value
    display.setFreeFont(&FreeSans18pt7b);
    display.setTextDatum(BR_DATUM);
    display.setTextColor(TFT_WHITE, TFT_BLACK, true);
    display.drawString((String(uint16_t(totalCO2.getCurrent())) + "ppm"), (display.width()-(2*kXMargins)), yValue - 3);

    // recent CO₂ graph
    screenHelperGraph(kXMargins, yValue, (display.width()-(2*kXMargins)),((display.height()-yValue)-kYMargins), totalCO2, CO2_DATA, "");
  }
  display.unloadFont();
  debugMessage("screenCO2() end",1);
}

void screenNOX()
{
  // screen layout assists in pixels
  const uint16_t xCircle = (display.width()/2);
  const uint16_t yCircle = (display.height()*4/5);
  const uint16_t xValue = xCircle;
  const uint16_t yValue = yCircle - 50;

  debugMessage("screenNOX() start",1);

  screenHelperHeaderBar(totalNOxIndex, NOX_DATA, "NOx Level");

  // handle sensors without NOx, e.g. SEN54
  if(isnan(totalNOxIndex.getCurrent())) {
    display.loadFont(Roboto_Bold_36);
    // display.setFreeFont(&FreeSans24pt7b);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_RED, TFT_BLACK, true);
    display.drawString("Not Available", xCircle, (display.height()/2));
  }
  else {
     // Draw segmented arc showing color ranges and current NOxIndex in one of those ranges
    arcMeter(xCircle,yCircle,display.width(),noxRange(totalNOxIndex.getCurrent()) );

    // NOx value and label inside the arc
    display.loadFont(Roboto_Bold_60);
    // display.setFreeFont(&FreeSans24pt7b);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(getWarningColor(NOX_DATA,totalNOxIndex.getCurrent()), TFT_BLACK, true);  // Use highlight color look-up 
    display.drawFloat((totalNOxIndex.getCurrent() +.5), 0, xValue, yValue);
    display.loadFont(Roboto_Regular_24);
    // display.setFreeFont(&FreeSans18pt7b);
    display.setTextColor(TFT_WHITE, TFT_BLACK, true);
    display.drawString(getWarningLabel(NOX_DATA,totalNOxIndex.getCurrent()), xValue, yCircle);
  }
  display.unloadFont();
  debugMessage("screenNOX() end",1);
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

//   debugMessage("screenAggregateData() start",1);

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

//   debugMessage("screenAggregateData() end",1);
// }

void screenHelperHeaderBar(Measure<kSampleCapacity> measure, uint8_t datatype, String header)
{
  // screen layout assists in pixels
  const uint8_t yStatusRegionFloor = kYStatusRegion - 7;
  const uint8_t yLabels = display.height() / 4; 
  constexpr uint8_t kHelperXSpacing = 3;
  constexpr uint8_t wifiBarHeightIncrement = 3;
  constexpr uint8_t wifiBarWidth = 3;
  constexpr uint8_t wifiBarSpacing = 5;
  constexpr uint8_t kIconHeight = 20;
  constexpr uint8_t kIconWidth = 20;

  uint16_t fgColor, bgColor;

  debugMessage("screenHelperHeaderBar() start",1);

  display.loadFont(Roboto_Regular_24);

  display.fillScreen(TFT_BLACK);
  if ((datatype == PM_DATA) || (datatype == UNK_DATA)) {
    // when displaying multiple data sources the header bare is a neutral color
    bgColor = TFT_DARKGREY;
    display.fillRect(0,0,display.width(),kYStatusRegion, bgColor);

    // vertical separator for indoor/outdoor
    display.drawFastVLine((display.width() / 2), kYStatusRegion, display.height(), bgColor);

    // indoor/outdoor labels
    // display.setFreeFont(&FreeSans12pt7b);
    display.setTextColor(TFT_WHITE, TFT_BLACK, true);
    display.setTextDatum(MC_DATUM);
    display.drawString("Indoor", display.width()/4, yLabels);
    display.drawString("Outside", (display.width()*3/4), yLabels);

    // set the color for the header bar label
    display.setTextColor(TFT_BLACK, bgColor, true);
  }
  else {
    // set header bar color and background text color to the most recent sample warning color
    bgColor = getWarningColor(datatype,measure.getMember(measure.getCurrent()));
    display.fillRect(0,0,display.width(),kYStatusRegion, bgColor);
    display.setTextColor(TFT_BLACK, bgColor, true);

  }
  // screen helpers in status region
  // screenHelperWiFiStatus((display.width() - kXMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), yStatusRegionFloor, wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperWiFiStatus((display.width() - kXMargins - kIconWidth), yStatusRegionFloor, bgColor);
  
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
    if ((timeLastReportMS == 0) || ((millis() - timeLastReportMS) >= (timeReportMS * reportFailureThreshold))) {
      // we haven't successfully written to a network endpoint at all or before the reportFailureThreshold
      // display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_BLACK);
      fgColor = TFT_RED;
      debugMessage(String("Post status in header bar is false"),2);
    }
    else {
      fgColor = TFT_BLACK;
      //display.drawiBtmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_BLACK);
      debugMessage(String("Post status in header bar is true"),2);
    }
    //screenHelperPostStatus(((display.width() - kXMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing)))-(kHelperXSpacing + kIconWidth)), (yStatusRegionFloor-kIconHeight), fgColor, bgColor);
    screenHelperPostStatus((display.width() - kXMargins - (2 * kIconWidth) - kHelperXSpacing), (yStatusRegionFloor-kIconHeight), fgColor, bgColor);
  #endif

  // header bar label
  // display.setFreeFont(&FreeSans12pt7b);
  display.setTextDatum(L_BASELINE);
  display.drawString(header, ((display.width()/2)-(display.textWidth(header)/2)), yStatusRegionFloor);

  display.unloadFont();
  debugMessage("screenHelperHeaderBar() end",1);
}

void screenHelperWiFiStatus(uint16_t x, uint16_t y, uint16_t bgColor)
// void screenHelperWiFiStatus(uint16_t initialX, uint16_t initialY, uint8_t barWidth, uint8_t barHeightIncrement, uint8_t barSpacing)
// Description: helper function for screenXXX() routines drawing WiFi RSSI strength
// Parameters: 
// Output : NA
// Improvement : error handling for initialX, initialY, and overall width and height
//  dedicated icon type for no WiFi?
{
  debugMessage("screenHelperWiFiStatus() start",1);

  uint16_t circleColor, arcOneColor, arcTwoColor;

  constexpr uint8_t dotRadius = 3;  
  const uint16_t cx = x + 10;
  const uint16_t cy = y - dotRadius;

  hardwareData.rssi = networkRSSIRead();

  if (hardwareData.rssi > 80) {
    // not usable internet, all white
    circleColor = TFT_WHITE;
    arcOneColor = TFT_WHITE;
    arcTwoColor = TFT_WHITE;
    // add debug message
  }
  if (hardwareData.rssi > 70) {
    // poor internet, circle black, arcs white
    circleColor = TFT_BLACK;
    arcOneColor = TFT_WHITE;
    arcTwoColor = TFT_WHITE;
    // add debug message
  }
  if (hardwareData.rssi > 60) {
    // moderate internet, circle and first arc black, last arc grey
    circleColor = TFT_BLACK;
    arcOneColor = TFT_BLACK;
    arcTwoColor = TFT_WHITE;
    // add debug message
  }
  else {
    // excellent internet, all black
    circleColor = TFT_BLACK;
    arcOneColor = TFT_BLACK;
    arcTwoColor = TFT_BLACK;
    // add debug message
  }

  // signal circle
  display.fillSmoothCircle(cx, cy, dotRadius, circleColor, bgColor);

  // Inner signal arc: 3 pixels thick
  display.drawSmoothArc(cx, cy, 9, 7, 138, 222, arcOneColor, bgColor, true);

  // Outer signal arc: 3 pixels thick.
  display.drawSmoothArc(cx, cy, 16, 14, 144, 216, arcTwoColor, bgColor, true);

    // uint8_t barCount;
    // if (hardwareData.rssi < 55) barCount = 5;
    // if (hardwareData.rssi < 67) barCount = 4;
    // if (hardwareData.rssi < 70) barCount = 3;
    // if (hardwareData.rssi < 80) 
    //   barCount = 2;
    // else
    //   barCount = 1;

    // for (uint8_t loop = 1; loop <= barCount; loop++) {
    //   display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, TFT_BLACK);
    // }
    // debugMessage(String("WiFi signal strength on screen as ") + barCount + " bars", 2);
  // }
  // else {
  //   // draw bars in red to represent no WiFi signal
  //   for (uint8_t loop = 1; loop <= 5; loop++) {
  //     display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, TFT_RED);
  //   }
  //   debugMessage("WiFi signal strength via red bars because no WiFi connection", 1);
  // }
  debugMessage("screenHelperWiFiStatus() end",1);
}

void screenHelperPostStatus(uint16_t x, uint16_t y, uint16_t fgColor, uint16_t bgColor) 
{
  debugMessage(String("screenHelperPostStatus() start"), 1); 

    constexpr int16_t W = 20;

    constexpr int16_t R_OUT = 8;
    constexpr int16_t R_IN  = 7;

    const int16_t cx = x + W / 2;

    const int16_t yTop = y + 5;
    const int16_t yMid = y + 10;
    const int16_t yBot = y + 15;

    // Solid filled cylinder body.
    display.fillRect(x + 2, yTop, 17, yBot - yTop + 1, fgColor);

    // Top cap: fill the lower/front half so the top reads as a filled cap.
    display.drawSmoothArc(cx, yTop, R_OUT, 0, 90, 270, fgColor, fgColor, false);

    // Bottom cap: filled lower/front half.
    display.drawSmoothArc(cx, yBot, R_OUT, 0, 270, 90, fgColor, fgColor, false);

    // Interior separator: cut out with bg.
    display.drawSmoothArc(cx, yMid, R_OUT, R_IN, 270, 90, bgColor, fgColor, false);

  debugMessage(String("screenHelperPostStatus() end"), 1);   
}

// Range and math functions
uint8_t co2Range(float co2) 
// converts co2 value to index value for labeling and color
{
  uint8_t co2Range = 
    (co2 <= sensorCO2Fair) ? 0 :
    (co2 <= sensorCO2Poor) ? 1 :
    (co2 <= sensorCO2Bad)  ? 2 : 3;

  debugMessage(String("CO2 input of ") + co2 + " yields CO2 band " + co2Range, 2);
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

  debugMessage(String("VOC index input of ") + vocIndex + " yields VOC band " + vocRange,2);
  return vocRange;
}

uint8_t noxRange(float noxIndex)
// converts noxIndex value to index value for labeling and color
{
  uint8_t noxRange =
  (noxIndex <= sensorNOxFair) ? 0 :
  (noxIndex <= sensorNOxPoor) ? 1 :
  (noxIndex <= sensorNOxBad)  ? 2 : 3;

  debugMessage(String("NOx index input of ") + noxIndex + " yields NOx band " + noxRange,2);
  return noxRange;
}

void screenHelperGraph(uint16_t initialX, uint16_t initialY, uint16_t width, uint16_t height, Measure<kSampleCapacity> measure, uint8_t datatype, String xLabel)
{
  uint8_t stored, capacity;
  int8_t loop; // upper bound is kSampleCapacity definition (size of Measure retained storage)
  uint16_t text1Width, text1Height, graphLineY;
  uint16_t deltaX, x, y, xp, yp;  // graphing positions
  float minValue, maxValue, value, range, average;
  bool firstpoint = true;

  // screen layout assists in pixels
  uint8_t labelSpacer = 2;

  debugMessage("screenHelperGraph() start",1);

  stored   = measure.getStored();
  capacity = measure.getCapacity();

  display.fillRect(initialX,initialY,width,height,TFT_BLACK);

  // Save ourselves some work if we don't have data to plot
  if(stored == 0) {
    xLabel = "Awaiting samples";
    minValue = 0;    // Nothing to plot so arbitrarily set min and max to produce a y axis, but
    maxValue = 100;  // might be good to make this smarter (perhaps don't try plotting at all)
  }
  else {
    // Scan the array for min/max, only checking valid values in retained storage
    for(loop=capacity-stored;loop<capacity;loop++) {
      value = measure.getMember(loop);  // As we use 'value' a lot here...
      if(firstpoint == true) {
        // This is our first data point. Initialize min/max
        minValue = value;
        maxValue = value;
        firstpoint = false;
        continue;
      }
      if(value < minValue) minValue = value;
      if(value > maxValue) maxValue = value;
    }
    debugMessage(String("Min sample value is ") + minValue + ", max is " + maxValue, 2);

    // Since we have data, attempt to scale graph area based on range in data values but with some
    // padding above and below the graphed data itself.  Also have max and min labels
    // as multiples of 10.
    range = maxValue - minValue;
    if(range < 10.0) range = 50.0;
    average = (maxValue + minValue)/2.0;
    maxValue = (int16_t)(10.0 * ceil((average + range)/10.0));
    minValue = (int16_t)(10.0 * floor((average - range)/10.0));
  }

  display.loadFont(Roboto_Regular_12);
  //display.setFreeFont(&FreeSans9pt7b);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_WHITE, TFT_BLACK, true);

  // draw the X axis description, if provided
  text1Height = display.fontHeight();
  if (strlen(xLabel.c_str())) {
    graphLineY = initialY + height - text1Height - labelSpacer;
    text1Width = display.textWidth(xLabel);
    display.drawString(xLabel, (((initialX + width)/2) - (text1Width/2)), (initialY + height - text1Height));
  }
  else {
    // there is no X axis label so use the entire height
    graphLineY = initialY + height;
  }

  // calculate text width and height of longest Y axis label (which we assume is the max value label)
  text1Width = display.textWidth(String(maxValue));
  text1Height = display.fontHeight(); 
  uint16_t graphLineX = initialX + text1Width + labelSpacer;

  // Draw vertical axis
  display.drawFastVLine(graphLineX,initialY,(graphLineY-initialY), TFT_WHITE);
  // Draw horitzonal axis
  display.drawFastHLine(graphLineX,graphLineY,(width-graphLineX),TFT_WHITE);
  
  // draw top Y axis label
  display.drawString(String(int16_t(maxValue)), initialX, initialY);

  // draw bottom Y axis label
  display.drawString(String(int16_t(minValue)), initialX, graphLineY-text1Height);

  // Plot however many data points we have both with filled circles at each
  // point and lines connecting the points.  Color the filled circles with the
  // appropriate warning level color for the type of data being graphed.
  deltaX = ((width-graphLineX) - 10) / (kSampleCapacity-1);  // X distance between points, 10 pixel padding for Y axis
  firstpoint = true;  // Reset for plotting use
  for(loop=capacity-stored;loop<capacity;loop++) {
    x = graphLineX + 10 + (loop*deltaX);  // Include 10 pixel padding for Y axis
    y = graphLineY - (((measure.getMember(loop) - minValue)/(maxValue-minValue)) * (graphLineY-initialY));
    debugMessage(String("Graph position ") + loop + "'s y value is " + y,2);

    if(firstpoint) {
      // If this is the first drawn point then don't try to draw a line
      firstpoint = false;
      xp = x;
      yp = y;
    }
    else {
      // Draw line from previous point (if one) to this point
      display.drawLine(xp,yp,x,y,TFT_WHITE);
    }

    // Draw a filled circle representing the data value, using the warning color scheme appropriate for
    // the specified sensor data type.
    display.fillSmoothCircle(x,y,4,getWarningColor(datatype,measure.getMember(loop)));

    // redraw the last circle to eliminate the line overdrawn on it
    if (!firstpoint)
      display.fillSmoothCircle(xp,yp,4,getWarningColor(datatype,measure.getMember(loop)));

    // Save x & y of this point to use as previous point for next one.
    xp = x;
    yp = y;
  }
  debugMessage("screenHelperGraph() end",1);
}

// Determine the right warning label to use for an arbitrary sensor data value given
// the type of data in question.
String getWarningLabel(uint8_t datatype, float datavalue)
{
  switch(datatype) {
    case CO2_DATA:
      return(warningLabel[co2Range(datavalue)]);
    case VOC_DATA:
      return(warningLabel[vocRange(datavalue)]);
    case NOX_DATA:
      return(warningLabel[noxRange(datavalue)]);
    case PM_DATA:
      return(warningLabel[pm25Range(datavalue)]);
    case TEMP_DATA:
      // Alternatively could explicitly return TFT_GREEN & TFT_YELLOW for temperature 
      // & humidity comfort zones but using warningColor[0] and warningColor[1] provides 
      // configurable consistency with other warning/comfort coloration
      if( (datavalue < sensorTempFComfortMin) || (datavalue > sensorTempFComfortMax) ) return(warningLabel[1]); // "Fair"
      else return(warningLabel[0]);  // "Good"
    case HUM_DATA:
      if( (datavalue < sensorHumidityComfortMin) || (datavalue > sensorHumidityComfortMax) ) return(warningLabel[1]); // "Fair"
      else return(warningLabel[0]); // "Good"
    default:
      return(warningLabel[0]);
  }
}

/**
 * @brief Maps an OpenWeatherMap (OWM) icon code to a Meteocon font character.
 *
 * Converts the OWM icon identifier (e.g. "01d", "10n") into the corresponding
 * character used by the Meteocon icon font set.
 *
 * OWM icon codes consist of:
 *  - Two digits identifying the weather condition (01, 02, 03, 04, 09, 10, 11, 13, 50)
 *  - A day/night suffix ('d' or 'n')
 *
 * @param icon Null-terminated C string containing the OWM icon code.
 *
 * @return Meteocon font character corresponding to the OWM icon.
 *         Returns ')' if the input is invalid, or '?' if no matching icon
 *         mapping is found.
 *
 * @note Meteocon font reference:
 *       https://demo.alessioatzeni.com/meteocons/
 *
 * @warning The caller must ensure that @p icon points to a string of at least
 *          three characters plus a null terminator.
 */
char OWMtoMeteoconIcon(const char* icon)
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

// Draw an "arc meter" to use in portraying the relative quality of an environmental value using a
// four tier scale from worst through good and using the colors specified by the warning color scheme .
// Can draw the arc meter at any size based on an (x,y) coordinate for its center and the desired
// overeall width of the meter itself. Will also display a dot in the proper tier corresponding to 
// a specicifed quality rating on the range [0-3] with 0 = good and 3 = bad as reflected by the
// warning color scheme.
// 
// Does not check that the arc will fit on screen (or be large enough to be clearly legible).
//
// Only draws the arc meter, so displaying the current numerical value or adding a label needs to be
// handled separately.
void arcMeter(uint16_t xcenter, uint16_t ycenter, uint16_t width, uint16_t quality)
{
  // screen layout assists in pixels
  const uint16_t arcOuterRadius = 0.5 + (0.9 * width/2);
  const uint16_t arcInnerRadius = 0.5 + (0.7 * width/2);

  // draw segmented arcs. First and last segments are drawn with rounded ends so are rendered first,
  // interior segments with straight ends are then added over the top.  Drawing order matters here.
  display.drawSmoothArc(xcenter,ycenter,arcOuterRadius,arcInnerRadius,180,270, warningColor[3],TFT_BLACK, true);
  display.drawSmoothArc(xcenter,ycenter,arcOuterRadius,arcInnerRadius,90,180, warningColor[0],TFT_BLACK, true);
  display.drawSmoothArc(xcenter,ycenter,arcOuterRadius,arcInnerRadius,180,225, warningColor[2],TFT_BLACK, false);
  display.drawSmoothArc(xcenter,ycenter,arcOuterRadius,arcInnerRadius,135,180, warningColor[1],TFT_BLACK, false);

  // Add an indicator dot in the zone in the arc that corresponds to the specified quality value. Confirms
  // that the quality value is in the valid range before drawing.
  if( (quality >= 0) && (quality <= 3)) {
    uint16_t dotrblack = 0.075*(width/2);  // radius of the black ring surrounding the indicator dot
    uint16_t dotrwhite = 0.05*(width/2);   // radius of the white circle inside the surrounding ring
    double angle = (22.5 + (quality * 45.0)) * PI / 180.0;  // Convert to radians for cos() and sin() below

    uint16_t xdot = xcenter - (0.5+(cos(angle) * 0.8 * (width/2))) + 1;  // +1 centers better, perhaps due to rendering
    uint16_t ydot = ycenter - (0.5+(sin(angle) * 0.8 * (width/2))) + 1;  // +1 centers better, perhaps due to rendering
    display.fillSmoothCircle(xdot,ydot,dotrblack,TFT_BLACK);
    display.fillSmoothCircle(xdot,ydot,dotrwhite,TFT_WHITE);
  }
}

void fillSmoothRoundRectWithBorder(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t fillColor, uint16_t borderColor, int32_t borderWidth)
{
    // Outer (border)
    display.fillSmoothRoundRect(x, y, w, h, radius, borderColor);
    // Inner (fill)
    display.fillSmoothRoundRect(x + borderWidth, y + borderWidth, w - (2 * borderWidth), h - (2 * borderWidth), radius - borderWidth, fillColor);
}


// Display Climatron main screen
void screenMain() {
  int32_t i, me, mt, mm, ws, hs, wl;
  int32_t x0, y0, w, h, mx, my;
  uint16_t wcolor, windex;

  debugMessage("screenMain() start",1);

  // Panel sizing for a 320x240 display. Important to have all these be integer values
  // but also fill up the screen.  Any unallocated pixels will be at the right and bottom
  // edges.
  me = 17;  // left edge margin
  ws = 86;  // width of small panels
  mm = 15;  // middle margin (between panels both horizontally and vertically)
  wl = 187; // width of large panel on top row (for CO2 value)
  mt = 17;  // top edge margin
  hs = 97;  // height of small panels, which is also the height of the large panel

  // Clear the screen
  display.fillScreen(TFT_BLACK);

  // Temperature and Humidity subpanel (leftmost on the top row)
  display.loadFont(Roboto_Regular_24);
  display.setTextDatum(MC_DATUM);
  x0 = me + (ws/2);
  y0 = mt + 36;
  display.setTextColor(TFT_WHITE,TFT_BLACK);
  display.drawString(String((uint16_t)(totalTemperatureF.getCurrent() + .5))+"°F",x0,y0);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.drawString(String((uint16_t)(totalHumidity.getCurrent() + .5))+"%",x0+10,y0+36);
  // Humidity "droplet" symbol
  display.fillSmoothCircle(x0-25,y0+35,7,TFT_CYAN,TFT_BLACK);
  display.fillTriangle(x0-25,y0+25,x0-20,y0+30,x0-30,y0+30,TFT_CYAN);
  // Panel border
  display.drawSmoothRoundRect(me,mt,8,6,ws,hs,TFT_WHITE);

  // Position arcGauges in the second (bottom) row given the panel sizing and layout
  
  // VOC gauge
  x0 = me;
  mx = x0 + (ws/2);
  y0 = mt + hs + mm;
  my = y0 + arcGaugeHeight(ws) + 10;
  wcolor = getWarningColor(VOC_DATA,totalVOCIndex.getCurrent());
  windex = vocRange(totalVOCIndex.getCurrent());
  display.fillRoundRect(x0,y0,ws,hs,8,wcolor);  // Panel background
  arcGauge(mx,my,ws,windex);  // Gauge
  display.loadFont(Roboto_Regular_24);
  display.setTextDatum(MC_DATUM);
  if(windex == 1) {
    display.setTextColor(TFT_BLACK,wcolor,true);
  }
  else {
    display.setTextColor(TFT_WHITE,wcolor,true);
  }
  display.drawString("VOC",mx,my+14);
  display.drawSmoothRoundRect(x0,y0,8,6,ws,hs,TFT_WHITE);  // Panel border

  // PM2.5 gauge
  // y0 and my don't change (all in the same horizontal row)
  x0 = me + ws + mm;
  mx = x0 + (ws/2);
  wcolor = getWarningColor(PM_DATA,totalPM25.getCurrent());
  windex = pm25Range(totalPM25.getCurrent());
  display.fillRoundRect(x0,y0,ws,hs,8,wcolor);  // Panel background
  arcGauge(mx,my,ws,windex);  // Gauge
  display.loadFont(Roboto_Regular_24);
  if(windex == 1) {
    display.setTextColor(TFT_BLACK,wcolor,true);
  }
  else {
    display.setTextColor(TFT_WHITE,wcolor,true);
  }
  display.drawString("PM25",mx,my+14);
  display.drawSmoothRoundRect(x0,y0,8,6,ws,hs,TFT_WHITE);

  #ifdef SENSOR_SEN66
    // NOX gauge
    // y0 and my don't change (all in the same horizontal row)
    x0 = me + (2*ws) + (2*mm);
    mx = x0 + (ws/2);
    wcolor = getWarningColor(NOX_DATA,totalNOxIndex.getCurrent());
    windex = noxRange(totalNOxIndex.getCurrent());
    display.fillRoundRect(x0,y0,ws,hs,8,wcolor);  // Panel background
    arcGauge(mx,my,ws,windex);  // Gauge
    display.loadFont(Roboto_Regular_24);
    if(windex == 1) {
      display.setTextColor(TFT_BLACK,wcolor,true);
    }
    else {
      display.setTextColor(TFT_WHITE,wcolor,true);
    }
    display.drawString("NOX",mx,my+14);
    display.drawSmoothRoundRect(x0,y0,8,6,ws,hs,TFT_WHITE);
  #endif

  // Now the wide CO2 panel on the right side of the top row

  wcolor = getWarningColor(CO2_DATA,totalCO2.getCurrent());
  windex = co2Range(totalCO2.getCurrent());

  // First draw the CO2 subpanel's quality scale. The dimensions of each element
  // are hand-calculated based on the width of the subpanel, which for this layout
  // works out to be (46x18)
  x0 = me + ws + mm + 1;
  y0 = mt + hs - 20;  // panel height of 18 + 2 pixels of panel border thickness
  display.fillRect(x0,y0,46,18,warningColor[0]);
  display.fillRect(x0+46,y0,46,18,warningColor[1]);
  display.fillRect(x0+92,y0,46,18,warningColor[2]);
  display.fillRect(x0+138,y0,46,18,warningColor[3]);

  // Add the current value indicator. Horizontal position is calculated based on
  // knowledge of the sizes of the quality scale element as handled above.
  mx = x0 + 23 + (windex * 46);  // Simulate quality value placement for CO2
  display.fillSmoothCircle(mx,y0+8,8,TFT_WHITE);
  display.fillSmoothCircle(mx,y0+8,4,TFT_BLACK);

  // Add twelve vertical rule markings, offset from the left edge by half a marking widtth
  y0 = y0 - 25;  // Above the quality panels by 25 pixels (15 rule height + 10 space)
  for(i=0;i<12;i++) {
    mx = x0 + 13 + (0.5 + (i*wl/13.0));
    display.drawWideLine(mx,y0,mx,y0 + 15, 2, TFT_LIGHTGREY);
  }

  // Add current CO2 value panel, which sits above the quality band
  // it corresponds to, overwriting some of the rule markings. It is 46 pixels wide
  // (same as the quality scale elements) and 24 pixels tall to allow for value display,
  // and with rounded ends of radius 12 (half of the 24 pixel height).
  mx = x0 + (windex * 46);  // Simulate quality value placement for CO2
  display.fillSmoothRoundRect(mx,y0-4,46,24,12,wcolor);
  display.loadFont(Roboto_Regular_18);
  display.setTextDatum(MC_DATUM);
  // Display CO2 values consistent with the quality zone
  if(windex == 1) {
    display.setTextColor(TFT_BLACK,wcolor,true);
  }
  else {
    display.setTextColor(TFT_WHITE,wcolor,true);
  }
  display.drawString(String((uint16_t)(totalCO2.getCurrent()+0.5)),mx+23,y0+8);

  // And the panel's label with quality string
  x0 = me + ws + mm + 44;
  y0 = mt + 16;
  display.loadFont(Roboto_Regular_24);
  display.setTextColor(TFT_WHITE,TFT_BLACK,true);
  display.setTextDatum(TL_DATUM);
  display.drawString("CO2: ", x0, y0);
  display.setTextColor(wcolor,TFT_BLACK,true);
  display.drawString(warningLabel[windex],x0+62,y0);

  // And then the CO2 subpanel's border last so it overlays everything.
  display.drawSmoothRoundRect(me+ws+mm,mt,8,6,wl,hs,TFT_WHITE);

}

// Draw a wedge-shaped "arc gauge" that can be used to indicate the current quality of 
// an environmental measurement.  Size of the gauge is determined by the width
// parameter, such that the resulting gauge will fill 90% of that width (leaving some
// margin to enhance visual appearance).  The gauge will be positioned with the center
// of the overall arc (wedge) shape at (xcenter,ycenter).  The height of the gauge is
// determined internally, but if needed the arcGaugeHeight() utility function can be
// used to obtain the height for a gauge of any specified width.
void arcGauge(uint16_t xcenter, uint16_t ycenter, uint16_t width, uint16_t quality)
{
  float ax, ay, bx, by, lwidth;
  uint16_t delta;
  double angle;

  // Calculate the radii to use in drawing the meter. The goal is to have it take up
  // 90% of the specified width, taking into account that it's a wedge that's only
  // a fraction of a semicircle.
  angle = 40 * PI / 180.0;   // The side angle of the wedge, measured from horizontal
  const uint16_t arcOuterRadius = 0.5 + ( (0.9 * width)/(2*cos(angle)) );
  const uint16_t arcInnerRadius = 0.5 + (0.4 * arcOuterRadius);
  const uint16_t arcZoneRadius = 0.5 + arcInnerRadius + (0.6 * (arcOuterRadius - arcInnerRadius));

  lwidth = width/200.0;  // Heuristic for unit line width;

  display.drawArc(xcenter,ycenter,arcOuterRadius,arcZoneRadius,205,230, warningColor[3],TFT_DARKGREY, true);
  display.drawArc(xcenter,ycenter,arcOuterRadius,arcZoneRadius,130,155, warningColor[0],TFT_DARKGREY, true);
  display.drawArc(xcenter,ycenter,arcOuterRadius,arcZoneRadius,180,205, warningColor[2],TFT_DARKGREY, true);
  display.drawArc(xcenter,ycenter,arcOuterRadius,arcZoneRadius,155,180, warningColor[1],TFT_DARKGREY, true);

  display.drawArc(xcenter,ycenter,arcZoneRadius,arcInnerRadius,130,230, TFT_WHITE,TFT_WHITE, true);

  delta = 0.5 + (2 * lwidth);  // Arc radii are integers
  display.drawSmoothArc(xcenter,ycenter,arcOuterRadius,arcOuterRadius-delta,130,230, TFT_BLACK,TFT_DARKGREY, false);
  display.drawSmoothArc(xcenter,ycenter,arcInnerRadius+delta,arcInnerRadius,130,230, TFT_BLACK,TFT_DARKGREY, false);

  // Draw left edge of the meter outline
  angle = 40 * PI / 180.0;
  ax = xcenter - (0.5 + (cos(angle)*arcOuterRadius)) + 1;
  ay = ycenter - (0.5 + (sin(angle)*arcOuterRadius)) + 1;
  bx = xcenter - (0.5 + (cos(angle)*arcInnerRadius)) - 1;
  by = ycenter - (0.5 + (sin(angle)*arcInnerRadius)) - 1;
  display.drawWideLine(ax,ay,bx,by,(2*lwidth),TFT_BLACK);

  // Draw right edge of the meter outline
  angle = 40 * PI / 180.0;
  ax = xcenter + (0.5 + (cos(angle)*arcOuterRadius)) - 1;
  ay = ycenter - (0.5 + (sin(angle)*arcOuterRadius)) + 1;
  bx = xcenter + (0.5 + (cos(angle)*arcInnerRadius)) + 1;
  by = ycenter - (0.5 + (sin(angle)*arcInnerRadius)) - 1;
  display.drawWideLine(ax,ay,bx,by,(2*lwidth),TFT_BLACK);


  // Add a pointer to the quadrant corresponding to the specified quality value. Confirms
  // that the quality value is in the valid range before drawing.
  if( (quality >= 0) && (quality <= 3)) {
    uint16_t dotrblack = 0.2*(width/2);  // radius of the black ring surrounding the indicator dot
    uint16_t dotrwhite = 0.1*(width/2);   // radius of the white circle inside the surrounding ring
    angle = (52.5 + (quality * 25.0)) * PI / 180.0;  // Convert to radians for cos() and sin() below

    uint16_t pointerRadius = (arcOuterRadius + arcZoneRadius)/2.0;
    ax = xcenter - (0.5 + (cos(angle) * pointerRadius));
    ay = ycenter - (0.5 + (sin(angle) * pointerRadius));
    bx = xcenter;
    by = ycenter - arcInnerRadius + dotrwhite;
    display.drawWedgeLine(ax,ay,bx,by,3*lwidth,(dotrwhite),TFT_BLACK);
    
    display.fillSmoothCircle(xcenter,ycenter-arcInnerRadius+dotrwhite,dotrblack,TFT_BLACK);
    display.fillSmoothCircle(xcenter,ycenter-arcInnerRadius+dotrwhite,dotrwhite,TFT_WHITE);
  }
}

// Convenience function to calculate the height of an arcGauge bounding box for any specified 
// bounding box width. To keep use and placement of the gauge simple, everything about the 
// gauge's geometry is calculated internal to the gauge drawing function based on a specified 
// bounding box width into which the gauge should fit.  The resulting bounding box height is  
// not easy to specify or derive exernally from the drawing function given complex internal 
// calculations.  This function allows determining the height from any specific width to simplify 
// meter placement, as the gauge's (x,y) position needs to be provided in drawing it.
uint16_t arcGaugeHeight(uint16_t width) {
  float angle = 40.0 * PI / 180.0;
  uint16_t height = width/(2.0 * cos(angle));
  return(height);
}

// Similar function for calculating the width of an arcGauge bounding box for any specified 
// bounding box height
uint16_t arcGaugeWidth(uint16_t height) {
  float angle = 40.0 * PI / 180.0;
  uint16_t width = height * 2.0 * cos(angle);
  return(width);
}