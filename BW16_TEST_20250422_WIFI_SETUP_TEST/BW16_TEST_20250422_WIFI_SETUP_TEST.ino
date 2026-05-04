/*

 Example guide:
 https://www.amebaiot.com/en/amebad-arduino-ble-wifi-configuration/
 */

// compiled with 3.1.7
#include <WiFi.h>
#include <WiFiClient.h>
#include <FlashMemory.h>

#include "BLEDevice.h"
#include "BLEWifiConfigService.h"

BLEWifiConfigService configService;

uint32_t lastWifiConnectedTime = 0;
bool bleAdvertisingForWifiConfig = false;
bool wifiWasConnected = false;

#define NO_CONNECTION_RESTART_BLE_CONFIG_TIME 15000

void setup() {
    Serial.begin(115200);

    initializeWifiAndBleConfig();

    char *ssidFromFlash, *passwordFromFlash;

    getSsidPasswordFromFlash(&ssidFromFlash, &passwordFromFlash);
    Serial.print("Stored SSID: ");
    Serial.println(ssidFromFlash);
    Serial.print("Stored Password: ");
    Serial.println(passwordFromFlash);

    if (ssidFromFlash[0] != 0) {
        Serial.println("Connecting to stored WiFi credentials...");
        WiFi.begin(ssidFromFlash, passwordFromFlash);
    } else {
        Serial.println("No stored WiFi credentials found, starting BLE advertising for WiFi config...");
        lastWifiConnectedTime = millis()-NO_CONNECTION_RESTART_BLE_CONFIG_TIME; // Set to 10 seconds ago to trigger advertising immediately
    }

    pinMode(PA15,INPUT_PULLUP);
    PINMUX->PADCTR[_PA_15] = 0x1D00;  //2.4V, change from 5D00, disable PAD_BIT_SDIO_H3L1 change voltage from 1.6V to 2.4V. Maybe still some conflict but OK for now.

}

void loop() {

    //seems not memorize, need to save it here.

    //delay(1000);
    int buttonState = checkButtonActivity();
    if (buttonState & (1<<5)) {
        Serial.println("Button long pressed");
        if (!bleAdvertisingForWifiConfig) {
            Serial.println("BLE advertising for wifi config started");
            BLE.configAdvert()->startAdv();
            bleAdvertisingForWifiConfig = true;
        }
    }

    fetchWifiSetting();
    char *ssid = getWifiSsidAfterFetch();
    char *password = getWifiPasswordAfterFetch();
    // WiFi.begin with wrong SSID or password, or not begin, or disconnected, the ssid will be empty.
    bool wifiConnected = (ssid[0]!= 0);
    if (wifiWasConnected != wifiConnected) {
        if (wifiConnected) {
            Serial.println("WiFi connected");
            Serial.print("ssid: ");
            Serial.println(ssid);
            Serial.print("password: ");
            Serial.println(password);
            // IPAddress ip = WiFi.localIP();
            // Serial.print("IP Address: ");
            // Serial.println(ip);
            if (bleAdvertisingForWifiConfig) {
                Serial.println("BLE advertising for wifi config stopped");
                BLE.configAdvert()->stopAdv();
                bleAdvertisingForWifiConfig = false;
            }
            saveSsidPasswordToFlash(ssid, password);
        } else {
            Serial.println("WiFi disconnected");
        }
    } else {
        if (wifiConnected) {
            lastWifiConnectedTime = millis();
        }
        if (!wifiConnected && (((int32_t)(millis() - lastWifiConnectedTime)) > NO_CONNECTION_RESTART_BLE_CONFIG_TIME) && !bleAdvertisingForWifiConfig) {
            Serial.println("WiFi lost connection for 10 seconds, restarting BLE advertising for wifi config");
            if (!bleAdvertisingForWifiConfig) {
                Serial.println("BLE advertising for wifi config started");
                BLE.configAdvert()->startAdv();
                bleAdvertisingForWifiConfig = true;
            }
        }
    }
    wifiWasConnected = wifiConnected;
}


