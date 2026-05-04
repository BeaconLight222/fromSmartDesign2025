/*

 Basic Amazon AWS IoT example

 Example guide:
 https://www.amebaiot.com/en/amebad-arduino-aws-shadow/
 */

#include <WiFi.h>
#include <PubSubClient.h>

#include "AwsMqtt.h"

// Update these with values suitable for your network.
char ssid[] = "SDGuest";       // your network SSID (name), this is the hotspot on Deqing's phone
char pass[] = "guest@sd";           // your network password
int status = WL_IDLE_STATUS;        // Indicator of Wifi status


AwsMqtt awsMqtt;

void setup() {
    Serial.begin(115200);

    while (status != WL_CONNECTED) {
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);
        // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
        status = WiFi.begin(ssid, pass);
        if (status == WL_CONNECTED) break;
        // retry after 1 second
        delay(1000);
    }

    awsMqtt.begin();
}

void loop() {
    awsMqtt.loop();
}
