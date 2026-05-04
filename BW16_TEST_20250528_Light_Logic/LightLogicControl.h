#ifndef LIGHT_LOGIC_CONTROL_H
#define LIGHT_LOGIC_CONTROL_H

#include <Arduino.h>
#include "src/Simple_Rd_03d/Simple_Rd_03d.h"
#include "src/Simple_MLX90640/Simple_MLX90640.h"


class LightLogicControl {
public:
    LightLogicControl();
    void begin();
    void startPeriodicLightProcess();
    void initRadar();
    void checkRadarData(bool printData = false);
    void initThermalSensor();
    void checkThermalSensorData(bool printData = false);

    void update();
    void setLightState(bool state);
    bool getLightState();

    int processSensorInfo();

    bool lightState;
    Simple_Rd_03D radar1,radar2;
    bool radar1Valid,radar2Valid;

    Simple_MLX90640 thermalSensor;
    bool thermalSensorValid;
    struct Detection *thermalDetections;
    int thermalDetectionCount;

};

#endif