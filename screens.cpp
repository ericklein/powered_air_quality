#include <Arduino.h>
#include <Measure.hpp>
#include "config.h"
#include "powered_air_quality.h"
#include <TFT_eSPI.h>  // https://github.com/Bodmer/TFT_eSPI

// fonts and glyphs
#include "Fonts/meteocons12pt7b.h"
#include "Fonts/meteocons16pt7b.h"
#include "Fonts/meteocons24pt7b.h"
#include "glyphs.h"

// Shared helper function(s) and globals
extern void debugMessage(String messageText, uint8_t messageLevel);
extern TFT_eSPI display;
extern uint32_t timeLastReportMS;
extern Measure<graphPoints> totalTemperatureF, totalHumidity, totalCO2, totalVOCIndex, totalPM25, totalNOxIndex;

// Forward declarations for local functions to help make ordering in this file easier
void screenHelperGraphNew(uint16_t, uint16_t, uint16_t, uint16_t, Measure<graphPoints>, uint8_t, String);
void screenHelperGraph(uint16_t, uint16_t, uint16_t, uint16_t, const float *, uint8_t, String);
void screenHelperComponentSetup(String);
uint16_t getWarningColor(uint8_t, float);
void screenHelperWiFiStatus(uint16_t, uint16_t, uint8_t, uint8_t, uint8_t);
void screenHelperReportStatus(uint16_t, uint16_t);
void screenHelperIndoorOutdoorStatusRegion();
uint8_t co2Range(float); 
uint8_t pm25Range(float);
uint8_t vocRange(float);
uint8_t noxRange(float);
char OWMtoMeteoconIcon(const char*);

// ***** Screen display routines, typically one per major screen ***** //


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

void screenTempHumidity() 
// Description: Displays indoor and outdoor temperature and humidity
// Parameters:
// Output: NA (void)
// Improvement: 
{
  // screen layout assists in pixels
  const uint8_t yStatusRegion = display.height()/8;

  debugMessage("screenTempHumidity() start",2);

  screenHelperComponentSetup("Temp/Humidity");
  // split indoor v. outside
  display.drawFastVLine((display.width() / 2), yStatusRegion, display.height(), TFT_DARKGREY);

  display.setTextDatum(MC_DATUM);

  // labels
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(TFT_WHITE);
  display.drawString("Indoor", display.width()/4, display.height()/6);
  display.drawString("Outside", (display.width()*3/4), display.height()/6);

  display.setFreeFont(&FreeSans24pt7b);

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
  if (owmCurrentData.tempF != 255) {
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
  const uint8_t   yStatusRegion = display.height()/8;
  const uint16_t  xOutdoorMargin = ((display.width() / 2) + xMargins);
  // temp & humidity
  const uint16_t  yPollution = 210;
  // pm25 rings
  const uint16_t  xIndoorPMCircle = (display.width() / 4);
  const uint16_t  xOutdoorPMCircle = (display.width()*3/4);
  const uint16_t  yPMCircles = 123;
  const uint16_t  circleRadius = 65;
  // inside the pm25 rings
  const uint16_t  xIndoorCircleText = (xIndoorPMCircle - 18);
  const uint16_t  xOutdoorCircleText = (xOutdoorPMCircle - 18);

  debugMessage("screenPM25() start",2);

  screenHelperComponentSetup("PM2.5");
  // split indoor v. outside
  display.drawFastVLine((display.width() / 2), yStatusRegion, display.height(), TFT_DARKGREY);

  display.setTextDatum(MC_DATUM);

  // labels
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(TFT_WHITE);
  display.drawString("Indoor", display.width()/4, display.height()/6);
  display.drawString("Outside", (display.width()*3/4), display.height()/6);

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
  if (owmAirQuality.aqi != 255) {
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

void screenVOC()
// Description: Display VOC index information (ppm, color grade, graph)
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  // screen layout assists in pixels
  const uint8_t legendHeight =  20;
  const uint8_t legendWidth =   10;
  const uint16_t xLegend =      display.width() - xMargins - 5 - legendWidth;
  const uint16_t yLegend =      ((display.height()/3) + (uint8_t(3.5*legendHeight)));
  const uint16_t yValue =       display.width()/6;

  debugMessage("screenVOC() start",1);

  screenHelperComponentSetup("VOC");

  display.setFreeFont(&FreeSans24pt7b);
  display.setTextDatum(MC_DATUM);

  // VOC numeric value
  display.setTextColor(warningColor[vocRange(sensorData.vocIndex[graphPoints-1])]);  // Use highlight color look-up table
  display.drawString(String(uint16_t(sensorData.vocIndex[graphPoints-1])), (display.width()/2), yValue);

  screenHelperGraph(xMargins, display.height()/3, (display.width()-(2*xMargins)-legendWidth-10),((display.height()*2/3)-yMargins), sensorData.vocIndex, VOC_DATA, "Recent values");

  // legend for CO2 color wheel
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  debugMessage("screenVOC() end",1);
}

void screenCO2()
// Description: Display CO2 information (ppm, color grade, graph)
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  // screen layout assists in pixels
  const uint8_t legendHeight =  20;
  const uint8_t legendWidth =   10;
  const uint16_t xLegend =      display.width() - xMargins - 5 - legendWidth;
  const uint16_t yLegend =      ((display.height()/3) + (uint8_t(3.5*legendHeight)));
  const uint16_t yValue =       display.width()/6;

  debugMessage("screenCO2() start",1);

  screenHelperComponentSetup("CO2");

  display.setFreeFont(&FreeSans24pt7b);
  display.setTextDatum(MC_DATUM);

  // CO2 numeric value
  display.setTextColor(warningColor[co2Range(sensorData.ambientCO2[graphPoints-1])]);
  display.drawString(String(uint16_t(sensorData.ambientCO2[graphPoints-1])), (display.width()/2), yValue);

  screenHelperGraph(xMargins, display.height()/3, (display.width()-(2*xMargins)-legendWidth-10),((display.height()*2/3)-yMargins), sensorData.ambientCO2, CO2_DATA, "Recent values");

  // legend for CO2 color wheel
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }
  debugMessage("screenCO2() end",1);
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

  screenHelperComponentSetup("NOx");

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



void screenHelperComponentSetup(String header)
// Description: helper function for screenXXX() routines to draw the status region frame
// Parameters: NA
// Output : NA
// Improvement : NA
{
  // screen layout assists in pixels
  const uint8_t   yStatusRegion = display.height()/8;
  const uint8_t   yStatusRegionFloor = yStatusRegion - 7;  
  const uint8_t   helperXSpacing = 15;

  debugMessage("screenHelperStatusBar() start",1);

  display.fillScreen(TFT_BLACK);
  display.fillRect(0,0,display.width(),yStatusRegion,TFT_DARKGREY);
  // screen helpers in status region
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), yStatusRegionFloor, wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperReportStatus(((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing)))-helperXSpacing), (yStatusRegionFloor-15));

  //label
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(TFT_WHITE);
  display.setTextDatum(L_BASELINE);
  display.drawString(header, ((display.width()/2)-(display.textWidth(header)/2)), yStatusRegionFloor);

  debugMessage("screenHelperStatusBar() end",1);
}

