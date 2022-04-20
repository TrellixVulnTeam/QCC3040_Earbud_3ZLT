############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""Module to launch the logging tool.

Examples:

    Launch interactive ACAT environment attached to a USB Debug device at 100
    and get an AANC operator::

        $ aanclogger -d0 100
        >>> a = aanc.find_operators()[0]
        >>> a.display()

    Launch a live plotting environment attached to a TRB device at 152670 using
    a plot configuration::

        $ aanclogger -t0 152670 -p aanc_mono.json

"""

import argparse
import json
import os

import ACAT

from aanclogger._version import __version__ as version
from aanclogger.graph import LiveGraph
from aanclogger.multiproc import MyManager, StereoConnection, SingleConnection
from aanclogger.connect import Connection

def launch_acat(param_list):
    """Launch an ACAT session.

    param_list (list(str)): List of ACAT session parameters.
    """
    ACAT.parse_args(param_list)
    session = ACAT.load_session()
    ACAT.do_analysis(session)

def find_local_file(fname):
    """Search for a file in the package directory.

    If the input ``fname`` doesn't resolve as a file, attempt to locate it
    amongst the subdirectories in this package.

    Args:
        fname (str): Filename to find.

    Returns:
        str: full path to the file.

    Raises:
        ValueError: If the file isn't found.
    """
    if os.path.isfile(fname):
        return fname

    dirname = os.path.dirname(__file__)
    fname1 = os.path.join(dirname, fname)
    if os.path.isfile(fname1):
        return fname1

    resourcename = os.path.join(dirname, 'resources')
    fname2 = os.path.join(resourcename, fname)
    if os.path.isfile(fname2):
        return fname2

    raise ValueError("Unable to find file %s (checked at %s)" % (
        fname, dirname))

def parse_args():
    """Parse input arguments.

    Returns:
        argparse.Namespace: parsed input arguments.
    """
    parser = argparse.ArgumentParser(description='AANC launcher',
                                     epilog='See documentation for examples')
    parser.add_argument('--connection', '-c',
                        help='JSON connection configuration file')
    parser.add_argument('--plot', '-p', default=None,
                        help='JSON plot configuration file')
    parser.add_argument('--single', '-s', action='store_true',
                        help='Run without multi-session mode')
    parser.add_argument('--version',
                        action='version',
                        version='aanclogger, v{}'.format(version))

    return parser.parse_args()

def main(): # pylint: disable=too-many-branches
    """Main run function."""
    args = parse_args()

    connection = Connection(find_local_file(args.connection))
    param_list = connection.left_acat_args

    # If no plotting is enabled then just launch ACAT on the first connection
    if args.plot is None:
        param_list.append("-I")
        launch_acat(param_list)
        return

    # Command-line option "single" avoids instantiating the multi-processing
    # option, which can be useful for plotting variables that aren't exposed
    # directly in the ACAT analysis.
    if args.single:
        sti = SingleConnection(param_list)
    else:
        # For any plotting launch a subprocess for the first connection
        manager0 = MyManager()
        manager0.start()
        # member AANC is registered via "MyManager.register"
        aanc0 = manager0.AANC(param_list) # pylint: disable=no-member

        if connection.right_acat_args:
            manager1 = MyManager()
            manager1.start()
            # member AANC is registered via "MyManager.register"
            aanc1 = manager1.AANC(connection.right_acat_args) # pylint: disable=no-member
        else:
            aanc1 = None

        # Instantiate a stereo object that represents the two connections.
        # A stereo object enables the graph to access attributes from both
        # sides.
        sti = StereoConnection(aanc0, aanc1)

    # Launch a plot
    content = {}
    with open(find_local_file(args.plot), 'r') as fid:
        content = json.load(fid)

    if content:
        grph = LiveGraph(content, sti)
        grph.plot()
    else:
        print("No configuration dictionary loaded")

if __name__ == "__main__":
    main()
