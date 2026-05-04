#ifndef LIGHT_LOGIC_CONTROL_H
#define LIGHT_LOGIC_CONTROL_H

#include <Arduino.h>
#include "src/Simple_Rd_03d/Simple_Rd_03d.h"
#include "src/Simple_MLX90640/Simple_MLX90640.h"
#include "src/Simple_misc_sensor/Simple_misc_sensor.h"

#define UV_LAMP_JULES_SCALE (100LLu)

#define EEPROM_LOG_START_ADDRESS 0x80
#define EEPROM_LOG_ENTRIES 2
#define EEPROM_LOG_ENTRY_SIZE (4+120)

enum {
    EXPOSURE_LEVEL_0 = 0,
    EXPOSURE_LEVEL_1 = 1,
    EXPOSURE_LEVEL_2 = 2,
    EXPOSURE_LEVEL_3 = 3
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
    int eepromWriteSection(uint32_t sectionStartTime, int julesLevel, int dataPointIndex);
    int processLightControl();

    String getSensorDataJson();

    void update();
    void setLightState(bool state);
    bool getLightState();

    int processSensorInfo(bool printData = false);
    int julesLevelForDistance(int distance);

    bool lightState;
    Simple_Rd_03D radar1,radar2;
    bool radar1Valid,radar2Valid;

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

};

#endif