void screenHelperWiFiStatus(uint16_t initialX, uint16_t initialY, uint8_t barWidth, uint8_t barHeightIncrement, uint8_t barSpacing)
// Description: helper function for screenXXX() routines drawing WiFi RSSI strength
// Parameters: 
// Output : NA
// Improvement : error handling for initialX, initialY, and overall width and height
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
// Improvement : NA
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



void screenHelperIndoorOutdoorStatusRegion()
// Description: helper function for screenXXX() routines to draw the status region frame for indoor/outdoor information
// Parameters: NA
// Output : NA
// Improvement : NA
{
  // screen layout assists in pixels
  const uint16_t yStatusRegion = display.height()/8;
  const uint16_t xOutdoorMargin = ((display.width() / 2) + xMargins);

  display.fillRect(0,0,display.width(),yStatusRegion,TFT_DARKGREY);
  // split indoor v. outside
  display.drawFastVLine((display.width() / 2), yStatusRegion, display.height(), TFT_DARKGREY);
  // screen helpers in status region
  // IMPROVEMENT: Pad the initial X coordinate by the actual # of bars
  screenHelperWiFiStatus((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), (yMargins + (5*wifiBarHeightIncrement)), wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperReportStatus(((display.width() - xMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing)))-20), yMargins);
  // labels
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(TFT_WHITE);
  display.setCursor(xMargins, ((display.height()/8)-7));
  display.print("Indoor");
  display.setCursor(xOutdoorMargin, ((display.height()/8)-7));
  display.print("Outdoor");
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


