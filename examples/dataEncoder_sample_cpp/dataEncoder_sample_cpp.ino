#include <Arduino.h>
#include <dataCodec.h>

// Example of using the C++ dataEncoder class from the dataCodec library.
// This sketch packs multiple values into one data packet and writes it out
// to the serial port every 100 ms.

dataEncoder enc(0);  // Encoder instance bound to port ID 0

struct Data {
  bool a;
  uint8_t b;
  int16_t c;
  float d;
};

Data encData;

void initEnc();
void updateValue();

void setup() {
  Serial.begin(115200);
  initEnc();
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  updateValue();

  // Encode the current values and send the packet over serial.
  ERROR error = enc.encode();
  if (error == ERROR::OK) {
    dataPacket_t p = enc.getPacket();
    Serial.write(p.data, p.length);
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    // Turn off the indicator if encoding failed.
    digitalWrite(LED_BUILTIN, LOW);
  }

  delay(100);
}

void initEnc() {
  // Register each data field in the packet in payload order.
  enc.append<bool>(0, &encData.a);

  // Use only 2 bits for this unsigned value. Valid transmitted values are 0..3.
  enc.append<uint8_t>(1, &encData.b, 2);

  // Use 10 bits for this signed integer value.
  enc.append<int16_t>(2, &encData.c, 10);

  // Float values are always encoded as 32-bit in this library.
  enc.append<float>(3, &encData.d);

  // Finalize the packet layout before encoding.
  enc.set();
}

void updateValue() {
  // Update the data fields before encoding the next packet.
  encData.a = !encData.a;
  encData.b += 1;
  encData.c -= 1;
  encData.d += PI;
}