#include <Arduino.h>
#include <dataCodec.h>

// Example of using the C++ dataDecoder class from the dataCodec library.
// This sketch reads incoming bytes from the serial port, decodes packets,
// and indicates success by lighting the LED.
// Note: if the serial line is connected directly to the encoder, serial
// printing is not useful and should be avoided.

dataDecoder dec(0);

struct Data {
  bool a;
  uint8_t b;
  int16_t c;
  float d;
};

Data decData;

void initDec();
void tryDecode();

void setup() {
  Serial.begin(115200);
  initDec();
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  while (Serial.available() > 0) {
    uint8_t byteIn = Serial.read();
    dec.appendToBuffer(byteIn);
    tryDecode();
  }
}

void initDec() {
  // Register the expected packet layout for packet ID 0.
  // The field order must match the encoder packet definition.
  dec.append<bool>(0, &decData.a);

  // Use only 2 bits for this unsigned field. Valid decoded values are 0..3.
  dec.append<uint8_t>(1, &decData.b, 2);

  // Use 10 bits for this signed integer field.
  dec.append<int16_t>(2, &decData.c, 10);

  // Float values are decoded as 32-bit float in this library.
  dec.append<float>(3, &decData.d);

  // Finalize the decoder layout before processing packets.
  dec.set();
}

void tryDecode() {
  ERROR error = dec.decode();
  if (error == ERROR::OK) {
    // Indicate successful packet decode by turning on the LED.
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    digitalWrite(LED_BUILTIN, LOW);
  }
}
