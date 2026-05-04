/// @file connectionUtils.ino
/// @brief Connection utilities for the BW16 project

extern "C" int wifi_set_autoreconnect(uint8_t mode);

#include "bte.h"
#include "wifi_conf.h"
#include "wifi_constants.h"
#include "wifi_drv.h"


#include "wifi_structures.h"
extern "C" int wifi_get_setting(const char *ifname, rtw_wifi_setting_t *pSetting);
rtw_wifi_setting_t wifi_setting_fetched = { RTW_MODE_NONE, { 0 }, 0, RTW_SECURITY_OPEN, { 0 }, 0 };

/// @brief wrapper for SDK wifi_get_setting
int fetchWifiSetting() {
  return wifi_get_setting(WLAN0_NAME, &wifi_setting_fetched);
}

char *getWifiSsidAfterFetch() {
  return (char *)wifi_setting_fetched.ssid;
}
char *getWifiPasswordAfterFetch() {
  return (char *)wifi_setting_fetched.password;
}

/// @brief Check the button on pin A15 activity and return a bitmask of the button states
/// @return A bitmask where: \n
/// - Bit 0: Button is short pressed \n
/// - Bit 1: Button is long pressed \n


volatile uint32_t buttonChangeTime = 0;
volatile uint32_t buttonLastChangeTime = 0;
volatile bool buttonPressed = false;
volatile int updateCount = 0;


int checkButtonActivity() {
  int returnValue = 0;

  // copy the volatile variables to local variables to avoid inconsistency during execution
  int updateCountbeforeRead;
  int updateCountAfterRead;
  uint32_t buttonChangeTimeLocal;
  uint32_t buttonLastChangeTimeLocal;
  bool buttonPressedLocal;
  static uint32_t lastRisingTime = 0;
  do {
    updateCountbeforeRead = updateCount;
    buttonChangeTimeLocal = buttonChangeTime;
    buttonLastChangeTimeLocal = buttonLastChangeTime;
    buttonPressedLocal = buttonPressed;
    updateCountAfterRead = updateCount;
  } while (updateCountbeforeRead != updateCountAfterRead);

  if (buttonPressedLocal) {
    int pressPassedTime = (int)(millis() - buttonChangeTimeLocal);
    // Serial.print("button pressed, time: ");
    // Serial.println(pressPassedTime);
    if (pressPassedTime > 4000) {
      //Serial.println("Long press detected");
      returnValue |= (1 << 1);
    }
  } else {
    if (lastRisingTime!= buttonChangeTimeLocal) {
      lastRisingTime = buttonChangeTimeLocal;
      int pressDuration = (int)(buttonChangeTimeLocal - buttonLastChangeTimeLocal);
      // Serial.print("Button released after ");
      // Serial.print(pressDuration);
      // Serial.println(" ms");
      if (pressDuration > 100) {
        //short press
        //Serial.println("Short press detected");
        returnValue |= (1 << 0);
      }
    }
  }

  return returnValue;
}

void buttonIrqHandler(uint32_t id, uint32_t event){
  bool currentButtonPressed = (digitalRead(PA15) == LOW);
  if (currentButtonPressed == buttonPressed) {
    // no change, to deal with first IRQ after setup
    return;
  }
  buttonPressed = currentButtonPressed;
  uint32_t now = millis();
  buttonLastChangeTime = buttonChangeTime;
  buttonChangeTime = now;
  updateCount++;
  // Serial.print("Button IRQ, state: ");
  // Serial.print(buttonPressed ? "PRESSED" : "RELEASED");
  // Serial.print(", last change time: ");
  // Serial.print(buttonLastChangeTime);
  // Serial.print(", change time: ");
  // Serial.println(buttonChangeTime);
}
