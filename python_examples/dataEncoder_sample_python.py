# Python sample for the dataCodec library.
# This encoder example creates a packet with multiple fields and sends it
# to the serial device specified by SERIAL_DEVICE.

import time
import os
import sys
import math

# Add the library path so this example can import the local dataCodec module.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'python'))

import serial
from data_codec.data_codec import DataEncoder, TYPE, ERROR

# Serial device to send encoded packets to.
# Change this to your actual serial device path.
SERIAL_DEVICE = '/dev/ttyUSB0'
BAUDRATE = 115200

# Example data structure for the encoder.
enc_data = {
    'a': [False],
    'b': [0],
    'c': [0],
    'd': [0.0],
}


def init_encoder() -> DataEncoder:
    enc = DataEncoder(id_=0)
    enc.append(0, enc_data['a'], TYPE.BOOL)
    enc.append(1, enc_data['b'], TYPE.UINT, bits=2)
    enc.append(2, enc_data['c'], TYPE.INT, bits=10)
    enc.append(3, enc_data['d'], TYPE.FLOAT)
    result = enc.set()
    if result != ERROR.OK:
        raise RuntimeError(f'Encoder set() failed: {result}')
    print('Encoder initialized, packet layout set.')
    return enc


def update_value():
    enc_data['a'][0] = not enc_data['a'][0]
    enc_data['b'][0] = (enc_data['b'][0] + 1) & 0x03
    enc_data['c'][0] -= 1
    enc_data['d'][0] += math.pi


def main():
    enc = init_encoder()
    with serial.Serial(SERIAL_DEVICE, BAUDRATE, timeout=0.1) as ser:
        print(f'Starting encoder on {SERIAL_DEVICE} at {BAUDRATE} baud.')
        while True:
            update_value()
            error = enc.encode()
            if error == ERROR.OK:
                packet = enc.get_packet()
                data = packet.data[:packet.length]
                ser.write(data)
                print(f'Sent packet id={packet.id} len={packet.length} a={enc_data["a"][0]} b={enc_data["b"][0]} c={enc_data["c"][0]} d={enc_data["d"][0]:.6f}')
            else:
                print(f'Encode failed: {error}')
            time.sleep(0.1)


if __name__ == '__main__':
    main()
