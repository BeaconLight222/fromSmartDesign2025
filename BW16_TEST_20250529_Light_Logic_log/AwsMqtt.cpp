/// @file AwsMqtt.cpp
/// @brief AWS MQTT client implementation for the BW16 project

#include "AwsMqtt.h"
#include "NonvolatileDataManager.h"

#include <PubSubClient.h>
#include <WiFi.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "sys_api.h"

#ifdef __cplusplus
}
#endif

#define ARDUINOJSON_ENABLE_STD_STREAM 0
#include "src/ArduinoJson-7.4.1/ArduinoJson-v7.4.1.h"

WiFiSSLClient wifiClient;
PubSubClient client(wifiClient);

char *thingName;
const char awsPrefix[] = "$aws/things/";
// const char mqttServer[] = "a1e9mdr77wm319-ats.iot.us-east-1.amazonaws.com";
// //got from
// https://us-east-1.console.aws.amazon.com/iot/home?region=us-east-1#/connectdevice
const char mqttServer[] =
    "ado4y320oe6ti-ats.iot.us-east-1.amazonaws.com"; // tad test 20250522
char *clientId;
char *subscribeServerTopic[] = {
    "/command/sub",
    "/deviceConfig/sub",
};

char *subscribeJobsTopic[] = {
    "/jobs/notify",
    // add more later, simple test for now
};

// AWS access state machine
enum JobStates {
  WAIT_FOR_CONNECTION = 0,
  CONNECTED = 1,
  READY_TO_SEND_INIT_JOB_QUERY = 2,
  WAIT_FOR_JOB_QUERY_RESPONSE = 3,
  IDLE = 4,
  REQUEST_JOB_DETAIL = 5,
  WAIT_FOR_JOB_DETAIL = 6,
  DO_THE_JOB = 7,
  DO_THE_OTA_JOB = 8
};

const char *jobStateToString(int state) {
  switch (state) {
  case WAIT_FOR_CONNECTION:
    return "WAIT_FOR_CONNECTION";
  case CONNECTED:
    return "CONNECTED";
  case READY_TO_SEND_INIT_JOB_QUERY:
    return "READY_TO_SEND_INIT_JOB_QUERY";
  case WAIT_FOR_JOB_QUERY_RESPONSE:
    return "WAIT_FOR_JOB_QUERY_RESPONSE";
  case IDLE:
    return "IDLE";
  case REQUEST_JOB_DETAIL:
    return "REQUEST_JOB_DETAIL";
  case WAIT_FOR_JOB_DETAIL:
    return "WAIT_FOR_JOB_DETAIL";
  case DO_THE_JOB:
    return "DO_THE_JOB";
  case DO_THE_OTA_JOB:
    return "DO_THE_OTA_JOB";
  default:
    return "UNKNOWN";
  }
}

int timeoutLimit[] = {
    0,    // WAIT_FOR_CONNECTION
    0,    // CONNECTED
    0,    // READY_TO_SEND_INIT_JOB_QUERY
    5000, // WAIT_FOR_JOB_QUERY_RESPONSE
    0,    // IDLE
    0,    // REQUEST_JOB_DETAIL
    5000, // WAIT_FOR_JOB_DETAIL
    0,    // DO_THE_JOB
    20000 // DO_THE_OTA_JOB
};

// messageDataList = []
// awsJobList = []
// processingJobName = ""

AwsMqtt *awsMqttPtr = NULL;

/// @brief MQTT callback function \n
/// This function processes incoming MQTT messages and routes them to the appropriate handler
/// @param topic The topic of the incoming message
/// @param payload The payload of the incoming message
/// @param length The length of the incoming message
void callback(char *topic, byte *payload, unsigned int length) {
  char buf[3072 + 1]; // payload is not null terminated

  strncpy(buf, (const char *)payload, length);
  buf[length] = '\0';

  Serial.print("Message arrived ");
  Serial.println(topic);
  if (length < 150) {
    Serial.println(buf);
  } else {
    Serial.print("Message too long ");
    for (int i = 0; i < 150; i++) {
      Serial.print(buf[i]);
    }
    Serial.println("...");
  }

  if (awsMqttPtr != NULL) {
    String serverTopicPrefix = String(thingName) + "/";
    if (strncmp(topic, serverTopicPrefix.c_str(), serverTopicPrefix.length()) ==
        0) {
      awsMqttPtr->serverCallback(topic, buf, length);
      return;
    }

    String jobsTopicPrefix = String(awsPrefix) + String(thingName) + "/jobs/";
    String streamTopicPrefix =
        String(awsPrefix) + String(thingName) + "/streams/";
    if (strstr(topic, jobsTopicPrefix.c_str()) != NULL) {
      awsMqttPtr->jobsCallback(topic, buf, length);
      return;
    } else if (strstr(topic, streamTopicPrefix.c_str()) != NULL) {
      awsMqttPtr->jobsCallback(topic, buf, length);
      return;
    }
  }
}

