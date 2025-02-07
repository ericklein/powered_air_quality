#ifndef INFLUXCLIENT_H
#define INFLUXCLIENT_H

#include "Arduino.h"

class InfluxPoint {
  friend class InfluxClient;
  public:
    InfluxPoint(String measurement);
    void addTag(String tagkey, String tagvalue);
    void addField(String fieldkey, String fieldvalue);
    void clearFields();
    void clearTags();
  private:
    String _measurement;
    String _tags;
    boolean _firstTag;
    String _fields;
    boolean _firstField;
};

class InfluxClient {
  public:
    InfluxClient(String server, String org, String bucket, String apitoken);
    void writePoint(InfluxPoint point);
  private:
    int _httpPOSTRequest(String influxurl, String authvalue, String pointdata);
    void _printCurl(String influxurl, String authvalue, String pointdata);
    String _server;
    String _org;
    String _bucket;
    String _apitoken;
};

#endif // INFLUXCLIENT_H