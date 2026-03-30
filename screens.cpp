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
void screenHelperGraph(uint16_t, uint16_t, uint16_t, uint16_t, Measure<graphPoints>, uint8_t, String);
uint16_t getWarningColor(uint8_t, float);
void screenHelperWiFiStatus(uint16_t, uint16_t, uint8_t, uint8_t, uint8_t);
void screenHelperReportStatus(uint16_t, uint16_t);
void screenHelperIndoorOutdoorStatusRegion();
uint8_t co2Range(float); 
uint8_t pm25Range(float);
uint8_t vocRange(float);
uint8_t noxRange(float);

// ***** Screen display routines, typically one per major screen ***** //

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
  display.fillSmoothRoundRect(0, 0, ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, getWarningColor(CO2_DATA,totalCO2.getCurrent()) );
  display.drawString("CO2",display.width()/4,display.height()/4);
  // PM2.5
  display.fillSmoothRoundRect(((display.width()/2)+halfBorderWidth), 0, ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, getWarningColor(PM_DATA,totalPM25.getCurrent()) );
  display.drawString("PM25",display.width()*3/4,display.height()/4);
  // VOC Index
  display.fillSmoothRoundRect(0, ((display.height()/2)+halfBorderWidth), ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, getWarningColor(VOC_DATA,totalVOCIndex.getCurrent()) );
  display.drawString("VOC",display.width()/4,display.height()*3/4);
  #ifdef SENSOR_SEN66
    // NOx index
    display.fillSmoothRoundRect(((display.width()/2)+halfBorderWidth), ((display.height()/2)+halfBorderWidth), ((display.width()/2)-halfBorderWidth), ((display.height()/2)-halfBorderWidth), cornerRoundRadius, getWarningColor(NOX_DATA,totalNOxIndex.getCurrent()) );
    display.drawString("NOx",display.width()*3/4,display.height()*3/4);
  #else
    // Temperature
    display.fillSmoothRoundRect(((display.width()/2)+halfBorderWidth),((display.height()/2)+halfBorderWidth),((display.width()/4)-halfBorderWidth),((display.height()/2)-halfBorderWidth),cornerRoundRadius,getWarningColor(TEMP_DATA,totalTemperatureF.getCurrent()));
    display.setFreeFont(&meteocons24pt7b);
    display.drawString("+",display.width()*5/8,display.height()*3/4);
    // Humdity
    display.fillSmoothRoundRect((((display.width()*3)/4)+halfBorderWidth),((display.height()/2)+halfBorderWidth),((display.width()/4)-halfBorderWidth),((display.height()/2)-halfBorderWidth),cornerRoundRadius,getWarningColor(HUM_DATA,totalHumidity.getCurrent()));
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
  uint16_t text1Width;  // For text size calculation

  // screen layout assists in pixels
  const uint16_t xOutdoorMargin = ((display.width() / 2) + xMargins);
  // temp & humidity
  const uint16_t xTempModifier = 15;
  const uint16_t xHumidityModifier = 60;
  const uint16_t yTempHumdidity = (display.height()*0.9);

  debugMessage("screenTempHumidity() start",2);

  // clear screen
  display.fillScreen(TFT_BLACK);

  screenHelperIndoorOutdoorStatusRegion();

  // Indoor
  // Indoor temp
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(getWarningColor(TEMP_DATA,totalTemperatureF.getCurrent()));
  display.setCursor(xMargins + xTempModifier, yTempHumdidity);
  display.print(String((uint8_t)(totalTemperatureF.getCurrent() + .5)));
  //display.drawBitmap(xMargins + xTempModifier + 35, yTempHumdidity, bitmapTempFSmall, 20, 28, TFT_WHITE);
  display.setFreeFont(&meteocons12pt7b);
  display.print("+");

  // Indoor humidity
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(getWarningColor(HUM_DATA,totalHumidity.getCurrent()));
  display.setCursor(xMargins + xTempModifier + xHumidityModifier, yTempHumdidity);
  display.print(String((uint8_t)(totalHumidity.getCurrent() + 0.5)));
  // IMPROVEMENT: original icon ratio was 5:7?
  // IMPROVEMENT: move this into meteoicons so it can be inline text
  display.drawBitmap(xMargins + xTempModifier + xHumidityModifier + 35, yTempHumdidity - 21, bitmapHumidityIconSmall, 20, 28, TFT_WHITE);

  // Outside

  // do we have OWM Current data to display?
  if (owmCurrentData.tempF != 10000) {
    // Outside temp
    display.setTextColor(getWarningColor(TEMP_DATA,owmCurrentData.tempF));
    display.setCursor(xOutdoorMargin + xTempModifier, yTempHumdidity);
    display.print(String((uint8_t)(owmCurrentData.tempF + 0.5)));
    display.setFreeFont(&meteocons12pt7b);
    display.print("+");

    // Outside humidity
    display.setFreeFont(&FreeSans12pt7b);
    display.setTextColor(getWarningColor(HUM_DATA,owmCurrentData.humidity));
    display.setCursor(xOutdoorMargin + xTempModifier + xHumidityModifier, yTempHumdidity);
    display.print(String((uint8_t)(owmCurrentData.humidity + 0.5)));
    // IMPROVEMENT: original icon ratio was 5:7?
    // IMPROVEMENT: move this into meteoicons so it can be inline text
    display.drawBitmap(xOutdoorMargin + xTempModifier + xHumidityModifier + 35, yTempHumdidity - 21, bitmapHumidityIconSmall, 20, 28, TFT_WHITE);
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

  screenHelperIndoorOutdoorStatusRegion();

  // Indoor PM2.5 ring
  display.fillSmoothCircle(xIndoorPMCircle,yPMCircles,circleRadius,getWarningColor(PM_DATA,totalPM25.getCurrent()));
  display.fillSmoothCircle(xIndoorPMCircle,yPMCircles,circleRadius*0.8,TFT_BLACK);

  // Indoor pm25 value and label inside the circle
  display.setFreeFont(&FreeSans12pt7b);
  display.setTextColor(getWarningColor(PM_DATA,totalPM25.getCurrent()));  // Use highlight color look-up
  display.setCursor(xIndoorCircleText,yPMCircles);
  display.print(totalPM25.getCurrent());
  // label
  display.setTextColor(TFT_WHITE);
  display.setCursor(xIndoorCircleText,yPMCircles+23);
  display.setFreeFont(&FreeSans9pt7b);
  display.print("PM25");
  
  // Outside
  // do we have OWM Air Quality data to display?
  if (owmAirQuality.aqi != 10000) {
    // Outside PM2.5
    display.fillSmoothCircle(xOutdoorPMCircle,yPMCircles,circleRadius,getWarningColor(PM_DATA,owmAirQuality.pm25));
    display.fillSmoothCircle(xOutdoorPMCircle,yPMCircles,circleRadius*0.8,TFT_BLACK);

    // outdoor pm25 value and label inside the circle
    display.setFreeFont(&FreeSans12pt7b);
    display.setTextColor(getWarningColor(PM_DATA,owmAirQuality.pm25));  // Use highlight color look-up
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


void screenSaver()
// Description: Display current CO2 reading at a random location (e.g. "screen saver")
// Parameters:  NA
// Returns: NA (void)
// Improvement: ?
{
  // screen assists
  uint16_t text1Width, text1Height;
  uint16_t x, y;

  debugMessage("screenSaver() start",1);

  display.fillScreen(TFT_BLACK);
  display.setFreeFont(&FreeSans24pt7b);
  display.setTextColor(getWarningColor(CO2_DATA,totalCO2.getCurrent()));

  // This may be font specific, but experimentation has shown a wide range of accuracy
  // in the horizontal text extent returned by display.textWidth().  When it is wrong it
  // seems to be wrong by too much, overestimating string extent but we have no choice
  // but to work with the value it give us. Using too little of the screen is better than
  // having text clipped off the edges of the screen...
  text1Width = display.textWidth(String(totalCO2.getCurrent()));

  // For "Free Fonts" display.fontHeight() returns yAdvance, which is the height of
  // the actual text plus inter-line spacing as recommended in the font's metrics.
  // Free Fonts are also imprecise about character baselines, so we need to pad things
  // a bit to make sure the CO2 value is fully on screen.  Alas there is no 
  // display.textHeight() function in the TFT_eSPI library to calculate actual string
  // extent (analogously to display.textWidth()).
  text1Height = display.fontHeight();

  // Display CO2 value in random, valid on-screen location
  // display.setCursor(random(xMargins,display.width()-xMargins-text1Width), random(yMargins, display.height() - yMargins - text1Height));
  x = random(xMargins,display.width()-xMargins-text1Width);
  y = random(yMargins + text1Height, display.height() - yMargins);
  debugMessage(text1Width + String(" by ") + text1Height + String(" at (") + x + String(",") + y + String(")"),1);
  display.setCursor(x,y);
  display.print(uint16_t(totalCO2.getCurrent()));

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
  display.fillScreen(getWarningColor(VOC_DATA,totalVOCIndex.getCurrent()));
  display.fillSmoothRoundRect(borderWidth, borderHeight,display.width()-(2*borderWidth),display.height()-(2*borderHeight),cornerRoundRadius,TFT_BLACK);

  // value and label
  display.setCursor(borderWidth + 20,yLabel);
  display.print("VOC");
  display.setTextColor(getWarningColor(VOC_DATA,totalVOCIndex.getCurrent()));  // Use highlight color look-up table
  display.setCursor(xLabel, yLabel);
  display.print(uint16_t(totalVOCIndex.getCurrent()));

  screenHelperGraph(borderWidth + 5, display.height()/3, (display.width()-(2*borderWidth + 10)),((display.height()*2/3)-(borderHeight + 5)), totalVOCIndex, VOC_DATA, "Recent values");

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
  display.fillSmoothCircle(xVOCCircle,yVOCCircle,circleRadius,getWarningColor(NOX_DATA,totalNOxIndex.getCurrent()));
  display.fillSmoothCircle(xVOCCircle,yVOCCircle,circleRadius*0.8,TFT_BLACK);

  // legend for NOx color wheel
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  // NOx value and label (displayed inside circle)
  display.setTextColor(getWarningColor(NOX_DATA,totalNOxIndex.getCurrent()));  // Use highlight color look-up
  display.setCursor(xVOCCircle-20,yVOCCircle);
  display.print(int(totalNOxIndex.getCurrent()+.5));
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
  display.fillScreen(getWarningColor(CO2_DATA,totalCO2.getCurrent()));
  display.fillSmoothRoundRect(borderWidth, borderHeight,display.width()-(2*borderWidth),display.height()-(2*borderHeight),cornerRoundRadius,TFT_BLACK);

  // value and label
  display.setCursor(borderWidth + 20,yLabel);
  display.print("CO2");
  display.setTextColor(getWarningColor(CO2_DATA,totalCO2.getCurrent()));  // Use highlight color look-up table
  display.setCursor(xLabel, yLabel);
  display.print(uint16_t(totalCO2.getCurrent()));

  screenHelperGraph(borderWidth + 5, display.height()/3, (display.width()-(2*borderWidth + 10)),((display.height()*2/3)-(borderHeight + 5)), totalCO2, CO2_DATA, "Recent values");

  // legend for CO2 color wheel
  for(uint8_t loop = 0; loop < 4; loop++){
    display.fillRect(xLegend,(yLegend-(loop*legendHeight)),legendWidth,legendHeight,warningColor[loop]);
  }

  debugMessage("screenCO2() end",1);
}


bool screenAlert(String messageText)
// Description: Display error message centered on screen, using different font sizes and/or splitting to fit on screen
// Parameters: String containing error message text
// Output: NA (void)
// Improvement: ?
{
  bool success = false;
  uint16_t text1Width, text1Height;

  debugMessage("screenAlert start",1);

  display.setTextColor(TFT_WHITE);
  display.fillScreen(TFT_BLACK);

  debugMessage(String("screenAlert text is '") + messageText + "'",2);

  // does message fit on one line with large font?
  display.setFreeFont(&FreeSans24pt7b);
  text1Width = display.textWidth(messageText);
  text1Height = display.fontHeight();
  debugMessage(String("Message at font size ") + text1Height + " is " + text1Width + " pixels wide",2);
  if (text1Width <= (display.width()-(display.width()/2-(text1Width/2)))) {
    // fits with large font, display
    display.setCursor(((display.width()/2)-(text1Width/2)),((display.height()/2)+(text1Height/2)));
    display.print(messageText);
    success = true;
  }
  else {
    // does message fit on two lines with large font?
    debugMessage(String("large font is ") + abs(display.width()-text1Width) + " pixels too long, trying 2 lines", 1);
    // does the string break into two pieces based on a space character?
    uint8_t spaceLocation;
    String messageTextPartOne, messageTextPartTwo;
    uint16_t text2Width;

    spaceLocation = messageText.indexOf(' ');
    if (spaceLocation) {
      // has a space character, measure two lines
      messageTextPartOne = messageText.substring(0,spaceLocation);
      messageTextPartTwo = messageText.substring(spaceLocation+1);
      // we can use the previous height calculation
      text1Width = display.textWidth(messageTextPartOne);
      text2Width = display.textWidth(messageTextPartTwo);
      debugMessage(String("Message part one with large font is ") + text1Width + " pixels wide",2);
      debugMessage(String("Message part two with large font is ") + text2Width + " pixels wide",2);
    }
    else {
      debugMessage("there is no space in message to break message into 2 lines",2);
    }
    if (spaceLocation && (text1Width <= (display.width()-(display.width()/2-(text1Width/2)))) && (text2Width <= (display.width()-(display.width()/2-(text2Width/2))))) {
        // fits on two lines, display
        display.setCursor(((display.width()/2)-(text1Width/2)),(display.height()/2+text1Height/2)-25);
        display.print(messageTextPartOne);
        display.setCursor(((display.width()/2)-(text2Width/2)),(display.height()/2+text1Height/2)+25);
        display.print(messageTextPartTwo);
        success = true;
    }
    else {
      // does message fit on one line with medium sized text?
      debugMessage("couldn't break text into 2 lines or one line is too long, trying medium text",1);

      display.setFreeFont(&FreeSans18pt7b);
      text1Width = display.textWidth(messageText);
      text1Height = display.fontHeight();
      debugMessage(String("Message at font size ") + text1Height + " is " + text1Width + " pixels wide",2);
      if (text1Width <= (display.width()-(display.width()/2-(text1Width/2)))) {
        // fits with small size
        display.setCursor(display.width()/2-text1Width/2,display.height()/2+text1Height/2);
        display.print(messageText);
        success = true;
      }
      else {
        // doesn't fit with medium font, display as truncated, small text
        debugMessage(String("medium font is ") + abs(display.width()-text1Width) + " pixels too long, displaying small and truncated", 1);
        display.setFreeFont(&FreeSans12pt7b);
        text1Width = display.textWidth(messageText);
        text1Height = display.fontHeight();
        display.setCursor(display.width()/2-text1Width/2,display.height()/2+text1Height/2);
        display.print(messageText);
      }
    }
  }
  debugMessage("screenAlert end",1);
  return success;
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

void screenHelperGraph(uint16_t initialX, uint16_t initialY, uint16_t xWidth, uint16_t yHeight, Measure<graphPoints> measure, uint8_t datatype, String xLabel)
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

  debugMessage("screenHelperGraph() start",1);

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
  debugMessage("screenHelperGraph() end",1);
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