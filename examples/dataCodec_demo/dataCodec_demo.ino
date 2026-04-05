#include "dataCodec.h"

dataEncoder enc(0);
dataDecoder dec;

bool a = false;
uint8_t b = 1;
int16_t c = -1;
float d = 1.00;
double e = 1.00;

bool A;
uint8_t B;
int16_t C;
float D;
double E;

void setup() {
  enc.append<bool>(0, &a);
  enc.append<uint8_t>(1, &b, 5);
  enc.append<int16_t>(2, &c, 9);
  enc.append<float>(3, &d);
  enc.append<double>(4, &e);
  enc.set();
  dec.append<bool>(0, 0, &A);
  dec.append<uint8_t>(0, 1, &B, 5);
  dec.append<int16_t>(0, 2, &C, 9);
  dec.append<float>(0, 3, &D);
  dec.append<double>(0, 4, &E);
  dec.set();
}

void loop() {
  a = !a;
  b++;
  c--;
  d *= 1.01;
  e *= 0.99;
  enc.encode();
  dataPacket_t dataPacket = enc.getPacket();
  Serial.print("raw data     : ");
  Serial.printf("%d, %d, %d, %f, %f\n", a, b, c, d, e);
  Serial.print("encoded data : 0x");
  for (uint8_t i = 0; i < dataPacket.length; i++) {
    Serial.printf("%02x", dataPacket.data[i]);
    dec.appendToBuffer(dataPacket.data[i]);
  }
  Serial.println("");
  dec.decode();
  Serial.print("decoded data : ");
  Serial.printf("%d, %d, %d, %f, %f\n\n", A, B, C, D, E);
  delay(1000);
}
