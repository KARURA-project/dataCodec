# Python sample for the dataCodec library.
# This decoder example reads bytes from the serial device specified by
# SERIAL_DEVICE, decodes packets, and prints decoded values for debugging.

import time
import os
import sys

# Add the library path so this example can import the local dataCodec module.
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'python'))

import serial
from data_codec.data_codec import DataDecoder, TYPE, ERROR

# Serial device to read encoded packets from.
# Change this to your actual serial device path.
SERIAL_DEVICE = '/dev/ttyUSB0'
BAUDRATE = 115200

# Example data structure for the decoder.
dec_data = {
    'a': [False],
    'b': [0],
    'c': [0],
    'd': [0.0],
}


def init_decoder() -> DataDecoder:
    dec = DataDecoder()
    dec.append(0, 0, dec_data['a'], TYPE.BOOL)
    dec.append(0, 1, dec_data['b'], TYPE.UINT, bits=2)
    dec.append(0, 2, dec_data['c'], TYPE.INT, bits=10)
    dec.append(0, 3, dec_data['d'], TYPE.FLOAT)
    result = dec.set()
    if result != ERROR.OK:
        raise RuntimeError(f'Decoder set() failed: {result}')
    print('Decoder initialized, packet layout set.')
    return dec


def try_decode(dec: DataDecoder):
    error = dec.decode()
    if error == ERROR.OK:
        # Use the internal packet object to get the last decoded packet ID.
        packet_id = dec._packet.id
        print(f'Decoded packet id={packet_id}')
        print(f' a = {dec_data["a"][0]}')
        print(f' b = {dec_data["b"][0]}')
        print(f' c = {dec_data["c"][0]}')
        print(f' d = {dec_data["d"][0]:.6f}')
        print('')
    elif error != ERROR.INCOMPLETE_PACKET:
        print(f'Decode error: {error}')


def main():
    dec = init_decoder()
    with serial.Serial(SERIAL_DEVICE, BAUDRATE, timeout=0.1) as ser:
        print(f'Starting decoder on {SERIAL_DEVICE} at {BAUDRATE} baud.')
        while True:
            data = ser.read(128)
            if data:
                for b in data:
                    dec.append_to_buffer(b)
                    try_decode(dec)
            time.sleep(0.01)


if __name__ == '__main__':
    main()
