#ifdef __cplusplus
extern "C" {
#endif

#include "flash_api.h"
#include "sys_api.h"

#ifdef __cplusplus
}
#endif

flash_t flash;

//refer a lot from https://github.com/MXV3A/BW16-OTA-Updates/blob/main/AnchorOTA.cpp

#define OTA_ADDRESS_1 0x6000
#define OTA_ADDRESS_2 0x206000

void setup() {
  // put your setup code here, to run once:
  pinMode(PB3, OUTPUT);
  pinMode(PA14, OUTPUT);
  pinMode(PA13, OUTPUT);

  digitalWrite(PB3, HIGH);
  digitalWrite(PA14, HIGH);
  digitalWrite(PA13, HIGH);

  delay(550);
  Serial.begin(115200);

  int otaIndex = ota_get_cur_index();
  Serial.print("Current OTA index: ");
  Serial.println(otaIndex);

  if (otaIndex == 1) {
    return;
  }

  for (int i = 0; i < 15; i++) {
    delay(1000);
    Serial.print("BOOT ");
    Serial.println(i);
  }

  uint32_t signature1 = 1, signature2 = 1;
  flash_read_word(&flash, OTA_ADDRESS_1, &signature1);  //flash parameter may not used in ameba-arduino-d/Arduino_package/hardware/system/component/common/mbed/targets/hal/rtl8721d/flash_api.c
  flash_read_word(&flash, OTA_ADDRESS_2, &signature2);

  Serial.print("Signature 1: ");
  Serial.println(signature1, HEX);
  Serial.print("Signature 2: ");
  Serial.println(signature2, HEX);


  uint8_t *p;  //km0_boot

  Serial.println("Dumping memory from 0x08006000");
  p = (uint8_t *)(0x08000000 + OTA_ADDRESS_1);  //km0_km4_app
  for (int i = 0; i < 0x100; i += 1) {
    uint8_t data = p[i];
    Serial.print("0x");
    if (data < 0x10) {
      Serial.print("0");
    }
    Serial.print(p[i], HEX);
    Serial.print(" ");
    if (i % 16 == 15) {
      Serial.println();
    }
  }

  Serial.println("Dumping memory from 0x08206000");
  p = (uint8_t *)(0x08000000 + OTA_ADDRESS_2);  //km0_km4_app
  for (int i = 0; i < 0x100; i += 1) {
    uint8_t data = p[i];
    Serial.print("0x");
    if (data < 0x10) {
      Serial.print("0");
    }
    Serial.print(p[i], HEX);
    Serial.print(" ");
    if (i % 16 == 15) {
      Serial.println();
    }
  }

  int blocksToCopyFromOta1ToOta2 = 100;
  int sameBlocksCount = 0;
  int differentBlocksCount = 0;
  for (int i = 0; i < blocksToCopyFromOta1ToOta2; i++) {
    uint8_t *ota1Ptr = (uint8_t *)(0x08000000 + OTA_ADDRESS_1 + i * 4096);
    uint8_t *ota2Ptr = (uint8_t *)(0x08000000 + OTA_ADDRESS_2 + i * 4096);
    int theBlockIsSame = 1;
    for (int j = 0; j < 4096; j++) {
      if (*ota1Ptr++ != *ota2Ptr++) {
        theBlockIsSame = 0;
        break;
      }
    }
    if (theBlockIsSame) {
      sameBlocksCount++;
    } else {
      differentBlocksCount++;
    }
  }

  Serial.print("Same blocks count: ");
  Serial.println(sameBlocksCount);
  Serial.print("Different blocks count: ");
  Serial.println(differentBlocksCount);

  Serial.println("Copying blocks from OTA 1 to OTA 2");
  for (int i = 0; i < blocksToCopyFromOta1ToOta2; i++) {
    uint8_t *ota1Ptr = (uint8_t *)(0x08000000 + OTA_ADDRESS_1 + i * 4096);
    uint8_t *ota2Ptr = (uint8_t *)(0x08000000 + OTA_ADDRESS_2 + i * 4096);
    int theBlockIsSame = 1;
    for (int j = 0; j < 4096; j++) {
      if (*ota1Ptr++ != *ota2Ptr++) {
        theBlockIsSame = 0;
        break;
      }
    }
    if (theBlockIsSame) {
    } else {
      Serial.print("Copying block ");
      Serial.print(i);
      Serial.println(" from OTA 1 to OTA 2");
      uint8_t swapData[4096];
      flash_erase_sector(&flash, OTA_ADDRESS_2 + i * 4096);
      flash_stream_read(&flash, OTA_ADDRESS_1 + i * 4096, 4096, swapData);
      flash_stream_write(&flash, OTA_ADDRESS_2 + i * 4096, 4096, swapData);
    }
  }

  for (int i = 0; i < 5; i++) {
    delay(1000);
    Serial.print("REBOOT ");
    Serial.println(i);
  }

  sys_reset();
}

void loop() {
  delay(100);
  digitalWrite(PB3, LOW);
  delay(100);
  digitalWrite(PB3, HIGH);
}
