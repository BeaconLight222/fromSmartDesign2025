#ifndef LIGHT_LOGIC_CONTROL_H
#define LIGHT_LOGIC_CONTROL_H

#include "src/Simple_MLX90640/Simple_MLX90640.h"
#include "src/Simple_Rd_03d/Simple_Rd_03d.h"
#include "src/Simple_misc_sensor/Simple_misc_sensor.h"
#include <Arduino.h>

#define UV_LAMP_JULES_SCALE (100LLu)

#define EEPROM_LOG_START_ADDRESS 0x80
#define EEPROM_LOG_ENTRIES 2
#define EEPROM_LOG_ENTRY_SIZE (4 + 120)
#define EEPROM_LOG_END_ADDRESS (EEPROM_LOG_START_ADDRESS + EEPROM_LOG_ENTRIES * EEPROM_LOG_ENTRY_SIZE)
#define EEPROM_LIGHT_LOG_DATA_SIZE 4
#define EEPROM_LIGHT_LOG_ENTRIES 4
#define EEPROM_LIGHT_LOG_START_ADDRESS (EEPROM_LOG_END_ADDRESS)

#define LED_ON_LOG_INTERVAL (1000*60*15)


enum {
  UI_RED_LED_OFF = 0 << 4,
  UI_RED_LED_BLINK = 1 << 4,
  UI_RED_LED_ON = 2 << 4,
  UI_BLUE_LED_OFF = 0 << 2,
  UI_BLUE_LED_BLINK = 1 << 2,
  UI_BLUE_LED_ON = 2 << 2,
  UI_WHITE_LED_OFF = 0 << 0,
  UI_WHITE_LED_BLINK = 1 << 0,
  UI_WHITE_LED_ON = 2 << 0,
};

enum { UI_LED_WHITE = 0, UI_LED_BLUE = 1, UI_LED_RED = 2 };

enum { UI_LED_OFF = 0, UI_LED_BLINK = 1, UI_LED_ON = 2 };

enum {
  EXPOSURE_LEVEL_0 = 0,
  EXPOSURE_LEVEL_1 = 1,
  EXPOSURE_LEVEL_2 = 2,
  EXPOSURE_LEVEL_3 = 3
};

enum uv_lamp_mode {
  UV_MODE_SMART = 0x00,
  UV_MODE_MANUAL = 0x01,
  UV_MODE_UNLIMITED = 0x02,
  UV_MODE_OCCUPIED_ROOM = 0x03,
  UV_MODE_EMPTY_ROOM = 0x04,
  UV_MODE_OFF = 0xFF,
};

class LightLogicControl {
public:
  LightLogicControl();
  void begin();
  void startPeriodicLightProcess();
  void initRadar();
  void checkRadarData(bool printData = false);
  void initThermalSensor();
  void checkThermalSensorData(bool printData = false);
  void initTemperatureSensor();
  float getTemperatureData();
  void initEEPROM();
  void initRTC();
  DateTime getCurrentRtcTime();
  uint32_t get8hourSectionStartTime();
  int eepromInitSection(uint32_t sectionStartTime);
  int eepromGetAccumulatedExposure(uint32_t sectionStartTime);
  int eepromWriteSection(uint32_t sectionStartTime, int julesLevel,
                         int dataPointIndex);
  String eepromGetSectionData(uint32_t sectionStartTime);
  int eepromGetNewestEntryLightOnData();
  uint32_t eepromReadLightOnData();
  int eepromWriteLightOnData(uint32_t lightOnTimeForLoggingInIntervalUnit);
  int processLightControl(bool scheduleEnabled, uint8_t* scheduleData);

  String getSensorDataJson();

  void update();
  void setLightState(bool state);
  bool getLightState();
  void setUiLedState(int ledIndex, int state);

  int processSensorInfo(bool printData = false);
  int julesLevelForDistance(int distance);

  const char *getCompiledTimeStr();

  bool lightState;
  Simple_Rd_03D radar1, radar2;
  bool radar1Valid, radar2Valid;

  Simple_MLX90640 thermalSensor;
  bool thermalSensorValid;
  struct Detection *thermalDetections;
  int thermalDetectionCount;

  Simple_TMP102 temperatureSensor;
  bool temperatureSensorValid;
  float temperatureData;

  Simple_DS3231 rtc;
  bool rtcValid;
  DateTime currentRtcTime;

  Simple_EEPROM_AT24C08 eeprom;
  bool eepromValid;

  int calculatedMinimalDistance;
  int minimalDistanceJulesLevel;

  uint32_t inProgress8hourSectionStartTime;
  int processedMinutesCount;

  int accumulatedExposure;
  int accumulatedExposureThreshold;

  int uvLampMode;

  uint32_t lightOnTimeForLoggingInIntervalUnit;
  uint32_t lightOnTimeLastCheckMilliseconds;
  uint32_t lightOnTimeNotLoggedYetMilliseconds;
};

#endif