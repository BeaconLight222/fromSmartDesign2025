extern "C" int wifi_set_autoreconnect(uint8_t mode);

void initializeWifiAndBleConfig(){
    BLE.init(); //this actually initialuze Wifi

    BLE.configServer(1);
    configService.addService();
    configService.begin();

    // Wifi config service requires a specific advertisement format to be recognised by the app
    // The advertisement needs the local BT address, which can only be obtained after starting peripheral mode
    // Thus, we stop advertising to update the advert data, wait for advertising to stop, then restart advertising with new data
    BLE.beginPeripheral();
    BLE.configAdvert()->stopAdv();
    BLE.configAdvert()->setAdvData(configService.advData());
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

char storedSsid[33] = {0};
char storedPassword[129] = {0};

void getSsidPasswordFromFlash(char **ssid, char **password){
    FlashMemory.read();
    int lengthSSID = 33;
    int lengthPassword = 129;
    for (int i = 0; i < 33; i++){
        storedSsid[i] = FlashMemory.buf[i+0];
    }
    for (int i = 0; i < 129; i++){
        storedPassword[i] = FlashMemory.buf[i+33];
    }
    //check if there is a valid string ending in ssid and password
    bool ssidValidEnding = false;
    for (int i = 0; i < lengthSSID; i++){
        if (storedSsid[i] == 0){
            ssidValidEnding = true;
            break;
        }
    }
    if (ssidValidEnding == false){
        for (int i = 0; i < lengthSSID; i++){
            storedSsid[i] = 0;
        }
    }

    bool passwordValidEnding = false;
    for (int i = 0; i < lengthPassword; i++){
        if (storedPassword[i] == 0){
            passwordValidEnding = true;
            break;
        }
    }
    if (passwordValidEnding == false){
        for (int i = 0; i < lengthPassword; i++){
            storedPassword[i] = 0;
        }
    }

    *ssid = storedSsid;
    *password = storedPassword;
}

void saveSsidPasswordToFlash(char *ssid, char *password){
    bool ssidAndPasswordTheSame = true;
    if (strcmp(ssid, storedSsid) != 0){
        ssidAndPasswordTheSame = false;
    }
    if (strcmp(password, storedPassword) != 0){
        ssidAndPasswordTheSame = false;
    }
    
    if (ssidAndPasswordTheSame){
        Serial.println("SSID and password are the same, not saving to flash.");
        return;
    } else {
        Serial.println("Saving SSID and password to flash.");
        memcpy(storedSsid, ssid, 33);
        memcpy(storedPassword, password, 129);
        FlashMemory.read();
        for (int i = 0; i < 33; i++){
            FlashMemory.buf[i+0] = ssid[i];
        }
        for (int i = 0; i < 129; i++){
            FlashMemory.buf[i+33] = password[i];
        }
        FlashMemory.update();
    }
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
