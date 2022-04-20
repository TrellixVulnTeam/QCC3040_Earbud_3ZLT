############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""A helper script to scan a directory and run ACAT with right parameters."""
import argparse
import os
import re
import sys

from ACAT._version import __version__
import ACAT

ACAT_BUILD_LOCATION = 'ACAT_BUILD_LOCATION'
ACAT_SCAN_LOCATION = 'ACAT_SCAN_LOCATION'

DESCRIPTION = """
Audio Coredump Analysis Tool Runner, v{}
""".format(__version__)

EPILOG = (
    "ACAT Runner is a helper script to scan for coredump, build and downloadable "
    "files and execute ACAT with them. The coredump file is an ``.xcd`` file, "
    "the build file is an ``.elf`` file and should follow the pattern of "
    "``kymera_[string]_audio.elf``, and any other ``.elf`` file(s) in the scan "
    "directory will be downloadables. User can set the scan location and build "
    "file location via system environment variables as well. These variables "
    "are {} and {}. If the scan location is not given, the current directory "
    "will be the scan location.".format(
        ACAT_SCAN_LOCATION, ACAT_BUILD_LOCATION
    )
)
BUILD_PATTERN = r'^kymera.*_audio\.elf$'


def argument_parser():
    parser = argparse.ArgumentParser(
        description=DESCRIPTION,
        epilog=EPILOG,
    )

    parser.add_argument(
        'connection_string',
        help="If connecting to a live chip, use this positional parameter "
             "to pass the transport to ACAT.",
        nargs='?',
        type=str,
        default=None,
    )

    parser.add_argument(
        '-b',
        '--build',
        help="The optional path to the build. This value will override the "
             "system environment value, if set. The environment variable for "
             "this option is {}.".format(ACAT_BUILD_LOCATION),
        dest='build'
    )

    parser.add_argument(
        '-s',
        '--scan',
        help="The optional path to the scan directory. This value will "
             "override the system environment value, if set. The environment "
             "variable for this option is {}.".format(ACAT_SCAN_LOCATION),
        dest='scan_location'
    )

    parser.add_argument(
        '-i',
        '--interactive',
        help="The optional path to run ACAT in interactive mode. This "
             "execution mode is incompatible with --write-html parameter.",
        dest='interactive',
        action='store_true'
    )

    parser.add_argument(
        '-w',
        '--write-html',
        help="The optional path to run ACAT and capture the output in HTML"
             "format. This option is incompatible with using the interactive "
             "mode.",
        dest='write_html'
    )

    parser.add_argument(
        '--version',
        action='version',
        version=DESCRIPTION
    )

    namespace = parser.parse_args()
    if namespace.write_html and namespace.interactive:
        parser.error(
            "Interactive mode (-i) and write html (-w) output parameters "
            "cannot use together."
        )

    return namespace


def find_files(base_directory, extension):
    files = []
    for folder_path, _, filenames in os.walk(base_directory):

        for filename in filenames:
            if filename[-len(extension):] == extension:
                files.append(os.path.join(folder_path, filename))

    return files


def get_builds_downloadables(elf_files):
    builds = []
    downloadables = []

    for elf_file in elf_files:
        filename = os.path.split(elf_file)[-1]
        if re.match(BUILD_PATTERN, filename.lower()):
            builds.append(elf_file)
        else:
            # The filename does not match the build filename pattern, assuming
            # it is a downloadable.
            downloadables.append(elf_file)

    return builds, downloadables


def main():
    acat_cmd = []
    arguments = argument_parser()

    if arguments.build:
        custom_build_location = arguments.build
    else:
        custom_build_location = os.environ.get(ACAT_BUILD_LOCATION)

    if arguments.scan_location:
        scan_location = arguments.scan_location
    else:
        # When there is no custom scan location, use the current directory.
        scan_location = os.environ.get(ACAT_SCAN_LOCATION, '.')

    elf_files = find_files(scan_location, '.elf')

    builds, downloadables = get_builds_downloadables(elf_files)
    if arguments.connection_string:
        # It is a live chip.
        acat_cmd.append('-s')
        acat_cmd.append(arguments.connection_string)
    else:
        xcd_files = find_files(scan_location, '.xcd')
        if not len(xcd_files):
            raise RuntimeError("No coredump file is found.")
        if len(xcd_files) > 1:
            xcd_files_str = '\n'.join(xcd_files)
            raise RuntimeError(
                "Too many coredump files found. There should be only one "
                "in the path. Found: \n{}".format(xcd_files_str)
            )

        acat_cmd.append('-c')
        acat_cmd.append(xcd_files[0])

    acat_cmd.append('-b')
    if custom_build_location:
        acat_cmd.append(custom_build_location)
    else:
        if not len(builds):
            raise RuntimeError("No builds found.")
        if len(builds) > 1:
            builds_str = '\n'.join(builds)
            raise RuntimeError(
                "More than one build found. There should be only one in the "
                "path. Found: \n{}".format(builds_str)
            )

        acat_cmd.append(builds[0])

    for downloadable in downloadables:
        acat_cmd.append('-j')
        acat_cmd.append(downloadable)

    if arguments.interactive:
        acat_cmd.append('-i')
    if arguments.write_html:
        acat_cmd.append('-w')
        acat_cmd.append(arguments.write_html)

    # Add the dual core option anyway. It is a harmless option if the target
    # is not dual core anyway.
    acat_cmd.append('-d')

    print('ACAT Parameters: {}'.format(' '.join(acat_cmd)))
    ACAT.main(acat_cmd)


if __name__ == '__main__':
    try:
        main()
        sys.exit(0)

    except RuntimeError as error:
        print(error)
        sys.exit(1)

    except KeyboardInterrupt:
        print("The script is interrupted. Quiting..")
        sys.exit(1)
