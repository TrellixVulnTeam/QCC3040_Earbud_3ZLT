#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Read packets from the KSP operator."""
import argparse
import sys
import threading
import time

from trbtrans.trbtrans import TrbError

from ksp._version import __version__ as version
from ksp.lib.trb import Ksp


def parse_arguments():
    """Parses script arguments and validates them.

    Returns:
        An argparse namespace.
    """
    parser = argparse.ArgumentParser(
        description=(
            "Kymera Stream Probe (KSP) TRB tool. \n"
            "It receives Audio/Data from a device URL and saves "
            "the output in a lrw file format."
        )
    )

    parser.add_argument(
        dest='device',
        type=str,
        help="A device to read from. i.e. 'trb:scar:0'",
    )

    parser.add_argument(
        dest='output',
        type=str,
        help=(
            "A filename which trb saves the data into it in LRW format. "
            "If the file exists, it will be overwritten."
        )
    )

    parser.add_argument(
        '-w',
        '--wait',
        dest='wait',
        type=float,
        default=3600,
        help=(
            "The waiting time to receive data in seconds. This value "
            "can be a decimal number. "
            "The application terminates if within this time no data has "
            "received. The default value is 3600 seconds or 1 hour."
        )
    )

    parser.add_argument(
        '-t',
        '--transactions',
        dest='transactions',
        type=int,
        default=100,
        help=(
            "number of transaction to poll each time. The default number "
            "is 100."
        )
    )

    parser.add_argument(
        '--version',
        action='version',
        version='KSP TRB Tool, v{}'.format(version)
    )

    parsed_arguments = parser.parse_args()
    _verify_arguments(parsed_arguments, parser)
    return parsed_arguments


def _verify_arguments(parsed_arguments, parser):
    if len(parsed_arguments.device) < 3:
        parser.error("Invalid device!")
    if parsed_arguments.device[:4].lower() != 'trb:':
        parser.error("Only trb device is supported. i.e. `trb:scar:0`")

    device = parsed_arguments.device[4:]
    if ':' in device:
        parsed_arguments.device, parsed_arguments.device_id = device.split(':')
        try:
            parsed_arguments.device_id = int(parsed_arguments.device_id)
        except ValueError:
            parser.error("Device ID must be in integer!")
    else:
        parsed_arguments.device = device
        parsed_arguments.device_id = None


def _set_trb_running(ksp_trb, output):
    stop_event = threading.Event()
    trb_thread = threading.Thread(
        target=ksp_trb.read_data,
        args=(
            output,
            stop_event,
        ),
        kwargs={'verbose': True}
    )

    trb_thread.start()
    try:
        while trb_thread.is_alive():
            time.sleep(0.1)

    except KeyboardInterrupt:
        if trb_thread.is_alive():
            stop_event.set()


def main():
    """Parses the arguments and get KSP TRB running to capture data."""
    args = parse_arguments()

    try:
        ksp_trb = Ksp(
            device=args.device,
            device_id=args.device_id,
            num_transactions=args.transactions,
            wait_time=args.wait
        )
    except TrbError as error:
        print(error)
        sys.exit(1)

    _set_trb_running(ksp_trb, args.output)


if __name__ == '__main__':
    main()
