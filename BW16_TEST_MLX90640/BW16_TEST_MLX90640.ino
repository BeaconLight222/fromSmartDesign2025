//compiled BW16 3.1.7

#define PRINT_TEMPERATURES 1

#include <Wire.h>
#include "src/Simple_MLX90640/Simple_MLX90640.h"
#include "src/mlx90640YoloProcessor/mlx90640YoloProcessor.h"

Simple_MLX90640 thermalSensor;


void setup() {
  delay(550);  //for auto upload

  //send SCK clocks on PA25 to unfreeze the I2C
  pinMode(PA12, OUTPUT);
  for (int i = 0; i < 10; i++) {
    digitalWrite(PA12, LOW);
    delayMicroseconds(10);
    digitalWrite(PA12, HIGH);
    delayMicroseconds(10);
  }
  pinMode(PA12, INPUT);

  Wire.begin();
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz

  Serial.begin(115200);

  boolean ret = thermalSensor.begin(0x33);  // MLX90640 I2C address, MLX90640_ExtractParameters takes a long time but it is OK

  if (!ret) {
    Serial.println("Failed to initialize thermal sensor!");
  } else {
    Serial.println("Thermal sensor initialized successfully.");

    Serial.print("Serial number: ");
    Serial.print(thermalSensor.serialNumber[0], HEX);
    Serial.print(thermalSensor.serialNumber[1], HEX);
    Serial.println(thermalSensor.serialNumber[2], HEX);

    thermalSensor.setMode(MLX90640_CHESS);
    Serial.print("Current mode: ");
    if (thermalSensor.getMode() == MLX90640_CHESS) {
      Serial.println("Chess");
    } else {
      Serial.println("Interleave");
    }

    thermalSensor.setResolution(MLX90640_ADC_18BIT);
    Serial.print("Current resolution: ");
    mlx90640_resolution_t res = thermalSensor.getResolution();
    switch (res) {
      case MLX90640_ADC_16BIT: Serial.println("16 bit"); break;
      case MLX90640_ADC_17BIT: Serial.println("17 bit"); break;
      case MLX90640_ADC_18BIT: Serial.println("18 bit"); break;
      case MLX90640_ADC_19BIT: Serial.println("19 bit"); break;
    }

    thermalSensor.setRefreshRate(MLX90640_4_HZ);
    Serial.print("Current frame rate: ");
    mlx90640_refreshrate_t rate = thermalSensor.getRefreshRate();
    switch (rate) {
      case MLX90640_0_5_HZ: Serial.println("0.5 Hz"); break;
      case MLX90640_1_HZ: Serial.println("1 Hz"); break;
      case MLX90640_2_HZ: Serial.println("2 Hz"); break;
      case MLX90640_4_HZ: Serial.println("4 Hz"); break;
      case MLX90640_8_HZ: Serial.println("8 Hz"); break;
      case MLX90640_16_HZ: Serial.println("16 Hz"); break;
      case MLX90640_32_HZ: Serial.println("32 Hz"); break;
      case MLX90640_64_HZ: Serial.println("64 Hz"); break;
    }
  }

  for (int i = 0; i < 2; i++) {
    float frame[32 * 24];
    thermalSensor.getFrame(frame);
    //discard first 2 frames
  }


}

void loop() {
  float frame[32 * 24];
  //delay(20);

  if (thermalSensor.getFrame(frame) != 0) {
    // 50+75ms for reading with 400K
    // 151+151ms for processing
    Serial.println("Failed");
    return;
  }
  float ambientTemp = thermalSensor.getTa(false);  // false = no new frame capture
  initMLX90640YoloProcessor();
  feedFloatMLX90640YoloProcessorAndRun(frame);
  deinitMLX90640YoloProcessor();
  struct Detection *detections = NULL;
  int detectionsCount = getDetectionsCount(&detections);

  Serial.print("Ambient temperature = ");
  Serial.print(ambientTemp);  // false = no new frame capture
  Serial.println(" degC");
  // Serial.println();
  // Serial.println();
#ifdef PRINT_TEMPERATURES
  for (uint8_t h = 0; h < 24; h++) {
    for (uint8_t w = 0; w < 32; w++) {
      float t = frame[h * 32 + w];
      Serial.print(t, 1);
      Serial.print(",");
    }
    Serial.println();
  }
#endif
  for (int i = 0; i < detectionsCount; i++) {
    Serial.print("Detection,");
    Serial.print(detections[i].x);
    Serial.print(",");
    Serial.print(detections[i].y);
    Serial.print(",");
    Serial.print(detections[i].w);
    Serial.print(",");
    Serial.print(detections[i].h);
    Serial.print(",");
    Serial.println(detections[i].conf);
  }
  Serial.println("===================================");
}
