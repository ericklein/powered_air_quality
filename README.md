# PM25_pmsa003i Air Quality Monitor

## Project Overview

This project is now in its third generation, having evolved from an intial form that used
an older Plantower PM2.5 air quality sensor](https://www.adafruit.com/product/3686) sensor, which did
a fine job measuring 2.5 micron particulates and allowing monitoring of overall air quality but
used a serial UART interface that was a bit tedious to program for. The second generation switched
to a newer sensor from the Sensirion SEN5x series, specifically an
[SEN54](https://sensirion.com/products/catalog/SEN54/), offering measurement of a wider range of
environmental values plus built-in I2C communications. Sparkfun sells the
[SEN54](https://www.sparkfun.com/products/19325), which measures PM1.0, PM2.5, and PM10.0
particulates as well as Volatile Organic Compounds (VOC), temperature and humidity. Delightfully,
Sensirion provides [Arduino libraries](https://github.com/Sensirion/arduino-i2c-sen5x) for the
SEN5x series, along with some excellent example sketches.

The [Adafruit Feather Huzzah](https://www.adafruit.com/product/2821) continues to be a great
microcontroller option for projects like this given the onboard ESP8266 with built-in WiFi support, 
flexible power options, ready access to GPIO, and Feather form factor.  Newer Feather devices
are built on the more powerful ESP32 and are supported here as well.

Over time the code here has evolved to add an increasing range of back-end data publishing and
reporting services, including [Dweet](https://dweet.io), [ThingSpeak](https://thingspeak.com),
[InfluxDB](https://www.influxdata.com), [MQTT](https://io.adafruit.com/api/docs/mqtt.html), and
[Home Assistant](https://www.home-assistant.io) via MQTT. This makes it easy to access AQI
data from the sensor in web pages, on mobile devices, through graphing facilities like
[Grafana](https://grafana.com/), and even in automating smart home system responses to good or
bad overall air. Services supported are configurable through compile-time settings and via
separate routines for posting data as appropriate.

## What is AQI Anyway?
It's worth noting that while most people have heard of an "Air Quality Index" from their local
weather service or news, the calculation of AQI from sensor readings is less well known.  Sensors
like the Sensirion SEN54 measure and report particulate concentrations in various size ranges, the
most widely used of which is "PM2.5", meaning airborne particulates of 2.5 microns in diameter or
smaller, measured in micrograms per cubic meter.  The more particulates measured the worse the air
quality and, eventually, the greater the danger to humans and animals.  Howeveer, the perceived
quality of the air and the risk from exposure vary in a non-obvious way based on the actual PM2.5
values observed.  That variability is what gave rise to the idea of an Air Quality Index in the
first place, though as is often the case in associating health factors and risk with environmental
data different governing bodies and standards organizations have put forward different ways of
calculating risk from sensor data.  You can read much more about this in the Wikipedia page for
Air Quality, which you'll find [here](https://en.wikipedia.org/wiki/Air_quality_index).

In the US, the Environmental Protection Agency (EPA) developed its own AQI measure, dividing the
normal range of measured particulates and pollutants into six categories indicating increased 
levels of health concerns.  An overall AQI value is calculated from a piecewise linear function,
with scaling and transition points defined by the EPA.  More details on that math are shared in the
Wikipedia page cited above, as well as this [post](https://forum.airnowtech.org/t/the-aqi-equation/169)
on the AirNow tech forum.  Code included in this project handles calculating US EPA AQI from
PM2.5 readings returned by the SEN54 sensor (which curiously is not a feature built into the
SEN5x Arduino library though probalby should be).

## Usage Notes

Important access settings like WiFi SSID and password, ThingSpeak keys, and InfluxDB credentials
are contained in a `secrets.h` file that is not included in this repo.  Instead you'll find the
file `secrets_template.h`, which should be copied to `secrets.h` and then edited to supply the
right access credentials and configuration values to match your deployment environment.

The routines that post data to back end services are generalized as much as practical, though do
need to be customized to match the data fieles of interest both within the scope of the project
and based on what users want to report and monitor.  Configuration values in config.h help with
basic customization, e.g. name of the device, tags to use for Influx data, though in some cases
code may need to be modified in the associated post routine.

