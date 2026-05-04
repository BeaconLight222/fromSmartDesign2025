#include "LightLogicControl.h"
#include <GTimer.h>

#include "src/mlx90640YoloProcessor/mlx90640YoloProcessor.h"

#include "NonvolatileDataManager.h"

const int exposureLevelToJules[4] = { 0, 825, 8000, 0 }; // Jules for exposure levels 0, 1, 2, 3

volatile int gTimer0Counter = 0;
void gTimer0handler(uint32_t data) {
    (void) data;
    gTimer0Counter++;
    // Serial.print("counter: ");
    // Serial.println(gTimer0Counter);
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
    uvLampMode = 0;
    accumulatedExposureThreshold = 20000;
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

        // if (initMLX90640YoloProcessor()==0) {
        //     //Serial.println("MLX90640 Yolo Processor initialized successfully");

        // } else {
        //     Serial.println("Failed to initialize MLX90640 Yolo Processor");
        // }
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
            if (initMLX90640YoloProcessor()==0){
                feedFloatMLX90640YoloProcessorAndRun(frame);
                deinitMLX90640YoloProcessor();
                thermalDetectionCount = getDetectionsCount(&thermalDetections);
            }else{
                Serial.println("Failed to allocate MLX90640 Yolo Processor");
            }
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

int LightLogicControl::processSensorInfo(bool printData) {
    if (radar1Valid){
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
    if (radar2Valid){
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
            int distance = radar1.radarObject[i].x * radar1.radarObject[i].x + radar1.radarObject[i].y * radar1.radarObject[i].y;
            distance = sqrt(distance); // Calculate Euclidean distance
            if (distance < minimalDistance) {
                minimalDistance = distance;
            }
        }
    }
    if (radar2Valid && radar2.radarObjectCount > 0) {
        for (int i = 0; i < radar2.radarObjectCount; i++) {
            int distance = radar2.radarObject[i].x * radar2.radarObject[i].x + radar2.radarObject[i].y * radar2.radarObject[i].y;
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
        String temperatureDataStr = String(temperatureData, 2);  // Format to 2 decimal places
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

    return calculatedMinimalDistance < INT_MAX ? 1 : 0; // Return 0 for no objects detected, 1 for objects detected
}

int LightLogicControl::julesLevelForDistance(int distance){
    //TODO: confirm relationship between distance and jules
    int julesLevel = EXPOSURE_LEVEL_0;

    if (distance <= 1000) {
        julesLevel = EXPOSURE_LEVEL_2;

    } else if (distance <= 2500) {
        julesLevel = EXPOSURE_LEVEL_1;

    } else {
        julesLevel = EXPOSURE_LEVEL_0;
    }

    return julesLevel;
}

String LightLogicControl::getSensorDataJson() {
    int radar1Count = radar1.radarObjectCount;
    int radar2Count = radar2.radarObjectCount;
    int thermalCount = thermalDetectionCount;
    
    String radar1Json = "";
    for (int i = 0; i < radar1Count; i++) {
        radar1Json += "{\"x\":" + String(radar1.radarObject[i].x) +
                      ",\"y\":" + String(radar1.radarObject[i].y) +
                      ",\"speed\":" + String(radar1.radarObject[i].speed) +
                      ",\"resolution\":" + String(radar1.radarObject[i].resolution) + "}";
        if (i < radar1Count - 1) {
            radar1Json += ",";
        }
    }
    radar1Json = "[" + radar1Json + "]";
    String radar2Json = "";
    for (int i = 0; i < radar2Count; i++) {
        radar2Json += "{\"x\":" + String(radar2.radarObject[i].x) +
                      ",\"y\":" + String(radar2.radarObject[i].y) +
                      ",\"speed\":" + String(radar2.radarObject[i].speed) +
                      ",\"resolution\":" + String(radar2.radarObject[i].resolution) + "}";
        if (i < radar2Count - 1) {
            radar2Json += ",";
        }
    }
    radar2Json = "[" + radar2Json + "]";
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
    thermalJson = "[" + thermalJson + "]"; 
    int rtcUnixtime = getCurrentRtcTime().unixtime();
    String rtcTimeStr = String(rtcUnixtime);
    rtcTimeStr = "\"" + rtcTimeStr + "\"";

    String temperatureDataStr = String(temperatureData, 2);  // Format to 2 decimal places
    if (isnan(temperatureData)) {
        temperatureDataStr = "null";  // Handle NaN case
    } else {
        temperatureDataStr = "\"" + temperatureDataStr + "\"";  // Wrap in quotes for JSON
    }

    String accumulatedExposureStr = String(accumulatedExposure);
    accumulatedExposureStr = "\"" + accumulatedExposureStr + "\"";  // Wrap in quotes for JSON

    String sendJson = "{\"rawdata\":{\"radar1\":" + radar1Json + ",\"radar2\":" + radar2Json + ",\"thermal\":" + thermalJson + ",\"temperature\":" + temperatureDataStr + ",\"rtcTime\":" + rtcTimeStr + ",\"accumulatedExposure\":" + accumulatedExposureStr + "},\"clientToken\":\"" + String(NonvolatileDataManager::getClientId()) + "\"}";

    return sendJson;
}

void LightLogicControl::initTemperatureSensor() {
    temperatureSensorValid = temperatureSensor.begin();
    if (temperatureSensorValid) {
        Serial.println("Temperature sensor initialized successfully");
    } else {
        Serial.println("Temperature sensor initialization failed");
    }
}
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

DateTime LightLogicControl::getCurrentRtcTime() {
    if (rtcValid) {
        currentRtcTime = rtc.now();
        return currentRtcTime;
    } else {
        Serial.println("RTC is not valid, returning default time.");
        return DateTime();  // Return default DateTime
    }
}

uint32_t LightLogicControl::get8hourSectionStartTime(){
    if (rtcValid) {
        currentRtcTime = rtc.now();
        uint32_t currentTime = currentRtcTime.unixtime();
        uint32_t sectionStartTime = currentTime - (currentTime % 300);  // 28800 seconds = 8 hours
        inProgress8hourSectionStartTime = sectionStartTime;
        return sectionStartTime;
    } else {
        Serial.println("RTC is not valid, returning default time.");
        return 0;  // Return 0 if RTC is not valid
    }
}

void LightLogicControl::initEEPROM() {
    eepromValid = eeprom.begin();
    if (eepromValid) {
        Serial.println("EEPROM initialized successfully");
    } else {
        Serial.println("EEPROM initialization failed");
    }
}

int LightLogicControl::eepromInitSection(uint32_t sectionStartTime){
    if (!eepromValid) {
        Serial.println("EEPROM is not valid, skipping section initialization.");
        return -1;  // Return -1 if EEPROM is not valid
    }

    //eeprom memory layout for log entries:
    //first #define EEPROM_LOG_START_ADDRESS bytes are used for other settings
    //then there are EEPROM_LOG_ENTRIES entries of EEPROM_LOG_ENTRY_SIZE bytes each
    //within each entry, the first 4 bytes are used for section start time
    //and the rest is used for log data

    uint32_t minimalStartTime = 0xFFFFFFFF;  // Initialize to a large value
    int minimalStartTimeSectionIndex = 0;  // Initialize to -1 to indicate no valid address found

    for (int i = 0; i < EEPROM_LOG_ENTRIES; i++) {
        uint16_t memAddress = EEPROM_LOG_START_ADDRESS + i * EEPROM_LOG_ENTRY_SIZE;
        uint8_t startLogTime[4] = {0};  // Initialize data to zero
        int ret = eeprom.read(memAddress, startLogTime, sizeof(startLogTime));
        if (ret < 0) {
            Serial.print("Failed to read EEPROM at address ");
            Serial.println(memAddress);
            return -1;  // Return -1 on read failure
        }
        // Check if the section start time matches
        uint32_t storedSectionStartTime;
        memcpy(&storedSectionStartTime, startLogTime, sizeof(storedSectionStartTime));
        if (storedSectionStartTime > sectionStartTime){ 
            storedSectionStartTime = 0;  // Reset if the stored time is greater than the section start time
        }
        if (storedSectionStartTime < minimalStartTime) {
            minimalStartTime = storedSectionStartTime;
            minimalStartTimeSectionIndex = i;
        }
        if (storedSectionStartTime == sectionStartTime) {
            Serial.print("Section already initialized at  ");
            Serial.println(i);
            return i;  // Return the address if section is already initialized
        }
    }

    //init the section with the minimal start time
    Serial.print("Initializing section at index ");
    Serial.println(minimalStartTimeSectionIndex);
    uint16_t memAddress = EEPROM_LOG_START_ADDRESS + minimalStartTimeSectionIndex * EEPROM_LOG_ENTRY_SIZE;
    uint8_t sectionData[EEPROM_LOG_ENTRY_SIZE] = {0};  // Initialize data to zero
    memcpy(sectionData, &sectionStartTime, sizeof(sectionStartTime));
    int ret = eeprom.write(memAddress, sectionData, sizeof(sectionData));
    if (ret < 0) {
        Serial.print("Failed to write EEPROM at address ");
        Serial.println(memAddress);
        return -1;  // Return -1 on write failure
    }

    return minimalStartTimeSectionIndex;
}

int LightLogicControl::eepromWriteSection(uint32_t sectionStartTime, int julesLevel, int dataPointIndex){
    if (!eepromValid) {
        Serial.println("EEPROM is not valid, skipping section write.");
        return -1;  // Return -1 if EEPROM is not valid
    }

    //eeprom memory layout for log entries:
    //first #define EEPROM_LOG_START_ADDRESS bytes are used for other settings
    //then there are EEPROM_LOG_ENTRIES entries of EEPROM_LOG_ENTRY_SIZE bytes each
    //within each entry, the first 4 bytes are used for section start time
    //and the rest is used for log data

    int sectionIndex = -1;
    for (int i = 0; i < EEPROM_LOG_ENTRIES; i++) {
        int16_t memAddress = EEPROM_LOG_START_ADDRESS + i * EEPROM_LOG_ENTRY_SIZE;
        uint8_t startLogTime[4] = {0};  // Initialize data to zero
        int ret = eeprom.read(memAddress, startLogTime, sizeof(startLogTime));
        if (ret < 0) {
            Serial.print("Failed to read EEPROM at address ");
            Serial.println(memAddress);
            return -1;  // Return -1 on read failure
        }
        // Check if the section start time matches
        uint32_t storedSectionStartTime;
        memcpy(&storedSectionStartTime, startLogTime, sizeof(storedSectionStartTime));
        if (storedSectionStartTime == sectionStartTime) {
            sectionIndex = i;
            break;  // Found the section
        }
    }

    if (sectionIndex < 0) {
        Serial.println("Section not found, cannot write data.");
        return -1;  // Return -1 if section is not found
    }

    //we use one bytes for 4 Jules levels
    int dataOffset = 4 + dataPointIndex/4;  // 4 bytes for section start time, then dataPointIndex byte for Jules level
    if (dataOffset >= EEPROM_LOG_ENTRY_SIZE) {
        Serial.println("Data point index out of bounds.");
        return -1;  // Return -1 if data point index is out of bounds
    }
    int readAddress = EEPROM_LOG_START_ADDRESS + sectionIndex * EEPROM_LOG_ENTRY_SIZE + dataOffset;
    uint8_t julesData = 0;  // Initialize data to zero
    int ret = eeprom.read(readAddress, &julesData, sizeof(julesData));
    if (ret < 0) {
        Serial.print("Failed to read EEPROM at address ");
        Serial.println(readAddress);
        return -1;  // Return -1 on read failure
    }
    // Set the Jules level
    julesData &= ~(0x03 << ((dataPointIndex % 4) * 2));  // Clear the bits for the current Jules level
    julesData |= (julesLevel & 0x03) << ((dataPointIndex % 4) * 2);
    ret = eeprom.write(readAddress, &julesData, sizeof(julesData));
    if (ret < 0) {
        Serial.print("Failed to write EEPROM at address ");
        Serial.println(readAddress);
        return -1;  // Return -1 on write failure
    }

    {
        //print debug info
        uint8_t readData[EEPROM_LOG_ENTRY_SIZE] = {0};  // Initialize data to zero
        ret = eeprom.read(EEPROM_LOG_START_ADDRESS + sectionIndex * EEPROM_LOG_ENTRY_SIZE, readData, sizeof(readData));
        if (ret < 0) {
            Serial.print("Failed to read EEPROM at address ");
            Serial.println(EEPROM_LOG_START_ADDRESS + sectionIndex * EEPROM_LOG_ENTRY_SIZE);
            return -1;  // Return -1 on read failure
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

    return 0;  // Success
}

int LightLogicControl::eepromGetAccumulatedExposure(uint32_t sectionStartTime){
     if (!eepromValid) {
        Serial.println("EEPROM is not valid, skipping section read.");
        return -1;  // Return -1 if EEPROM is not valid
    }

    //eeprom memory layout for log entries:
    //first #define EEPROM_LOG_START_ADDRESS bytes are used for other settings
    //then there are EEPROM_LOG_ENTRIES entries of EEPROM_LOG_ENTRY_SIZE bytes each
    //within each entry, the first 4 bytes are used for section start time
    //and the rest is used for log data

    int sectionIndex = -1;
    for (int i = 0; i < EEPROM_LOG_ENTRIES; i++) {
        int16_t memAddress = EEPROM_LOG_START_ADDRESS + i * EEPROM_LOG_ENTRY_SIZE;
        uint8_t startLogTime[4] = {0};  // Initialize data to zero
        int ret = eeprom.read(memAddress, startLogTime, sizeof(startLogTime));
        if (ret < 0) {
            Serial.print("Failed to read EEPROM at address ");
            Serial.println(memAddress);
            return -1;  // Return -1 on read failure
        }
        // Check if the section start time matches
        uint32_t storedSectionStartTime;
        memcpy(&storedSectionStartTime, startLogTime, sizeof(storedSectionStartTime));
        if (storedSectionStartTime == sectionStartTime) {
            sectionIndex = i;
            break;  // Found the section
        }
    }

    if (sectionIndex < 0) {
        Serial.println("Section not found, cannot read data.");
        return -1;  // Return -1 if section is not found
    }

    uint8_t allDataInSection[EEPROM_LOG_ENTRY_SIZE] = {0};  // Initialize data to zero
    int ret = eeprom.read(EEPROM_LOG_START_ADDRESS + sectionIndex * EEPROM_LOG_ENTRY_SIZE, allDataInSection, sizeof(allDataInSection));
    if (ret < 0) {
        Serial.print("Failed to read EEPROM at address ");
        Serial.println(EEPROM_LOG_START_ADDRESS + sectionIndex * EEPROM_LOG_ENTRY_SIZE);
        return -1;  // Return -1 on read failure
    }

    accumulatedExposure = 0;
    for (int i = 0; i < EEPROM_LOG_ENTRY_SIZE - 4; i++) {
        uint8_t julesData = allDataInSection[4 + i];
        for (int j = 0; j < 4; j++) {
            int julesLevel = (julesData >> (j * 2)) & 0x03;  // Extract the Jules level for each 2 bits
            if (julesLevel > 0) {
                accumulatedExposure += exposureLevelToJules[julesLevel];
            }
        }
    }
    return accumulatedExposure;
}

int LightLogicControl::processLightControl(){
    if (accumulatedExposure >= accumulatedExposureThreshold) {
        Serial.println("Accumulated exposure reached threshold, turning off light.");
        setLightState(false);
        return 0;  // Light turned off
    }else{
        Serial.println("Accumulated exposure is below threshold, turning on light.");
        setLightState(true);
        return 1;  // Light turned on
    }
}
