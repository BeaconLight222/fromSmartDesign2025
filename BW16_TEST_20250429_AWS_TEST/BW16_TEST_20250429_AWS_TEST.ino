/*

 Example guide:
 https://www.amebaiot.com/en/amebad-arduino-ble-wifi-configuration/
 */

// compiled with 3.1.7
#include <WiFi.h>

#include "BLEDevice.h"
#include "BLEWifiConfigService.h"

#include "AwsMqtt.h"
#include "NonvolatileDataManager.h"

BLEWifiConfigService configService;

uint32_t lastWifiConnectedTime = 0;
bool bleAdvertisingForWifiConfig = false;
bool wifiWasConnected = false;

extern const u32 ftl_phy_page_start_addr;

#define NO_CONNECTION_RESTART_BLE_CONFIG_TIME 15000

AwsMqtt awsMqtt;

void setup() {
    //malloc(14000); //12000->72576 OK 13500->71072 OK 14000->14000 fail 14500->70048 fail 15000->69568 fail 20000->64576 fail
    //new project has 228K
    delay(600); //seems needed for Mac, too much print during the 500ms reset interval will disturb auto reset

    Serial.begin(115200);

    int flashSize = OtaProcessor::checkFlashSize();
    if (flashSize != 4*1024*1024) {
        while (1) {
        Serial.println("Flash size is not 4MB");
        delay(1000);
        }
    }
    int img2Address = OtaProcessor::checkImg2AddressInBootloader();
    if (img2Address < 0x08200000) {
        while (1) {
        Serial.print("img2Address is now ");
        Serial.println(img2Address, HEX);
        Serial.println("It is likely you are using a bootloader for 2MB flash chips");
        Serial.println("Please use the bootloader for 4MB flash chips as we need to do OTA");
        Serial.println("Please modify OTA_Region through source code or binary modification");
        //dump the bootloader to packages/realtek/tools/ameba_d_tools/1.1.3/bsp/image/PMU_bins/NONE
        delay(1000);
        }
    }

    if (ftl_phy_page_start_addr<0x200000){
        // the ftl_phy_page_start_addr is compiled in a lib. not easy to modify code
        // modify before the BLEDevice::init()
        //TODO: inherite the BLEDevice and modify the ftl_phy_page_start_addr
    //     uint32_t *ftl_phy_page_start_addr_ptr = (uint32_t*)(&ftl_phy_page_start_addr);
    //     *ftl_phy_page_start_addr_ptr = 0x00202000;
    //     Serial.print("ftl_phy_page_start_addr is now ");
    //     Serial.println(ftl_phy_page_start_addr, HEX);
    }

    int imgNumber = OtaProcessor::getCurrentImgNumber();
    Serial.print("Current image number is ");
    Serial.println(imgNumber);


    NonvolatileDataManager::initialize();
    // NonvolatileDataManager::printMemoryDump(0x08100000, 0x6000); // print the memory dump of the user data in default layout
    // NonvolatileDataManager::printMemoryDump(0x08200000, 0x6000); // print the memory dump of the user data in 4M layout
    //NonvolatileDataManager::printMemoryDump(0x08206000, 4096*2);


    initializeWifiAndBleConfig();

    char *ssidFromFlash, *passwordFromFlash;

    Serial.println("Reading stored WiFi credentials from flash...");

    NonvolatileDataManager::getSsidPasswordFromFlash(&ssidFromFlash, &passwordFromFlash);
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

    pinMode(PB3, OUTPUT);
    pinMode(PA14, OUTPUT);
    pinMode(PA13, OUTPUT);

    digitalWrite(PB3, HIGH); // 6 red
    digitalWrite(PA14, HIGH);
    digitalWrite(PA13, HIGH); // 11 blue


    awsMqtt.begin();

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
            NonvolatileDataManager::saveSsidPasswordToFlash(ssid, password);
        } else {
            Serial.println("WiFi disconnected");
        }
    } else {
        if (wifiConnected) {
            lastWifiConnectedTime = millis();
            awsMqtt.loop();
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


