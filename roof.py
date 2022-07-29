import argparse
import serial
import sys
import time

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('port', help='Port device', type=str)
    parser.add_argument('--open', help='Open the roof', action='store_true')
    parser.add_argument('--close', help='Close the roof', action='store_true')
    parser.add_argument('--heartbeat', help='Set a heartbeat value', default=-1, type=int)
    args = parser.parse_args()
    
    port = serial.Serial(args.port, 9600, 5)
    if args.open:
        port.write(b'\xf1')
    elif args.close:
        port.write(b'\xf2')
    elif 0 <= args.heartbeat <= 120:
        port.write(bytes([2*args.heartbeat]))
    try:
        while True:
            sys.stdout.write(port.readline().decode('ascii'))
    except KeyboardInterrupt:
        print('lll')
        port.write(b'\xf3')
        sys.stdout.flush()
