/// @file LightLogicControl.cpp
/// @brief Light logic control for the BW16 project, also manages radar, thermal sensor, temperature sensor, RTC, and EEPROM interactions.

#include "LightLogicControl.h"
#include <GTimer.h>

#include "src/mlx90640YoloProcessor/mlx90640YoloProcessor.h"

#include "NonvolatileDataManager.h"

const char compiledTimeStr[] = __DATE__ " " __TIME__;

/// @brief Exposure levels in micro Joules per minute, divided into 4 intensity levels.
const int exposureLevelToJules[4] = {
    0, 20, 90, 360}; // Jules for exposure levels 0, 1, 2, 3
// level3 < 1M
// level2 < 2.5M
// level1 further

// exposure from Andrea slack message
// 0.0127 meters/ 0.042 Feet/ .5 inches: 2.4mW/sec
// .25m / 0.8ft: .023
// .50m / 1.7ft: .006
// 1 meter/3.3 Feet: 0.0035
// 1.5 Meters/4.9 Ft:	0.0015
// 2 Meters/6.6 Feet: 0.001

// distance is ranged in <1m, <2.5m
// for <1m, lets use 0.006mW/sec, for a minute it is 0.36mJ, which is 360uJ
// for <2.5m, lets use 0.0015mW/sec, for a minute it is 0.09mJ, which is 90uJ

// We are using the lower threshold eye TLV of 161 mJ/cm2., that would be 161000
// uJ/cm2 write it to accumulatedExposureThreshold

volatile int gTimer0Counter = 0;
volatile int gTimer0LedState = 0;
/// @brief Timer interrupt handler for LED blinking
void gTimer0handler(uint32_t data) {
  (void)data;
  gTimer0Counter++;

  int redLedState = gTimer0LedState & (3 << 4);
  if (redLedState == UI_RED_LED_BLINK) {
    GPIO_WriteBit(_PB_3, gTimer0Counter & 1);
  } else if (redLedState == UI_RED_LED_ON) {
    GPIO_WriteBit(_PB_3, 1);
  } else {
    GPIO_WriteBit(_PB_3, 0);
  }

  int blueLedState = gTimer0LedState & (3 << 2);
  if (blueLedState == UI_BLUE_LED_BLINK) {
    GPIO_WriteBit(_PA_14, gTimer0Counter & 1);
  } else if (blueLedState == UI_BLUE_LED_ON) {
    GPIO_WriteBit(_PA_14, 1);
  } else {
    GPIO_WriteBit(_PA_14, 0);
  }

  int whiteLedState = gTimer0LedState & (3 << 0);
  if (whiteLedState == UI_WHITE_LED_BLINK) {
    GPIO_WriteBit(_PA_13, gTimer0Counter & 1);
  } else if (whiteLedState == UI_WHITE_LED_ON) {
    GPIO_WriteBit(_PA_13, 1);
  } else {
    GPIO_WriteBit(_PA_13, 0);
  }
}

LightLogicControl::LightLogicControl() {
  lightState = false;
  radar1Valid = false;
  radar2Valid = false;
  thermalSensorValid = false;
  eepromValid = false;
  thermalDetections = NULL;
  thermalDetectionCount = 0;
  temperatureSensorValid = false;
  temperatureData = 0.0f;
  calculatedMinimalDistance = INT_MAX;
  minimalDistanceJulesLevel = EXPOSURE_LEVEL_0;
  inProgress8hourSectionStartTime = 0;
  processedMinutesCount = -1;
  accumulatedExposure = 0;
  uvLampMode = UV_MODE_SMART;
  accumulatedExposureThreshold = 161000;

  lightOnTimeForLoggingInIntervalUnit = 0;
  lightOnTimeLastCheckMilliseconds = 0;
  lightOnTimeNotLoggedYetMilliseconds = 0;
}

void LightLogicControl::begin() {
  pinMode(PA12, OUTPUT);
  update();
}

/// @brief setup the periodic light process timer
void LightLogicControl::startPeriodicLightProcess() {
  GTimer.begin(0, (200 * 1000), gTimer0handler); // 200ms
}

/// @brief initialize the 2 radar sensors
void LightLogicControl::initRadar() {
  radar1Valid = radar1.begin(0);
  if (radar1Valid) {
    Serial.println("Radar1 initialized successfully");
  } else {
    Serial.println("Radar1 initialization failed");
  }
  // radar1.disableRadarStreaming();
  radar2Valid = radar2.begin(1);
  if (radar2Valid) {
    Serial.println("Radar2 initialized successfully");
  } else {
    Serial.println("Radar2 initialization failed");
  }
}

