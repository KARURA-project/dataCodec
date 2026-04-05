#include "dataCodec.h"

dataEncoder enc(0);
dataDecoder dec;

bool a = false;
uint8_t b = 1;
int16_t c = -1;
float d = 1.00;
double e = 1.00;

bool A = false;
uint8_t B = 1;
int16_t C = -1;
float D = 1.00;
double E = 1.00;

void setup() {
  Serial.begin(115200);
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
  while (Serial.available()) dec.appendToBuffer(Serial.read());
  while (dec.decode() == ERROR::OK) {
    a = !A;
    b = B + 1;
    c = C - 1;
    d = D * 1.01;
    e = E * 0.99;
    enc.encode();
    dataPacket_t dataPacket = enc.getPacket();
    for (uint8_t i = 0; i < dataPacket.length; i++) Serial.write(dataPacket.data[i]);
  }
}