/// @brief Initialize the AWS MQTT client \n
/// The AWS root CA and firmware signing public key are fetched from hard-coded string. \n
/// The client ID and thing name are fetched from the user data area. \n
/// The certificate and private key are fetched from the user data area as well.
void AwsMqtt::begin() {
  awsMqttPtr = this;

  processingJobName = "";
  processingJobContent = "";
  processingOTAStreamName = "";
  processingOTAFileSize = 0;
  processingOTASignature = "";
  processingOTAReadedBytes = 0;
  processingOTARequestedBytes = 0;

  otaProcessor.initCodeSigningCert(
      (char *)NonvolatileDataManager::getFirmwareSignPubKey());

  jobState = WAIT_FOR_CONNECTION;
  prevJobState = jobState;
  jobStateChangeTimeMillis = millis();
  jobsMessages.clear();
  jobsList.clear();

  thingName = NonvolatileDataManager::getThingName();
  clientId = NonvolatileDataManager::getClientId();

  wifiClient.setRootCA(NonvolatileDataManager::getRootCA());
  wifiClient.setClientCertificate(NonvolatileDataManager::getCertificate(),
                                  NonvolatileDataManager::getPrivateKey());

  client.setServer(mqttServer, 8883);
  client.setCallback(callback);
  client.setBufferSize(3072); // from 512
  client.setSocketTimeout(22); //make sure it does not trigger 30s WDT reset

  initOtaJobTimeMessageMillis = 0;
  initOtaJobTimeMessageUnixTime = 0;

  newModeCommand = "";

  memset(scheduleData, 0, sizeof(scheduleData));
  scheduleEnabled = false;
  scheduleTimezoneOffsetMinutes = 0;

  // Allow the hardware to sort itself out
  delay(1500);
}

/// @brief Connect or Reconnect to the AWS MQTT broker after Wifi is connected \n
/// If the connection is successful, it subscribes to the necessary topics
void AwsMqtt::reconnect() {
  static bool wait = false;
  static uint32_t waitStartTime = 0;

  jobState = WAIT_FOR_CONNECTION;

  // Loop until we're reconnected
  while (!client.connected()) {
    if (wait) {
      // Check if 5 seconds have passed
      if (((int32_t)(millis() - waitStartTime)) >= 5000) {
        wait = false; // Reset wait flag
      } else {
        return; // Wait until 5 seconds have passed
      }
    }

    Serial.print("\r\n Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientId)) {
      Serial.println("connected");

      jobState = CONNECTED;

      {
        for (int i = 0;
             i < sizeof(subscribeServerTopic) / sizeof(subscribeServerTopic[0]);
             i++) {
          String serverTopic = String(thingName) + subscribeServerTopic[i];
          client.subscribe(serverTopic.c_str());
          Serial.print("Subscribe :");
          Serial.println(serverTopic.c_str());
        }
      }

      String notifyTopic =
          String(awsPrefix) + String(thingName) + "/jobs/notify";
      client.subscribe(notifyTopic.c_str());
      Serial.print("Subscribe :");
      Serial.println(notifyTopic.c_str());

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Start waiting
      wait = true;
      waitStartTime = millis();
    }
  }
}


