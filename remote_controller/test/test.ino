#include <SPI.h>

#define CSN 8
#define CE 7

void setup() {
  Serial.begin(9600);

  pinMode(CSN, OUTPUT);
  pinMode(CE, OUTPUT);

  digitalWrite(CE, LOW);
  digitalWrite(CSN, HIGH);

  SPI.begin();

  delay(100);

  // Read CONFIG register (0x00 | 0x1F = read command)
  digitalWrite(CSN, LOW);
  SPI.transfer(0x00); // CONFIG register
  byte result = SPI.transfer(0xFF);
  digitalWrite(CSN, HIGH);

  Serial.print("CONFIG = 0x");
  Serial.println(result, HEX);
}

void loop() {}