/// @brief Check the radar data from both radar sensors
/// @param printData Whether to print the radar data in serial monitor
void LightLogicControl::checkRadarData(bool printData) {
  if (radar1Valid) {
    radar1.activeSerialMux();
    // radar1.enableRadarStreaming();
    int radarNewdata = radar1.checkRadarData(
        200); // no enable/disable streaming, takes about 80ms, enable/disable
              // streaming takes about 180ms
    if (radarNewdata) {
      if (printData) {
        Serial.println("Radar1 data: ");
        for (int i = 0; i < radar1.radarObjectCount; i++) {
          Serial.print("Object ");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(radar1.radarObject[i].x);
          Serial.print(", ");
          Serial.print(radar1.radarObject[i].y);
          Serial.print(", ");
          Serial.print(radar1.radarObject[i].speed);
          Serial.print(", ");
          Serial.println(radar1.radarObject[i].resolution);
        }
        if (radar1.radarObjectCount < 0) {
          Serial.println("Radar1 data error, no data received");
        }
      }
    } else {
      if (printData) {
        Serial.println("No new data from Radar1");
      }
    }
    // radar1.disableRadarStreaming();
  } else {
    if (printData) {
      Serial.println("Radar1 is not valid, skipping data check.");
    }
  }

  if (radar2Valid) {
    radar2.activeSerialMux();
    // radar2.enableRadarStreaming();
    int radarNewdata = radar2.checkRadarData(200);
    if (radarNewdata) {
      if (printData) {
        Serial.println("Radar2 data: ");
        for (int i = 0; i < radar2.radarObjectCount; i++) {
          Serial.print("Object ");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(radar2.radarObject[i].x);
          Serial.print(", ");
          Serial.print(radar2.radarObject[i].y);
          Serial.print(", ");
          Serial.print(radar2.radarObject[i].speed);
          Serial.print(", ");
          Serial.println(radar2.radarObject[i].resolution);
        }
        if (radar2.radarObjectCount < 0) {
          Serial.println("Radar2 data error, no data received");
        }
      }
    } else {
      if (printData) {
        Serial.println("No new data from Radar2");
      }
    }
    // radar2.disableRadarStreaming();
  } else {
    if (printData) {
      Serial.println("Radar2 is not valid, skipping data check.");
    }
  }
}

/// @brief Initialize the thermal sensor (MLX90640)
void LightLogicControl::initThermalSensor() {
  thermalSensorValid = thermalSensor.begin(
      MLX90640_I2CADDR_DEFAULT); // MLX90640 I2C address,
                                 // MLX90640_ExtractParameters takes a long time
                                 // but it is OK
  if (thermalSensorValid) {
    Serial.println("Thermal sensor initialized successfully");
    Serial.print("Serial number: ");
    Serial.print(thermalSensor.serialNumber[0], HEX);
    Serial.print(thermalSensor.serialNumber[1], HEX);
    Serial.println(thermalSensor.serialNumber[2], HEX);
    thermalSensor.setMode(MLX90640_CHESS);
    // Set the resolution and refresh rate
    thermalSensor.setResolution(MLX90640_ADC_18BIT);
    thermalSensor.setRefreshRate(MLX90640_4_HZ);

    for (int i = 0; i < 2; i++) {
      float frame[32 * 24];
      thermalSensor.getFrame(frame);
      // discard first 2 frames
    }

    // if (initMLX90640YoloProcessor()==0) {
    //     //Serial.println("MLX90640 Yolo Processor initialized successfully");

    // } else {
    //     Serial.println("Failed to initialize MLX90640 Yolo Processor");
    // }
  } else {
    Serial.println("Thermal sensor initialization failed");
  }
}

/// @brief Check the thermal sensor data and process it with MLX90640 Yolo Processor
/// @param printData Whether to print the thermal sensor data in serial monitor
void LightLogicControl::checkThermalSensorData(bool printData) {
  if (thermalSensorValid) {
    float frame[32 * 24];
    int ret = thermalSensor.getFrame(frame);
    if (ret == 0) {
      if (printData) {
        Serial.println("Thermal sensor data:");
        for (int i = 0; i < 32 * 24; i++) {
          Serial.print(frame[i], 2);
          Serial.print(" ");
        }
        Serial.println();
      }
      if (initMLX90640YoloProcessor() == 0) {
        feedFloatMLX90640YoloProcessorAndRun(frame);
        deinitMLX90640YoloProcessor();
        thermalDetectionCount = getDetectionsCount(&thermalDetections);
      } else {
        Serial.println("Failed to allocate MLX90640 Yolo Processor");
      }
    } else {
      if (printData) {
        Serial.println("Failed to get thermal sensor data");
      }
      thermalDetectionCount = -1; // Reset detection count on error
    }
  } else {
    if (printData) {
      Serial.println("Thermal sensor is not valid, skipping data check.");
    }
  }
}

/// @brief Empty now
void LightLogicControl::update() {}

/// @brief Set the UI LED state
/// @param ledIndex The index of the LED to set
/// @param state The state to set the LED to (ON/OFF/Blink)
void LightLogicControl::setUiLedState(int ledIndex, int state) {
  int gTimer0LedStateCache = gTimer0LedState;
  gTimer0LedStateCache &= ~(3 << (ledIndex * 2));    // clear led state
  gTimer0LedStateCache |= (state << (ledIndex * 2)); // set led state
  gTimer0LedState = gTimer0LedStateCache;
}

/// @brief Set the light state and update the UI LED state
void LightLogicControl::setLightState(bool state) {
  lightState = state;

  setUiLedState(UI_LED_WHITE, lightState ? UI_LED_ON : UI_LED_OFF);

  digitalWrite(PA12, lightState ? LOW : HIGH); // ballast on when Low
}

bool LightLogicControl::getLightState() { return lightState; }

