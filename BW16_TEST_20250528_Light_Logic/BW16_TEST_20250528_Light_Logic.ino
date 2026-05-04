#include <Wire.h>

#include "LightLogicControl.h"

LightLogicControl lightControl;

void setup() {

  //send SCK clocks on PA25 to unfreeze the I2C
  pinMode(PA12, OUTPUT);
  for (int i = 0; i < 10; i++) {
    digitalWrite(PA12, LOW);
    delayMicroseconds(10);
    digitalWrite(PA12, HIGH);
    delayMicroseconds(10);
  }
  pinMode(PA12, INPUT);

  pinMode(PB3, OUTPUT);
  pinMode(PA14, OUTPUT);
  pinMode(PA13, OUTPUT);

  digitalWrite(PB3, HIGH);
  digitalWrite(PA14, HIGH);
  digitalWrite(PA13, HIGH);

  Wire.begin();
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz

  Serial.begin(115200);

  lightControl.begin();
  lightControl.setLightState(false);  // Turn on the light
  Serial.println("Starting radar...");
  lightControl.initRadar();           // Initialize the radar sensors
  Serial.println("Radar started");

  Serial.println("Starting ir sensor...");
  lightControl.initThermalSensor();  // Initialize the thermal sensor
  Serial.println("IR sensor started");

  lightControl.startPeriodicLightProcess();  // Start periodic light process

  if (lightControl.radar1Valid && lightControl.radar2Valid && lightControl.thermalSensorValid) {
    digitalWrite(PA14, LOW);
  }else{
    digitalWrite(PB3, LOW);
    while(1){
      Serial.print("radar1Valid: ");
      Serial.print(lightControl.radar1Valid);
      Serial.print(" radar2Valid: ");
      Serial.print(lightControl.radar2Valid);
      Serial.print(" thermalSensorValid: ");
      Serial.println(lightControl.thermalSensorValid);
      delay(1000);
    }
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  //delay(10000);

  lightControl.checkRadarData(false);
  lightControl.checkThermalSensorData(false);
  lightControl.processSensorInfo();

  Serial.print("Dynamic memory size: ");
  Serial.println(os_get_free_heap_size_arduino());
}
