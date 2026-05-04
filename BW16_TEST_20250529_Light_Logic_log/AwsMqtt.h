#ifndef AWS_MQTT_H
#define AWS_MQTT_H

#include <Arduino.h>
#undef max
#undef min
#include "OtaProcessor.h"
#include <vector>

struct MQTTMessageData {
  String topic;
  String payload;
};

class AwsMqtt {
public:
  // AwsMqtt(const char* mqttServer, int mqttPort, const char* clientId, const
  // char* rootCA, const char* privateKey, const char* certificate);
  void begin();
  void loop();
  void reconnect();
  void serverCallback(char *topic, char *payload, unsigned int length);
  void jobsCallback(char *topic, char *payload, unsigned int length);
  void logSensorToServer(char *payload);
  void sendTelemetryToServer(char *payload);
  // bool publish(const char* topic, const char* payload);
  // bool subscribe(const char* topic, std::function<void(char*, uint8_t*,
  // unsigned int)> callback);

  // std::vector<String> jobsToDo;
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

  uint32_t initOtaJobTimeMessageMillis;
  uint32_t initOtaJobTimeMessageUnixTime;

  /// @brief newModeCommand pass the mode command from the server to the light control logic in the main loop
  String newModeCommand;

  uint8_t scheduleData[42]; //42*8bits = 336bits, enough for 7 days schedule with 48 half-hour slots per day
  bool scheduleEnabled;
  int scheduleTimezoneOffsetMinutes;

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

#endif // AWS_MQTT_H