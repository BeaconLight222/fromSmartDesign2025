extern "C" int wifi_set_autoreconnect(uint8_t mode);

#include "bte.h"
#include "wifi_conf.h"
#include "wifi_constants.h"
#include "wifi_drv.h"
extern "C" uint32_t ftl_init(uint32_t u32PageStartAddr, uint8_t pagenum);

void initializeWifiAndBleConfig(){
    //BLE.init(); //this actually initialuze Wifi
    //but the ftl_init is called for wrong address, let's see if we can do it by ourself{
    
    {
        
        T_GAP_DEV_STATE new_state;
        if (!(wifi_is_up(RTW_STA_INTERFACE) || wifi_is_up(RTW_AP_INTERFACE))) {
            wiFiDrv.wifiDriverInit();
        }
        while (!(wifi_is_up(RTW_STA_INTERFACE) || wifi_is_up(RTW_AP_INTERFACE))) {
            vTaskDelay(1000 / portTICK_RATE_MS);
            printf("WiFi not up\r\n");
        }
        le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);
        if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
            printf("BT Stack already on\r\n");
        } else {
            bt_trace_init();
            //ftl_init(ftl_phy_page_start_addr, ftl_phy_page_num);
            ftl_init(0x00202000, 3);
            bte_init();
        }
    }

    BLE.configServer(1);
    configService.addService();
    configService.begin();

    // Wifi config service requires a specific advertisement format to be recognised by the app
    // The advertisement needs the local BT address, which can only be obtained after starting peripheral mode
    // Thus, we stop advertising to update the advert data, wait for advertising to stop, then restart advertising with new data
    BLE.beginPeripheral();
    BLE.configAdvert()->stopAdv(); // No freeze here
    //BLEAdvertData advData = configService.advData();  //sprintf inside
    //BLE.configAdvert()->setAdvData(advData);
    //do the advertisement data generation here to avoid sprintf freeze
    {
        BLEAdvertData advData;
        char device_name[GAP_DEVICE_NAME_LEN] = {0};
        uint8_t btaddr[GAP_BD_ADDR_LEN] = {0};
        BLE.getLocalAddr(btaddr);
        
        // Construct device_name as "Ameba_" + 3 bytes of btaddr in hex, without using snprintf
        const char prefix[] = "Ameba_";
        memcpy(device_name, prefix, sizeof(prefix) - 1);
        const char hex_chars[] = "0123456789ABCDEF";
        for (int i = 0; i < 3; ++i) {
            device_name[6 + i * 2]     = hex_chars[(btaddr[2 - i] >> 4) & 0xF];
            device_name[6 + i * 2 + 1] = hex_chars[btaddr[2 - i] & 0xF];
        }
        device_name[12] = '\0';

        advData.clear();
        advData.addFlags(GAP_ADTYPE_FLAGS_LIMITED | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED);
        advData.addCompleteServices(BLEUUID("FF01"));
        advData.addCompleteName((char *)device_name);

        BLE.configAdvert()->setAdvData(advData);
    }
    BLE.configAdvert()->updateAdvertParams();
    delay(100);
    //BLE.configAdvert()->startAdv();

    wifi_set_autoreconnect(1);
}

#include "wifi_structures.h"
extern "C" int wifi_get_setting(const char *ifname,rtw_wifi_setting_t *pSetting);
rtw_wifi_setting_t wifi_setting_fetched = {RTW_MODE_NONE, {0}, 0, RTW_SECURITY_OPEN, {0}, 0};

int fetchWifiSetting(){
    return wifi_get_setting(WLAN0_NAME, &wifi_setting_fetched);
}

char* getWifiSsidAfterFetch(){
    return (char *)wifi_setting_fetched.ssid;
}
char* getWifiPasswordAfterFetch(){
    return (char *)wifi_setting_fetched.password;
}

int checkButtonActivity(){
    int returnValue = 0;
    static bool buttonWasPressed = false;
    static bool buttonWasDebouncedPressed = false;
    static uint32_t buttonLastChangeTime = 0;
    static bool buttonWasLongPressed = false;
    bool buttonPressed = (digitalRead(PA15) == LOW);
    if (buttonPressed){
        //just check if the button is pressed
        returnValue |= (1<<0);
    }
    if (buttonWasPressed != buttonPressed){
        //button state changed
        returnValue |= (1<<1);
        buttonLastChangeTime = millis();
    }
    buttonWasPressed = buttonPressed;

    if (buttonWasDebouncedPressed != buttonPressed){
        int buttonPressDuration = millis() - buttonLastChangeTime;
        if (buttonPressDuration > 50){
            //button state changed (debounced)
            returnValue |= (1 << 3);
            buttonWasDebouncedPressed = buttonPressed;
            buttonWasLongPressed = false;
            //Serial.println("Button changed");
        }
    }

    if (buttonWasDebouncedPressed) {
        //button state (debounced)
        returnValue |= (1 << 2);
    }

    if (buttonWasDebouncedPressed) {
        int buttonPressDuration = millis() - buttonLastChangeTime;
        if (buttonPressDuration > 3000) {
            //long press detected
            returnValue |= (1 << 4);
            if (!buttonWasLongPressed) {
                returnValue |= (1 << 5);
                //Serial.println("Button long pressed");
                buttonWasLongPressed = true;
            }
        }
    }

    return returnValue;
}
