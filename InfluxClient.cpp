#include "Arduino.h"
#include <HTTPClient.h>
#include "InfluxClient.h"

/****** InfluxClient class -- manages server-related operations ******/
// Constructor
InfluxClient::InfluxClient(String server, String org, String bucket, String apitoken) {
  _server = server;
  _org = org;
  _bucket = bucket;
  _apitoken = apitoken;
}

// Carries out an HTTP POST operation to publish a Point of data to Influx,
// generating the necessary components of that operation from the tags and
// fields supplied.
void InfluxClient::writePoint(InfluxPoint point)
{
  int httpcode;
  String influxurl, authvalue, pointdata;

  // Based on Influx v2 API documentation
  authvalue = "Token " + _apitoken;
  influxurl = "http://" + _server + "/api/v2/write?org=" + _org + "&bucket=" + _bucket + "&precision=ns";
  pointdata = point._measurement + "," + point._tags + " " + point._fields;

  Serial.print("BEGIN: "); Serial.println(influxurl);
  Serial.print("Authorization: ");  Serial.println(authvalue);
  Serial.println("DATA:");
  Serial.println(pointdata);

  // Print a 'curl' version of this Influx post for testing purposes. Enabled only in DEBUG mode (see code below)
  // but commented out here as it's an optional debugging tool even then.  Uncomment to have the DEBUG output
  // include a 'curl' command you can copy and paste into a command shell in order to test InfluxDB access.
  // _printCurl(influxurl, authvalue, pointdata);

  // Send it!
  httpcode = _httpPOSTRequest(influxurl, authvalue, pointdata);

  // httpcode will be negative on error, but HTTP status might indicate failure
  if (httpcode > 0) {
    // HTTP POST complete, print result code
    Serial.println("HTTP POST [" + _server + "], result code: " + String(httpcode));
  } else {
    Serial.println("HTTP POST [" + _server + "] failed, result code: " + String(httpcode));
  }
}

#ifdef DEBUG
// To aid in development and testing, print a 'curl' command that could carry out
// the post to Influx
void InfluxClient::_printCurl(String influxurl, String authvalue, String pointdata)
{
  Serial.println("***** Try this 'curl' command *****");
  Serial.println("curl --request POST \\");
  Serial.print("\""); Serial.print(influxurl); Serial.println("\" \\");
  Serial.print("  --header \"Authorization: "); Serial.print(authvalue); Serial.println("\" \\");
  Serial.println("  --header \"Content-Type: text/plan; charset=utf-8\" \\");
  Serial.println("  --header \"Accept: application/json\" \\");
  Serial.println("  --data-binary '");
  Serial.println(pointdata);
  Serial.println("  '");
  Serial.println("***** END *****");
}
#endif // DEBUG

// Following the Influx API documentation ()
/*
curl --request POST \
"http://localhost:8086/api/v2/write?org=YOUR_ORG&bucket=YOUR_BUCKET&precision=ns" \
  --header "Authorization: Token YOUR_API_TOKEN" \
  --header "Content-Type: text/plain; charset=utf-8" \
  --header "Accept: application/json" \
  --data-binary '
    airSensors,sensor_id=TLM0201 temperature=73.97038159354763,humidity=35.23103248356096,co=0.48445310567793615 1630424257000000000
    airSensors,sensor_id=TLM0202 temperature=75.30007505999716,humidity=35.651929918691714,co=0.5141876544505826 1630424257000000000
    '
*/
int InfluxClient::_httpPOSTRequest(String influxurl, String authvalue, String pointdata)
{
  int httpCode = -1;
  HTTPClient http;

  // There are a variety of ways to initiate an HTTP operation, the easiest of which
  // is just to specify the target server URL.
  http.begin(influxurl);
  http.addHeader("Authorization",authvalue);
  http.addHeader("Content-Type", "text/plain; charset=utf-8"); 
  http.addHeader("Accept","application/json");

  // Now POST the tag and field payload constructed via addTag() and addField()
  // TODO: Check that there is pointdata to write
  Serial.println("Sending:");
  Serial.println(pointdata);
  httpCode = http.POST(pointdata);
  
  // httpCode will be negative on error, but HTTP status might indicate failure
  if (httpCode > 0) {
    // If POST succeeded (status 200), output any response as it might be useful to
    // the calling application and need to do so before closing server connection
    if (httpCode == HTTP_CODE_OK) {
      const String& response = http.getString();
      Serial.println("OK, received response:\n<<");
      Serial.println(response);
      Serial.println(">>");
    }
    // For other HTTP status codes we'll just return httpCode
  } 
  // If HTTP error provide reason associated with HTTP status returned 
  else {
    Serial.println("HTTP POST [" + _server + "] failed, error: " + http.errorToString(httpCode).c_str());
  }

  http.end();  // Closes connection to host begun above
  // Serial.println("closing connection to host"); 
  return(httpCode);
}

/****** InfluxPoint class -- represents a "point" of data to be written to the server ******/
// Constructor
InfluxPoint::InfluxPoint(String measurement) {
  _measurement = measurement;
  _tags = "";
  _fields = "";
  _firstTag = _firstField = true;
}

void InfluxPoint::addTag(String tagkey, String tagvalue)
{
  if(!_firstTag) {
    _tags += ",";
  }
  else {
    _firstTag = false;
  }
  _tags += tagkey + "=" + tagvalue;
}

void InfluxPoint::addField(String fieldkey, String fieldvalue)
{
  if(!_firstField) {
    _fields += ",";
  }
  else {
    _firstField = false;
  }
  _fields += fieldkey + "=" + fieldvalue;
}

void InfluxPoint::clearFields()
{
  _firstField = true;
  _fields = "";
}

void InfluxPoint::clearTags()
{
  _firstTag = true;
  _tags = "";
}