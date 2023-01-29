// Copy this file to "secrets.h" and adjust the access credentials here to
// match your deployment environment.

// // WiFi configuration settings
// #define WIFI_SSID "YOUR_WIFI_SSID"
// #define WIFI_PASS "YOUR_WIFI_PASSWORD"

// // Device configuration info for dweet.io & ThingSpeak
// /* Info for home cellar monitor */
// #define DWEET_DEVICE "UNIQUE_DWEET_DEVICE_NAME"
// #define THINGS_CHANID 9999999           // ThingSpeak channel ID
// #define THINGS_APIKEY "CHANNEL_WRITE_APIKEY"// write API key for ThingSpeak Channel

// // InfluxDB server url using name or IP address (not localhost)
// #define INFLUXDB_URL "http://influxdbhost.local:8086"
// // InfluxDB v1 database name 
// #define INFLUXDB_DB_NAME "home"
// // InfluxDB v1 user name
// #define INFLUXDB_USER "GRAFANA_USER"
// // InfluxDB v1 user password
// #define INFLUXDB_PASSWORD "GRAFANA_PASSWORD"

// // Tags values for InfluxDB data points.  Should be customized to match your 
// // InfluxDB data model, and can add more here and in post_influx.cpp if desired
// #define DEVICE_NAME "aqi_pm25"
// #define DEVICE_LOCATION "backyard"
// #define DEVICE_SITE "outdoor"