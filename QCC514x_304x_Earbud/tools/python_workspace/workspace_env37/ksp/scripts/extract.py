#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Extract audio from a KSP lrw file."""
import argparse
import os
import sys

from ksp.lib.exceptions import StreamDataError
from ksp.lib.extractor import LrwStreamExtractor
from ksp.lib.pretty import PrettyDictionaryPrinter


def parse_arguments():
    """Parses script arguments and validates them.

    Returns:
        An argparse namespace.
    """
    parser = argparse.ArgumentParser(
        description=(
            "Kymera Stream Probe (KSP) extract tool. \n"
            "It extracts Audio/Data from a recorded KSP lrw file."
        )
    )

    parser.add_argument(
        dest='filename',
        type=str,
        help="A recorded KSP lrw file.",
    )

    parser.add_argument(
        '--output-pattern',
        dest='output_pattern',
        type=str,
        help="The pattern of output files."
    )

    parser.add_argument(
        '--sample-rates',
        '-s',
        dest='sample_rates',
        nargs="+",
        type=int,
        default=[0],
        help=(
            "Space separated sample rates in integer. Valid sample rates are "
            "between 8000 (8Khz) and 192000 (192Khz) inclusive. The first "
            "Sample rate will be applied to `Stream0`, the second one will be "
            "applied to `Stream1` and so on. Extra Sample Rates will be "
            "ignored."
            "\n\n"
            "i.e. If the given lrw file has only two streams but three sample "
            "rates are given, the third sample rate will be ignored."
            "\n\n"
            "If the sample rate is not provided, the script will produce multi "
            "channel raw file instead of wav file."
            "\n\n"
            "Please note, in case of DATA type streams, the script ignores "
            "sample rates."
        )
    )

    parser.add_argument(
        '--byte-swap',
        dest='byte_swap',
        action="store_true",
        help=(
            "Set this flag if KSP packets in the input file are formed of "
            "little endian 32-bit words. This should not be used against "
            "live captured KSP files using transaction bridge."
        )
    )

    arguments = parser.parse_args()

    # Validate sample rates.
    for sample_rate in arguments.sample_rates:
        if sample_rate != 0 and (sample_rate < 8000 or sample_rate > 192000):
            parser.error("Sample rate {} is invalid.".format(sample_rate))

    # Check whether the given filename exists or not.
    if os.path.isfile(arguments.filename) is False:
        parser.error("Please enter a valid lrw file name.")

    return arguments


def _extract(filename, sample_rates, byte_swap, output_pattern):
    extractor = LrwStreamExtractor(
        filename,
        byte_swap=byte_swap,
        sample_rates=sample_rates,
        gen_header_file=False
    )
    result = extractor.extract(output_pattern)

    printer = PrettyDictionaryPrinter(4, result, title="Streams:")
    printer.pprint()


def main():
    """Parses the arguments and do the extraction method."""
    arguments = parse_arguments()

    try:
        _extract(
            arguments.filename,
            arguments.sample_rates,
            arguments.byte_swap,
            arguments.output_pattern
        )
        return 0

    except StreamDataError as error:
        print(error)
        return 1


if __name__ == '__main__':
    sys.exit(main())
