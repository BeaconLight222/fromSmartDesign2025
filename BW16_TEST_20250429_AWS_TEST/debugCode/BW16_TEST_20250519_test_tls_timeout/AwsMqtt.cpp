#include "AwsMqtt.h"
#include "NonvolatileDataManager.h"

#include <WiFi.h>
#include <PubSubClient.h>

#define ARDUINOJSON_ENABLE_STD_STREAM 0
#include "src/ArduinoJson-7.4.1/ArduinoJson-v7.4.1.h"

WiFiSSLClient wifiClient;
PubSubClient client(wifiClient);

#define THING_NAME "amebaDevBoard"

char* thingName;
char* awsPrefix = "$aws/things/";
char mqttServer[] = "a1e9mdr77wm319-ats.iot.us-east-1.amazonaws.com";  //got from https://us-east-1.console.aws.amazon.com/iot/home?region=us-east-1#/connectdevice
char* clientId;
char publishPayload[MQTT_MAX_PACKET_SIZE];
char* subscribeShadowTopic[] = {
  "/shadow/update/accepted",
  "/shadow/update/rejected",
  "/shadow/update/delta",
  "/shadow/get/accepted",
  "/shadow/get/rejected"
};

char* subscribeJobsTopic[] = {
  "/jobs/notify",
  //add more later, simple test for now
};

//AWS access state machine
enum JobStates {
  WAIT_FOR_CONNECTION = 0,
  CONNECTED = 1,
  READY_TO_SEND_INIT_JOB_QUERY = 2,
  WAIT_FOR_JOB_QUERY_RESPONSE = 3,
  IDLE = 4,
  REQUEST_JOB_DETAIL = 5,
  WAIT_FOR_JOB_DETAIL = 6,
  DO_THE_JOB = 7
};

int timeoutLimit[] = {
  0,  // WAIT_FOR_CONNECTION
  0,  // CONNECTED
  0,  // READY_TO_SEND_INIT_JOB_QUERY
  5000,  // WAIT_FOR_JOB_QUERY_RESPONSE
  0,  // IDLE
  0,  // REQUEST_JOB_DETAIL
  5000,  // WAIT_FOR_JOB_DETAIL
  0   // DO_THE_JOB
};

// messageDataList = []
// awsJobList = []
// processingJobName = ""




int led_pin = 10;
int led_state = 1;

void updateLedState(int desired_led_state) {
  // in MQTT test client
  // $aws/things/amebaDevBoard/shadow/update
  // send
  // {"state":{"reported":{"led":0}}}

  //printf("change led_state to %d\r\n", desired_led_state);
  led_state = desired_led_state;
  digitalWrite(led_pin, led_state);

  //   sprintf(publishPayload, "{\"state\":{\"reported\":{\"led\":%d}},\"clientToken\":\"%s\"}",
  //           led_state,
  //           clientId);
  //   client.publish(publishTopic, publishPayload);
  //   printf("Publish [%s] %s\r\n", publishTopic, publishPayload);
}

AwsMqtt* awsMqttPtr = NULL;

void callback(char* topic, byte* payload, unsigned int length) {
  char buf[MQTT_MAX_PACKET_SIZE];

  strncpy(buf, (const char*)payload, length);
  buf[length] = '\0';
  //printf("Message arrived [%s] %s\r\n", topic, buf);

  if (awsMqttPtr != NULL) {
    String shadowTopicPrefix = String(awsPrefix) + String(thingName) + "/shadow/";
    if (strstr(topic, shadowTopicPrefix.c_str()) != NULL) {
      awsMqttPtr->shadowCallback(topic, buf, length);
      return;
    }

    String jobsTopicPrefix = String(awsPrefix) + String(thingName) + "/jobs/";
    if (strstr(topic, jobsTopicPrefix.c_str()) != NULL) {
      awsMqttPtr->jobsCallback(topic, buf, length);
      return;
    }
  }
}

void AwsMqtt::begin() {
  awsMqttPtr = this;

  processingJobName = "";
  jobState = WAIT_FOR_CONNECTION;
  prevJobState = jobState;
  jobStateChangeTimeMillis = millis();
  jobsMessages.clear();
  jobsList.clear();

  thingName = NonvolatileDataManager::getThingName();
  clientId = NonvolatileDataManager::getClientId();

  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, led_state);

  wifiClient.setRootCA(NonvolatileDataManager::getRootCA());
  wifiClient.setClientCertificate(NonvolatileDataManager::getCertificate(),
                                  NonvolatileDataManager::getPrivateKey());

  client.setServer(mqttServer, 8883);
  client.setCallback(callback);
  client.setBufferSize(3072);  // from 512

  // Allow the hardware to sort itself out
  delay(1500);
}

