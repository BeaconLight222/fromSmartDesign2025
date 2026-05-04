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
  uint8_t *p = (uint8_t *)0x08000000; //km0_boot
  Serial.print("Dumping memory from 0x08000000\n");
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

  Serial.println("Dumping memory from 0x08004000\n");
  p = (uint8_t *)0x08004000; //km4_boot
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

  Serial.println("Dumping memory from 0x08006000\n");
  p = (uint8_t *)0x08006000; //km0_km4_app
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






  
}

void loop() {
  delay(100);
  digitalWrite(PB3, LOW);
  delay(100);
  digitalWrite(PB3, HIGH);
}
