#include "src/Simple_misc_sensor/Simple_misc_sensor.h"



Simple_TMP102 tmp102;
Simple_DS3231 ds3231;  // DS3231 object, not used in this sketch but can be used later
Simple_EEPROM_AT24C08 eeprom;  // EEPROM object

void setup() {
  //send SCK clocks on PA25 to unfreeze the I2C
  pinMode(PA25, OUTPUT);
  for (int i = 0; i < 10; i++) {
    digitalWrite(PA25, LOW);
    delayMicroseconds(10);
    digitalWrite(PA25, HIGH);
    delayMicroseconds(10);
  }
  pinMode(PA25, INPUT);

  Wire.begin();
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz

  Serial.begin(115200);
  Serial.println("TMP102 Temperature Sensor Test");
  int temperatureSensorRet = tmp102.begin();
  if (temperatureSensorRet) {
    Serial.println("TMP102 initialized successfully.");
  } else {
    Serial.println("Failed to initialize TMP102.");
  }

  int ds3231Ret = ds3231.begin();  // Initialize DS3231, not used in this sketch but can be used later
  if (ds3231Ret) {
    Serial.println("DS3231 initialized successfully.");
  } else {
    Serial.println("Failed to initialize DS3231.");
  }

  int eepromRet = eeprom.begin();  // Initialize EEPROM, not used in this sketch but can be used later
  if (eepromRet) {
    Serial.println("EEPROM initialized successfully.");
  } else {
    Serial.println("Failed to initialize EEPROM.");
  }

}

void loop() {
  // put your main code here, to run repeatedly:

  

  static int didJobOnce = 0;
  if (millis() > 5000){
    if (didJobOnce == 0) {
      didJobOnce = 1;  // Set flag to indicate the job has been done once
      Serial.println("Checking RTC time synchronization with AWS timestamp...");
      uint32_t pretendAWStimestamp = 1750098534;
      DateTime now = ds3231.now();
      uint32_t currentRTCTimestamp = now.unixtime();
      int timeDifference = currentRTCTimestamp - pretendAWStimestamp;
      if (abs(timeDifference) > 10) {  // Check if the difference is more than 10 seconds
        Serial.println("RTC time is not synchronized with the expected timestamp.");
        Serial.print("RTC Timestamp: ");
        Serial.println(currentRTCTimestamp);
        Serial.print("Expected Timestamp: ");
        Serial.println(pretendAWStimestamp);

        DateTime expectedTime(pretendAWStimestamp);
        Serial.print("Setting RTC to expected time: ");
        Serial.println(expectedTime.timestamp());
        ds3231.adjust(expectedTime);  // Adjust RTC to the expected time
        Serial.println("RTC time has been adjusted to the expected timestamp.");
      } else {
        Serial.println("RTC time is synchronized with the expected timestamp.");
      }
      // do some EEPROM test
      delay(100);
      uint8_t eepromData[256];
      eepromData[0] = 0x55;
      eeprom.write(0, eepromData, 1);  
      eepromData[0] = 0xAA;
      eeprom.write(768, eepromData, 1);  
      for (int i = 0; i < 256; i++) {
        eepromData[i] = i;  // Fill the EEPROM data with sequential values
      }

      eeprom.write(130, eepromData, 256); 

      uint8_t readData[1024];
      eeprom.read(0, readData, 1024);  // Read back the data from EEPROM
      Serial.println("EEPROM data read:");
      for (int i = 0; i < 1024; i++) {
        if (i % 16 == 0) {
          Serial.println();  // New line every 16 bytes
          Serial.print("Address ");
          Serial.print(i);
          Serial.print(": ");
        }
        Serial.print(readData[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      eeprom.read(0, eepromData, 1);  // Read back the first byte
      Serial.print("First byte read from EEPROM: ");
      Serial.println(eepromData[0], HEX);  // Print the first byte in hexadecimal
      eeprom.read(768, eepromData, 1);  // Read back the byte at address 768
      Serial.print("Byte at address 768 read from EEPROM: ");
      Serial.println(eepromData[0], HEX);  // Print the byte at address

    }
  }else{
    float temperatureData = tmp102.checkTemperatureData();
    if (!isnan(temperatureData)) {
      Serial.print("Temperature: ");
      String temperatureDataStr = String(temperatureData, 2);  // Format to 2 decimal places
      Serial.print(temperatureDataStr);
      Serial.println(" °C");
    } else {
      Serial.println("Failed to read temperature data.");
    }

    // float ds3231Temperature = ds3231.getTemperature();  // Get temperature from DS3231
    // if (!isnan(ds3231Temperature)) {
    //   Serial.print("DS3231 Temperature: ");
    //   String ds3231TemperatureStr = String(ds3231Temperature, 2);  // Format to 2 decimal places
    //   Serial.print(ds3231TemperatureStr);
    //   Serial.println(" °C");
    // } else {
    //   Serial.println("Failed to read DS3231 temperature data.");
    // }

    DateTime now = ds3231.now();  // Get current time from DS3231
    Serial.print("Current Time: ");
    Serial.println(now.timestamp());  // Print current time in ISO 8601 format
    uint32_t currentTime = now.unixtime();  // Get current time in Unix timestamp format
    Serial.print("Current Unix Time: ");
    Serial.println(currentTime);  // Print current Unix time
    Serial.println("====================================");
  }



  delay(1000);  // Wait for a second before the next reading

}
