############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""ACAT Entry point.

Audio Coredump Analysis Tool or ACAT is a pyhton package which is used for
analysing coredumps and live chips running the Kymera firmware.
"""
import os
import sys

from ACAT import acat_runner
from ACAT.Core import CoreUtils as cu
from ACAT.Core.CoreUtils import global_options
from ACAT.Core.argument_parser import AcatArgumentParser
from ACAT.Core.exceptions import UsageError
from ACAT.Core.logger import config_logger
from ACAT._version import __version__, version_info

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))


def parse_args(parameters=None):
    """
    See ACAT.CoreUtils.parse_args.

    @param parameters   Parameters in a list form to parse. When not provided
                        the default `sys.argv` will be used
    """
    cu.global_options.set_defaults()

    argument_parser = AcatArgumentParser(parameters)
    argument_parser.populate_global_options(cu.global_options)


def load_session(analyses=None):
    """
    See ACAT.CoreUtils.load_session.
    """
    return cu.load_session(analyses)


def do_analysis(session):
    """
    See ACAT.CoreUtils.do_analysis.
    """
    cu.do_analysis(session)

def reset_package():
    """
    Resets every global ACAT variable. Function used for test purposes.
    """
    cu.reset_package()


def main(parameters=None):
    """
    Execute the ACAT command line
    """
    _set_graphviz_path()
    parse_args(parameters)

    return_value = 0
    try:
        config_logger(global_options.verbose)
        session = load_session()
        do_analysis(session)

    except RuntimeError as err:
        print(str(err), file=sys.stderr)
        return_value = 2  # signal the error.

    return return_value


def _set_graphviz_path():
    """
    Add Graphviz path, if available, to the right location

    Search PATH environment and add graphviz path to the `sys.path`
    """
    part_of_package_name = 'graphviz'
    for path in sys.path:
        if part_of_package_name in path.lower():
            return

    for path in os.environ['PATH'].split(';'):
        if part_of_package_name in path.lower():
            sys.path.append(path)


def get_documents():
    """Gives documentation information."""
    doc_index = os.path.join(CURRENT_DIRECTORY, 'docs', 'index.html')
    return dict(
        title="ACAT {} Documentation".format(__version__),
        location=doc_index,
        summary="Audio Coredump Analysis Tool User Guide.",
        rank=1
    )


def acat_runner_script():
    """ACAT runner is to scan and populate the ACAT command."""
    try:
        acat_runner.main()
        sys.exit(0)

    except RuntimeError as error:
        print(error)
        sys.exit(1)

    except KeyboardInterrupt:
        print("The script is interrupted. Quiting..")
        sys.exit(1)