/// @brief Use sensor data to calculate the minimal distance and jules level
/// @param printData Whether to print the sensor data in serial monitor
/// @return 1 if objects detected, 0 if no objects detected
int LightLogicControl::processSensorInfo(bool printData) {
  if (radar1Valid) {
    if (radar1.radarObjectCount > 0) {
      if (printData) {
        Serial.println("Radar1 detected objects:");
        for (int i = 0; i < radar1.radarObjectCount; i++) {
          Serial.print("Object ");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(radar1.radarObject[i].x);
          Serial.print(", ");
          Serial.print(radar1.radarObject[i].y);
          Serial.print(", ");
          Serial.print(radar1.radarObject[i].speed);
          Serial.print(", ");
          Serial.println(radar1.radarObject[i].resolution);
        }
      }
    } else {
      if (printData) {
        Serial.println("Radar1 detected no objects.");
      }
    }
  }
  if (radar2Valid) {
    if (radar2.radarObjectCount > 0) {
      if (printData) {
        Serial.println("Radar2 detected objects:");
        for (int i = 0; i < radar2.radarObjectCount; i++) {
          Serial.print("Object ");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(radar2.radarObject[i].x);
          Serial.print(", ");
          Serial.print(radar2.radarObject[i].y);
          Serial.print(", ");
          Serial.print(radar2.radarObject[i].speed);
          Serial.print(", ");
          Serial.println(radar2.radarObject[i].resolution);
        }
      }
    } else {
      if (printData) {
        Serial.println("Radar2 detected no objects.");
      }
    }
  }
  if (thermalSensorValid) {
    if (thermalDetectionCount > 0) {
      if (printData) {
        Serial.println("Thermal sensor detected objects:");
        for (int i = 0; i < thermalDetectionCount; i++) {
          Serial.print("Detection ");
          Serial.print(i);
          Serial.print(": ");
          Serial.print(thermalDetections[i].x);
          Serial.print(", ");
          Serial.print(thermalDetections[i].y);
          Serial.print(", ");
          Serial.print(thermalDetections[i].w);
          Serial.print(", ");
          Serial.println(thermalDetections[i].h);
        }
      }
    } else {
      if (printData) {
        Serial.println("Thermal sensor detected no objects.");
      }
    }
  }

  int minimalDistance = INT_MAX;
  if (radar1Valid && radar1.radarObjectCount > 0) {
    for (int i = 0; i < radar1.radarObjectCount; i++) {
      int distance = radar1.radarObject[i].x * radar1.radarObject[i].x +
                     radar1.radarObject[i].y * radar1.radarObject[i].y;
      distance = sqrt(distance); // Calculate Euclidean distance
      if (distance < minimalDistance) {
        minimalDistance = distance;
      }
    }
  }
  if (radar2Valid && radar2.radarObjectCount > 0) {
    for (int i = 0; i < radar2.radarObjectCount; i++) {
      int distance = radar2.radarObject[i].x * radar2.radarObject[i].x +
                     radar2.radarObject[i].y * radar2.radarObject[i].y;
      distance = sqrt(distance); // Calculate Euclidean distance
      if (distance < minimalDistance) {
        minimalDistance = distance;
      }
    }
  }
  if (thermalSensorValid && thermalDetectionCount > 0) {
    for (int i = 0; i < thermalDetectionCount; i++) {
      int distance = 0;
      float w = thermalDetections[i].w;
      float h = thermalDetections[i].h;
      float area = w * h;
      if (area > 50) {
        distance = 900;
      } else if (area > 20) {
        distance = 2400;
      } else {
        distance = 3000; // No really detection
      }

      if (distance < minimalDistance) {
        minimalDistance = distance;
      }
    }
  }

  if (printData) {
    Serial.print("Temperature: ");
    String temperatureDataStr =
        String(temperatureData, 2); // Format to 2 decimal places
    Serial.print(temperatureDataStr);
    Serial.println(" °C");
  }

  calculatedMinimalDistance = minimalDistance;

  minimalDistanceJulesLevel = julesLevelForDistance(calculatedMinimalDistance);

  if (printData) {
    Serial.print("Minimal distance: ");
    Serial.println(calculatedMinimalDistance);
    Serial.print("Minimal distance in Jules Level: ");
    Serial.print(minimalDistanceJulesLevel);
    Serial.print(", Jules: ");
    Serial.println(exposureLevelToJules[minimalDistanceJulesLevel]);
  }

  return calculatedMinimalDistance < INT_MAX
             ? 1
             : 0; // Return 0 for no objects detected, 1 for objects detected
}

/// @brief convert distance to exposure level (4 levels)
/// @param distance The distance in mm
int LightLogicControl::julesLevelForDistance(int distance) {
  // TODO: confirm relationship between distance and jules
  int julesLevel = EXPOSURE_LEVEL_0;

  if (distance <= 1000) {
    julesLevel = EXPOSURE_LEVEL_3;

  } else if (distance <= 2500) {
    julesLevel = EXPOSURE_LEVEL_2;

  } else if (distance <= 4000) {
    julesLevel = EXPOSURE_LEVEL_1;
  }

  return julesLevel;
}

