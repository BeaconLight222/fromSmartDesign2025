#include "Simple_Rd_03d.h"

Simple_Rd_03D::Simple_Rd_03D() {
  serial_mux = 0;
  radarRecvBufferIndex = 0;
  radarDataLength = 0;
  radarObjectCount = 0;
}

boolean Simple_Rd_03D::begin(uint8_t _serial_mux) {
  serial_mux = _serial_mux;

  Serial1.flush();
  delay(1);

  pinMode(UART_MUX_A_PIN, OUTPUT);
  pinMode(UART_MUX_B_PIN, OUTPUT);
  digitalWrite(UART_MUX_A_PIN, (serial_mux & 0x01) ? HIGH : LOW);
  digitalWrite(UART_MUX_B_PIN, (serial_mux & 0x02) ? HIGH : LOW);

  Serial1.begin(256000);
  delay(1);
  while (millis() < 700) {
    // wait for radar port to be ready
  }

  int initAttempts = 0;

  while ((initAttempts++) < 3) {
    while (Serial1.available()) {
      Serial1.read();
    }
    // unsigned char dataMulti[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00, 0x90,
    // 0x00, 0x04, 0x03, 0x02, 0x01}; Serial1.write(dataMulti,
    // sizeof(dataMulti));
    unsigned char dataSingle[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00,
                                  0x80, 0x00, 0x04, 0x03, 0x02, 0x01};
    Serial1.write(dataSingle, sizeof(dataSingle));
    Serial1.flush();
    unsigned long startTime = millis();
    // expecting response: FD FC FB FA 04 00 90 01 01 00 04 03 02 01
    // unsigned char dataResponseExpected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04,
    // 0x00, 0x90, 0x01, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
    unsigned char dataResponseExpected[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04,
                                            0x00, 0x80, 0x01, 0x01, 0x00,
                                            0x04, 0x03, 0x02, 0x01};
    int bytesExpected = 14;
    char recvBuffer[14];
    int recvBufferIndex = 0;
    while (millis() - startTime < 100) {
      if (Serial1.available()) {
        char c = Serial1.read();
        if (recvBufferIndex < (int)(sizeof(recvBuffer))) {
          recvBuffer[recvBufferIndex++] = c;
          if (recvBufferIndex >= bytesExpected) {
            recvBufferIndex = 0;
          }
        }
        // Serial.print(c, HEX);
        // Serial.print(" ");
        // check if we have received the expected response
        uint8_t dataReceivedAsExpected = 1;
        int checkPtr = recvBufferIndex;
        for (int i = 0; i < (int)(sizeof(dataResponseExpected)); i++) {
          if (i != 8) {
            // byte 8 can be 0x00 or 0x01, so we skip the check
            if (recvBuffer[checkPtr] != dataResponseExpected[i]) {
              dataReceivedAsExpected = 0;
              break;
            }
          }
          checkPtr++;
          if (checkPtr >= bytesExpected) {
            checkPtr = 0;
          }
        }
        if (dataReceivedAsExpected) {
          // Serial.println("Received expected response");
          return true;
        }
      }
    }
  }

  return false;
}

void Simple_Rd_03D::activeSerialMux() {
  bool muxA = (serial_mux & 0x01) ? true : false;
  bool muxB = (serial_mux & 0x02) ? true : false;
  bool readedMuxA = digitalRead(UART_MUX_A_PIN);
  bool readedMuxB = digitalRead(UART_MUX_B_PIN);
  if (readedMuxA != muxA || readedMuxB != muxB) {
    Serial1.flush();
    delay(1);
    digitalWrite(UART_MUX_A_PIN, (serial_mux & 0x01) ? HIGH : LOW);
    digitalWrite(UART_MUX_B_PIN, (serial_mux & 0x02) ? HIGH : LOW);
    // clear input buffer
    while (Serial1.available()) {
      Serial1.read();
    }
  } else {
    return; // no need to change the mux
  }
}

