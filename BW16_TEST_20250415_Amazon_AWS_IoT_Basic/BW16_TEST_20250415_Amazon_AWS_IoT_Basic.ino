/*

 Basic Amazon AWS IoT example

 Example guide:
 https://www.amebaiot.com/en/amebad-arduino-aws-shadow/
 */

#include <WiFi.h>
#include <PubSubClient.h>

// Update these with values suitable for your network.
char ssid[] = "TP-LINK_Test";       // your network SSID (name), this is the hotspot on Deqing's phone
char pass[] = "12344321";           // your network password
int status = WL_IDLE_STATUS;        // Indicator of Wifi status

WiFiSSLClient wifiClient;
PubSubClient client(wifiClient);

#define THING_NAME "amebaDevBoard"

char mqttServer[]     = "a1e9mdr77wm319-ats.iot.us-east-1.amazonaws.com"; //got from https://us-east-1.console.aws.amazon.com/iot/home?region=us-east-1#/connectdevice
char clientId[]       = "amebaClient";
char publishTopic[]   = "$aws/things/" THING_NAME "/shadow/update";
char publishPayload[MQTT_MAX_PACKET_SIZE];
char *subscribeTopic[5] = {
    "$aws/things/" THING_NAME "/shadow/update/accepted",
    "$aws/things/" THING_NAME "/shadow/update/rejected",
    "$aws/things/" THING_NAME "/shadow/update/delta",
    "$aws/things/" THING_NAME "/shadow/get/accepted",
    "$aws/things/" THING_NAME "/shadow/get/rejected"
};

/* Amazon Root CA can be download here:
 * https://docs.aws.amazon.com/iot/latest/developerguide/server-authentication.html?icmpid=docs_iot_console#server-authentication-certs
 */
//Deqing: also 
//Invoke-WebRequest -Uri https://www.amazontrust.com/repository/AmazonRootCA1.pem -OutFile root-CA.crt
char* rootCABuff = \
// Amazon Root CA 1 RSA 2048
"-----BEGIN CERTIFICATE-----\n" \
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n" \
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n" \
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n" \
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n" \
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n" \
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n" \
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n" \
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n" \
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n" \
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n" \
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n" \
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n" \
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n" \
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n" \
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n" \
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n" \
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n" \
"rqXRfboQnoZsG4q5WTP468SQvvG5\n" \
"-----END CERTIFICATE-----\n";

/* Fill in YOUR certificate.pem.crt with LINE ENDINGS */
char* certificateBuff = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDWjCCAkKgAwIBAgIVAP+LRDEd0edTDwgxJ2edemCR0NukMA0GCSqGSIb3DQEB\n" \
"CwUAME0xSzBJBgNVBAsMQkFtYXpvbiBXZWIgU2VydmljZXMgTz1BbWF6b24uY29t\n" \
"IEluYy4gTD1TZWF0dGxlIFNUPVdhc2hpbmd0b24gQz1VUzAeFw0yNTA0MTUxNjIz\n" \
"MTNaFw00OTEyMzEyMzU5NTlaMB4xHDAaBgNVBAMME0FXUyBJb1QgQ2VydGlmaWNh\n" \
"dGUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDV1X2FY/DDb7RX2tbv\n" \
"0t2uExY5b1YE02Q7knR8E2RYY+gLwGFyeluiVBMl/xxeLHGZmpsXAXIg4h3jmQqY\n" \
"KeGJcvncAvvtyJUvELQCx7RSqPQU1dr3js4fQAtpqZdk5E3sQWb51mcaNkh82OAN\n" \
"KXvxw920VxoPPAz7nkzWMifvFWwYk/RgtBfT+4Ph/mZpkUCicu1Z2PzufnCYYzOu\n" \
"8Di1h+PgLUFQjfOT4NTZaqJ8/ErzgVJjggMQwo4O9hctVcvSH3cqbdU6kDRQwVKu\n" \
"OxqVMEnPijQBEUz0igxvWtg+0NcqLAwcn2nhHkgw1UyycVjo9EZZt/+xkVlzcLwh\n" \
"YsPNAgMBAAGjYDBeMB8GA1UdIwQYMBaAFBvp9SgBJmqzrijLeRvlea+oSeC0MB0G\n" \
"A1UdDgQWBBQrtWM9kPoYapj+WcCmzhtDX0L7vjAMBgNVHRMBAf8EAjAAMA4GA1Ud\n" \
"DwEB/wQEAwIHgDANBgkqhkiG9w0BAQsFAAOCAQEAV+OGVckr8VhcQZvKDETZdt8d\n" \
"yszpW4DjziyvrUfxVMTmyGVS5RN9LLbiVEP3JUqEer1V6XqeYp0MsQ1F8K65rojg\n" \
"0+a2wIaQ0pBoi/xR3/EfKlZQdlwFSD8jjhN81Y1u0XP9EnXLtZBFmZI23waQeNhq\n" \
"rFuhtRRAQGOV5kXlQOnLtU1Xr0J0HOxPHLeuOweZb+leTK55Pu3d3vIJIM79iZrI\n" \
"bbC+1FJTmfrUi3t+E+LDruwflgR87+/bDokYE2h48DEok5mkmt0BDcbCHZMgeXvV\n" \
"jw4HBNoEGP4ORv+k2s2yyq+5Bf4ScWbMRbO9KKO0QkMMFVNAGZueuSe5UU9Amg==\n" \
"-----END CERTIFICATE-----\n";

