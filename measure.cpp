#include "Arduino.h"
#include "measure.h"

// Simple utility class to manage sensor readings and other environmental values that are
// gathered and monitored over time

Measure::Measure()
{
    _value = _total = _maxvalue = _minvalue = _average = 0.0;
    _count = 0;
    _new_min_max = true;
}

void Measure::include(float value)
{
  _count++; // started with 0
  _value = value;
  _total += value;
  if(_new_min_max == true) {
    _maxvalue = _minvalue = value;
    _new_min_max = false;
  }
  else {
    if(value > _maxvalue) _maxvalue = value;
    if(value < _minvalue) _minvalue = value;  
  }
  _average = _total / _count;
}

// clear() completely zeroes everything, including the min and max values
void Measure::clear()
{
    _value = _total = _average = 0;
    _count = 0;
    _maxvalue = _minvalue = 0.0;
    _new_min_max = true;
}

// resetAvg() clears the value, total, count and average but leaves the min
// and max values unmodified.  Use resetAvg() to reset the cumulative averaging
// behavior, e.g., to begin a new sampling interval, but leave the 
// longer term observed max/min values alone.
void Measure::resetAvg()
{
    _value = _total = _average = 0.0;
    _count = 0;
}

uint32_t Measure::getCount()
{
  return _count;
}

float Measure::getTotal()
{
  return _total;
}

float Measure::getMax()
{
  return _maxvalue;
}

float Measure::getMin()
{
  return _minvalue;
}

float Measure::getAverage()
{
  return _average;
}

float Measure::getCurrent()
{
  return _value;
}

void Measure::printMeasure()
{
  Serial.print("[#");
  Serial.print(_count);
  Serial.print("] ");
  Serial.print(_value,2);
  Serial.print(" (");
  Serial.print(_minvalue,2);
  Serial.print(",");
  Serial.print(_average,2);
  Serial.print(",");
  Serial.print(_maxvalue,2);
  Serial.println(")");
}
