void setup() {
  // put your setup code here, to run once:

  Serial.begin(115200);
  PAD_PullCtrl(_PA_15, GPIO_PuPd_UP);
  PAD_PullCtrl(_PA_25, GPIO_PuPd_UP);
  PAD_PullCtrl(_PA_27, GPIO_PuPd_UP);
  PAD_PullCtrl(_PA_30, GPIO_PuPd_UP);
  PAD_PullCtrl(_PA_26, GPIO_PuPd_UP);

  Serial.println(PINMUX->PADCTR[_PA_15], HEX);
  Serial.println("==============");

  Pinmux_Config(_PA_15, PINMUX_FUNCTION_GPIO);

  PINMUX->PADCTR[_PA_15] = 0x1500;  //0.23V
  PINMUX->PADCTR[_PA_15] = 0x1D00;  //2.4V
  PINMUX->PADCTR[_PA_15] = 0x1900;  //2.4V

  pinMode(PA12, OUTPUT);
  pinMode(PB3, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:

  // Serial.println(PAD_BIT_PULL_RESISTOR_SMALL );
  // Serial.println(PINMUX->PADCTR[_PA_15],HEX);
  // Serial.println(PINMUX->PADCTR[_PA_15],HEX);
  // Serial.println(Pinmux_ConfigGet(_PA_15),HEX);
  // Serial.println(Pinmux_ConfigGet(_PA_25),HEX);
  // Serial.println(Pinmux_ConfigGet(_PA_27),HEX);
  // Serial.println(Pinmux_ConfigGet(_PA_30),HEX);
  // Serial.println(Pinmux_ConfigGet(_PA_26),HEX);

  // digitalWrite(PB3, HIGH);
  // digitalWrite(PA12, HIGH); //PA12 HIGH, no light
  // delay(5000);

  // digitalWrite(PB3, LOW);
  // digitalWrite(PA12, LOW);  //PA12 LOW, light
  // delay(1000);
  analogWritePeriod(80000);
  analogWrite(PA12, 128);  
  delay(1000);


}
