/// @file BW16_TEST_20250529_Light_Logic_log.ino
/// @brief Main file compiled with 3.1.9

// compiled with 3.1.9
#include <WiFi.h>
#include <WiFiClient.h>

#include "esp32Provision.h"

#include "AwsMqtt.h"
#include "NonvolatileDataManager.h"

#include <Wire.h>

#include "WDT.h"

#include "LightLogicControl.h"


#ifdef __cplusplus
extern "C" {
#endif

#include "sys_api.h"

#ifdef __cplusplus
}
#endif

LightLogicControl lightControl;

// BLEWifiConfigService configService;

uint32_t lastWifiConnectedTime = 0;
uint32_t lastWifiTryConnectTime = 0;
bool bleAdvertisingForWifiConfig = false;
bool wifiWasConnected = false;
uint32_t inProgress8hourSectionStartTimePrevious = 0;

#define NO_CONNECTION_RESTART_BLE_CONFIG_TIME 15000

ESP32Provision *esp32Provision = nullptr;

uint32_t lastEsp32ProvisionActivityMillis = 0;

AwsMqtt awsMqtt;
WDT wdt;

/// @brief setup is an Arduino function that is called once at the start of the program. \n
/// In this project, it checks bootloader, initializes the sensors \n
/// And then Wifi and BLE
void setup() {

  //send SCK clocks on PA25 to unfreeze the I2C
  pinMode(PA25, OUTPUT);
  for (int i = 0; i < 10; i++) {
    digitalWrite(PA25, LOW);  //send SCK clock
    delayMicroseconds(10);
    digitalWrite(PA25, HIGH);
    delayMicroseconds(10);
  }
  pinMode(PA25, INPUT);

  pinMode(PB3, OUTPUT);
  pinMode(PA14, OUTPUT);
  pinMode(PA13, OUTPUT);

  GPIO_WriteBit(_PB_3, 0);   // Turn off the red LED
  GPIO_WriteBit(_PA_14, 0);  // Turn off the blue LED
  GPIO_WriteBit(_PA_13, 0);  // Turn off the white LED

  Wire.begin();
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz

  Serial.begin(115200);

  int flashSize = OtaProcessor::checkFlashSize();
  if (flashSize != 4 * 1024 * 1024) {
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

  int imgNumber = OtaProcessor::getCurrentImgNumber();
  Serial.print("Current image number is ");
  Serial.println(imgNumber);

  wdt.InitWatchdog(30000);
  wdt.StartWatchdog();

  NonvolatileDataManager::initialize();

  Serial.println("Firmware compiled at: ");
  Serial.println(lightControl.getCompiledTimeStr());
  Serial.println("device name: ");
  Serial.println(NonvolatileDataManager::getThingName());

  lightControl.begin();
  lightControl.setLightState(false);  // Turn on the light
  Serial.println("Starting radar...");
  lightControl.initRadar();  // Initialize the radar sensors
  Serial.println("Radar started");

  Serial.println("Starting ir sensor...");
  lightControl.initThermalSensor();  // Initialize the thermal sensor
  Serial.println("IR sensor started");

  Serial.println("Starting temperature sensor...");
  lightControl.initTemperatureSensor();  // Initialize the temperature sensor
  Serial.println("Temperature sensor started");

  Serial.println("Starting RTC...");
  lightControl.initRTC();  // Initialize the RTC
  Serial.println("RTC started");

  Serial.println("Starting EEPROM...");
  lightControl.initEEPROM();  // Initialize the EEPROM
  Serial.println("EEPROM started");

  lightControl.startPeriodicLightProcess();  // Start periodic light process

  lightControl.lightOnTimeForLoggingInIntervalUnit = lightControl.eepromReadLightOnData();
  Serial.print("Initial light on time in interval unit read from EEPROM: ");
  Serial.println(lightControl.lightOnTimeForLoggingInIntervalUnit);

  // if radar or thermal sensor is not valid, we retry 5 times
  if (!lightControl.radar1Valid || !lightControl.radar2Valid || !lightControl.thermalSensorValid) {
    for (int i = 0; i < 5; i++) {
      Serial.println("Retrying sensor initialization...");
      lightControl.initRadar();
      lightControl.initThermalSensor();
      if (lightControl.radar1Valid && lightControl.radar2Valid && lightControl.thermalSensorValid) {
        break;  // Exit loop if all sensors are valid
      }
      delay(1000);  // Wait before retrying
    }
  }

  if (lightControl.radar1Valid && lightControl.radar2Valid && lightControl.thermalSensorValid && lightControl.temperatureSensorValid && lightControl.rtcValid && lightControl.eepromValid) {
    // All sensors are valid, proceed with normal operation
  } else {
    bool atLeastOneSensorValid = lightControl.radar1Valid || lightControl.radar2Valid || lightControl.thermalSensorValid;
    if (!atLeastOneSensorValid) {
      lightControl.setUiLedState(UI_LED_RED, UI_LED_BLINK);  // Set red LED to blink if any sensor is not valid
    }

    int printCycle = atLeastOneSensorValid ? 1 : 5;

    for (int i = 0; i < printCycle; i++) {
      Serial.print("radar1Valid: ");
      Serial.print(lightControl.radar1Valid);
      Serial.print(" radar2Valid: ");
      Serial.print(lightControl.radar2Valid);
      Serial.print(" thermalSensorValid: ");
      Serial.print(lightControl.thermalSensorValid);
      Serial.print(" temperatureSensorValid: ");
      Serial.print(lightControl.temperatureSensorValid);
      Serial.print(" rtcValid: ");
      Serial.print(lightControl.rtcValid);
      Serial.print(" eepromValid: ");
      Serial.println(lightControl.eepromValid);
      delay(1000);
    }
    if (!atLeastOneSensorValid) {
      sys_reset();
      delay(100000);
    }
    //keep going even some sensors are not valid
  }

  char *ssidFromFlash, *passwordFromFlash;

  Serial.println("Reading stored WiFi credentials from flash...");

  NonvolatileDataManager::getSsidPasswordFromFlash(&ssidFromFlash, &passwordFromFlash);
  Serial.print("Stored SSID: ");
  Serial.println(ssidFromFlash);
  Serial.print("Stored Password: ");
  Serial.println(passwordFromFlash);

  if (ssidFromFlash[0] != 0) {
    Serial.println("Connecting to stored WiFi credentials...");
    lastWifiTryConnectTime = millis();
    lightControl.setUiLedState(UI_LED_BLUE, UI_LED_BLINK);  // Set blue LED to blink while connecting
    if (strlen(passwordFromFlash) == 0) {
      WiFi.begin(ssidFromFlash);  // Connect without password
    } else {
      WiFi.begin(ssidFromFlash, passwordFromFlash);  // Connect with password
    }
  } else {
    Serial.println("No stored WiFi credentials found, starting BLE advertising for WiFi config...");
    lastWifiConnectedTime = millis() - NO_CONNECTION_RESTART_BLE_CONFIG_TIME;  // Set to 10 seconds ago to trigger advertising immediately
  }

  //pinMode(PA15, INPUT_PULLUP);
  pinMode(PA15, INPUT_IRQ_CHANGE);
  digitalSetIrqHandler(PA15, buttonIrqHandler);
  //2.4V, change from 5D00, disable PAD_BIT_SDIO_H3L1 change voltage from 1.6V to 2.4V. Maybe still some conflict but OK for now.
  //This also ensures the pin is pulled up
  PINMUX->PADCTR[_PA_15] = 0x1D00;  

  awsMqtt.begin();
}

/// @brief getSensorData will fetch the sensor data \n
/// including radar, thermal sensor, temperature sensor, and RTC time \n
/// also it will check if the time difference between AWS message and RTC is more than 10 seconds \n
/// if so, it will reset the RTC to AWS time \n
/// it will also use radar and thermal sensor data to calculate the minimal distance \n
/// and turn off the light if the minimal distance is less than 30cm as a precaution \n

void getSensorData(bool wifiStatus) {
  //calculate lamp on time for logging
  {
    if (lightControl.lightOnTimeLastCheckMilliseconds == 0) {
      lightControl.lightOnTimeLastCheckMilliseconds = millis();
    }else{
      uint32_t elapsedTime = millis() - lightControl.lightOnTimeLastCheckMilliseconds;
      lightControl.lightOnTimeNotLoggedYetMilliseconds += elapsedTime;
      if (lightControl.lightOnTimeNotLoggedYetMilliseconds >= LED_ON_LOG_INTERVAL) {
        lightControl.lightOnTimeForLoggingInIntervalUnit += 1;
        lightControl.eepromWriteLightOnData(lightControl.lightOnTimeForLoggingInIntervalUnit);
        lightControl.lightOnTimeNotLoggedYetMilliseconds -= LED_ON_LOG_INTERVAL;
      }
      lightControl.lightOnTimeLastCheckMilliseconds = millis();
    }
  }

  Serial.println("Fetching sensor data...");
  lightControl.checkRadarData(false);
  lightControl.checkThermalSensorData(false);
  lightControl.getTemperatureData();
  lightControl.getCurrentRtcTime();
  if (awsMqtt.initOtaJobTimeMessageUnixTime != 0) {
    //update RTC time if the time difference between AWS message and RTC is more than 10 seconds
    //The AWS time is based on the time when the job inquiry was received
    uint32_t timeDiffFromAwsMessage = (uint32_t)(millis() - awsMqtt.initOtaJobTimeMessageMillis);
    uint32_t currentUnixTimeWithAwsReference = awsMqtt.initOtaJobTimeMessageUnixTime + (timeDiffFromAwsMessage / 1000);
    uint32_t currentUnixTimeRTC = lightControl.currentRtcTime.unixtime();
    int32_t timeDiffAwsRtc = (int32_t)(currentUnixTimeWithAwsReference - currentUnixTimeRTC);
    if (abs(timeDiffAwsRtc) > 10) {  // If the difference is more than 10 seconds
      Serial.print("Time difference between AWS message and RTC is ");
      Serial.print(timeDiffAwsRtc);
      Serial.println(" seconds, resetting RTC to AWS time.");
      DateTime awsDateTime(currentUnixTimeWithAwsReference);
      lightControl.rtc.adjust(awsDateTime);  // Adjust RTC to AWS time
      lightControl.getCurrentRtcTime();      // Update current RTC time
    }
    awsMqtt.initOtaJobTimeMessageUnixTime = 0;  // Reset after use
  }
  lightControl.get8hourSectionStartTime();
  lightControl.processSensorInfo(false);

  Serial.print("Sensor minimal distance: ");
  Serial.println(lightControl.calculatedMinimalDistance);

  //Emergency shutoff if user appear in 1FT, 30cm
  if ((lightControl.calculatedMinimalDistance < 300)) {
    if (((lightControl.uvLampMode == UV_MODE_SMART) || (lightControl.uvLampMode == UV_MODE_MANUAL))) {
      Serial.println("Minimal distance is less than 30cm, turning off the light");
      if (lightControl.getLightState()) {
        lightControl.setLightState(false);  // Turn off the light
        // If the light is on, log the sensor data before turning it off
        String emergencyData = "{\"emergencyOff\": {\"minimalDistanceMm\": " + String(lightControl.calculatedMinimalDistance) + "}}";
        if (wifiStatus) {
          awsMqtt.logSensorToServer((char *)emergencyData.c_str());
          sendLoggingAndTelemetryDataToServer();
        }
      }
    }
  }else{
    if ((lightControl.uvLampMode == UV_MODE_MANUAL)) {
      lightControl.setLightState(true);  // Turn on the light
    }
  }
}

void sendLoggingAndTelemetryDataToServer() {
  awsMqtt.logSensorToServer((char *)lightControl.getSensorDataJson().c_str());
  bool occupied = (lightControl.calculatedMinimalDistance < 2000);  //consider occupied if someone is closer than 2 meters
  bool lampStatus = lightControl.getLightState();
  bool lampEnable = (lightControl.uvLampMode != UV_MODE_OFF);
  int detectionMode = 0;
  if ((lightControl.uvLampMode == UV_MODE_MANUAL) || (lightControl.uvLampMode == UV_MODE_OFF)) {
    detectionMode = 1;
  }
  bool scheduleEnable = awsMqtt.scheduleEnabled;

  /*
      settings: {
        "lampEnable": true,
        "detectionMode": 1,
        "scheduleEnable": false,
        "factoryReset": false,
        "wifiReset": false
    },
  
  */

  String telemetryData = "{\"occupied\": " + String(occupied ? "true" : "false") +
                        ", \"lampStatus\": " + String(lampStatus ? "true" : "false") +
                        ", \"settings\": {\"lampEnable\": " + String(lampEnable ? "true" : "false") +
                        ", \"detectionMode\": " + String(detectionMode) +
                        ", \"scheduleEnable\": " + String(scheduleEnable ? "true" : "false") +
                        "}" +
                        ", \"firmwareVersion\": \"0.0.0\", \"errorCode\": 0, \"rtcUnixTime\": " +
                        String(lightControl.currentRtcTime.unixtime()) + "}";
  awsMqtt.sendTelemetryToServer((char *)telemetryData.c_str());
}

/// @brief loop is an Arduino function that is called repeatedly \n
/// In this project, it checks the button activity, check sensor data, and handle WiFi connection. \n
/// Every minute it will calculate the accumulated exposure with minimal distance data and write the data to EEPROM. \n
/// Depending on the Mode, the light will be turned on or off. \n
void loop() {
  int buttonState = checkButtonActivity();
  //if ((buttonState & (1<<3)) && ((buttonState & (1<<0))==0)){
  if ((buttonState & (1<<0)) ){
    Serial.println("Button short pressed");

    if ( lightControl.uvLampMode == UV_MODE_MANUAL) {
      //turn off the light if it is on
      Serial.println("Button switching UV lamp mode to OFF");
      lightControl.uvLampMode = UV_MODE_OFF;
    } else {
      //turn on the light if it is off
      Serial.println("Button switching UV lamp mode to MANUAL");
      lightControl.uvLampMode = UV_MODE_MANUAL;
    }
    lightControl.processLightControl(awsMqtt.scheduleEnabled, awsMqtt.scheduleData);
    sendLoggingAndTelemetryDataToServer();
  } else if (buttonState & (1 << 1)) {
    Serial.println("Button long pressed");
    NonvolatileDataManager::eraseSsidPasswordFromFlash();
    sys_reset();
    while(1);
  }

  fetchWifiSetting();
  char *ssid = getWifiSsidAfterFetch();
  char *password = getWifiPasswordAfterFetch();
  // WiFi.begin with wrong SSID or password, or not begin, or disconnected, the ssid will be empty.
  bool wifiConnected = (ssid[0] != 0);
  if (wifiConnected && (!bleAdvertisingForWifiConfig)) {
    if (awsMqtt.otaProcessor.pageSwapData == NULL) {
      //do not compete with memory
      getSensorData(wifiConnected);
    } else {
      //just turn off the light when OTA is in progress
      lightControl.setLightState(false);
    }
  }
  if (wifiWasConnected != wifiConnected) {
    if (wifiConnected) {
      lightControl.setUiLedState(UI_LED_BLUE, UI_LED_ON);
      lastWifiTryConnectTime = 0;
      Serial.println("WiFi connected");
      Serial.print("ssid: ");
      Serial.println(ssid);
      Serial.print("password: ");
      Serial.println(password);
      // IPAddress ip = WiFi.localIP();
      // Serial.print("IP Address: ");
      // Serial.println(ip);
      if (bleAdvertisingForWifiConfig) {
        unsigned long waitSendOut = millis();
        while ((millis() - waitSendOut) < (10000) && (esp32Provision->wifiConnectedMessageSent < 2)) {
          //wait for 3 seconds to send out the response, especially for iOS
          esp32Provision->loop();
          delay(100);
        }
        if (esp32Provision->wifiConnectedMessageSent >= 2) {
          //give iOS more time to close connection
          delay(500);
        }
        Serial.println("BLE advertising for wifi config stopped");
        //BLE.configAdvert()->stopAdv();
        if (esp32Provision != nullptr) {
          esp32Provision->stop();
          delete esp32Provision;
          esp32Provision = nullptr;
        }
        bleAdvertisingForWifiConfig = false;
      }
      NonvolatileDataManager::saveSsidPasswordToFlash(ssid, password);
    } else {
      Serial.println("WiFi disconnected");
      if (lightControl.uvLampMode != UV_MODE_SMART) {
        // force UV lamp mode to smart if WiFi is disconnected
        lightControl.uvLampMode = UV_MODE_SMART;
        Serial.println("WiFi disconnected, setting UV lamp mode to SMART");
        lightControl.processLightControl(awsMqtt.scheduleEnabled, awsMqtt.scheduleData);
      }
    }
  } else {
    if (wifiConnected) {
      lastWifiConnectedTime = millis();
      awsMqtt.loop();
    }
    if (!wifiConnected) {
      if ((((int32_t)(millis() - lastWifiConnectedTime)) > NO_CONNECTION_RESTART_BLE_CONFIG_TIME) && !bleAdvertisingForWifiConfig) {
        Serial.println("WiFi lost connection for 10 seconds, restarting BLE advertising for wifi config");
        lightControl.setUiLedState(UI_LED_BLUE, UI_LED_OFF);
        if (!bleAdvertisingForWifiConfig) {
          Serial.println("BLE advertising for wifi config started");
          //BLE.configAdvert()->startAdv();

          esp32Provision = new ESP32Provision();
          esp32Provision->begin();

          lastEsp32ProvisionActivityMillis = millis();

          bleAdvertisingForWifiConfig = true;
        }
      }

      if (esp32Provision != nullptr) {
        esp32Provision->loop();
      }

      if (BLEPatched.connected()) {
        lastEsp32ProvisionActivityMillis = millis();
      }

      if (bleAdvertisingForWifiConfig && ((int)(millis() - lastEsp32ProvisionActivityMillis) > 30000)) {
        Serial.println("BLE advertising for wifi config stopped due to inactivity");
        if (esp32Provision != nullptr) {
          esp32Provision->stop();
          delete esp32Provision;
          esp32Provision = nullptr;
        }
        bleAdvertisingForWifiConfig = false;

        //try wifi one more time
        int wifiReturn;
        char *ssidFromFlash, *passwordFromFlash;
        NonvolatileDataManager::getSsidPasswordFromFlash(&ssidFromFlash, &passwordFromFlash);
        if (ssidFromFlash[0] != 0) {
          Serial.print("Trying to connect to stored WiFi credentials: ");
          Serial.print(ssidFromFlash);
          Serial.print(" with password: ");
          Serial.println(passwordFromFlash);
          lightControl.setUiLedState(UI_LED_BLUE, UI_LED_BLINK);
          if (strlen(passwordFromFlash) == 0) {
            wifiReturn = WiFi.begin(ssidFromFlash);  // Connect without password
          } else {
            wifiReturn = WiFi.begin(ssidFromFlash, passwordFromFlash);  // Connect with password
          }
          lastWifiTryConnectTime = millis();
        }
      }


      if ((((int32_t)(millis() - lastWifiTryConnectTime)) > 15000)) {
        lightControl.setUiLedState(UI_LED_BLUE, UI_LED_OFF);
        if (awsMqtt.otaProcessor.pageSwapData == NULL) {
          //do not compete with memory
          delay(2000);
          getSensorData(wifiConnected);
        }
      }

    }

    {
      //do logic every minute
      if (lightControl.inProgress8hourSectionStartTime > 0) {
        //get8hourSectionStartTime() update the inProgress8hourSectionStartTime
        if (inProgress8hourSectionStartTimePrevious != lightControl.inProgress8hourSectionStartTime) {
          //new section started
          inProgress8hourSectionStartTimePrevious = lightControl.inProgress8hourSectionStartTime;
          lightControl.processedMinutesCount = -1;
          Serial.print("Start new session: ");
          Serial.println(lightControl.inProgress8hourSectionStartTime);
          lightControl.eepromInitSection(lightControl.inProgress8hourSectionStartTime);
        }
        int timeDiff = (int)(lightControl.currentRtcTime.unixtime() - lightControl.inProgress8hourSectionStartTime);
        int timeDiffMinutes = timeDiff / 60;
        if (timeDiffMinutes > lightControl.processedMinutesCount) {
          lightControl.processedMinutesCount = timeDiffMinutes;
          Serial.print("Processed minutes count: ");
          Serial.println(lightControl.processedMinutesCount);

          //update the accumulated exposure
          lightControl.eepromGetAccumulatedExposure(lightControl.inProgress8hourSectionStartTime);
          lightControl.processLightControl(awsMqtt.scheduleEnabled, awsMqtt.scheduleData);

          if (lightControl.getLightState()) {
            int accumulatedExposureThisMinute = lightControl.minimalDistanceJulesLevel;
            lightControl.eepromWriteSection(lightControl.inProgress8hourSectionStartTime, accumulatedExposureThisMinute, lightControl.processedMinutesCount);
          }

          int accumulatedExposure = lightControl.eepromGetAccumulatedExposure(lightControl.inProgress8hourSectionStartTime);
          Serial.print("Accumulated exposure for section ");
          Serial.print(lightControl.inProgress8hourSectionStartTime);
          Serial.print(" is ");
          Serial.print(accumulatedExposure);
          Serial.print(" with threshold ");
          Serial.println(lightControl.accumulatedExposureThreshold);

          if (wifiConnected) {
            sendLoggingAndTelemetryDataToServer();
          }
        }
      }
    }
  }

  // process information from AWS MQTT
  if (awsMqtt.newModeCommand.length() > 0) {
    String mode = awsMqtt.newModeCommand;
    Serial.print("Received new mode command: ");
    Serial.println(mode);
    if (mode == "UV_MODE_MANUAL") {
      lightControl.uvLampMode = UV_MODE_MANUAL;
    } else if (mode == "UV_MODE_SMART") {
      lightControl.uvLampMode = UV_MODE_SMART;
    } else if (mode == "UV_MODE_OFF") {
      lightControl.uvLampMode = UV_MODE_OFF;
    } else if (mode == "RESET_WIFI") {
      NonvolatileDataManager::eraseSsidPasswordFromFlash();
      sys_reset();
      while(1);
    }
    awsMqtt.newModeCommand = "";  // Clear the command after processing
    Serial.print("Set UV lamp mode to: ");
    Serial.println(lightControl.uvLampMode);
    lightControl.processLightControl(awsMqtt.scheduleEnabled, awsMqtt.scheduleData);
    if (wifiConnected) {
      sendLoggingAndTelemetryDataToServer();
    }
  }

  wifiWasConnected = wifiConnected;
  wdt.RefreshWatchdog();
}