/// @brief Get the sensor data in JSON format for sending to the server
/// @return JSON string containing radar, thermal, temperature, RTC, EEPROM data
String LightLogicControl::getSensorDataJson() {
  int radar1Count = radar1.radarObjectCount;
  int radar2Count = radar2.radarObjectCount;
  int thermalCount = thermalDetectionCount;

  String radar1Json = "";
  for (int i = 0; i < radar1Count; i++) {
    radar1Json +=
        "{\"x\":" + String(radar1.radarObject[i].x) +
        ",\"y\":" + String(radar1.radarObject[i].y) +
        ",\"speed\":" + String(radar1.radarObject[i].speed) +
        ",\"resolution\":" + String(radar1.radarObject[i].resolution) + "}";
    if (i < radar1Count - 1) {
      radar1Json += ",";
    }
  }
  if ((radar1Count >= 0) && radar1Valid) {
    radar1Json = "[" + radar1Json + "]";
  } else {
    radar1Json = "\"error\""; // Handle error case for radar1
  }
  String radar2Json = "";
  for (int i = 0; i < radar2Count; i++) {
    radar2Json +=
        "{\"x\":" + String(radar2.radarObject[i].x) +
        ",\"y\":" + String(radar2.radarObject[i].y) +
        ",\"speed\":" + String(radar2.radarObject[i].speed) +
        ",\"resolution\":" + String(radar2.radarObject[i].resolution) + "}";
    if (i < radar2Count - 1) {
      radar2Json += ",";
    }
  }
  if ((radar2Count >= 0) && radar2Valid) {
    radar2Json = "[" + radar2Json + "]";
  } else {
    radar2Json = "\"error\""; // Handle error case for radar2
  }
  String thermalJson = "";
  for (int i = 0; i < thermalCount; i++) {
    thermalJson += "{\"x\":" + String(thermalDetections[i].x) +
                   ",\"y\":" + String(thermalDetections[i].y) +
                   ",\"w\":" + String(thermalDetections[i].w) +
                   ",\"h\":" + String(thermalDetections[i].h) + "}";
    if (i < thermalCount - 1) {
      thermalJson += ",";
    }
  }
  if ((thermalCount >= 0) && thermalSensorValid) {
    thermalJson = "[" + thermalJson + "]";
  } else {
    thermalJson = "\"error\""; // Handle error case for thermal sensor
  }
  int rtcUnixtime = getCurrentRtcTime().unixtime();
  String rtcTimeStr = String(rtcUnixtime);
  rtcTimeStr = "\"" + rtcTimeStr + "\"";

  String temperatureDataStr =
      String(temperatureData, 2); // Format to 2 decimal places
  if (isnan(temperatureData)) {
    temperatureDataStr = "null"; // Handle NaN case
  } else {
    temperatureDataStr =
        "\"" + temperatureDataStr + "\""; // Wrap in quotes for JSON
  }

  String accumulatedExposureStr = String(accumulatedExposure);
  accumulatedExposureStr =
      "\"" + accumulatedExposureStr + "\""; // Wrap in quotes for JSON

  String eightHourSectionStartTimeStr = String(inProgress8hourSectionStartTime);
  eightHourSectionStartTimeStr = "\"" + eightHourSectionStartTimeStr + "\"";

  String eightHourSectionDataStr =
      eepromGetSectionData(inProgress8hourSectionStartTime);
  eightHourSectionDataStr =
      "\"" + eightHourSectionDataStr + "\""; // Wrap in quotes for JSON

  String compileTimeStr = String(compiledTimeStr);

  String lampOnTimeForLoggingInIntervalUnitStr = String(lightOnTimeForLoggingInIntervalUnit);

  String sendJson =
      "{\"rawdata\":{\"radar1\":" + radar1Json + ",\"radar2\":" + radar2Json +
      ",\"thermal\":" + thermalJson + ",\"temperature\":" + temperatureDataStr +
      ",\"rtcTime\":" + rtcTimeStr +
      ",\"accumulatedExposure\":" + accumulatedExposureStr +
      ",\"eightHourSectionStartTime\":" + eightHourSectionStartTimeStr +
      ",\"eightHourSectionData\":" + eightHourSectionDataStr +
      ",\"lampOnTime15Minute\":" + lampOnTimeForLoggingInIntervalUnitStr +
      "},\"clientToken\":\"" + String(NonvolatileDataManager::getClientId()) +
      "\",\"compileTime\":\"" + compileTimeStr +
      "\"}";

  return sendJson;
}

/// @brief Initialize the temperature sensor TMP102
void LightLogicControl::initTemperatureSensor() {
  temperatureSensorValid = temperatureSensor.begin();
  if (temperatureSensorValid) {
    Serial.println("Temperature sensor initialized successfully");
  } else {
    Serial.println("Temperature sensor initialization failed");
  }
}

/// @brief Get the temperature data from the temperature sensor
/// @return Temperature data in Celsius, or NaN if the sensor is not valid or data is not available
float LightLogicControl::getTemperatureData() {
  if (temperatureSensorValid) {
    temperatureData = temperatureSensor.checkTemperatureData();
    if (!isnan(temperatureData)) {
      return temperatureData;
    } else {
      Serial.println("Failed to read temperature data.");
      return NAN;
    }
  } else {
    Serial.println("Temperature sensor is not valid, skipping data check.");
    return NAN;
  }
}

/// @brief Initialize the RTC (Real Time Clock) DS3231
/// @note This function initializes the RTC and sets the current time if the RTC is valid.
void LightLogicControl::initRTC() {
  rtcValid = rtc.begin();
  if (rtcValid) {
    Serial.println("RTC initialized successfully");
    currentRtcTime = rtc.now();
    Serial.print("Current RTC time: ");
    Serial.println(currentRtcTime.timestamp());
  } else {
    Serial.println("RTC initialization failed");
  }
}

/// @brief Get the current RTC time
/// @return Current RTC time as DateTime object
DateTime LightLogicControl::getCurrentRtcTime() {
  if (rtcValid) {
    currentRtcTime = rtc.now();
    return currentRtcTime;
  } else {
    Serial.println("RTC is not valid, returning default time.");
    return DateTime(); // Return default DateTime
  }
}

