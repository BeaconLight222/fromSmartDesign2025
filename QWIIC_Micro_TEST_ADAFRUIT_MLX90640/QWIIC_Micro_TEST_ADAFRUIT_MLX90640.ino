#include <Wire.h>
#include "Simple_MLX90640.h"

#define PRINT_TEMPERATURES 1

Simple_MLX90640 thermalSensor;

void setup() {
  Wire.begin();
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz
  // put your setup code here, to run once:
  SerialUSB.begin(115200);
  while (!SerialUSB) {
    delay(10);
  }
  SerialUSB.println("Hello, world!");
  // pinMode(7,OUTPUT);
  boolean ret = thermalSensor.begin(0x33);  // MLX90640 I2C address, MLX90640_ExtractParameters takes a long time but it is OK

  if (!ret) {
    SerialUSB.println("Failed to initialize thermal sensor!");
  } else {
    SerialUSB.println("Thermal sensor initialized successfully.");

    SerialUSB.print("Serial number: ");
    SerialUSB.print(thermalSensor.serialNumber[0], HEX);
    SerialUSB.print(thermalSensor.serialNumber[1], HEX);
    SerialUSB.println(thermalSensor.serialNumber[2], HEX);

    thermalSensor.setMode(MLX90640_CHESS);
    SerialUSB.print("Current mode: ");
    if (thermalSensor.getMode() == MLX90640_CHESS) {
      SerialUSB.println("Chess");
    } else {
      SerialUSB.println("Interleave");
    }

    thermalSensor.setResolution(MLX90640_ADC_18BIT);
    SerialUSB.print("Current resolution: ");
    mlx90640_resolution_t res = thermalSensor.getResolution();
    switch (res) {
      case MLX90640_ADC_16BIT: SerialUSB.println("16 bit"); break;
      case MLX90640_ADC_17BIT: SerialUSB.println("17 bit"); break;
      case MLX90640_ADC_18BIT: SerialUSB.println("18 bit"); break;
      case MLX90640_ADC_19BIT: SerialUSB.println("19 bit"); break;
    }

    thermalSensor.setRefreshRate(MLX90640_4_HZ);
    SerialUSB.print("Current frame rate: ");
    mlx90640_refreshrate_t rate = thermalSensor.getRefreshRate();
    switch (rate) {
      case MLX90640_0_5_HZ: SerialUSB.println("0.5 Hz"); break;
      case MLX90640_1_HZ: SerialUSB.println("1 Hz"); break;
      case MLX90640_2_HZ: SerialUSB.println("2 Hz"); break;
      case MLX90640_4_HZ: SerialUSB.println("4 Hz"); break;
      case MLX90640_8_HZ: SerialUSB.println("8 Hz"); break;
      case MLX90640_16_HZ: SerialUSB.println("16 Hz"); break;
      case MLX90640_32_HZ: SerialUSB.println("32 Hz"); break;
      case MLX90640_64_HZ: SerialUSB.println("64 Hz"); break;
    }
  }
}

void loop() {
  float frame[32*24];
  //delay(20);
  
  if (thermalSensor.getFrame(frame) != 0) {
    // 50+75ms for reading with 400K
    // 151+151ms for processing
    SerialUSB.println("Failed");
    return;
  }
  SerialUSB.println("===================================");
  SerialUSB.print("Ambient temperature = ");
  SerialUSB.print(thermalSensor.getTa(false));  // false = no new frame capture
  SerialUSB.println(" degC");
  SerialUSB.println();
  SerialUSB.println();
  for (uint8_t h = 0; h < 24; h++) {
    for (uint8_t w = 0; w < 32; w++) {
      float t = frame[h * 32 + w];
#ifdef PRINT_TEMPERATURES
      SerialUSB.print(t, 1);
      SerialUSB.print(", ");
#endif
#ifdef PRINT_ASCIIART
      char c = '&';
      if (t < 20) c = ' ';
      else if (t < 23) c = '.';
      else if (t < 25) c = '-';
      else if (t < 27) c = '*';
      else if (t < 29) c = '+';
      else if (t < 31) c = 'x';
      else if (t < 33) c = '%';
      else if (t < 35) c = '#';
      else if (t < 37) c = 'X';
      SerialUSB.print(c);
#endif
    }
    SerialUSB.println();
  }
}
