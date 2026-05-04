#ifndef ESP32_PROVISION_H
#define ESP32_PROVISION_H

#include <Arduino.h>
#undef max
#undef min
#include <vector>

#include "src/patchedBle/BLEDevicePatched.h"
#include <WiFi.h>
#include <wifi_conf.h>

#define ESP32_PROVISIONING_SERVICE_UUID "021A9004-0382-4AEA-BFF4-6B3F1C5ADFB4"
#define ESP32_PROVISIONING_CTRL_CHARACTERISTIC_UUID "021AFF4F-0382-4AEA-BFF4-6B3F1C5ADFB4"
#define ESP32_PROVISIONING_SCAN_CHARACTERISTIC_UUID "021AFF50-0382-4AEA-BFF4-6B3F1C5ADFB4"
#define ESP32_PROVISIONING_SESSION_CHARACTERISTIC_UUID "021AFF51-0382-4AEA-BFF4-6B3F1C5ADFB4"
#define ESP32_PROVISIONING_CONFIG_CHARACTERISTIC_UUID "021AFF52-0382-4AEA-BFF4-6B3F1C5ADFB4"
#define ESP32_PROTO_VER_CHARACTERISTIC_UUID "021AFF53-0382-4AEA-BFF4-6B3F1C5ADFB4"

#define WIFI_SCAN_RESULT__SSID__FIELD_NUMBER 1
#define WIFI_SCAN_RESULT__CHANNEL__FIELD_NUMBER 2
#define WIFI_SCAN_RESULT__RSSI__FIELD_NUMBER 3
#define WIFI_SCAN_RESULT__BSSID__FIELD_NUMBER 4
#define WIFI_SCAN_RESULT__AUTH__FIELD_NUMBER 5

#define WIFI_SCAN_RESULT__ENTRY__FIELD_NUMBER 1

#define MY_SCAN_LIMIT 15

enum WifiAuthMode {
    Open = 0,
    WEP  = 1,
    WPA_PSK = 2,
    WPA2_PSK = 3,
    WPA_WPA2_PSK = 4,
    WPA2_ENTERPRISE = 5,
    WPA3_PSK = 6,
    WPA2_WPA3_PSK = 7,
};

class ESP32Provision {
public:


    ESP32Provision() 
        : provisioningService(ESP32_PROVISIONING_SERVICE_UUID),
          provisioningControlCharacteristic(ESP32_PROVISIONING_CTRL_CHARACTERISTIC_UUID),
          provisioningScanCharacteristic(ESP32_PROVISIONING_SCAN_CHARACTERISTIC_UUID),
          provisioningSessionCharacteristic(ESP32_PROVISIONING_SESSION_CHARACTERISTIC_UUID),
          provisioningConfigCharacteristic(ESP32_PROVISIONING_CONFIG_CHARACTERISTIC_UUID),
          provisioningProtoVerCharacteristic(ESP32_PROTO_VER_CHARACTERISTIC_UUID),
          advdata(),
          scandata(),
          toConnectSSID(""),
          toConnectPassword(""),
          triggerWifiConnect(0),
          wifiConnectFailed(0) {}

    void begin();
    void stop();
    void loop();

    std::vector<unsigned char> encodeLEB128(int64_t value);
    //follow rtw_scan_result_t
    std::vector<unsigned char> encodeWifiScanResult(char *ssid, uint8_t channel, int32_t rssi, uint8_t *bssid, uint8_t auth);
    int convertFromRealtekAuthModeToEsp32AuthMode(uint32_t realtekAuthMode);

    BLEServicePatched provisioningService;
    BLECharacteristicPatched provisioningControlCharacteristic;
    BLECharacteristicPatched provisioningScanCharacteristic;
    BLECharacteristicPatched provisioningSessionCharacteristic;
    BLECharacteristicPatched provisioningConfigCharacteristic;
    BLECharacteristicPatched provisioningProtoVerCharacteristic;
    BLEAdvertDataPatched advdata;
    BLEAdvertDataPatched scandata;

    String toConnectSSID;
    String toConnectPassword;

    boolean triggerWifiConnect;
    boolean wifiConnectFailed;

    uint8_t _networkCount;
    char _networkSsid[MY_SCAN_LIMIT][WL_SSID_MAX_LENGTH];
    int32_t _networkRssi[MY_SCAN_LIMIT];
    uint32_t _networkEncr[MY_SCAN_LIMIT];
    uint8_t _networkChannel[MY_SCAN_LIMIT];
    uint8_t _networkBand[MY_SCAN_LIMIT];
    uint8_t _networkMac[MY_SCAN_LIMIT][6];
    uint32_t scanResultTime = 0;

    uint8_t wifiConnectedMessageSent;
};

#endif