/// @brief Get the start time of the current 8-hour section
/// @return Start time of the current 8-hour section in seconds since epoch
uint32_t LightLogicControl::get8hourSectionStartTime() {
  if (rtcValid) {
    currentRtcTime = rtc.now();
    uint32_t currentTime = currentRtcTime.unixtime();
    uint32_t sectionStartTime =
        currentTime - (currentTime % 28800); // 28800 seconds = 8 hours
    inProgress8hourSectionStartTime = sectionStartTime;
    return sectionStartTime;
  } else {
    Serial.println("RTC is not valid, returning default time.");
    return 0; // Return 0 if RTC is not valid
  }
}

/// @brief Initialize the EEPROM 24C08 for logging
void LightLogicControl::initEEPROM() {
  eepromValid = eeprom.begin();
  if (eepromValid) {
    Serial.println("EEPROM initialized successfully");
  } else {
    Serial.println("EEPROM initialization failed");
  }
}

/// @brief find a block for the 8-hour section in EEPROM, erase the block and write the section start time at the start of the block
/// @param sectionStartTime The start time of the section to initialize
int LightLogicControl::eepromInitSection(uint32_t sectionStartTime) {
  if (!eepromValid) {
    Serial.println("EEPROM is not valid, skipping section initialization.");
    return -1; // Return -1 if EEPROM is not valid
  }

  // eeprom memory layout for log entries:
  // first #define EEPROM_LOG_START_ADDRESS bytes are used for other settings
  // then there are EEPROM_LOG_ENTRIES entries of EEPROM_LOG_ENTRY_SIZE bytes
  // each within each entry, the first 4 bytes are used for section start time
  // and the rest is used for log data

  uint32_t minimalStartTime = 0xFFFFFFFF; // Initialize to a large value
  int minimalStartTimeSectionIndex =
      0; // Initialize to -1 to indicate no valid address found

  for (int i = 0; i < EEPROM_LOG_ENTRIES; i++) {
    uint16_t memAddress = EEPROM_LOG_START_ADDRESS + i * EEPROM_LOG_ENTRY_SIZE;
    uint8_t startLogTime[4] = {0}; // Initialize data to zero
    int ret = eeprom.read(memAddress, startLogTime, sizeof(startLogTime));
    if (ret < 0) {
      Serial.print("Failed to read EEPROM at address ");
      Serial.println(memAddress);
      return -1; // Return -1 on read failure
    }
    // Check if the section start time matches
    uint32_t storedSectionStartTime;
    memcpy(&storedSectionStartTime, startLogTime,
           sizeof(storedSectionStartTime));
    if (storedSectionStartTime > sectionStartTime) {
      storedSectionStartTime =
          0; // Reset if the stored time is greater than the section start time
    }
    if (storedSectionStartTime < minimalStartTime) {
      minimalStartTime = storedSectionStartTime;
      minimalStartTimeSectionIndex = i;
    }
    if (storedSectionStartTime == sectionStartTime) {
      Serial.print("Section already initialized at  ");
      Serial.println(i);
      return i; // Return the address if section is already initialized
    }
  }

  // init the section with the minimal start time
  Serial.print("Initializing section at index ");
  Serial.println(minimalStartTimeSectionIndex);
  uint16_t memAddress = EEPROM_LOG_START_ADDRESS +
                        minimalStartTimeSectionIndex * EEPROM_LOG_ENTRY_SIZE;
  uint8_t sectionData[EEPROM_LOG_ENTRY_SIZE] = {0}; // Initialize data to zero
  memcpy(sectionData, &sectionStartTime, sizeof(sectionStartTime));
  int ret = eeprom.write(memAddress, sectionData, sizeof(sectionData));
  if (ret < 0) {
    Serial.print("Failed to write EEPROM at address ");
    Serial.println(memAddress);
    return -1; // Return -1 on write failure
  }

  return minimalStartTimeSectionIndex;
}