/* Fill in YOUR private.pem.key with LINE ENDINGS */
char* privateKeyBuff = \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"MIIEpAIBAAKCAQEA1dV9hWPww2+0V9rW79LdrhMWOW9WBNNkO5J0fBNkWGPoC8Bh\n" \
"cnpbolQTJf8cXixxmZqbFwFyIOId45kKmCnhiXL53AL77ciVLxC0Ase0Uqj0FNXa\n" \
"947OH0ALaamXZORN7EFm+dZnGjZIfNjgDSl78cPdtFcaDzwM+55M1jIn7xVsGJP0\n" \
"YLQX0/uD4f5maZFAonLtWdj87n5wmGMzrvA4tYfj4C1BUI3zk+DU2WqifPxK84FS\n" \
"Y4IDEMKODvYXLVXL0h93Km3VOpA0UMFSrjsalTBJz4o0ARFM9IoMb1rYPtDXKiwM\n" \
"HJ9p4R5IMNVMsnFY6PRGWbf/sZFZc3C8IWLDzQIDAQABAoIBAQCWGgrGe+UiC2OY\n" \
"2DFQn8Ck1RkgsBq9wHX3q1LBVgt2UIsu1JiS93kjdckLPwz2vlPv8ysy9vzaQF+i\n" \
"yGc7cQq0pVTnL+EQhWDTbPIvkWfvLlJH3eabKE9geGCKh1WSqQBZ+38BmZM+PySb\n" \
"HtIH6zrp9wfF8+6DCzBB4FkncoW9uQ7NpCMVGb1eQgdvW71n1pqDQtPoNzL1cgTi\n" \
"R0jy/hWP8HfEngxj6RR0qy7hvXsrlNyZCcWjw1KZbf6KOrnA26SamdjoAKKlm2ht\n" \
"lw0inDM97Ejvn+gqTCcOMiGPnFewmvi9NkPw0VYfJ+f4pZHvwrfdyCv1/ue82fjr\n" \
"o/fa5XGxAoGBAPkSblj822+t+WH5REMzbx0AmCRGgXyrSwPWeBPPYKPG4OeNJA71\n" \
"B8Gh1xEFKgM3LYzZQWQ7LaYCYTuKqPuiTCHnKMAe6wZC8J2sKkYz9s0xdfHO2D/v\n" \
"66eCjX7CVvhfkGIZUPySIwnqaP9TX1ht5hJ32yXm6cKMsCAP6PUzHzFzAoGBANvI\n" \
"I7BOeSFIFxGV5/yYNMdyl2mLqAQkokibefEvAiU5w9AF5gtToug20fyP3veXGNdx\n" \
"5GivNpzHgSFpj6lyNhWdx0L/tIUqm2/cwJGZWug1/QmyP/0i6G06d1XGIodo6JJQ\n" \
"JLWH60CkhFHgerm4Jz7e99FUAWy8Kguo83f2W+W/AoGAa9igMSXjehrxhYuiFBr8\n" \
"PKmaNvLUdH/S7ml5+tHrfV7K2VgSyestHZmO/w6mX1gQABG+L8E0BdK3+UkT3Eks\n" \
"/+0QhwecKkzn1M3MTDOJ5NVKxZYTqrOe7RwpWj6Z29e9M8zUdVhtlYiLSCr4eNi1\n" \
"Kz/8gw/WHeg/BtL8wtcM5aMCgYEAn9xKgTIDDz81cFgePm+jbDMgiOfJFQJke/WC\n" \
"0/hCUjta/1NbCYATLV66jD9FiceAjSzNW4ueaJkAhwIWOcTWLBDwX/5IGthr6Qij\n" \
"lQP7yI2EeOoLex9J+jEdnekZMm8PQ2VB3jx31DQV/swN2EpnWWaq0LqOsr2Gw6Zn\n" \
"OSmbnFcCgYAmEGzUkqosBRj+9ryo99R3E7KiOj0ZgalAivbRhFEPBYoMqboP4TDk\n" \
"q7RPlHDmV5RA7IYoGsfwmdpJAxsaAb/GL8wvh+eDlNeKo4CRK2cH5anG4FgfHz0v\n" \
"E9v4E41uxURMvEpRES0bUTKiVfXWtM6TQfOmZq7tjVkfNUYl+PgTkw==\n" \
"-----END RSA PRIVATE KEY-----\n";

