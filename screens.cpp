/*
  Project:      Powered Air Quality
  Description:  screen related routines
*/

#include <Arduino.h>
#include <Measure.hpp>
#include "config.h"
#include "powered_air_quality.h"
#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI

// https://fonts.google.com/specimen/Roboto
#include "ui/fonts/Roboto_Regular_12.h"
#include "ui/fonts/Roboto_Regular_16.h"  // Just used for Weather Forecast screen
#include "ui/fonts/Roboto_Regular_18.h"
#include "ui/fonts/Roboto_Regular_24.h"
#include "ui/fonts/Roboto_Regular_36.h"

#include "ui/fonts/Roboto_Bold_12.h"    // Just used for Weather Forecast screen
#include "ui/fonts/Roboto_Bold_36.h"
#include "ui/fonts/Roboto_Bold_60.h"

// Shared helper function(s) and globals
extern uint8_t networkRSSIRead();
extern bool OWMAirPollutionRead();
extern bool OWMForecastRead();
extern void debugMessage(String messageText, uint8_t messageLevel);
extern uint16_t getWarningColor(uint8_t, float);
extern uint16_t getWarningTextColor(uint8_t, float);
extern TFT_eSPI display;
extern uint32_t timeLastReportMS;
extern Measure<kSampleCapacity> totalTemperatureF, totalHumidity, totalCO2, totalVOCIndex, totalPM25, totalNOxIndex;
extern struct SiteForecast owmSiteForecast;

// Forward declarations for local functions to help make ordering in this file easier
void screenHelperGraph(uint16_t, uint16_t, uint16_t, uint16_t, Measure<kSampleCapacity>, uint8_t, String);
void screenHelperHeaderBar(uint16_t, uint16_t, String header);
String getWarningLabel(uint8_t, float);
void screenHelperWiFiStatus(uint16_t, uint16_t, uint16_t);
void screenHelperPostStatus(uint16_t, uint16_t, uint16_t, uint16_t);
uint8_t co2Range(float); 
uint8_t pm25Range(float);
uint8_t vocRange(float);
void arcMeter(uint16_t, uint16_t, uint16_t, uint16_t);
void arcGauge(uint16_t, uint16_t, uint16_t, uint16_t);
uint16_t arcGaugeHeight(uint16_t);
uint16_t arcGaugeWidth(uint16_t);
void fillSmoothRoundRectWithBorder(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, uint16_t fillColor, uint16_t borderColor, int32_t borderWidth = 2);
// Weather forecast drawing functions
void wxSunnyIcon(uint16_t, uint16_t, uint32_t);
void wxCloudyIcon(uint16_t, uint16_t, uint32_t);
void wxRainyIcon(uint16_t, uint16_t, uint32_t);
void wxPartlyCloudyIcon(uint16_t, uint16_t, uint32_t);
void wxSnowIcon(uint16_t, uint16_t, uint32_t);
void wxDay(String, uint16_t, uint16_t, uint32_t);
void wxTemperatures(uint16_t, uint16_t, uint16_t, uint16_t, uint32_t);
void wxHumidity(uint16_t, uint16_t, uint16_t, uint32_t);

