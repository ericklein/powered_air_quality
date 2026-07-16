/*
  Project:      Powered Air Quality
  Description:  public (non-secret) hardware configuration
*/

#pragma once
#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

// Configuration Step 1: Define base hardware platform
// PAQ: CYD ESP32-2432S028R (2.8" TFT, micro-USB, 2 GPIO pin connectors), no LEDs
// Climatron: CYD JC2432W328 (2.8" TFT, USB-C, 3 GPIO pin connectors), LED strip
// #define PAQ
#define CLIMATRON

#if defined(PAQ) && defined(CLIMATRON)
  #error "Define only one device target: PAQ or CLIMATRON"
#endif

#if !defined(PAQ) && !defined(CLIMATRON)
  #error "Define one device target: PAQ or CLIMATRON"
#endif

// Configuration Step 2: Define sensor platform
// Climatron has as Sensirion SEN66 embedded (temp, humidity, PM25, CO2, VOC, NOx)
// PAQ can use either Sensirion SCD40 (temp, humidity, CO2) + SEN54 (PM25) or SEN66
#ifdef CLIMATRON
  #define SENSOR_SEN66
#else
  // #define SENSOR_SEN54SCD40
  #define SENSOR_SEN66
#endif

constexpr uint8_t screenBLMax = 255;
constexpr uint8_t screenBLLow  = 52;   // 255 * 0.20

#ifdef PAQ
  const String hardwareDeviceType = "AirQuality";
  constexpr uint8_t pinButton = 0; // boot button on most ESP32 boards
  constexpr uint8_t pinSensorSDA = 22;
  constexpr uint8_t pinSensorSCL = 27;
  constexpr uint8_t pinTouchIRQ = 36;
  constexpr uint8_t pinTouchMOSI = 32;
  constexpr uint8_t pinTouchMISO = 39;
  constexpr uint8_t pinTouchCLK = 25;
  constexpr uint8_t pinTouchCS = 33;
  constexpr uint8_t pinAudio = 26;
  constexpr uint32_t audioFrequency = 2000; // Hz
  constexpr uint8_t  audioResolution = 8;    // bit
  // touchscreen calibration
  constexpr uint16_t touchscreenMinX = 200;
  constexpr uint16_t touchscreenMaxX = 3700;
  constexpr uint16_t touchscreenMinY = 240;
  constexpr uint16_t touchscreenMaxY = 3800;
#endif

#ifdef CLIMATRON
  const String hardwareDeviceType = "Climatron";
  constexpr uint8_t pinButton = 0; // boot button on most ESP32 boards
  constexpr uint8_t pinSensorSDA = 22;
  constexpr uint8_t pinSensorSCL = 21;
  constexpr uint8_t pinTouchSDA = 33;
  constexpr uint8_t pinTouchSCL = 32;
  constexpr uint8_t pinTouchRST = 25;
  constexpr int8_t pinTouchIRQ = -1;
  constexpr uint8_t pinLEDStripOne = 4;
  constexpr uint8_t ledStripPixelCount = 3; // number of LEDs on each strip
  constexpr int8_t pinAudio = 26;
  constexpr uint32_t audioFrequency = 1000; // Hz
  constexpr uint8_t  audioResolution = 8;    // bit
#endif

  #endif // DEVICE_CONFIG_H