int led_pin = 10;
int led_state = 1;

void updateLedState(int desired_led_state) {
    printf("change led_state to %d\r\n", desired_led_state);
    led_state = desired_led_state;
    digitalWrite(led_pin, led_state);

    sprintf(publishPayload, "{\"state\":{\"reported\":{\"led\":%d}},\"clientToken\":\"%s\"}",
        led_state,
        clientId
    );
    client.publish(publishTopic, publishPayload);
    printf("Publish [%s] %s\r\n", publishTopic, publishPayload);
}

void callback(char* topic, byte* payload, unsigned int length) {
    char buf[MQTT_MAX_PACKET_SIZE];
    char *pch;
    int desired_led_state;

    strncpy(buf, (const char *)payload, length);
    buf[length] = '\0';
    printf("Message arrived [%s] %s\r\n", topic, buf);

    if ((strstr(topic, "/shadow/get/accepted") != NULL) || (strstr(topic, "/shadow/update/accepted") != NULL)) {
        pch = strstr(buf, "\"reported\":{\"led\":");
        if (pch != NULL) {
            pch += strlen("\"reported\":{\"led\":");
            desired_led_state = *pch - '0';
            if (desired_led_state != led_state) {
                updateLedState(desired_led_state);
            }
        }
    }
}

void reconnect() {
    // Loop until we're reconnected
    while (!client.connected()) {
    Serial.print("\r\n Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect(clientId)) {
            Serial.println("connected");

            for (int i=0; i<5; i++) {
                client.subscribe(subscribeTopic[i]);
            }
            sprintf(publishPayload, "{\"state\":{\"reported\":{\"led\":%d}},\"clientToken\":\"%s\"}",
                led_state,
                clientId
            );
            client.publish(publishTopic, publishPayload);
            printf("Publish [%s] %s\r\n", publishTopic, publishPayload);

        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(led_pin, OUTPUT);
    digitalWrite(led_pin, led_state);

    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(ssid, pass);
        if (status == WL_CONNECTED) break;
        // retry after 1 second
        delay(1000);
    }

    wifiClient.setRootCA((unsigned char*)rootCABuff);
    wifiClient.setClientCertificate((unsigned char*)certificateBuff, (unsigned char*)privateKeyBuff);

    client.setServer(mqttServer, 8883);
    client.setCallback(callback);

    // Allow the hardware to sort itself out
    delay(1500);
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();
    delay(1000);
}