// ***** Screen display routines, typically one per major screen ***** //
void screenPM25() 
{
  // screen layout assists in pixels
  const uint8_t yLabels = display.height() / 4; 
  const uint16_t  xOutdoorMargin = ((display.width() / 2) + kXMargins);
  const uint16_t  xIndoorPMCircle = (display.width() / 4);
  const uint16_t  xOutdoorPMCircle = (display.width()*3/4);
  constexpr uint16_t  yPMCircles = 150;
  constexpr uint16_t  circleRadius = 65;
  constexpr uint16_t circleInnerRadius = circleRadius * 8 / 10;
  uint16_t fgcolor, bgcolor;

  debugMessage("screenPM25() start",1);

  // Draw header bar using appropriate color scheme
  bgcolor = TFT_DARKGREY;
  fgcolor = TFT_WHITE;

  screenHelperHeaderBar(fgcolor,bgcolor,"PM 2.5");

  // vertical separator for indoor/outdoor
  display.drawFastVLine((display.width() / 2), kYStatusRegion, display.height(), bgcolor);

  // indoor/outdoor labels
  display.loadFont(Roboto_Regular_24);
  display.setTextColor(TFT_WHITE, TFT_BLACK, true);
  display.setTextDatum(MC_DATUM);
  display.drawString("Indoor", display.width()/4, yLabels);
  display.drawString("Outside", (display.width()*3/4), yLabels);

  display.setTextDatum(MC_DATUM);

  // Indoor
  display.drawSmoothArc(xIndoorPMCircle, yPMCircles, circleRadius, circleInnerRadius, 0, 360, getWarningColor(PM_DATA, totalPM25.getCurrent()), TFT_BLACK);
  // value and label inside the circle
  display.loadFont(Roboto_Bold_36);
  display.setTextColor(getWarningColor(PM_DATA,totalPM25.getCurrent()), TFT_BLACK, true);  // Use highlight color look-up
  display.drawFloat(totalPM25.getCurrent(), 1, xIndoorPMCircle, yPMCircles);
  
  // Outside
  if (OWMAirPollutionRead()) {
    display.drawSmoothArc(xOutdoorPMCircle, yPMCircles, circleRadius, circleInnerRadius, 0, 360, getWarningColor(PM_DATA,owmAirQuality.pm25), TFT_BLACK);
    // value and label inside the circle
    display.setTextColor(getWarningColor(PM_DATA,owmAirQuality.pm25), TFT_BLACK, true); // Use highlight color look-up 
    display.drawFloat(owmAirQuality.pm25, 1, xOutdoorPMCircle, yPMCircles);
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
  uint16_t fgcolor, bgcolor;

  debugMessage("screenVOC() start",1);

  bgcolor = getWarningColor(VOC_DATA,totalVOCIndex.getCurrent());
  fgcolor = getWarningTextColor(VOC_DATA,totalVOCIndex.getCurrent());
  screenHelperHeaderBar(fgcolor,bgcolor,"VOC Level");

  display.setTextDatum(MC_DATUM);

  // If VOCIndex has no values, alert the user
  if (totalVOCIndex.getStored() == 0) {
    display.loadFont(Roboto_Regular_18);
    display.setTextColor(TFT_RED, TFT_BLACK, true);
    display.drawString("No data", (display.width() / 2), (display.height() / 2));
  }
  else {
    // Draw segmented arc showing color range and current VOCIndex in that range
    arcMeter(xCircle,yCircle,display.width(),vocRange(totalVOCIndex.getCurrent()));

    // Display VOCIndex value and label inside the arc
    display.loadFont(Roboto_Bold_60);
    display.setTextColor(getWarningColor(VOC_DATA,totalVOCIndex.getCurrent()), TFT_BLACK, true);  // Use highlight color look-up 
    display.drawFloat((totalVOCIndex.getCurrent() +.5), 0, xValue, yValue);
    display.loadFont(Roboto_Regular_24);
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
  uint16_t fgcolor, bgcolor;

  debugMessage("screenCO2() start",1);

  bgcolor = getWarningColor(CO2_DATA,totalCO2.getCurrent());
  fgcolor = getWarningTextColor(CO2_DATA,totalCO2.getCurrent());
  screenHelperHeaderBar(fgcolor,bgcolor,"Recent CO2 Values");

  display.loadFont(Roboto_Regular_36);

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
    display.setTextDatum(BR_DATUM);
    display.setTextColor(TFT_WHITE, TFT_BLACK, true);
    display.drawString((String(uint16_t(totalCO2.getCurrent())) + "ppm"), (display.width()-(2*kXMargins)), yValue - 3);

    // recent CO₂ graph
    screenHelperGraph(kXMargins, yValue, (display.width()-(2*kXMargins)),((display.height()-yValue)-kYMargins), totalCO2, CO2_DATA, "");
  }
  display.unloadFont();
  debugMessage("screenCO2() end",1);
}

void screenHelperHeaderBar(uint16_t fgcolor, uint16_t bgcolor, String header)
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
  uint16_t iconfgcolor;

  debugMessage("screenHelperHeaderBar() start",1);

  display.loadFont(Roboto_Regular_18);

  display.fillScreen(TFT_BLACK);

  // Draw header bar background and set matching text color based on
  // values passed in
  display.fillRect(0,0,display.width(),kYStatusRegion, bgcolor);
  display.setTextColor(fgcolor, bgcolor, true);

  // screen helpers in status region
  // screenHelperWiFiStatus((display.width() - kXMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing))), yStatusRegionFloor, wifiBarWidth, wifiBarHeightIncrement, wifiBarSpacing);
  screenHelperWiFiStatus((display.width() - kXMargins - kIconWidth), yStatusRegionFloor, bgcolor);
  
  #if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
    if ((timeLastReportMS == 0) || ((millis() - timeLastReportMS) >= (timeReportMS * reportFailureThreshold))) {
      // we haven't successfully written to a network endpoint at all or before the reportFailureThreshold
      // display.drawBitmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_BLACK);
      iconfgcolor = TFT_RED;
      debugMessage(String("Post status in header bar is false"),2);
    }
    else {
      iconfgcolor = TFT_BLACK;
      //display.drawiBtmap(initialX, initialY, checkmark_12x15, 12, 15, TFT_BLACK);
      debugMessage(String("Post status in header bar is true"),2);
    }
    //screenHelperPostStatus(((display.width() - kXMargins - ((5*wifiBarWidth)+(4*wifiBarSpacing)))-(kHelperXSpacing + kIconWidth)), (yStatusRegionFloor-kIconHeight), fgColor, bgColor);
    screenHelperPostStatus((display.width() - kXMargins - (2 * kIconWidth) - kHelperXSpacing), (kYStatusRegion-24), iconfgcolor, bgcolor);
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

  constexpr uint8_t dotRadius = 2;  
  const uint16_t cx = x + 10;
  const uint16_t cy = y - dotRadius;

  hardwareData.rssi = networkRSSIRead();

  if (hardwareData.rssi > 80) {
    // not usable internet, all black
    circleColor = TFT_BLACK;
    arcOneColor = TFT_BLACK;
    arcTwoColor = TFT_BLACK;
    // add debug message
  }
  if (hardwareData.rssi > 70) {
    // poor internet, circle white, arcs black
    circleColor = TFT_WHITE;
    arcOneColor = TFT_BLACK;
    arcTwoColor = TFT_BLACK;
    // add debug message
  }
  if (hardwareData.rssi > 60) {
    // moderate internet, circle and first arc white, last arc black
    circleColor = TFT_WHITE;
    arcOneColor = TFT_WHITE;
    arcTwoColor = TFT_BLACK;
    // add debug message
  }
  else {
    // excellent internet, all white
    circleColor = TFT_WHITE;
    arcOneColor = TFT_WHITE;
    arcTwoColor = TFT_WHITE;
    // add debug message
  }

  // signal circle
  display.fillSmoothCircle(cx, cy, dotRadius, circleColor, bgColor);

  // Inner signal arc: 4 pixels thick
  display.drawSmoothArc(cx, cy, 9, 6, 135, 225, arcOneColor, bgColor, false);

  // Outer signal arc: 4 pixels thick.
  display.drawSmoothArc(cx, cy, 16, 13, 135, 225, arcTwoColor, bgColor, false);

  debugMessage("screenHelperWiFiStatus() end",1);
}

