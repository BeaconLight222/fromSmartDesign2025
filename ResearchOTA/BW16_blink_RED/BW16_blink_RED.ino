void setup() {
  // put your setup code here, to run once:
  pinMode(PB3, OUTPUT);
  pinMode(PA14, OUTPUT);
  pinMode(PA13, OUTPUT);

  digitalWrite(PB3, HIGH);
  digitalWrite(PA14, HIGH);
  digitalWrite(PA13, HIGH);

  //digitalWrite(6, LOW);
}

void loop() {
  delay(100);
  //digitalWrite(PB3, LOW);
  //digitalWrite(6, LOW);
  GPIO_WriteBit(0x23, 0);
  delay(100);
  //digitalWrite(PB3, HIGH);
  //digitalWrite(6, HIGH);
  GPIO_WriteBit(0x23, 1);
}
