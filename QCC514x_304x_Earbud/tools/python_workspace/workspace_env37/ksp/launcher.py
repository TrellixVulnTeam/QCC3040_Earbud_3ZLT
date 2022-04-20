#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KSP Launcher.

Parses the arguments and execute the CLI.
"""
import argparse
import sys

import ksp.cli

from ksp._version import __version__ as version
from ksp.lib.argparse_actions import StreamArgParser
from ksp.lib.exceptions import ConfigurationError
from ksp.lib.logger import config_logger
from ksp.lib.types import RunMode


def parse_arguments():
    """Parses the given arguments to the script."""
    parser = argparse.ArgumentParser(
        description="Kymera Stream Probe (KSP)  is a powerful debugging tool "
                    "where user can probe internal nodes of a graph."
    )
    parser.add_argument(
        dest='device_url',
        type=str,
        help="A Device URL. i.e. 'trb:scar'",
    )
    parser.add_argument(
        '-f',
        '--firmware-build',
        dest='firmware_build',
        type=str,
        required=True,
        help="Firmware build directory required for Pydbg. It is advised to "
             "give the full path.",
    )

    parser.add_argument(
        '--interactive', '-i',
        action='store_const', dest='run_mode', const=RunMode.INTERACTIVE,
        default=RunMode.INTERACTIVE,
        help="Run interactively. Load the last successful interactive "
             "configuration, and prompt for further commands to edit the "
             "configuration and start recording. Enter 'help' for further "
             "information. This is the default if neither -i nor -b are "
             "specified."
    )
    parser.add_argument(
        '--batch', '-b',
        action='store_const', dest='run_mode', const=RunMode.NON_INTERACTIVE,
        help="Run non-interactively. This does not load or save the last "
             "interactive configuration, and requires --stream, --output "
             "and either --duration or --wait-input."
    )
    parser.add_argument(
        '--output', '-o',
        dest='output_file_name',
        type=str,
        help="Output file name for non-interactive operation"
    )
    parser.add_argument(
        '--stream', '-s',
        type=str,
        dest='stream_config',
        action=StreamArgParser,
        metavar='id:data_type:...',
        help="Configure a stream. "
             "This option can appear one or two times. "
             "The argument has the format "
             "<stream_id>:<data_format>:<key[=value]>:<key[=value]> "
             "with the following keys: "
             "tr=<transform list> (default for format TTR is 'all', "
             "other formats require this key), "
             "samples=<number> (no default), "
             "fs=<sampling rate> (no default), "
             "md|metadata[=y|n] (default 'n'), "
             "td|timed_data=<endpoint id> (no default). "
             "The values have the same formats as in the interactive mode, "
             "except transform IDs are separated with commas without spaces. "
    )
    parser.add_argument(
        '--duration', '-d',
        type=int,
        help="Run for a fixed duration, in seconds."
    )
    parser.add_argument(
        '--wait-input', '-w',
        action='store_true',
        dest='wait_input',
        help="Run until the enter key is pressed, "
             "resp. newline is written to stdin."
    )

    parser.add_argument(
        '--verbose',
        action='store_true',
        help="When set, extra logging information will appear on the screen "
             "and log files will be created in the specified log directory by "
             "logging configurations. "
    )

    parser.add_argument(
        '--remove-datetime',
        action='store_true',
        help="When set, KSP removes the Date-Time to the output filename. By "
             "default, KSP adds the date and the time of the recording to the "
             "filename. This option is available to batch mode."
    )

    parser.add_argument(
        '--use-builtin-capability',
        action='store_true',
        help="When set, use the built-in capability. Some chips benefit from "
             "having the KSP capability built into the silicon. For these "
             "cases, the user can enable the built-in capability option by "
             "using this argument. This option is available to batch mode."
    )

    parser.add_argument(
        '--version',
        action='version',
        version='Kymera Stream Probe Tool, v{}'.format(version)
    )

    arguments = parser.parse_args()
    try:
        config_logger(arguments.verbose)

    except ConfigurationError as error:
        parser.error(error)

    return arguments


def main():
    """Parses the given command line arguments and starts the KSP console."""
    return ksp.cli.main(parse_arguments())


if __name__ == '__main__':
    sys.exit(main())