void Simple_Rd_03D::disableRadarStreaming() {
  // drop the current consumption to 125mA
  // send command to disable radar streaming
  // try FD FC FB FA 04 00 FF 00 01 00 04 03 02 01
  unsigned char dataMulti[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x04, 0x00, 0xFF,
                               0x00, 0x01, 0x00, 0x04, 0x03, 0x02, 0x01};
  Serial1.write(dataMulti, sizeof(dataMulti));
  Serial1.flush();
}

void Simple_Rd_03D::enableRadarStreaming() {
  // increase the current consumption to 127mA
  // send command to enable radar streaming
  // try FD FC FB FA 02 00 FE 00 04 03 02 01
  unsigned char dataMulti[] = {0xFD, 0xFC, 0xFB, 0xFA, 0x02, 0x00,
                               0xFE, 0x00, 0x04, 0x03, 0x02, 0x01};
  Serial1.write(dataMulti, sizeof(dataMulti));
  Serial1.flush();
}

int Simple_Rd_03D::checkRadarData(int timeout) {
  // check if the mux changed
  // do digitalRead, then clear the buffer, etc, etc, TODO

  unsigned long startTime = millis();
  while ((int)(millis() - startTime) < timeout) {
    if (Serial1.available()) {
      char c = Serial1.read();
      radarRecvBuffer[radarRecvBufferIndex++] = c;
      radarDataLength++;
      if (radarRecvBufferIndex >= (int)(sizeof(radarRecvBuffer))) {
        radarRecvBufferIndex = 0;
      }
      // Serial.print(c, HEX);
      // Serial.print(" ");
      if (radarDataLength >= 30) {
        if (radarRecvBuffer[(radarRecvBufferIndex + 0) % 30] == 0xAA &&
            radarRecvBuffer[(radarRecvBufferIndex + 1) % 30] == 0xFF &&
            radarRecvBuffer[(radarRecvBufferIndex + 2) % 30] == 0x03 &&
            radarRecvBuffer[(radarRecvBufferIndex + 3) % 30] == 0x00) {
          // check if the last two bytes are 0x55 and 0xCC
          if (radarRecvBuffer[(radarRecvBufferIndex + 28) % 30] == 0x55 &&
              radarRecvBuffer[(radarRecvBufferIndex + 29) % 30] == 0xCC) {
            int detectedObjectCount = 0;
            for (int i = 0; i < 3; i++) {
              int offset = (radarRecvBufferIndex + 4 + i * 8) % 30;
              // we are going to check 4 of the 8 bytes, each 2 bytes is little
              // endian int16 type x is mm, y is mm, speed is cm/s, resolution
              // is mm need more optimization

              int16_t x = (radarRecvBuffer[(offset + 1) % 30] << 8) |
                          radarRecvBuffer[(offset + 0) % 30];
              if (x < 0)
                x = -32768 - x;
              int16_t y = (radarRecvBuffer[(offset + 3) % 30] << 8) |
                          radarRecvBuffer[(offset + 2) % 30];
              if (y < 0)
                y = -32768 - y;
              int16_t speed = (radarRecvBuffer[(offset + 5) % 30] << 8) |
                              radarRecvBuffer[(offset + 4) % 30];
              if (speed < 0)
                speed = -32768 - speed;
              uint16_t resolution = (radarRecvBuffer[(offset + 7) % 30] << 8) |
                                    radarRecvBuffer[(offset + 6) % 30];
              // check if the x, y, speed are all 0
              if (x == 0 && y == 0 && speed == 0) {
                // no target
                // Serial.println("No target");
              } else {
                radarObject_t *radarObjectPtr =
                    &radarObject[detectedObjectCount];
                radarObjectPtr->x = x;
                radarObjectPtr->y = y;
                radarObjectPtr->speed = speed;
                radarObjectPtr->resolution = resolution;
                detectedObjectCount++;
                if (detectedObjectCount >= 3) {
                  break;
                }
              }
            }

            radarObjectCount = detectedObjectCount;
            return 1;
          }
        }
      }
    }
  }
  radarObjectCount = -1; // no data received, sensor might be error
  return 0;
}