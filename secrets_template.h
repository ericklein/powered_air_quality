/*
  Project:		Powered Air Quality
  Description:	private configuration data template that needs to be saved as secrets.h after github cloning the project
*/

// Configuration Step 1: Set site environment variables
// set the site altitude in meters to calibrate the SCD40
// const String defaultAltitude =	236; // e.g. downtown Pasadena, CA (SuperCon!) is 236m above sea level

// Configuration Step 2: Set Open Weather Map credential
//	const String OWMKey =		"keyvalue";

// Configuration Step 3: If storing data to a network endpoint, set default endpoint path.
// This will only be used if the user doesn't enter then in the configuration AP portal.
//#if defined(MQTT) || defined(INFLUX) || defined(HASSIO_MQTT) || defined(THINGSPEAK)
	// const String defaultSite = "key_value";            // physical address of the device, e.g. "1234 Main"
	// const String defaultLocation = "key_value";        // general location of device at physical address, e.g. "indoor"
	// const String defaultRoom = "key_value";            // specific location of device within location, e.g. "kitchen"
	// const String defaultDeviceID = hardwareDeviceType + "-" + String((uint32_t)((ESP.getEfuseMac() >> 32 ) % 0xFFFFFFFF), HEX);
// #endif 

// Configuration Step #4: If needed, set default MQTT broker information. This will only
// be used if the user doesn't enter then in the configuration AP portal.
// #ifdef MQTT
	// const String defaultMQTTBroker = "192.168.1.1"; // mqtt.hostname.local or IP address
	// const String defaultMQTTPort = "1883";          // use 8883 for SSL (codepath not tested!)
	// const String defaultMQTTUser = "username";      // if needed by MQTT broker
	// const String defaultMQTTPassword = "password";  // if needed by MQTT broker
// #endif

// Configuration Step 5: If needed, set default Influxdb connection parameters
// be used if the user doesn't enter then in the configuration AP portal.
// #ifdef INFLUX
// 	const String defaultInfluxAddress = "192.168.1.1"; // influxdb IP address
// 	const String defaultInfluxPort = "8086";	// influxdb port associated with IP address
// 	const String defaultInfluxOrg = "key_value";		// influxdb organization name
// 	const String defaultInfluxBucket = "key_value"; // influxdb bucket name
// 	// Specify Measurement to use with InfluxDB for sensor and device info
//   const String defaultInfluxEnvMeasurement = "key_value";  // Used for environmental sensor data
// const String defaultInfluxDevMeasurement =  "key_value";   // Used for logging AQI device data (e.g. battery)
// const String influxKey = "key_value";
// #endif

// Configuration Step 6: If using ThingSpeak set channel parameters
// #define THINGS_CHANID 1234567                      // Seven digit ThingSpeak channel ID (number)
// #define THINGS_APIKEY "TS-Channel-Write-API-Key"   // ThingSpeak channel Write API Key (string)