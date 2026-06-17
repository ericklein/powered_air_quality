/*
  Project:      Powered Air Quality
  Description:  screen related routines
*/

#include <Arduino.h>
#include <Measure.hpp>
#include "config.h"
#include "powered_air_quality.h"
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI
#include <PNGdec.h>   // https://github.com/bitbank2/PNGdec

// fonts and glyphs
#include "ui/meteocons24pt7b.h"
#include "ui/glyphs.h"

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
void screenHelperWiFiStatus(uint16_t, uint16_t, uint8_t, uint8_t, uint8_t);
void screenHelperReportStatus(uint16_t, uint16_t);
uint8_t co2Range(float); 
uint8_t pm25Range(float);
uint8_t vocRange(float);
uint8_t noxRange(float);
char OWMtoMeteoconIcon(const char*);
void arcMeter(uint16_t, uint16_t, uint16_t, uint16_t);
int pngDraw(PNGDRAW *pDraw);
void drawPNGFromFlash(const uint8_t *imageData, size_t imageSize, TFT_eSPI &display, int16_t xpos, int16_t ypos);
void fillSmoothRoundRectWithBorder(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t fillColor, uint16_t borderColor, int32_t borderWidth = 2);

PNG png; // PNG decoder instance

struct DrawContext {
  TFT_eSPI *display;
  int16_t xpos;
  int16_t ypos;
};

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

void screenMain()
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
  // display.drawSmoothRoundRect(110,11,4,2,200,95,TFT_WHITE,TFT_BLACK);
  drawPNGFromFlash(co2_base_png, sizeof(co2_base_png), display,110,11);
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
  constexpr uint8_t helperXSpacing = 15;
  constexpr uint8_t wifiBarHeightIncrement = 3;
  constexpr uint8_t wifiBarWidth = 3;
  constexpr uint8_t wifiBarSpacing = 5;

  debugMessage("screenHelperHeaderBar() start",1);

  display.loadFont(Roboto_Regular_24);

  display.fillScreen(TFT_BLACK);
  if ((datatype == PM_DATA) || (datatype == UNK_DATA)) {
    // when displaying multiple data sources the header bare is a neutral color
    display.fillRect(0,0,display.width(),kYStatusRegion,TFT_DARKGREY);

    // vertical separator for indoor/outdoor
    display.drawFastVLine((display.width() / 2), kYStatusRegion, display.height(), TFT_DARKGREY);

    // indoor/outdoor labels
    // display.setFreeFont(&FreeSans12pt7b);
    display.setTextColor(TFT_WHITE, TFT_BLACK, true);
    display.setTextDatum(MC_DATUM);
    display.drawString("Indoor", display.width()/4, yLabels);
    display.drawString("Outside", (display.width()*3/4), yLabels);

    // set the color for the header bar label
    display.setTextColor(TFT_BLACK,TFT_DARKGREY, true);
  }
  else {
    // set header bar color and background text color to the most recent sample warning color
    display.fillRect(0,0,display.width(),kYStatusRegion,getWarningColor(datatype,measure.getMember(measure.getCurrent())));
    display.setTextColor(TFT_BLACK,getWarningColor(datatype,measure.getMember(measure.getCurrent())), true);

  }
  // screen helpers in status region
  screenHelperWiFiStatus((display.width() - kXMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), yStatusRegionFloor, wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperReportStatus(((display.width() - kXMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing)))-helperXSpacing), (yStatusRegionFloor-15));

  // header bar label
  // display.setFreeFont(&FreeSans12pt7b);
  display.setTextDatum(L_BASELINE);
  display.drawString(header, ((display.width()/2)-(display.textWidth(header)/2)), yStatusRegionFloor);

  display.unloadFont();
  debugMessage("screenHelperHeaderBar() end",1);
}

