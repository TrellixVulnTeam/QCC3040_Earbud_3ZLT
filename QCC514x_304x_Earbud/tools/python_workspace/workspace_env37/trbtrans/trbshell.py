# ***********************************************************************
# * Copyright 2014-2019 Qualcomm Technologies International, Ltd.
# ***********************************************************************

"""
This module overrides the standard Python command interpreter to add features
like a persistent history and auto completion. It imports the trbtrans module,
the trbutils module, and also adds a few handy shortcuts (implemented in the
push function below).
"""

from __future__ import print_function
from __future__ import absolute_import
import code
import os
import sys

try:
    import readline
except ImportError:
    try:
        # noinspection PyUnresolvedReferences
        import pyreadline as readline
    except ImportError:
        print("""Neither readline nor pyreadline available. trbshell requires readline functionality.
pyreadline can be installed using pip: <python executable> -m pip install pyreadline
Note that trbtrans can be used without trbshell as a normal Python package.""")
        sys.exit(1)

from trbtrans import trbtrans

try:
    # noinspection PyUnresolvedReferences
    from trbtrans import trbutils as trbutils_mod
except ImportError:
    # Treat this as non-fatal. If the utils aren't there, it doesn't prevent the core bindings working.
    trbutils_mod = None
    pass


def to_hex_fn(data):
    """
    For an iterable input, maps the input to a list of hex-strings constructed by formatting each element.
    For a single datum, returns a hex-string constructed by formatting the datum.
    """
    try:
        return ["{0:#x}".format(x) for x in data]
    except TypeError:
        return "{0:#x}".format(data)


class HistoryConsole(code.InteractiveConsole):
    def __init__(self, supplied_locals, hist_file):
        self.histfile = hist_file
        code.InteractiveConsole.__init__(self, locals=supplied_locals)
        from rlcompleter import Completer
        readline.parse_and_bind("tab: complete")
        # We supply the completer explicitly in this way so that completion works when
        # the namespace is not __main__ (imported as a module).
        readline.set_completer(Completer(supplied_locals).complete)

        try:
            readline.read_history_file(hist_file)
        except IOError:
            # On some versions of readline on Linux, a missing history file is treated as an exception.
            pass

    @staticmethod
    def display_help():
        print("""
    *************
    trbshell help
    *************

    You are sitting at a slightly modified interactive Python prompt.

    An object called trb has been created. Most functionality is accessed through it.
    For example, type 'trb.open("usb2trb")' to open a connection to a usb2trb dongle.
    The module trbutils has also been imported if it was found. This provides some extra utility functions.

    Type 'help(trb)' or 'trb help' to see the built in Python docstrings for the trb
    object.""")

    def push(self, line):
        """
        This function is called every time the user enters a line.
        """

        # Save the history every time because there seems to be no other way
        # to ensure that history is kept when the session is Ctrl+C'd.
        readline.write_history_file(self.histfile)

        # 'line' is in unicode. Convert line to ascii. Ignore non-ascii characters.
        line = line.encode("ascii", "ignore")
        # For Python 3, decode from bytes to str
        if not isinstance(line, str):
            line = line.decode()

        if line[-3:] == "hex":
            # If it ends in hex, wrap the line in a call to to_hex()
            line = line[:-3]
            line = "to_hex(" + line + ")"

        elif line[-4:] == "help":
            # Allow "help" to be supplied after an object or function
            # e.g. "trb.read help" as an convenient alternative to "help(trb.read)".
            if line == "help":
                self.display_help()
                line = ""
            else:
                line = line[:-4]
                line = "help(" + line + ")"

        elif line == "exit":
            sys.exit()

        # Give the line to the Python interpreter
        return code.InteractiveConsole.push(self, line)


def main():
    # We need these in locals() so we can make them available to the interactive console.
    trb = trbtrans.Trb()
    to_hex = to_hex_fn
    if trbutils_mod:
        trbutils = trbutils_mod

    hc = HistoryConsole(locals(), os.path.expanduser("~/.trbshell.history"))

    is64 = sys.maxsize > 2**31 - 1
    banner = """
    ************
      trbshell
    ************
    
    Running on Python {0}.{1}.{2} ({3}-bit)
    Type 'help' to learn more.
    Press Ctrl+D to quit\n""".format(
        sys.version_info.major,
        sys.version_info.minor,
        sys.version_info.micro,
        "64" if is64 else "32"
    )

    sys.ps1 = "\n\001\033[1;36m\002>>> \001\033[0m\002"

    try:
        # Python 3.6+: suppress default exit message
        hc.interact(banner, exitmsg='')
    except TypeError:
        # Python < 3.6 doesn't accept the exitmsg arg.
        hc.interact(banner)


if __name__ == "__main__":
    main()
