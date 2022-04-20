#
# \copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
#            All Rights Reserved.\n
#            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#\file
#\brief      Charger Case DFU script
#

import serial
import time
import argparse
from enum import Enum

global state
global state_time

class dfu_state(Enum):
    OFF = 0
    START = 1
    WAIT_FOR_READY = 2
    SEND_RECORD = 3
    WAIT_FOR_ACK = 4
    WAIT_FOR_COMPLETE = 5


def change_state(new_state):
    global state
    global state_time

    state = new_state
    state_time = time.time()


def make_s3(address, payload, payload_count):

    # Pad out payload if it is not divisible by 4 bytes.
    while (payload_count % 4):
        payload += 'FF'
        payload_count += 1

    rec = '{:02X}{:08X}'.format(payload_count + 5, address) + payload

    csum = 0
    rlen = len(rec)

    for n in range(0, rlen, 2):
        csum += int(rec[n:n+2], 16)

    csum = csum & 0xFF
    csum = csum ^ 0xFF

    rec = 'S3' + rec + '{:02X}'.format(csum)

    return rec


def resize(all_lines_orig, size):

    all_lines = []

    address = 0
    start_address = 0
    payload_count = 0
    payload = ''

    for line in all_lines_orig:
        line = line.rstrip()

        if line.startswith("S3"):

            line_address = int(line[4:12], 16)

            if address==0:
                address = line_address
                start_address = address
            else:
                if address != line_address:
                    # This record does not follow on immediately from
                    # the previous one, so we cannot add its payload
                    # to the data we have stored. We must first output
                    # the stored data.
                    if payload_count:
                        all_lines.append(make_s3(start_address, payload, payload_count))
                        payload_count = 0
                        payload = ''

                    address = line_address
                    start_address = address

            line_len = len(line)

            for n in range(12, line_len - 2, 2):
                payload += line[n:n+2]
                payload_count += 1
                address += 1
                if payload_count == size:
                    # We have accumulated enough payload data for one
                    # S3 record, so output it.
                    all_lines.append(make_s3(start_address, payload, payload_count))
                    payload_count = 0
                    payload = ''
                    start_address = address

        else:
            # Not an S3 record, but we may have some stored S3 payload
            # data, which we will need to output before the current
            # record.
            if payload_count:
                all_lines.append(make_s3(start_address, payload, payload_count))
                payload_count = 0
                payload = ''
                address = 0

            all_lines.append(line)

    return all_lines


def main(argv = []):

    global state
    global state_time

    start_time = time.time()

    parser = argparse.ArgumentParser(description='Charger Case DFU')
    parser.add_argument("port", help = "COM port")
    parser.add_argument("file", help = "Filename")
    parser.add_argument("-n", default=False, action="store_true", help = "Spurious NACKs")
    parser.add_argument("-o", default=False, action="store_true", help = "Override compatibility check")
    parser.add_argument("-t", default=False, action="store_true", help = "Disable timeout")
    parser.add_argument("-s", default=115200, type=int, help = "Speed")
    parser.add_argument("-p", default=192, type=int, help = "Payload size")
    args = parser.parse_args()

    # Force payload size to be divisible by 4.
    args.p &= ~0x3

    try:
        ser = serial.Serial(args.port, args.s, timeout=0.005)
    except:       
        try:
            ser = serial.Serial("\\\\.\\" + args.port, args.s, timeout=0.005)
        except:
            print('ERROR: Cannot connect to {}.'.format(args.port))
            exit(1)

    if ser:

        try:
            f = open(args.file, "r")
        except:
            print('ERROR: Cannot open {}.'.format(args.file))
            exit(1)

        if f:
            # Resize the list of S-records according to the payload size
            # argument.
            all_lines = resize(f.readlines(), args.p)

            change_state(dfu_state.START)
            line_no = 0
            s3_count = 0
            nack_count = 0
            nr_count = 0

            while state != dfu_state.OFF:
                try:
                    ser_line = ser.readline().decode()
                except:
                    print('ERROR: Cannot read from {}.'.format(args.port))
                    exit(1)

                if (str.find(ser_line, 'DFU: ') != -1):
                    if (str.find(ser_line, 'ACK') == -1):
                        print('\r{}'.format(ser_line), end='')
                else:
                    ser_line = ''

                if (str.find(ser_line, 'Error') != -1):
                    change_state(dfu_state.OFF)
                elif (str.find(ser_line, '------') != -1):
                    change_state(dfu_state.OFF)

                if (state==dfu_state.START):
                    opt = 0x01

                    if (args.t):
                        opt += 0x04

                    if (args.n):
                        opt += 0x08

                    if (args.o):
                        opt += 0x10

                    ser.write(("\r\ndfu " + format(opt, 'x') + "\r\n").encode())
                    change_state(dfu_state.WAIT_FOR_READY)

                elif (state==dfu_state.WAIT_FOR_READY):
                    if (str.find(ser_line, 'Ready') != -1):

                        # Edit out the S3 records that we are not using this
                        # time around.

                        rlen = len(all_lines)
                        s3_count = int((rlen - 2) / 2)

                        if (str.find(ser_line, '(A)') != -1):
                            # Running from A, so we don't need the A records.
                            del all_lines[1 : s3_count + 1]
                        else:
                            # Running from B, so we don't need the B records.
                            del all_lines[s3_count + 1 : rlen - 1]

                        change_state(dfu_state.SEND_RECORD)

                elif (state==dfu_state.SEND_RECORD):
                    print('\r{}/{}'.format(line_no, s3_count + 1), end='')
                    ser.write(all_lines[line_no].encode())
                    ser.write(b'\r\n')
                    change_state(dfu_state.WAIT_FOR_ACK)

                elif (state==dfu_state.WAIT_FOR_ACK):
                    if (str.find(ser_line, 'NACK') != -1):
                        nack_count += 1
                        change_state(dfu_state.SEND_RECORD)
                    elif (str.find(ser_line, 'ACK') != -1):
                        if (str.find(all_lines[line_no], 'S7') != -1):
                            print('')
                            change_state(dfu_state.WAIT_FOR_COMPLETE)
                        else:
                            line_no = line_no + 1
                            change_state(dfu_state.SEND_RECORD)
                    elif ((time.time() - state_time) > 0.1):
                        nr_count += 1
                        change_state(dfu_state.SEND_RECORD)

                elif (state==dfu_state.WAIT_FOR_COMPLETE):
                    if (str.find(ser_line, 'Complete') != -1):
                        change_state(dfu_state.OFF)

            print('NACK: {} NR: {}'.format(nack_count, nr_count))

    print('Time taken = {} seconds'.format(int(time.time() - start_time)))

    exit(0)


if __name__ == "__main__":
    main()
