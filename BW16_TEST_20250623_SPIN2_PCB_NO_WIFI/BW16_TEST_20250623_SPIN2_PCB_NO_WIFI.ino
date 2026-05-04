// compiled with 3.1.9

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

uint32_t lastWifiConnectedTime = 0;
bool bleAdvertisingForWifiConfig = false;
bool wifiWasConnected = false;
uint32_t inProgress8hourSectionStartTimePrevious = 0;

WDT wdt;

int checkButtonActivity() {
  int returnValue = 0;
  static bool buttonWasPressed = false;
  static bool buttonWasDebouncedPressed = false;
  static uint32_t buttonLastChangeTime = 0;
  static bool buttonWasLongPressed = false;
  bool buttonPressed = (digitalRead(PA15) == LOW);
  if (buttonPressed) {
    //just check if the button is pressed
    returnValue |= (1 << 0);
  }
  if (buttonWasPressed != buttonPressed) {
    //button state changed
    returnValue |= (1 << 1);
    buttonLastChangeTime = millis();
  }
  buttonWasPressed = buttonPressed;

  if (buttonWasDebouncedPressed != buttonPressed) {
    int buttonPressDuration = millis() - buttonLastChangeTime;
    if (buttonPressDuration > 50) {
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

  pinMode(PB3, OUTPUT);
  pinMode(PA14, OUTPUT);
  pinMode(PA13, OUTPUT);

  digitalWrite(PB3, LOW);
  digitalWrite(PA14, LOW);
  digitalWrite(PA13, LOW);

  pinMode(PA12, OUTPUT);
  digitalWrite(PA12, HIGH);  // Turn off UV light

  for (int i = 0; i < 3; i++) {
    digitalWrite(PB3, HIGH);
    digitalWrite(PA14, HIGH);
    digitalWrite(PA13, HIGH);
    delay(100);
    digitalWrite(PB3, LOW);
    digitalWrite(PA14, LOW);
    digitalWrite(PA13, LOW);
    delay(100);
  }

  Wire.begin();
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz

  Serial.begin(115200);

  wdt.InitWatchdog(30000);
  wdt.StartWatchdog();

  NonvolatileDataManager::initialize();

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

  if (lightControl.radar1Valid && lightControl.radar2Valid && lightControl.thermalSensorValid && lightControl.temperatureSensorValid && lightControl.rtcValid && lightControl.eepromValid) {

  } else {
    digitalWrite(PB3, HIGH);  //red
    for (int i = 0; i < 5; i++) {
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
    sys_reset();
    delay(100000);
  }

  pinMode(PA15, INPUT_PULLUP);
  PINMUX->PADCTR[_PA_15] = 0x1D00;  //2.4V, change from 5D00, disable PAD_BIT_SDIO_H3L1 change voltage from 1.6V to 2.4V. Maybe still some conflict but OK for now.
}

void loop() {
  // put your main code here, to run repeatedly:
  //delay(10000);

  //Dynamic memory size: 84608
  //Dynamic memory size: 84608
  //Dynamic memory size: 37120

  //Serial.print("Dynamic memory size: ");
  //Serial.println(os_get_free_heap_size_arduino());

  int buttonState = checkButtonActivity();
  if (buttonState & (1 << 5)) {
    Serial.println("Button long pressed");
  }


  lightControl.checkRadarData(false);
  lightControl.checkThermalSensorData(false);
  lightControl.getTemperatureData();
  lightControl.getCurrentRtcTime();

  lightControl.processSensorInfo(true);

  if (buttonState & (1<<0)) {
    digitalWrite(PA12, LOW); 

    digitalWrite(PB3, HIGH);
    digitalWrite(PA14, HIGH);
    digitalWrite(PA13, HIGH);
    delay(1000);
    digitalWrite(PB3, LOW);
    digitalWrite(PA14, LOW);
    digitalWrite(PA13, LOW);
  }else{

    digitalWrite(PA12, HIGH); 

    for (int i = 0; i < 3; i++) {
      if (i<lightControl.radar1.radarObjectCount){
        digitalWrite(PA13, HIGH); //white
      }

      if (i<lightControl.radar2.radarObjectCount){
        digitalWrite(PA14, HIGH); //blue
      }
      delay(100);
      digitalWrite(PB3, LOW);
      digitalWrite(PA14, LOW);
      digitalWrite(PA13, LOW);
      delay(100);
    }

    for (int i = 0; i < lightControl.thermalDetectionCount; i++) {
      digitalWrite(PB3, HIGH); //red
      delay(100);
      digitalWrite(PB3, LOW);
      delay(100);
    }
  }

  wdt.RefreshWatchdog();
}
