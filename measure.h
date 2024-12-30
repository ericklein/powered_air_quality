#ifndef measure_h
#define measure_h

#include "Arduino.h"

class Measure {
  public:
    Measure();
    void include(float value);
    void clear();
    void resetAvg();
    uint32_t getCount();
    float getMax();
    float getMin();
    float getAverage();
    float getCurrent();
    float getTotal();
    void printMeasure();
  private:
    float _value;
    uint32_t _count;
    float _total;
    float _maxvalue;
    float _minvalue;
    float _average;
    bool _new_min_max; 
};

#endif