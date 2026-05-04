#ifndef AWS_MQTT_H
#define AWS_MQTT_H

#include <Arduino.h>
#include <vector>
#include "OtaProcessor.h"

struct MQTTMessageData {
    String topic;
    String payload;
};

class AwsMqtt {
public:
  // AwsMqtt(const char* mqttServer, int mqttPort, const char* clientId, const char* rootCA, const char* privateKey, const char* certificate);
  void begin();
  void loop();
  void reconnect();
  void shadowCallback(char* topic, char* payload, unsigned int length);
  void jobsCallback(char* topic, char* payload, unsigned int length);
  // bool publish(const char* topic, const char* payload);
  // bool subscribe(const char* topic, std::function<void(char*, uint8_t*, unsigned int)> callback);

  //std::vector<String> jobsToDo;
  String processingJobName;
  String processingJobContent;
  String processingOTAStreamName;
  int processingOTAFileSize;
  String processingOTASignature;
  int processingOTAReadedBytes;
  int processingOTARequestedBytes;
  int jobState;
  int prevJobState;
  uint32_t jobStateChangeTimeMillis;
  std::vector<MQTTMessageData> jobsMessages;
  std::vector<String> jobsList;

  OtaProcessor otaProcessor;

private:
  // const char* mqttServer;
  // int mqttPort;
  // const char* clientId;
  // const char* rootCA;
  // const char* privateKey;
  // const char* certificate;

  // WiFiClientSecure wifiClient;
  // PubSubClient mqttClient;
};

#endif  // AWS_MQTT_H