void screenHelperPostStatus(uint16_t x, uint16_t y, uint16_t fgColor, uint16_t bgColor) 
{
  debugMessage(String("screenHelperPostStatus() start"), 1); 

    constexpr int16_t W = 10;
    constexpr int16_t H = 16;

    constexpr int16_t R_OUT = 8;
    constexpr int16_t R_IN  = 7;

    const int16_t cx = x + W / 2;
    const int16_t cy = y + 9;

    const int16_t yTop = y + 5;
    const int16_t yMid = y + 5;
    const int16_t yBot = y + 10;

    float theta;

    theta = 180.0*atan(((float)W)/(H))/PI;
    display.drawSmoothArc(cx,cy+(H/2),H+4,0,180-theta,180+theta,fgColor,bgColor,false);
    display.drawSmoothArc(cx,cy-(H/2),H+4,0,360-theta,theta    ,fgColor,bgColor,false);
    display.fillRect(cx-W-1,cy-(H/2),2*(W+1)+1,H,fgColor);
    display.drawSmoothArc(cx,cy-H-4,H+4,H+3,360-theta,theta,bgColor,fgColor,false);

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
  // Draw horizontal axis
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


// Display main screen
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
  if((windex == 1) || (windex == 0)) {
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
  if((windex == 1) || (windex == 0)) {
    display.setTextColor(TFT_BLACK,wcolor,true);
  }
  else {
    display.setTextColor(TFT_WHITE,wcolor,true);
  }
  display.drawString("PM25",mx,my+14);
  display.drawSmoothRoundRect(x0,y0,8,6,ws,hs,TFT_WHITE);

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
  if((windex == 1) || (windex == 0)) {
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

// Display a 5-day weather forecast as a screen, using data fetched elsewhere
// from OpenWeatherMap
void screenForecast() {
  uint8_t i;
  uint16_t x0, y0;
  uint32_t bgcolor;
  int cond;

  String wdayname[7] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};

  debugMessage("screenForecast() start",1);

  // Retrieve forecast data from OpenWeatherMap
  if(!OWMForecastRead()) {
    // Unable to fetch forecast from OWM. 
    debugMessage("OWM Forecast - fetch failed!",1); 
    //TODO: What else to do here??
    return;
  }

  // Erase the screen
  display.fillScreen(TFT_BLACK);
  display.setTextColor(TFT_WHITE, TFT_BLACK);  // Adding a background colour erases previous text automatically

  // Draw status bar at the top of the screen
  screenHelperHeaderBar(TFT_WHITE,TFT_DARKGREY,owmSiteForecast.cityName);
  /*
  display.fillRect(0,0,320,30,TFT_DARKGREY);
  display.loadFont(Roboto_Regular_18);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_WHITE,TFT_DARKGREY,true);
  display.drawString(owmSiteForecast.cityName,160,15);
  */

  x0 = 32;
  for(i=0;i<5;i++,x0+=64) {
    // Day label
    y0 = 45;
    if(i==0) {
      bgcolor = TFT_TODAYBG;
      display.fillRect(0,30,64,240,bgcolor);  // Today's info has a special background color
      wxDay("TODAY",x0,y0,bgcolor);
    }
    else {
      bgcolor = TFT_BLACK;
      wxDay(wdayname[owmSiteForecast.forecastData[i].wday],x0,y0,bgcolor);
    }

    // Add weather condition icon
    y0 = 80;
    switch(owmSiteForecast.forecastData[i].wxFcst) {
      case FCST_NONE:
        // TODO: How to handle?  Ignore? Question mark??
        break;
      case FCST_CLEAR:
        wxSunnyIcon(x0,y0,bgcolor);
        break;
      case FCST_CLOUDY:
        wxCloudyIcon(x0,y0,bgcolor);
        break;
      case FCST_PTCLOUDY:
        wxPartlyCloudyIcon(x0,y0,bgcolor);
        break;
      case FCST_RAIN:
        wxRainyIcon(x0,y0,bgcolor);
        break;
      case FCST_SNOW:
        wxSnowIcon(x0,y0,bgcolor);
        break;    
    }

    // High and Low temperatures for the day
    y0 = 140;
    wxTemperatures(owmSiteForecast.forecastData[i].maxTempF,
      owmSiteForecast.forecastData[i].minTempF,x0,y0,bgcolor);

    // Humidity for the day
    y0 = 200;
    wxHumidity(owmSiteForecast.forecastData[i].humidity,x0,y0,bgcolor);
  }
  debugMessage("screenForecast() end",1);
}

// Draw the weather condition icon for "sunny".
void wxSunnyIcon(uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  display.fillSmoothCircle(x0,y0,8,TFT_ORANGE,TFT_TODAYBG);
  display.drawWideLine(x0,y0-12,x0,y0-16,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0,y0+12,x0,y0+16,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0+12,y0,x0+16,y0,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0-12,y0,x0-16,y0,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0+9,y0-9,x0+12,y0-12,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0+9,y0+9,x0+12,y0+12,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0-9,y0+9,x0-12,y0+12,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0-9,y0-9,x0-12,y0-12,4,TFT_ORANGE,bgcolor);
}

// Draw the weather condition icon for "cloudy"
void wxCloudyIcon(uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  display.fillSmoothCircle(x0,y0-4,10,TFT_CYAN,bgcolor);
  display.fillSmoothCircle(x0-10,y0+2,8,TFT_CYAN,bgcolor);
  display.fillSmoothCircle(x0+10,y0+4,6,TFT_CYAN,bgcolor);
  display.fillRect(x0-10,y0-6,20,17,TFT_CYAN);
}

// Draw the weather condition icon for "rainy" (reusing "cloudy" design)
void wxRainyIcon(uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  // Cloud should be same as wxCloudyIcon()
  display.fillSmoothCircle(x0,y0-6,10,TFT_CYAN,bgcolor);
  display.fillSmoothCircle(x0-10,y0,8,TFT_CYAN,bgcolor);
  display.fillSmoothCircle(x0+10,y0+2,6,TFT_CYAN,bgcolor);
  display.fillRect(x0-10,y0-8,20,17,TFT_CYAN);
  // Now draw lines representing rain
  display.drawWideLine(x0-1,y0+12,x0-4,y0+16,4,TFT_CYAN,bgcolor);
  display.drawWideLine(x0-11,y0+12,x0-15,y0+16,4,TFT_CYAN,bgcolor);
  display.drawWideLine(x0+9,y0+12,x0+5,y0+16,4,TFT_CYAN,bgcolor);
}

void wxPartlyCloudyIcon(uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  // Render the same sun as used for Sunny weather but obscure part of
  // it with a small cloud
  display.fillSmoothCircle(x0+2 ,y0  ,8,TFT_ORANGE,bgcolor);   // Sun's disc
  display.fillSmoothCircle(x0   ,y0+4,8,TFT_CYAN,TFT_ORANGE);  // Cloud that entirely overlaps sun
  display.fillSmoothCircle(x0-8 ,y0+8,6,TFT_CYAN,bgcolor);     // Left cloud part
  display.fillSmoothCircle(x0+11,y0+9,5,TFT_CYAN,bgcolor);     // Right cloud part
  display.fillRect(x0-6,y0+2,10,13,TFT_CYAN);                  // Cover smoothing on cloud parts
  display.fillRect(x0+4,y0+4,8,11,TFT_CYAN);                   // Cover smoothing on cloud parts

  // Sun's rays, only some of which are visible (not behind the cloud)
  display.drawWideLine(x0+2,y0-12,x0+2,y0-16,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0+14,y0,x0+18,y0,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0-10,y0,x0-14,y0,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0+11,y0-9,x0+13,y0-12,4,TFT_ORANGE,bgcolor);
  display.drawWideLine(x0-7,y0-9,x0-10,y0-12,4,TFT_ORANGE,bgcolor);
}

void wxSnowIcon(uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  // Small, white cloud
  display.fillSmoothCircle(x0   ,y0-9,8,TFT_WHITE,bgcolor);     // Top cloud
  display.fillSmoothCircle(x0-8 ,y0-5,6,TFT_WHITE,bgcolor);     // Left cloud part
  display.fillSmoothCircle(x0+11,y0-4,5,TFT_WHITE,bgcolor);     // Right cloud part
  display.fillRect(x0-6,y0-11,10,13,TFT_WHITE);                  // Cover smoothing on cloud parts
  display.fillRect(x0+4,y0-9 ,8,11,TFT_WHITE);                   // Cover smoothing on cloud parts
  // Add some snow flakes
  // display.fillSmoothCircle(x0+12,y0+9,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0+11,y0+6 ,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0+1 ,y0+6 ,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0-9 ,y0+6 ,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0-14,y0+11,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0-4 ,y0+11,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0+6 ,y0+11,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0+11,y0+16,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0+1 ,y0+16,2,TFT_WHITE,bgcolor);
  display.fillSmoothCircle(x0-9 ,y0+16,2,TFT_WHITE,bgcolor);
}

void wxDay(String label, uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  display.loadFont(Roboto_Bold_12);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_YELLOW,bgcolor,true);
  display.drawString(label,x0,y0);
}

void wxTemperatures(uint16_t high, uint16_t low, uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  display.loadFont(Roboto_Regular_24);
  display.setTextDatum(MC_DATUM);
  display.drawWideLine(x0-16,y0,x0+16,y0,4,TFT_WHITE,bgcolor);
  display.setTextColor(TFT_WHITE,bgcolor,true);
  display.drawString(String(high)+"°",x0,y0-12);
  display.drawString(String(low)+"°",x0,y0+17);
}

void wxHumidity(uint16_t humidity, uint16_t x0, uint16_t y0, uint32_t bgcolor) {
  // Hollow (layered) indicator
  display.fillSmoothCircle(x0,y0,14,TFT_CYAN,bgcolor);
  display.fillTriangle(x0-10,y0-10,x0,y0-20,x0+10,y0-10,TFT_CYAN);
  display.fillSmoothCircle(x0,y0,12,bgcolor,TFT_CYAN);
  display.fillTriangle(x0-9,y0-9,x0,y0-18,x0+9,y0-9,bgcolor);
  display.loadFont(Roboto_Regular_16);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(TFT_WHITE,bgcolor,true);
  display.drawString(String(humidity),x0,y0+2);
}