/// @brief Main loop for the AWS MQTT client, called by Arduino loop \n
/// This function checks the connection status, processes incoming messages, and manages the job state machine 
void AwsMqtt::loop() {
  if (!client.connected()) {
    reconnect();
  }
  // Serial.print("LOOP");
  client.loop(); // ARDUINO_MBEDTLS_DEBUG_LEVEL
  // Serial.println("...");

  // job statemachine

  int32_t passedMillis = (int32_t)(millis() - jobStateChangeTimeMillis);
  switch (jobState) {
  case WAIT_FOR_CONNECTION:
    break;
  case CONNECTED: {
    String getAcceptedTopic =
        String(awsPrefix) + String(thingName) + "/jobs/get/accepted";
    client.subscribe(getAcceptedTopic.c_str());
    Serial.print("Subscribe :");
    Serial.println(getAcceptedTopic.c_str());
    jobState = READY_TO_SEND_INIT_JOB_QUERY;
  } break;
  case READY_TO_SEND_INIT_JOB_QUERY: {
    String getJobTopic = String(awsPrefix) + String(thingName) + "/jobs/get";
    client.publish(getJobTopic.c_str(), "{}");
    Serial.print("Publish :");
    Serial.println(getJobTopic.c_str());
    jobState = WAIT_FOR_JOB_QUERY_RESPONSE;
  } break;
  case WAIT_FOR_JOB_QUERY_RESPONSE: {
    // iterate through the message data list and check if there is a job message
    // iterate backward to avoid invalidating the iterator
    int jobFoundCount = -1;
    for (int i = (int)(jobsMessages.size()) - 1; i >= 0; i--) {
      if (strstr(jobsMessages[i].topic.c_str(), "/jobs/get/accepted") != NULL) {
        JsonDocument doc;
        DeserializationError error =
            deserializeJson(doc, jobsMessages[i].payload.c_str(),
                            jobsMessages[i].payload.length());
        if (!error) {
          if (doc["timestamp"].is<int>()) {
            initOtaJobTimeMessageUnixTime = doc["timestamp"];
            initOtaJobTimeMessageMillis = millis();
          }
          if (doc["inProgressJobs"].is<JsonArray>()) {
            if (jobFoundCount < 0) {
              jobFoundCount = 0;
            }
            JsonArray inProgressJobs = doc["inProgressJobs"];
            for (JsonVariant job : inProgressJobs) {
              if (job.is<JsonObject>()) {
                if (job["jobId"].is<const char *>()) {
                  const char *jobId = job["jobId"];
                  jobsList.push_back(String(jobId));
                  jobFoundCount++;
                }
              }
            }
          }
          if (doc["queuedJobs"].is<JsonArray>()) {
            if (jobFoundCount < 0) {
              jobFoundCount = 0;
            }
            JsonArray queuedJobs = doc["queuedJobs"];
            for (JsonVariant job : queuedJobs) {
              if (job.is<JsonObject>()) {
                if (job["jobId"].is<const char *>()) {
                  const char *jobId = job["jobId"];
                  jobsList.push_back(String(jobId));
                  jobFoundCount++;
                }
              }
            }
          }
        }
        // remove the message from the list
        jobsMessages.erase(jobsMessages.begin() + i);
      }
    }

    if (jobFoundCount >= 0) {
      String getAcceptedTopic =
          String(awsPrefix) + String(thingName) + "/jobs/get/accepted";
      client.unsubscribe(getAcceptedTopic.c_str());
      Serial.print("Unsubscribe :");
      Serial.println(getAcceptedTopic.c_str());
      jobState = IDLE;
    }

    // check if the timeout has passed
    if (passedMillis > timeoutLimit[WAIT_FOR_JOB_QUERY_RESPONSE]) {
      Serial.println("Timeout waiting for job query response");
      jobState = READY_TO_SEND_INIT_JOB_QUERY;
    }

  } break;
  case IDLE: {
    // check if there is a job in the jobsList
    if (jobsList.size() > 0) {
      // get the first job in the list
      Serial.print("we have jobs: ");
      Serial.println(jobsList.size());
      processingJobName = jobsList[0];
      jobsList.erase(jobsList.begin());
      Serial.print("Go to process job: ");
      Serial.println(processingJobName);
      Serial.print("left jobs: ");
      Serial.println(jobsList.size());
      jobState = REQUEST_JOB_DETAIL;
    }
  }

  break;

  case REQUEST_JOB_DETAIL: {
    // subscribe to the job detail topic
    String getAcceptedTopic = String(awsPrefix) + String(thingName) + "/jobs/" +
                              processingJobName + "/get/accepted";
    client.subscribe(getAcceptedTopic.c_str());
    Serial.print("Subscribe :");
    Serial.println(getAcceptedTopic.c_str());
    // publish the job detail request
    String getJobTopic = String(awsPrefix) + String(thingName) + "/jobs/" +
                         processingJobName + "/get";
    client.publish(getJobTopic.c_str(), "{}");
    Serial.print("Publish :");
    Serial.println(getJobTopic.c_str());
    jobState = WAIT_FOR_JOB_DETAIL;
  } break;
  case WAIT_FOR_JOB_DETAIL: {
    // iterate through the message data list and check if there is a job message
    // iterate backward to avoid invalidating the iterator
    String getAcceptedTopic = String(awsPrefix) + String(thingName) + "/jobs/" +
                              processingJobName + "/get/accepted";
    for (int i = (int)(jobsMessages.size()) - 1; i >= 0; i--) {
      if (strstr(jobsMessages[i].topic.c_str(), getAcceptedTopic.c_str()) !=
          NULL) {
        JsonDocument doc;
        DeserializationError error =
            deserializeJson(doc, jobsMessages[i].payload.c_str(),
                            jobsMessages[i].payload.length());
        if (!error) {
          JsonObject execution = doc["execution"];
          if (execution) {
            JsonObject jobDocument = execution["jobDocument"];
            if (jobDocument) {
              serializeJson(jobDocument, processingJobContent);
              Serial.print("Job document: ");
              serializeJsonPretty(jobDocument, Serial);
              Serial.println();
            }
          }
        }
        // remove the message from the list
        jobsMessages.erase(jobsMessages.begin() + i);
        String updateTopic = String(awsPrefix) + String(thingName) + "/jobs/" +
                             processingJobName + "/update";
        client.publish(updateTopic.c_str(), "{\"status\": \"IN_PROGRESS\"}");
        Serial.print("Publish :");
        Serial.println(updateTopic.c_str());
        client.unsubscribe(getAcceptedTopic.c_str());
        Serial.print("Unsubscribe :");
        Serial.println(getAcceptedTopic.c_str());
        jobState = DO_THE_JOB;
      }
    }

    // check if the timeout has passed
    if (passedMillis > timeoutLimit[WAIT_FOR_JOB_DETAIL]) {
      Serial.println("Timeout waiting for job detail response");
      jobState = IDLE;
    }
  } break;

  case DO_THE_JOB: {
    // pretend to do the job
    Serial.print("Doing the job: ");
    Serial.println(processingJobName);
    Serial.print("Job content: ");
    Serial.println(processingJobContent);
    // regular job sample:
    //  {"version": "1.0", "steps": [{"action": {"name": "Reboot", "type":
    //  "runHandler", "input": {"handler": "reboot.sh", "path": ""},
    //  "runAsUser": ""}}]}
    // OTA job sample:
    //  {"afr_ota": {"protocols": ["MQTT"], "streamname":
    //  "AFR_OTA-6f19676b-e00a-495e-95ae-3800f36a2050", "files": [{"filepath":
    //  "/", "filesize": 2574, "fileid": 0, "certfile": "/test.pem",
    //  "sig-sha256-ecdsa":
    //  "MEUCIQDeMuJNNznBaZjgV2ldHZKbW6Zn6/YFfKvMe4gJmWwTQQIgfK5paVfXwDQit8kLucTLcj002xPDVH0q9Bo8Am8Z3/Q="}]}}

    // parse the job content
    JsonDocument jobContentDoc;
    DeserializationError error =
        deserializeJson(jobContentDoc, processingJobContent.c_str(),
                        processingJobContent.length());
    if (!error) {
      // check if the job content has "afr_ota" key
      if (jobContentDoc["afr_ota"].is<JsonObject>()) {
        JsonObject afrOta = jobContentDoc["afr_ota"];
        if (afrOta["protocols"].is<JsonArray>()) {
          JsonArray protocols = afrOta["protocols"];
          for (JsonVariant protocol : protocols) {
            if (protocol.is<const char *>()) {
              const char *protocolStr = protocol;
              if (strcmp(protocolStr, "MQTT") == 0) {
                Serial.println("MQTT is in the protocols");
                if (afrOta["streamname"].is<const char *>()) {
                  processingOTAStreamName =
                      afrOta["streamname"].as<const char *>();
                  Serial.print("Processing OTA stream name: ");
                  Serial.println(processingOTAStreamName);
                }
                if (afrOta["files"].is<JsonArray>()) {
                  JsonArray files = afrOta["files"];
                  for (JsonVariant file : files) {
                    if (file.is<JsonObject>()) {
                      JsonObject fileObj = file.as<JsonObject>();
                      if (fileObj["filesize"].is<int>()) {
                        processingOTAFileSize = fileObj["filesize"];
                        Serial.print("Processing OTA file size: ");
                        Serial.println(processingOTAFileSize);
                      }
                      if (fileObj["sig-sha256-ecdsa"].is<const char *>()) {
                        processingOTASignature =
                            fileObj["sig-sha256-ecdsa"].as<const char *>();
                        Serial.print("Processing OTA signature: ");
                        Serial.println(processingOTASignature);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    // check if the file size > 0 and the streamename, signature is not empty
    if (processingOTAFileSize > 0 && processingOTAStreamName.length() > 0 &&
        processingOTASignature.length() > 0) {
      // OTA job
      Serial.println("OTA job accepted");
      processingOTAReadedBytes = 0;
      processingOTARequestedBytes = 0;
      jobState = DO_THE_OTA_JOB;
    }
    if (jobState != DO_THE_OTA_JOB) {
      // regular job
      Serial.println("Regular job accepted");
      Serial.println("Pretending to do the job.....");
      // publish the job update
      String updateTopic = String(awsPrefix) + String(thingName) + "/jobs/" +
                           processingJobName + "/update";
      client.publish(updateTopic.c_str(), "{\"status\": \"SUCCEEDED\"}");
      Serial.print("Publish :");
      Serial.println(updateTopic.c_str());
      String getAcceptedTopic = String(awsPrefix) + String(thingName) +
                                "/jobs/" + processingJobName + "/get/accepted";
      client.unsubscribe(getAcceptedTopic.c_str());
      Serial.print("Unsubscribe :");
      Serial.println(getAcceptedTopic.c_str());
      processingJobName = "";
      processingJobContent = "";
      jobState = IDLE;
    }
  } break;

  case DO_THE_OTA_JOB: {
    int endOta = 0;
    int OtaSuccess = 0;
    int requestBytes = 2048;

    if ((processingOTAReadedBytes == 0) && (processingOTARequestedBytes == 0)) {
      Serial.println("OTA job start to request");
      otaProcessor.OtaProcessInit();
      String jsonDataTopic = String(awsPrefix) + String(thingName) +
                             "/streams/" + processingOTAStreamName +
                             "/data/json";
      client.subscribe(jsonDataTopic.c_str());
      Serial.print("OTA job subscribe topic: ");
      Serial.println(jsonDataTopic.c_str());
      String jsonDataRejectedTopic = String(awsPrefix) + String(thingName) +
                                     "/streams/" + processingOTAStreamName +
                                     "/rejected/json";
      client.subscribe(jsonDataRejectedTopic.c_str());
      Serial.print("OTA job subscribe topic: ");
      Serial.println(jsonDataRejectedTopic.c_str());
    }

    if (processingOTAReadedBytes < processingOTARequestedBytes) {
      String jsonDataTopic = String(awsPrefix) + String(thingName) +
                             "/streams/" + processingOTAStreamName +
                             "/data/json";
      String jsonDataRejectedTopic = String(awsPrefix) + String(thingName) +
                                     "/streams/" + processingOTAStreamName +
                                     "/rejected/json";
      // iterate through messageDataList backwards to avoid index error
      for (int i = (int)(jobsMessages.size()) - 1; i >= 0; i--) {
        if (strstr(jobsMessages[i].topic.c_str(), jsonDataTopic.c_str()) !=
            NULL) {
          JsonDocument doc;
          DeserializationError error =
              deserializeJson(doc, jobsMessages[i].payload.c_str(),
                              jobsMessages[i].payload.length());
          if (!error) {
            int dataLen = 0;
            char *dataPayload = NULL;
            int blockIndex = -1;
            if (doc["l"].is<int>()) {
              dataLen = doc["l"];
            }
            if (doc["p"].is<const char *>()) {
              dataPayload = (char *)doc["p"].as<const char *>();
            }
            if (doc["i"].is<int>()) {
              blockIndex = doc["i"];
            }
            if (dataLen > 0 && dataPayload != NULL && blockIndex >= 0) {
              otaProcessor.OtaProcessDataChunkBase64(
                  (char *)dataPayload, dataLen, blockIndex * requestBytes);
            } else {
              Serial.println("OTA job data error!!!!!!!");
              endOta = 1;
            }

            if (dataLen > 0) {
              processingOTAReadedBytes += dataLen;
              if (processingOTAReadedBytes != processingOTARequestedBytes) {
                Serial.print("OTA job readed bytes: ");
                Serial.println(processingOTAReadedBytes);
                Serial.print("OTA job requested bytes: ");
                Serial.println(processingOTARequestedBytes);
                // OTA job error
                Serial.println("OTA job error!!!!!!!");
                endOta = 1;
              } else {
                Serial.print("OTA job readed bytes: ");
                Serial.println(processingOTAReadedBytes);
                Serial.print("OTA job requested bytes: ");
                Serial.println(processingOTARequestedBytes);
                // process the data
              }
            }
          }
          // pop the message from the list
          jobsMessages.erase(jobsMessages.begin() + i);
        } else if (strstr(jobsMessages[i].topic.c_str(),
                          jsonDataRejectedTopic.c_str()) != NULL) {
          Serial.print("OTA job rejected: ");
          Serial.println(jobsMessages[i].payload.c_str());
          endOta = 1;
          // pop the message from the list
          jobsMessages.erase(jobsMessages.begin() + i);
        }
      }
    } else if (processingOTAReadedBytes == processingOTARequestedBytes) {
      if (processingOTAReadedBytes == processingOTAFileSize) {
        Serial.println("OTA job finished");
        otaProcessor.OtaWriteLeftoverData();
        endOta = 1;
        if (otaProcessor.OtaProcessVerifySignature(
                (char *)processingOTASignature.c_str()) == 0) {
          Serial.println("OTA job signature is valid.");
          otaProcessor.OtaEnableOTA();
          OtaSuccess = 1;
        } else {
          Serial.println("OTA job signature is invalid.");
          OtaSuccess = 0;
        }
      } else {
        String jsonDataRequestTopic = String(awsPrefix) + String(thingName) +
                                      "/streams/" + processingOTAStreamName +
                                      "/get/json";
        // create the GetStream request
        // https://docs.aws.amazon.com/iot/latest/developerguide/mqtt-based-file-delivery-in-devices.html
        int idOfStream = 0;
        int payloadLength = requestBytes;
        int offset = processingOTAReadedBytes / requestBytes;
        int numberOfBlocks = 1;
        String getStreamRequest = "{\"f\": " + String(idOfStream) +
                                  ", \"l\": " + String(payloadLength) +
                                  ", \"o\": " + String(offset) +
                                  ", \"n\": " + String(numberOfBlocks) + "}";
        Serial.print("OTA job request: ");
        Serial.println(getStreamRequest);
        client.publish(jsonDataRequestTopic.c_str(), getStreamRequest.c_str());
        int actualExpectedBytes = requestBytes;
        int leftoverBytes = processingOTAFileSize - processingOTAReadedBytes;
        if (leftoverBytes < requestBytes) {
          actualExpectedBytes = leftoverBytes;
        }
        processingOTARequestedBytes =
            processingOTAReadedBytes + actualExpectedBytes;
        jobStateChangeTimeMillis = millis();
      }
    } else {
      Serial.println("OTA job error!!!!!!!");
      endOta = 1;
    }

    if (passedMillis > timeoutLimit[DO_THE_OTA_JOB]) {
      Serial.println("Timeout waiting for job OTA response");
      if (endOta == 0) {
        endOta = 1;
      }
    }

    if (endOta != 0) {
      String jsonDataTopic = String(awsPrefix) + String(thingName) +
                             "/streams/" + processingOTAStreamName +
                             "/data/json";
      client.unsubscribe(jsonDataTopic.c_str());
      Serial.print("OTA job unsubscribe topic: ");
      Serial.println(jsonDataTopic.c_str());
      String jsonDataRejectedTopic = String(awsPrefix) + String(thingName) +
                                     "/streams/" + processingOTAStreamName +
                                     "/rejected/json";
      client.unsubscribe(jsonDataRejectedTopic.c_str());
      Serial.print("OTA job unsubscribe topic: ");
      Serial.println(jsonDataRejectedTopic.c_str());
      String jobUpdateTopic = String(awsPrefix) + String(thingName) + "/jobs/" +
                              processingJobName + "/update";
      client.unsubscribe(jobUpdateTopic.c_str());
      Serial.print("OTA job unsubscribe topic: ");
      Serial.println(jobUpdateTopic.c_str());
      if (OtaSuccess != 0) {
        client.publish(jobUpdateTopic.c_str(), "{\"status\": \"SUCCEEDED\"}");
        Serial.println("update OTA job status to SUCCEEDED");
        delay(2000);
        sys_reset();
      } else {
        client.publish(jobUpdateTopic.c_str(), "{\"status\": \"FAILED\"}");
        Serial.println("update OTA job status to FAILED");
        delay(2000);
        sys_reset();
      }

      processingJobName = "";
      processingOTAStreamName = "";
      processingOTAFileSize = 0;
      processingOTASignature = "";
      processingOTAReadedBytes = 0;
      processingOTARequestedBytes = 0;

      jobState = IDLE;
    }
  } break;
  default:
    break;
  }

  if (jobState != prevJobState) {
    jobStateChangeTimeMillis = millis();
    Serial.print("Job state changed from ");
    Serial.print(jobStateToString(prevJobState));
    Serial.print(" to ");
    Serial.print(jobStateToString(jobState));
    Serial.print(" at ");
    Serial.print((int)jobStateChangeTimeMillis);
    Serial.println(" ms");
    prevJobState = jobState;
  }

  // general dealing with jobsMessages

  // there should be no jobsMessages in the list, but just in case
  if (jobsMessages.size() > 0) {
    // deal with one job, just for test, set it to succeeded
    Serial.println("leftover job messages");
    for (int i = 0; i < (int)(jobsMessages.size()); i++) {
      Serial.println(jobsMessages[i].topic);
    }

    // iterate backward to avoid invalidating the iterator
    for (int i = (int)(jobsMessages.size()) - 1; i >= 0; i--) {
      if (strstr(jobsMessages[i].topic.c_str(), "/jobs/notify") != NULL) {
        JsonDocument doc;
        DeserializationError error =
            deserializeJson(doc, jobsMessages[i].payload.c_str(),
                            jobsMessages[i].payload.length());
        if (!error) {
          if (doc["jobs"].is<JsonObject>()) {
            if (doc["jobs"]["QUEUED"].is<JsonArray>()) {
              JsonArray queuedJobs = doc["jobs"]["QUEUED"];
              Serial.print("Number of queued jobs: ");
              Serial.println(queuedJobs.size());
              for (JsonVariant job : queuedJobs) {
                if (job.is<JsonObject>()) {
                  if (job["jobId"].is<const char *>()) {
                    const char *jobId = job["jobId"];
                    Serial.print("Queue Job ID: ");
                    Serial.println(jobId);
                    jobsList.push_back(String(jobId));
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  jobsMessages.clear();
}

/// @brief Callback function for server MQTT messages \n
/// This function processes incoming server commands for Node.js application \n
/// The mode command is expected to be in the format of {"mode":"UV_MODE_MANUAL"}
/// @param topic The topic of the incoming message
/// @param payload The payload of the incoming message
/// @param length The length of the incoming message
void AwsMqtt::serverCallback(char *topic, char *payload, unsigned int length) {
  if ((strstr(topic, "/command/sub") != NULL)) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      return;
    }

    // sample:
    //  {"lampEnable":true,"detectionMode":1,"scheduleEnable":false,"factoryReset":false,"wifiReset":false,"exposureLimit":null,"source":"api"}
    if (doc["detectionMode"].is<int>()) {
      int mode = doc["detectionMode"];
      Serial.print("Server Command mode: ");
      Serial.println(mode);
      if (mode == 0) {
        // this is smart mode
        newModeCommand = "UV_MODE_SMART";
      }else if (mode == 1) {
        //this is manual mode, but the On/Off state is in lampEnable
        if (doc["lampEnable"].is<bool>()) {
          bool lampEnable = doc["lampEnable"];
          if (lampEnable) {
            newModeCommand = "UV_MODE_MANUAL";
          } else {
            newModeCommand = "UV_MODE_OFF";
          }
        }
      }
    }
    //check wifiReset, on 20250929, the ResetWifi button in APP does not rescan properly, need to remove beacon and add again.
    if (doc["wifiReset"].is<bool>()) {
      bool wifiReset = doc["wifiReset"];
      if (wifiReset) {
        Serial.println("Server Command wifiReset");
        newModeCommand = "RESET_WIFI";
      }
    }
    if (doc["scheduleEnable"].is<bool>()) {
      scheduleEnabled = doc["scheduleEnable"];
      Serial.print("Server Config scheduleEnabled: ");
      Serial.println(scheduleEnabled);
    }
  }else if ((strstr(topic, "/deviceConfig/sub") != NULL)) {

    //in V1 code, there is code
    //const time_t current_rtc_time = state->rtc_time + (int64_t)(shared.config.time_zone * 60);
    //const struct tm ts = *localtime(&current_rtc_time);
    //And there is no setenv("TZ" or tzset();. So any localtime will be direct convert without timezone.
    //In schedule.c
    //get_day_time(shared.rtc_time, &day, &hour, &min);
    //const size_t slot_index = find_slot_index(day, hour, min);
    //And get_day_time is a UTC time, find_slot_index does not do timezone adjustment. V1 don't have timezone adjustment.
    //So in this code, we will ignore time zone for schedule to match V1 code.

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      return;
    }

    if (doc["rtcUnixTime"].is<int>()) {
      int rtcUnixTime = doc["rtcUnixTime"];
      Serial.print("Server Config rtcUnixTime: ");
      Serial.println(rtcUnixTime);
    }
    if (doc["schedule"].is<JsonArray>()) {
      JsonArray scheduleArr = doc["schedule"];
      int idx = 0;
      for (JsonVariant v : scheduleArr) {
        int data = v.as<int>();
        int byteIndex = idx / 8;
        int bitIndex = idx % 8;
        if (byteIndex < sizeof(scheduleData)) {
          if (data) {
            scheduleData[byteIndex] |= (1 << bitIndex);
          } else {
            scheduleData[byteIndex] &= ~(1 << bitIndex);
          }
        }
        idx++;
      }
      Serial.print("Server Config schedule updated: ");
      for (int i = 0; i < sizeof(scheduleData); i++) {
        int data = scheduleData[i];
        if (data < 0x10){
          Serial.print("0");
        }
        Serial.print(scheduleData[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
    }
    if (doc["timeZoneOffsetMinutes"].is<int>()) {
      scheduleTimezoneOffsetMinutes = doc["timeZoneOffsetMinutes"];
      Serial.print("Server Config timeZoneOffsetMinutes: ");
      Serial.println(scheduleTimezoneOffsetMinutes);
    }
  }
}

/// @brief Callback function for jobs MQTT messages \n
/// This function processes incoming job messages and adds them to the jobsMessages list \n
/// The topics should be in the format of $aws/things/THINGNAME/jobs/ \n
/// The jobs are implemented by AWS IoT Jobs service. The FreeRTOS OTA job is also a job
/// @param topic The topic of the incoming message
/// @param payload The payload of the incoming message
/// @param length The length of the incoming message
void AwsMqtt::jobsCallback(char *topic, char *payload, unsigned int length) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    return;
  }

  // add the topic and payload to the messageDataList
  MQTTMessageData messageData;
  messageData.topic = String(topic);
  messageData.payload = String(payload);
  jobsMessages.push_back(messageData);
}

/// @brief Function for logging sensor data to the server
/// @param payload The payload of the logging message
void AwsMqtt::logSensorToServer(char *payload) {
  String debugTopic = String(thingName) + "/logging/sub";
  client.publish(debugTopic.c_str(), payload);
}

void AwsMqtt::sendTelemetryToServer(char *payload) {
  String telemetryTopic = String(thingName) + "/telemetry/pub";
  client.publish(telemetryTopic.c_str(), payload);
}