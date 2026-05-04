#include "LightLogicControl.h"
#include <GTimer.h>

#include "src/mlx90640YoloProcessor/mlx90640YoloProcessor.h"

volatile int gTimer0Counter = 0;
void gTimer0handler(uint32_t data) {
    (void) data;
    gTimer0Counter++;
    Serial.print("counter: ");
    Serial.println(gTimer0Counter);
}


LightLogicControl::LightLogicControl() {
    lightState = false;
    radar1Valid = false;
    radar2Valid = false;
    thermalSensorValid = false;
    thermalDetections = NULL;
    thermalDetectionCount = 0;
}

void LightLogicControl::begin() {
    pinMode(PA12, OUTPUT);
    update();
}

void LightLogicControl::startPeriodicLightProcess() {
    GTimer.begin(0, (1 * 1000 * 1000), gTimer0handler);
}

void LightLogicControl::initRadar() {
    radar1Valid = radar1.begin(0);
    if (radar1Valid){
        Serial.println("Radar1 initialized successfully");
    }else{
        Serial.println("Radar1 initialization failed");
    }
    //radar1.disableRadarStreaming();
    radar2Valid = radar2.begin(1);
    if (radar2Valid){
        Serial.println("Radar2 initialized successfully");
    }else{
        Serial.println("Radar2 initialization failed");
    }
}

void LightLogicControl::checkRadarData(bool printData) {
    if (radar1Valid){
        radar1.activeSerialMux();
        //radar1.enableRadarStreaming();
        int radarNewdata = radar1.checkRadarData(200);  //no enable/disable streaming, takes about 80ms, enable/disable streaming takes about 180ms
        if (radarNewdata){
            if (printData) {
                Serial.println("Radar1 data: ");
                for (int i = 0; i < radar1.radarObjectCount; i++){
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
        }else{
            if (printData) {
                Serial.println("No new data from Radar1");
            }
        }
        //radar1.disableRadarStreaming();
    }else{
        if (printData) {
            Serial.println("Radar1 is not valid, skipping data check.");
        }
    }

    if (radar2Valid){
        radar2.activeSerialMux();
        //radar2.enableRadarStreaming();
        int radarNewdata = radar2.checkRadarData(200);
        if (radarNewdata){
            if (printData) {
                Serial.println("Radar2 data: ");
                for (int i = 0; i < radar2.radarObjectCount; i++){
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
        }else{
            if (printData) {
                Serial.println("No new data from Radar2");
            }
        }
        //radar2.disableRadarStreaming();
    }else{
        if (printData) {
            Serial.println("Radar2 is not valid, skipping data check.");
        }
    }
}

void LightLogicControl::initThermalSensor() {
    thermalSensorValid = thermalSensor.begin(MLX90640_I2CADDR_DEFAULT); // MLX90640 I2C address, MLX90640_ExtractParameters takes a long time but it is OK
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
            //discard first 2 frames
        }

        if (initMLX90640YoloProcessor()==0) {
            //Serial.println("MLX90640 Yolo Processor initialized successfully");
        } else {
            Serial.println("Failed to initialize MLX90640 Yolo Processor");
        }
    } else {
        Serial.println("Thermal sensor initialization failed");
    }
}

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

            feedFloatMLX90640YoloProcessorAndRun(frame);
            thermalDetectionCount = getDetectionsCount(&thermalDetections);
        } else {
            if (printData) {
                Serial.println("Failed to get thermal sensor data");
            }
        }
    } else {
        if (printData) {
            Serial.println("Thermal sensor is not valid, skipping data check.");
        }
    }
}

void LightLogicControl::update() {
    
}

void LightLogicControl::setLightState(bool state) {
    lightState = state;
    digitalWrite(PA12, lightState ? LOW : HIGH);    //ballast on when Low
}

bool LightLogicControl::getLightState() {
    return lightState;
}

int LightLogicControl::processSensorInfo() {
    if (radar1Valid){
        if (radar1.radarObjectCount > 0) {
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
        } else {
            Serial.println("Radar1 detected no objects.");
        }
    }
    if (radar2Valid){
        if (radar2.radarObjectCount > 0) {
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
        } else {
            Serial.println("Radar2 detected no objects.");
        }
    }
    if (thermalSensorValid) {
        if (thermalDetectionCount > 0) {
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
        } else {
            Serial.println("Thermal sensor detected no objects.");
        }
    }

    return 0; // Return 0 for no objects detected, 1 for objects detected
}
