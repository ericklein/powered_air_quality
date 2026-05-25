#pragma once

#define PAQ
// #define CLIMATRON

#if defined(PAQ) && defined(CLIMATRON)
  #error "Define only one device target: PAQ or CLIMATRON"
#endif

#if !defined(PAQ) && !defined(CLIMATRON)
  #error "Define one device target: PAQ or CLIMATRON"
#endif

// Configuration Step 7: Which sensor configuration do we have?  Later generation devices
// use Sensirion SEN66 sensor which measures CO2, particulates, VOC, NOX, temperature and humidity
// in one package.  Earlier generation devices use a combination of the SEN54 particulates
// sensor and the SCD40 CO2 sensor (which also provides VOC, temperature and humidity readings).
// Note that only the newer SEN66 configuration provides NOX readings (using Sensirion's 
// NOX Index).
// Use the one that corresponds to your device hardware and leave the other commented out.
#define SENSOR_SEN66
// #define SENSOR_SEN54SCD40