void screenHelperWiFiStatus(uint16_t initialX, uint16_t initialY, uint8_t barWidth, uint8_t barHeightIncrement, uint8_t barSpacing)
// Description: helper function for screenXXX() routines drawing WiFi RSSI strength
// Parameters: 
// Output : NA
// Improvement : error handling for initialX, initialY, and overall width and height
//  dedicated icon type for no WiFi?
{
  debugMessage("screenHelperWiFiStatus() start",1);

  hardwareData.rssi = networkRSSIRead();

  if (hardwareData.rssi != 255) {
    uint8_t barCount;
    if (hardwareData.rssi < 55) barCount = 5;
    if (hardwareData.rssi < 67) barCount = 4;
    if (hardwareData.rssi < 70) barCount = 3;
    if (hardwareData.rssi < 80) 
      barCount = 2;
    else
      barCount = 1;

    for (uint8_t loop = 1; loop <= barCount; loop++) {
      display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, TFT_BLACK);
    }
    debugMessage(String("WiFi signal strength on screen as ") + barCount + " bars", 2);
  }
  else {
    // draw bars in red to represent no WiFi signal
    for (uint8_t loop = 1; loop <= 5; loop++) {
      display.fillRect((initialX + (loop * barSpacing)), (initialY - (loop * barHeightIncrement)), barWidth, loop * barHeightIncrement, TFT_RED);
    }
    debugMessage("WiFi signal strength via red bars because no WiFi connection", 1);
  }
  debugMessage("screenHelperWiFiStatus() end",1);
}

void screenHelperReportStatus(uint16_t initialX, uint16_t initialY)
// Description: helper function for screenXXX() routines that displays an icon relaying success of network endpoint writes
// Parameters: initial x and y coordinate to draw from
// Output : NA
// Improvement : NA
// 
{
  debugMessage(String("screenHelperReportStatus() start"), 1); 
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
    if ((timeLastReportMS == 0) || ((millis() - timeLastReportMS) >= (timeReportMS * reportFailureThreshold))) {
      // we haven't successfully written to a network endpoint at all or before the reportFailureThreshold
      // display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_BLACK);
      debugMessage(String("Report status on screen as false"),2);
    }
    else {
      drawPNGFromFlash(network_store_png, sizeof(network_store_png),display,initialX, initialY);
      //display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_BLACK);
      debugMessage(String("Report status on screen as true"),2);
    }
  #endif
  debugMessage(String("screenHelperReportStatus() end"), 1);   
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

void drawPNGFromFlash(
  const uint8_t *imageData,
  size_t imageSize,
  TFT_eSPI &display,
  int16_t xpos,
  int16_t ypos
) {
  DrawContext ctx = {
    &display,
    xpos,
    ypos
  };

  if (png.openFLASH((uint8_t *)imageData, imageSize, pngDraw) == PNG_SUCCESS) {
    display.startWrite();
    png.decode(&ctx, 0);
    display.endWrite();
    //png.close();
  }
}

// Callback function to renders each image line to the TFT
int pngDraw(PNGDRAW *pDraw) {
  DrawContext *ctx = (DrawContext *)pDraw->pUser;

  uint16_t lineBuffer[ctx->display->width()];
  uint8_t maskBuffer[1 + ((ctx->display->width() + 7) / 8)];

  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_BIG_ENDIAN, 0xffffffff);

  // for getAlphaMask's third parameter
  // 1 -> preserve all visible alpha pixels
  // 128 ->  cleaner hard edge, ignores faint antialiasing
  // 255  -> only fully opaque pixels; often too aggressive

  if (png.getAlphaMask(pDraw, maskBuffer, 100)) {
    display.pushMaskedImage(ctx->xpos, ctx->ypos + pDraw->y, pDraw->iWidth, 1, lineBuffer, maskBuffer);
  }

  return 1;  // non-zero = continue decoding
}

void fillSmoothRoundRectWithBorder(
    int32_t x, int32_t y,
    int32_t w, int32_t h,
    int32_t radius,
    uint16_t fillColor,
    uint16_t borderColor,
    int32_t borderWidth)
{
    // Outer (border)
    display.fillSmoothRoundRect(
        x,
        y,
        w,
        h,
        radius,
        borderColor
    );

    // Inner (fill)
    display.fillSmoothRoundRect(
        x + borderWidth,
        y + borderWidth,
        w - (2 * borderWidth),
        h - (2 * borderWidth),
        radius - borderWidth,
        fillColor
    );
}