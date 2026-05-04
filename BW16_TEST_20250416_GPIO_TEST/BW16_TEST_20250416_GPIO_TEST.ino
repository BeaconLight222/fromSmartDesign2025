//SCL PA25, SDA PA26, tested with HT16K33
//SERIAL1, TX PB1, RX PB2
//CH340 TXD->PA8 RX
//CH340 RXD->PA7 TX
#include <Wire.h>

void setup() {
    Wire.begin();

    Serial.begin(115200);
    while (!Serial);
    Serial.println("\nI2C Scanner");

    Serial1.begin(115200);
}

void loop() {
    byte error, address;
    byte x = 0;
    int nDevices;

    Serial.println("Scanning...");

    nDevices = 0;
    for (address = 1; address < 127; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        Wire.write(x);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.print(address, HEX);
            Serial.println("  !");

            nDevices++;
        } else if (error == 4) {
            Serial.print("Unknown error at address 0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0) {
        Serial.println("No I2C devices found\n");
    } else {
        Serial.println("done\n");
    }

    Serial1.println("OK\n");
    delay(1000); 
    Serial.println("Serial1:\n");
    Serial.println(Serial1.readString());

    delay(1000);           
}
