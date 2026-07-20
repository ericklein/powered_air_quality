/*
  Project:		Powered Air Quality
  Description:	private configuration data template that needs to be saved as secrets.h after github cloning the project
*/

// Configuration Step 1: Set default device latitude, longitude, altitude.
// These values will populate the web configuration portal until replaced by the user
// const String kDefaultAltitude = "0"; // meters
// const String kDefaultLatitude = "0";
// const String kDefaultLongitude = "0";

// Configuration Step 2: Set Open Weather Map credential
//	const String OWMKey =		"keyvalue";

// Configuration Step 3: If storing data to a network endpoint, set default endpoint path.
// These populate the web configuration portal and are default values until replaced by the user
// const String kDefaultSite = "key_value";            // physical address of the device, e.g. "1234 Main"
// const String kDefaultLocation = "key_value";        // general location of device at physical address, e.g. "indoor"
// const String kDefaultRoom = "key_value";            // specific location of device within location, e.g. "kitchen"

// Configuration Step #4: If needed, set default MQTT broker information. This will only
// be used if the user doesn't enter then in the configuration AP portal.
// const String kDefaultMQTTBroker = "192.168.1.1"; // mqtt.hostname.local or IP address
// const String kDefaultMQTTPort = "1883";          // use 8883 for SSL (codepath not tested!)
// const String kDefaultMQTTUser = "username";      // if needed by MQTT broker
// const String kDefaultMQTTPassword = "password";  // if needed by MQTT broker

// Configuration Step 5: If needed, set default Influxdb connection parameters
// be used if the user doesn't enter then in the configuration AP portal.
// 	const String kDefaultInfluxAddress = "192.168.1.1"; // influxdb IP address
// 	const String kDefaultInfluxPort = "8086";	// influxdb port associated with IP address
// 	const String kDefaultInfluxOrg = "key_value";		// influxdb organization name
// 	const String kDefaultInfluxBucket = "key_value"; // influxdb bucket name
// 	// Specify Measurement to use with InfluxDB for sensor and device info
//  const String kDefaultInfluxEnvMeasurement = "key_value";  // Used for environmental sensor data
//	const String kDefaultInfluxDevMeasurement =  "key_value";   // Used for logging AQI device data (e.g. battery)
//	const String influxKey = "key_value";

// Configuration Step 6: If using ThingSpeak set channel parameters
// #define THINGS_CHANID 1234567                      // Seven digit ThingSpeak channel ID (number)
// #define THINGS_APIKEY "TS-Channel-Write-API-Key"   // ThingSpeak channel Write API Key (string)