void screenHelperGraph(uint16_t initialX, uint16_t initialY, uint16_t xWidth, uint16_t yHeight, const float *values, uint8_t datatype, String xLabel)
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

  switch (datatype) {
    case CO2_DATA:
      minValue = sensorCO2Max;
      maxValue = sensorCO2Min;
      break;
    case VOC_DATA:
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
    if (datatype == CO2_DATA)
      display.fillSmoothCircle(x,y,4,warningColor[co2Range(sensorData.ambientCO2[loop])]);
    else
      if (datatype == VOC_DATA)
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


void screenHelperGraphNew(uint16_t initialX, uint16_t initialY, uint16_t xWidth, uint16_t yHeight, Measure<graphPoints> measure, uint8_t datatype, String xLabel)
{
  int i, stored, capacity;
  int8_t loop; // upper bound is graphPoints definition
  uint16_t text1Width, text1Height;
  uint16_t deltaX, x, y, xp, yp;  // graphing positions
  float minValue, maxValue, value, range, average;
  bool firstpoint = true;

  // screen layout assists in pixels
  uint8_t labelSpacer = 2;
  uint16_t graphLineX; // dynamically defined below
  uint16_t graphLineY;

  debugMessage("screenHelperGraphNew() start",1);

  stored   = measure.getStored();
  capacity = measure.getCapacity();

  display.fillRect(initialX,initialY,xWidth,yHeight,TFT_BLACK);
  display.setFreeFont();
  display.setTextColor(TFT_WHITE);

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
    debugMessage(String("Min value in samples is ") + minValue + ", max is " + maxValue, 2);

    // Since we have data, attempt to scale graph area based on range in data values but with some
    // padding above and below the graphed data itself.  Also have max and min labels
    // as multiples of 10.
    range = maxValue - minValue;
    if(range < 10.0) range = 50.0;
    average = (maxValue + minValue)/2.0;
    maxValue = (int16_t)(10.0 * ceil((average + range)/10.0));
    minValue = (int16_t)(10.0 * floor((average - range)/10.0));

  }

  // draw the X axis description, if provided, and set the position of the horizontal axis line
  if (strlen(xLabel.c_str())) {
    text1Width = display.textWidth(xLabel);
    text1Height = display.fontHeight();
    graphLineY = initialY + yHeight - text1Height - labelSpacer;
    display.setCursor((((initialX + xWidth)/2) - (text1Width/2)), (initialY + yHeight - text1Height));
    display.print(xLabel);
  }

  // calculate text width and height of longest Y axis label (which we assume is the max value label)
  text1Width = display.textWidth(String(maxValue));
  text1Height = display.fontHeight(); 
  graphLineX = initialX + text1Width + labelSpacer;
  
  // draw top Y axis label
  display.setCursor(initialX, initialY);
  display.print(int16_t(maxValue));
  // draw bottom Y axis label
  display.setCursor(initialX, graphLineY-text1Height); 
  display.print(int16_t(minValue));

  // Draw vertical axis
  display.drawFastVLine(graphLineX,initialY,(graphLineY-initialY), TFT_WHITE);
  // Draw horitzonal axis
  display.drawFastHLine(graphLineX,graphLineY,(xWidth-graphLineX),TFT_WHITE);

  // Plot however many data points we have both with filled circles at each
  // point and lines connecting the points.  Color the filled circles with the
  // appropriate warning level color for the type of data being graphed.
  deltaX = ((xWidth-graphLineX) - 10) / (graphPoints-1);  // X distance between points, 10 pixel padding for Y axis
  xp = graphLineX;
  yp = graphLineY;
  firstpoint = true;  // Reset for plotting use
  for(loop=capacity-stored;loop<capacity;loop++) {
    x = graphLineX + 10 + (loop*deltaX);  // Include 10 pixel padding for Y axis
    y = graphLineY - (((measure.getMember(loop) - minValue)/(maxValue-minValue)) * (graphLineY-initialY));
    debugMessage(String("Array ") + loop + " y value is " + y,2);

    // Draw a filled circle to represent the data value, using the warning color scheme appropriate for
    // the specified sensor data type.
    display.fillSmoothCircle(x,y,4,getWarningColor(datatype,measure.getMember(loop)) );

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
  debugMessage("screenHelperGraphNew() end",1);
}

// Determine the right warning color to use for an arbitrary sensor data value given
// the type of data in question.
uint16_t getWarningColor(uint8_t datatype, float datavalue)
{
  switch(datatype) {
    case CO2_DATA:
      return(warningColor[co2Range(datavalue)]);
    case VOC_DATA:
      return(warningColor[vocRange(datavalue)]);
    case NOX_DATA:
      return(warningColor[noxRange(datavalue)]);
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