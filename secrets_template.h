/*
  Project:		Powered Air Quality
  Description:	private configuration data template that needs to be saved as secrets.h after github cloning the project
*/

// Configuration Step 1: Set site environment variables
// set the site altitude in meters to calibrate the SCD40
// Example; downtown Pasadena, CA (SuperCon!) is at 236m
// #define SITE_ALTITUDE	236

// Configuration Step 2: Set WiFi credentials
// set the WiFi SSID and password to connect to network (data endpoints)
// #define WIFI_SSID "YOUR_WIFI_SSID"
// #define WIFI_PASS "YOUR_WIFI_PASSWORD"

// Configuration Step 3: Set Open Weather Map credential and location
//	#define OWM_KEY 		"keyvalue"
// 	#define OWM_LAT_LONG	"lat=34.1448&lon=-118.1509" // Pasadena, CA

// Configuration Step 4: If using MQTT, set MQTT broker login parameters
// #ifdef MQTT
//  #define MQTT BROKER		"mqtt.hostname.local or IP address"
// 	#define MQTT_PORT  		port_number	// use 8883 for SSL
// 	#define MQTT_USER		"key_value"
//  #define MQTT_PASSWORD	"key_value"
// #endif

// Configuration Step 5: If using influxdb, set login and storage parameters
// #ifdef INFLUX
	// For an InfluxDB v1.X server:
	// #define INFLUX_V1			"key_value"
	// #define INFLUXDB_URL			"key_value"
	// #define INFLUXDB_DB_NAME		"key_value"
	// #define INFLUXDB_USER		"key_value"
	// #define INFLUXDB_PASSWORD	"key_value"
	//
	// For an InfluxDB v2.X server:
	// #define INFLUX_V2			"key_value"
	// #define INFLUXDB_URL			"IP address with port number"
	// #define INFLUXDB_TOKEN		"key_value"
	// #define INFLUXDB_ORG			"key_value"
	// #define INFLUXDB_BUCKET		"key_value"
// #endif

// Configuration Step 6: Set key device and installation configuration parameters.  These are used
// widely throughout the code to properly identify the device and generate important
// operating elements like MQTT topics, InfluxDB data tags (metadata).  Should be
// customized to match the target installation. Values here are examples.
// #define DEVICE           "key_value"	// e.g. name of device, "realtime_co2"
// #define DEVICE_SITE      "key_value"	// e.g. physical address of the device, "1234 Main Street"
// #define DEVICE_LOCATION  "key_value"	// e.g. general location of device at physical address, "indoor"
// #define DEVICE_ROOM      "key_value"	// e.g. specific location of device within location, "kitchen"
// #define DEVICE_ID        "key_value"	// e.g. unique ID for the device, "pm25-007"