void AwsMqtt::reconnect() {
  static bool wait = false;
  static uint32_t waitStartTime = 0;

  jobState = WAIT_FOR_CONNECTION;

  // Loop until we're reconnected
  while (!client.connected()) {
    if (wait) {
      // Check if 5 seconds have passed
      if (((int32_t)(millis() - waitStartTime)) >= 5000) {
        wait = false;  // Reset wait flag
      } else {
        return;  // Wait until 5 seconds have passed
      }
    }

    Serial.print("\r\n Attempting MQTT connection...");
    // trigger result = _client->connect(this->domain, this->port);
    //ret = ssldrv.startClient(&sslclient, ip, port, rootCABuff, cli_cert, cli_key, NULL, NULL, _sni_hostname);
    //int start_ssl_client(sslclient_context *ssl_client, uint32_t ipAddress, uint32_t port, unsigned char* rootCABuff, unsigned char* cli_cert, unsigned char* cli_key, unsigned char* pskIdent, unsigned char* psKey, char* SNI_hostname) {
    //(mbedtls_x509_crt_parse(cacert, rootCABuff, (strlen((char*)rootCABuff)) + 1) != 0) take 2K
    //if ((mbedtls_ssl_setup(ssl_client->ssl, ssl_client->conf)) != 0) { //take 36K (IO buffer 8.8K)
    //ret = mbedtls_ssl_handshake(ssl_client->ssl); take 16K
    //mbedtls_pk_free(_clikey_rsa); mbedtls_x509_crt_free(_cli_crt);  mbedtls_x509_crt_free(cacert); recover 8K
    //

    // Attempt to connect
    if (client.connect(clientId)) {
      Serial.println("connected");

      jobState = CONNECTED;

      //if we do not do any subscribe or publish here, the client.loop() takes 100s for the first time
      // String notifyTopic = String(awsPrefix) + String(thingName) + "/jobs/notify";
      // client.subscribe(notifyTopic.c_str());
      // Serial.print("Subscribe :");
      // Serial.println(notifyTopic.c_str());

      // for (int i = 0; i < (int)(sizeof(subscribeShadowTopic) / sizeof(subscribeShadowTopic[0])); i++) {
      //   String subscribeTopic = String(awsPrefix) + String(thingName) + subscribeShadowTopic[i];
      //   //printf("Subscribe [%s]\r\n", subscribeTopic.c_str());
      //   client.subscribe(subscribeTopic.c_str());  // PubSubClient uses writeString to copy the string, it is OK to provide temp one
      // }

      // for (int i = 0; i < (int)(sizeof(subscribeJobsTopic) / sizeof(subscribeJobsTopic[0])); i++) {
      //   String subscribeTopic = String(awsPrefix) + String(thingName) + subscribeJobsTopic[i];
      //   //printf("Subscribe [%s]\r\n", subscribeTopic.c_str());
      //   client.subscribe(subscribeTopic.c_str());  // PubSubClient uses writeString to copy the string, it is OK to provide temp one
      // }

      // JsonDocument doc;
      // //for some reason if we use doc["state"]["reported"]["led"] = led_state; there will only be 1 letter s r l
      // doc[(char*)"state"][(char*)"reported"][(char*)"led"] = led_state;
      // doc[(char*)"clientToken"] = clientId;
      // serializeJson(doc, publishPayload, sizeof(publishPayload));

      // String publishTopicStr = String(awsPrefix) + String(thingName) + "/shadow/update";
      // client.publish(publishTopicStr.c_str(), publishPayload);
      // printf("Publish [%s] %s\r\n", publishTopicStr.c_str(), publishPayload);

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

void AwsMqtt::loop() {
  if (!client.connected()) {
    reconnect();
  }
Serial.print("looping...");
  client.loop();//ARDUINO_MBEDTLS_DEBUG_LEVEL
Serial.println("done...");

  //job statemachine

  int32_t passedMillis = (int32_t)(millis() - jobStateChangeTimeMillis);
  switch (jobState){
    case WAIT_FOR_CONNECTION:
      break;
    case CONNECTED:
      {
        String getAcceptedTopic = String(awsPrefix) + String(thingName) + "/jobs/get/accepted";
        client.subscribe(getAcceptedTopic.c_str());
        Serial.print("Subscribe :");
        Serial.println(getAcceptedTopic.c_str());
        jobState = READY_TO_SEND_INIT_JOB_QUERY;
      }
      break;
    case READY_TO_SEND_INIT_JOB_QUERY:
      {
        String getJobTopic = String(awsPrefix) + String(thingName) + "/jobs/get";
        client.publish(getJobTopic.c_str(), "{}");
        Serial.print("Publish :");
        Serial.println(getJobTopic.c_str());
        jobState = WAIT_FOR_JOB_QUERY_RESPONSE;
      }
      break;
    case WAIT_FOR_JOB_QUERY_RESPONSE:
      {
        //iterate through the message data list and check if there is a job message
      
        //check if the timeout has passed
        if (passedMillis > timeoutLimit[WAIT_FOR_JOB_QUERY_RESPONSE]) {
          Serial.println("Timeout waiting for job query response");
          jobState = READY_TO_SEND_INIT_JOB_QUERY;
        }
      
      }

      break;

    case IDLE:

      break;

    case DO_THE_JOB:
      //do the job
      break;

    default:
      break;

  }


  if (jobState != prevJobState) {
    jobStateChangeTimeMillis = millis();
    //printf("Job state changed from %d to %d at %d ms\r\n", prevJobState, jobState, (int)jobStateChangeTimeMillis);
    prevJobState = jobState;
  }

  // general dealing with jobsMessages

  // there should be no jobsMessages in the list, but just in case
  if (jobsMessages.size() > 0) {
    //deal with one job, just for test, set it to succeeded
    Serial.println("leftover job messages");
    for (int i = 0; i < (int)(jobsMessages.size()); i++) {
      Serial.println(jobsMessages[i].topic);
    }
  }
  jobsMessages.clear();

  // if (jobsToDo.size() > 0) {
  //   //deal with one job, just for test, set it to succeeded
  //   String jobId = jobsToDo[0];
  //   jobsToDo.erase(jobsToDo.begin());
  //   printf("Process Job ID: %s\r\n", jobId.c_str());
  //   String updateJobTopic = String(awsPrefix) + String(thingName) + "/jobs/" + jobId + "/update";
  //   strcpy(publishPayload, "{\"status\":\"SUCCEEDED\"}");
  //   client.publish(updateJobTopic.c_str(), publishPayload);
  //   printf("Publish [%s] %s\r\n", updateJobTopic.c_str(), publishPayload);
  // }

  // //subscription seems controlled by server, so there is no memory usage on client side.
  // static bool subscribingShadow  = true;
  // if (subscribingShadow && millis() > 30000) {
  //   subscribingShadow = false;
  //   printf("Unsubscribing shadow topics\r\n");
  //   for (int i = 0; i < (int)(sizeof(subscribeShadowTopic) / sizeof(subscribeShadowTopic[0])); i++) {
  //     String subscribeTopic = String(awsPrefix) + String(thingName) + subscribeShadowTopic[i];
  //     //printf("Subscribe [%s]\r\n", subscribeTopic.c_str());
  //     client.unsubscribe(subscribeTopic.c_str());
  //   }
  // }
}

void AwsMqtt::shadowCallback(char* topic, char* payload, unsigned int length) {
  if ((strstr(topic, "/shadow/get/accepted") != NULL) || (strstr(topic, "/shadow/update/accepted") != NULL)) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      return;
    }

    //sample:
    // {"state":{"reported":{"led":1}},"metadata":{"reported":{"led":{"timestamp":1745983892}}},"version":116,"timestamp":1745983892,"clientToken":"amebaClient"}

    // if (doc["state"].is<JsonObject>()) {
    //   if (doc["state"]["reported"].is<JsonObject>()) {
    //     if (doc["state"]["reported"]["led"].is<int>()) {
    //       int desired_led_state = doc["state"]["reported"]["led"];
    //       if (desired_led_state != led_state) {
    //         updateLedState(desired_led_state);
    //       }
    //     }
    //   }
    // }
  }
}

void AwsMqtt::jobsCallback(char* topic, char* payload, unsigned int length) {
  // JsonDocument doc;
  // DeserializationError error = deserializeJson(doc, payload, length);
  // if (error) {
  //   Serial.print(F("deserializeJson() failed: "));
  //   return;
  // }

  //add the topic and payload to the messageDataList
  MQTTMessageData messageData;
  messageData.topic = String(topic);
  messageData.payload = String(payload);
  jobsMessages.push_back(messageData);

  //$aws/things/amebaDevBoard/jobs/notify-next
  //has details of the command

  // if (strstr(topic, "/jobs/notify") != NULL) {
  //   if (doc["jobs"].is<JsonObject>()) {
  //     if (doc["jobs"]["QUEUED"].is<JsonArray>()) {
  //       JsonArray queuedJobs = doc["jobs"]["QUEUED"];
  //       printf("Number of queued jobs: %d\r\n", queuedJobs.size());
  //       jobsToDo.clear();
  //       for (JsonVariant job : queuedJobs) {
  //         if (job.is<JsonObject>()) {
  //           if (job["jobId"].is<const char*>()) {
  //             const char* jobId = job["jobId"];
  //             printf("Queue Job ID: %s\r\n", jobId);
  //             jobsToDo.push_back(String(jobId));
  //           }
  //         }
  //       }
  //     }
  //   }
  // }
}