/// @brief Write the Jules level data to the EEPROM for a certain 8-hour section. Each data point takes 2 bits, so we can store 4 Jules levels in one byte.
/// @param sectionStartTime The start time of the section to write data to
/// @param julesLevel The Jules level to write (0-3)
/// @param dataPointIndex The index of the data point to write
int LightLogicControl::eepromWriteSection(uint32_t sectionStartTime,
                                          int julesLevel, int dataPointIndex) {
  if (!eepromValid) {
    Serial.println("EEPROM is not valid, skipping section write.");
    return -1; // Return -1 if EEPROM is not valid
  }

  // eeprom memory layout for log entries:
  // first #define EEPROM_LOG_START_ADDRESS bytes are used for other settings
  // then there are EEPROM_LOG_ENTRIES entries of EEPROM_LOG_ENTRY_SIZE bytes
  // each within each entry, the first 4 bytes are used for section start time
  // and the rest is used for log data

  int sectionIndex = -1;
  for (int i = 0; i < EEPROM_LOG_ENTRIES; i++) {
    int16_t memAddress = EEPROM_LOG_START_ADDRESS + i * EEPROM_LOG_ENTRY_SIZE;
    uint8_t startLogTime[4] = {0}; // Initialize data to zero
    int ret = eeprom.read(memAddress, startLogTime, sizeof(startLogTime));
    if (ret < 0) {
      Serial.print("Failed to read EEPROM at address ");
      Serial.println(memAddress);
      return -1; // Return -1 on read failure
    }
    // Check if the section start time matches
    uint32_t storedSectionStartTime;
    memcpy(&storedSectionStartTime, startLogTime,
           sizeof(storedSectionStartTime));
    if (storedSectionStartTime == sectionStartTime) {
      sectionIndex = i;
      break; // Found the section
    }
  }

  if (sectionIndex < 0) {
    Serial.println("Section not found, cannot write data.");
    return -1; // Return -1 if section is not found
  }

  // we use one bytes for 4 Jules levels
  int dataOffset =
      4 + dataPointIndex / 4; // 4 bytes for section start time, then
                              // dataPointIndex byte for Jules level
  if (dataOffset >= EEPROM_LOG_ENTRY_SIZE) {
    Serial.println("Data point index out of bounds.");
    return -1; // Return -1 if data point index is out of bounds
  }
  int readAddress = EEPROM_LOG_START_ADDRESS +
                    sectionIndex * EEPROM_LOG_ENTRY_SIZE + dataOffset;
  uint8_t julesData = 0; // Initialize data to zero
  int ret = eeprom.read(readAddress, &julesData, sizeof(julesData));
  if (ret < 0) {
    Serial.print("Failed to read EEPROM at address ");
    Serial.println(readAddress);
    return -1; // Return -1 on read failure
  }
  // Set the Jules level
  julesData &= ~(0x03 << ((dataPointIndex % 4) *
                          2)); // Clear the bits for the current Jules level
  julesData |= (julesLevel & 0x03) << ((dataPointIndex % 4) * 2);
  ret = eeprom.write(readAddress, &julesData, sizeof(julesData));
  if (ret < 0) {
    Serial.print("Failed to write EEPROM at address ");
    Serial.println(readAddress);
    return -1; // Return -1 on write failure
  }

  {
    // print debug info
    uint8_t readData[EEPROM_LOG_ENTRY_SIZE] = {0}; // Initialize data to zero
    ret = eeprom.read(EEPROM_LOG_START_ADDRESS +
                          sectionIndex * EEPROM_LOG_ENTRY_SIZE,
                      readData, sizeof(readData));
    if (ret < 0) {
      Serial.print("Failed to read EEPROM at address ");
      Serial.println(EEPROM_LOG_START_ADDRESS +
                     sectionIndex * EEPROM_LOG_ENTRY_SIZE);
      return -1; // Return -1 on read failure
    }
    Serial.print("EEPROM section data at index ");
    Serial.print(sectionIndex);
    Serial.print(": ");
    for (int i = 0; i < EEPROM_LOG_ENTRY_SIZE; i++) {
      Serial.print(readData[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  return 0; // Success
}

/// @brief Get the accumulated exposure for a specific 8-hour section from EEPROM.
/// @param sectionStartTime The start time of the section to read.
/// @return The accumulated exposure for the section, or -1 on error.
int LightLogicControl::eepromGetAccumulatedExposure(uint32_t sectionStartTime) {
  if (!eepromValid) {
    Serial.println("EEPROM is not valid, skipping section read.");
    return -1; // Return -1 if EEPROM is not valid
  }

  // eeprom memory layout for log entries:
  // first #define EEPROM_LOG_START_ADDRESS bytes are used for other settings
  // then there are EEPROM_LOG_ENTRIES entries of EEPROM_LOG_ENTRY_SIZE bytes
  // each within each entry, the first 4 bytes are used for section start time
  // and the rest is used for log data

  int sectionIndex = -1;
  for (int i = 0; i < EEPROM_LOG_ENTRIES; i++) {
    int16_t memAddress = EEPROM_LOG_START_ADDRESS + i * EEPROM_LOG_ENTRY_SIZE;
    uint8_t startLogTime[4] = {0}; // Initialize data to zero
    int ret = eeprom.read(memAddress, startLogTime, sizeof(startLogTime));
    if (ret < 0) {
      Serial.print("Failed to read EEPROM at address ");
      Serial.println(memAddress);
      return -1; // Return -1 on read failure
    }
    // Check if the section start time matches
    uint32_t storedSectionStartTime;
    memcpy(&storedSectionStartTime, startLogTime,
           sizeof(storedSectionStartTime));
    if (storedSectionStartTime == sectionStartTime) {
      sectionIndex = i;
      break; // Found the section
    }
  }

  if (sectionIndex < 0) {
    Serial.println("Section not found, cannot read data.");
    return -1; // Return -1 if section is not found
  }

  uint8_t allDataInSection[EEPROM_LOG_ENTRY_SIZE] = {
      0}; // Initialize data to zero
  int ret = eeprom.read(EEPROM_LOG_START_ADDRESS +
                            sectionIndex * EEPROM_LOG_ENTRY_SIZE,
                        allDataInSection, sizeof(allDataInSection));
  if (ret < 0) {
    Serial.print("Failed to read EEPROM at address ");
    Serial.println(EEPROM_LOG_START_ADDRESS +
                   sectionIndex * EEPROM_LOG_ENTRY_SIZE);
    return -1; // Return -1 on read failure
  }

  accumulatedExposure = 0;
  for (int i = 0; i < EEPROM_LOG_ENTRY_SIZE - 4; i++) {
    uint8_t julesData = allDataInSection[4 + i];
    for (int j = 0; j < 4; j++) {
      int julesLevel = (julesData >> (j * 2)) &
                       0x03; // Extract the Jules level for each 2 bits
      if (julesLevel > 0) {
        accumulatedExposure += exposureLevelToJules[julesLevel];
      }
    }
  }
  return accumulatedExposure;
}

/// @brief Get the section data for a specific 8-hour section from EEPROM.
/// @param sectionStartTime The start time of the section to read.
/// @return The section data as a string for JSON, or an empty string on error.
String LightLogicControl::eepromGetSectionData(uint32_t sectionStartTime) {
  if (!eepromValid) {
    Serial.println("EEPROM is not valid, skipping section read.");
    return ""; // Return empty string if EEPROM is not valid
  }

  int sectionIndex = -1;
  for (int i = 0; i < EEPROM_LOG_ENTRIES; i++) {
    int16_t memAddress = EEPROM_LOG_START_ADDRESS + i * EEPROM_LOG_ENTRY_SIZE;
    uint8_t startLogTime[4] = {0}; // Initialize data to zero
    int ret = eeprom.read(memAddress, startLogTime, sizeof(startLogTime));
    if (ret < 0) {
      Serial.print("Failed to read EEPROM at address ");
      Serial.println(memAddress);
      return ""; // Return empty string on read failure
    }
    // Check if the section start time matches
    uint32_t storedSectionStartTime;
    memcpy(&storedSectionStartTime, startLogTime,
           sizeof(storedSectionStartTime));
    if (storedSectionStartTime == sectionStartTime) {
      sectionIndex = i;
      break; // Found the section
    }
  }

  if (sectionIndex < 0) {
    Serial.println("Section not found, cannot read data.");
    return ""; // Return empty string if section is not found
  }

  uint8_t allDataInSection[EEPROM_LOG_ENTRY_SIZE] = {
      0}; // Initialize data to zero
  int ret = eeprom.read(EEPROM_LOG_START_ADDRESS +
                            sectionIndex * EEPROM_LOG_ENTRY_SIZE,
                        allDataInSection, sizeof(allDataInSection));
  if (ret < 0) {
    Serial.print("Failed to read EEPROM at address ");
    Serial.println(EEPROM_LOG_START_ADDRESS +
                   sectionIndex * EEPROM_LOG_ENTRY_SIZE);
    return ""; // Return empty string on read failure
  }

  int dataByteLength = EEPROM_LOG_ENTRY_SIZE -
                       4; // Exclude the first 4 bytes for section start time
  String textData = "";
  textData.reserve(dataByteLength *
                   2); // Reserve memory for the string to avoid reallocations
  String data =
      "{\"sectionStartTime\":" + String(sectionStartTime) + ",\"data\":[";
  for (int i = 4; i < EEPROM_LOG_ENTRY_SIZE; i++) {
    uint8_t julesData = allDataInSection[i];
    // convert julesData to padded hex string
    char hexHigh = (julesData >> 4) + '0';
    if (hexHigh > '9') {
      hexHigh += 7; // Convert to A-F
    }
    char hexLow = (julesData & 0x0F) + '0';
    if (hexLow > '9') {
      hexLow += 7; // Convert to A-F
    }
    textData += hexHigh;
    textData += hexLow;
  }
  return textData;
}

int LightLogicControl::eepromGetNewestEntryLightOnData() {
  if (!eepromValid) {
    Serial.println("EEPROM is not valid, skipping section index read.");
    return 0; // Return 0 if EEPROM is not valid
  }
  uint8_t localCopy[EEPROM_LIGHT_LOG_DATA_SIZE * EEPROM_LIGHT_LOG_ENTRIES] = {0}; // Initialize data to zero
  eeprom.read(EEPROM_LIGHT_LOG_START_ADDRESS, localCopy, sizeof(localCopy));
  // {
  //   //debug print all
  //   Serial.print("EEPROM light log data: ");
  //   for (int i = 0; i < EEPROM_LIGHT_LOG_ENTRIES; i++) {
  //     uint32_t lightOnTimeThisEntry = *(uint32_t *)(localCopy + i * EEPROM_LIGHT_LOG_DATA_SIZE);
  //     Serial.println(lightOnTimeThisEntry);
  //   }
  // }
  int newestEntryIndex = 0;
  for (int i = 0; i < EEPROM_LIGHT_LOG_ENTRIES; i++) {
    uint32_t lightOnTimeThisEntry = *(uint32_t *)(localCopy + i * EEPROM_LIGHT_LOG_DATA_SIZE);
    uint32_t lightOnTimeNextEntry = *(uint32_t *)(localCopy + ((i + 1) % EEPROM_LIGHT_LOG_ENTRIES) * EEPROM_LIGHT_LOG_DATA_SIZE);
    if ((lightOnTimeThisEntry + 1) == lightOnTimeNextEntry) {
      newestEntryIndex = ((i + 1) % EEPROM_LIGHT_LOG_ENTRIES); // Found the newest entry
      // Serial.print("Newest entry index: ");
      // Serial.println(newestEntryIndex);
    }else{
      break;
    }
  }
  return newestEntryIndex; 
}
/// @brief Read the light on data from EEPROM.

uint32_t LightLogicControl::eepromReadLightOnData() {
  if (!eepromValid) {
    Serial.println("EEPROM is not valid, skipping light on data read.");
    return 0; // Return 0 if EEPROM is not valid
  }

  int newestEntryIndex = eepromGetNewestEntryLightOnData();
  uint8_t lightOnData[4] = {0}; // Initialize data to zero
  eeprom.read(EEPROM_LIGHT_LOG_START_ADDRESS + newestEntryIndex * EEPROM_LIGHT_LOG_DATA_SIZE, lightOnData, sizeof(lightOnData));
  uint32_t lightOnTimeForLoggingInIntervalUnit;
  memcpy(&lightOnTimeForLoggingInIntervalUnit, lightOnData, sizeof(lightOnTimeForLoggingInIntervalUnit));
  return lightOnTimeForLoggingInIntervalUnit; // Return the light on time as uint32_t
}

int LightLogicControl::eepromWriteLightOnData(uint32_t lightOnTimeForLoggingInIntervalUnit) {
  if (!eepromValid) {
    Serial.println("EEPROM is not valid, skipping light on data write.");
    return -1; // Return -1 if EEPROM is not valid
  }

  int newestEntryIndex = eepromGetNewestEntryLightOnData();
  newestEntryIndex = (newestEntryIndex + 1) % EEPROM_LIGHT_LOG_ENTRIES; // Move to the next entry
  Serial.print("Writing light on data to entry index: ");
  Serial.println(newestEntryIndex);

  uint8_t lightOnData[4];
  memcpy(lightOnData, &lightOnTimeForLoggingInIntervalUnit, sizeof(lightOnTimeForLoggingInIntervalUnit));
  int ret = eeprom.write(EEPROM_LIGHT_LOG_START_ADDRESS + newestEntryIndex * EEPROM_LIGHT_LOG_DATA_SIZE, lightOnData, sizeof(lightOnData));
  return ret; // Success
}

/// @brief Process the light control logic based on the current mode and conditions.
/// @return 0 if the light is turned off, 1 if the light is turned on.
int LightLogicControl::processLightControl(bool scheduleEnabled, uint8_t* scheduleData) {
  switch (uvLampMode) {
  case UV_MODE_SMART: {
    //check schedule first
    if (scheduleEnabled) {
      DateTime currentUTCTime = getCurrentRtcTime();
      uint8_t dayOfTheWeek = currentUTCTime.dayOfTheWeek(); // 0 = Sunday, 1 = Monday, ..., 6 = Saturday
      uint8_t hour = currentUTCTime.hour();
      uint8_t min = currentUTCTime.minute();
      //each slot is 30 minutes, so there are 48 slots in a day
      int scheduleIndex = dayOfTheWeek * 48 + (hour * 2) + (min / 30);
      int byteIndex = scheduleIndex / 8;
      int bitIndex = scheduleIndex % 8;
      bool scheduleInEffect = false;
      if (byteIndex >= 0 && byteIndex < 42){ // 42 bytes for 7 days * 48 slots / 8 bits
        scheduleInEffect = (scheduleData[byteIndex] & (1 << bitIndex)) != 0;
      } 
      Serial.print("Current UTC time:W");
      Serial.print(dayOfTheWeek);
      Serial.print("H");
      Serial.print(hour);
      Serial.print("M");
      Serial.print(min);
      Serial.print(" Schedule:");
      Serial.print(scheduleInEffect ? "EN" : "DIS");
      Serial.println();

      if (scheduleInEffect) {
        Serial.println("Schedule as ENABLED, do smart");
      } else {
        Serial.println("Schedule as DISABLED, turn off light");
        setLightState(false);
        return 0; // Light turned off
      }
    }

    if (accumulatedExposure >= accumulatedExposureThreshold) {
      Serial.println(
          "Accumulated exposure reached threshold, turning off light.");
      setLightState(false);
      return 0; // Light turned off
    } else {
      Serial.println(
          "Accumulated exposure is below threshold, turning on light.");
      setLightState(true);
      return 1; // Light turned on
    }
  } break;
  case UV_MODE_MANUAL: {
    Serial.println("Manual mode, turning on light.");
    setLightState(true);
    return 1; // Light turned on
  }
  case UV_MODE_UNLIMITED: {
    Serial.println("UV Lamp mode: Unlimited");
    setLightState(true);
  }
    return 1; // Light turned on
  case UV_MODE_OCCUPIED_ROOM: {
    int radar1Count = radar1.radarObjectCount;
    int radar2Count = radar2.radarObjectCount;
    int thermalCount = thermalDetectionCount;
    if (radar1Count > 0 || radar2Count > 0 || thermalCount > 0) {
      Serial.println("Occupied room detected, turning on light.");
      setLightState(true);
      return 1; // Light turned on
    } else {
      Serial.println("No occupancy detected, turning off light.");
      setLightState(false);
      return 0; // Light turned off
    }
  } break;
  case UV_MODE_EMPTY_ROOM: {
    int radar1Count = radar1.radarObjectCount;
    int radar2Count = radar2.radarObjectCount;
    int thermalCount = thermalDetectionCount;
    if (radar1Count > 0 || radar2Count > 0 || thermalCount > 0) {
      Serial.println("Occupied room detected, turning off light.");
      setLightState(false);
      return 0; // Light turned off
    } else {
      Serial.println("No occupancy detected, turning on light.");
      setLightState(true);
      return 1; // Light turned on
    }
  } break;
  case UV_MODE_OFF:
  default: {
    Serial.println("UV Lamp mode: Off");
    setLightState(false);
    return 0; // Light turned off
  }
  }
}

const char *LightLogicControl::getCompiledTimeStr() {
  return compiledTimeStr;
}
