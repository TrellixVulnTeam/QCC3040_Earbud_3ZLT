############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides alternatives for the REPL behaviour for pydbg in the module functions
in module functions logging_interact and daemonised_console_interact.

Provides logging of console content via class LoggingStdout.
Provides an interactive console via whichever one of LoggingConsole or
DaemonisedConsole is used.

Users of daemoniser_pydbg.py (similar to pydbg but for use in a daemon) uses
DaemonisedConsole, whereas normal pydbg uses LoggingConsole.

The console object is made available in the locals dictionary passed
to the constructor of the LoggingConsole.  E.g. this can be the shell
dictionary supplied by pydbg, in which case console becomes a global object.
This aids debugging and also allow developer to remind themselves of the
filename of any logfile, access other properties etc.
"""
import sys
import code
import contextlib
from csr.wheels import gstrm, AnsiColourCodes

class LoggingStdout(gstrm.IStreamWrapper):
    """
    Standard output clone that copies anything passed to write() to a given
    file handle. It strips out any ANSI colour codes that might be present in
    the text written to stdout.
    """

    def __init__(self, real_stdout, logfile):
        self._stdout = real_stdout
        self._logfile = logfile
        self._ansi = AnsiColourCodes()

    def write(self, data, *args, **kwargs):
        """
        Implements a normal file-like write operation but writing data both
        to the logfile and the real_stdout (as as passed to the constructor).
        """
        self._logfile.write(self._ansi.strip_codes(data))
        self._logfile.flush()
        return self._stdout.write(data, *args, **kwargs)

    def __getattr__(self, attr):
        return getattr(self._stdout, attr)

    def __repr__(self):
        return "LoggingStdout(<logfile>, %s)" % repr(self._stdout)

    def __str__(self):
        return repr(self)

    # IStreamWrapper interface
    def replace_stream(self, prev, new):
        if self._stdout is prev:
            self._stdout = new
        elif isinstance(self._stdout, gstrm.IStreamWrapper):
            self._stdout.replace_stream(prev, new)

class LoggingConsole(code.InteractiveConsole):
    """
    Simple subclass of the standard interactive console that copies input and
    output lines to a file as well as to stdout/stderr.
    """

    def __init__(self, logfile, logging_stdout, real_stdout, *args, **kwargs):

        code.InteractiveConsole.__init__(self, *args, **kwargs)
        self._logfile = logfile
        self._logging_stdout = logging_stdout
        self._real_stdout = real_stdout

    def raw_input(self, prompt): # pylint: disable=signature-differs
        # Switch stdout here temporarily because the prompt output goes wrong
        # if we don't, and we're logging the prompt + input line separately
        # anyway
        sys.stdout = self._real_stdout
        data = code.InteractiveConsole.raw_input(self, prompt)
        sys.stdout = self._logging_stdout
        self._logfile.write(prompt+data+"\n")
        return data

    def write(self, data):
        """
        Write a string to the standard error stream (sys.stderr).
        Derived classes should override this to provide the appropriate
        output handling as needed.

        Here we write the data to the logfile as well as usual place.
        """
        self._logfile.write(data+"\n")
        code.InteractiveConsole.write(self, data)

class DaemonisedConsole(code.InteractiveConsole):
    """
    A subclass of code.InteractiveConsole that tries to ensure that the REPL
    prompt goes to same output channel as all other channels because we have
    a socket that expects pylib output and the REPL prompt to appear in an
    expected order rather than being weirdly intermingled.
    (The standard python REPL outputs to console stdout rather than the current
    setting of sys.stdout).

    Does this by :
    1. overrides raw_input, and flush all the new data from the string
    buffer down the socket
    2. sends a REPL prompt string down the socket as well in same manner
    along with newline so that it is not held back by daemoniser.py
    3. listens on the socket (stdin) for the next string, terminated by a
    newline
    4. returns that from raw_input()
    5. when output is sent to stderr, it goes to write(), which first flushes
    stdout and ensures that the "Traceback" line starts on a newline.

    A local echo (principally for debugging purposes) can be turned on via:
        console.echo = True

    Note this console assumes there is no output using colorama because it
    does not filter out the ansi codes from the stdout content. Given the
    context of use is typically it going to a test system log file, this can
    be controlled by not installing colorama on the test system.
    """

    def __init__(self, real_stdout, *args, **kwargs):
        code.InteractiveConsole.__init__(self, *args, **kwargs)
        self._stdout = real_stdout
        self._locals = kwargs['locals']
        self.console_output = None

        # By default does not echo the input command received back down
        # the daemon's socket, as doing so will affect any client program
        # parsing the content. But allow it to be turned on under program
        # control: that's one motivation for making console accessible in
        # locals/globals.
        self.echo = False
        self._stdout.write('Using DaemonisedConsole\n')

    def raw_input(self, prompt): # pylint: disable=signature-differs
        # First flush all the latest output down the socket
        # at same time as a REPL prompt string;
        # we need a newline for daemoniser.py to send on to TCL to match it
        # otherwise it sticks in the daemoniser tool buffer awaiting eol.
        # We send all this in one flushed operation to increase likelihood
        # that network delivers the whole lot to client program.

        self._stdout.write("\n"+prompt+"\n")
        self._stdout.flush()

        # listen on the socket (stdin) for the next string,
        # terminated by a newline
        try:
            data = raw_input()
        except NameError: # python3
            #pylint: disable=redefined-builtin, invalid-name
            data = input()

        # Don't echo data back down the socket by default
        # as this messes up parsing of the output
        if self.echo:
            self._stdout.write("(pydbg echo)"+prompt+data+"\n")
            self._stdout.flush()
        return data

    def write(self, data):
        """
        Write a string to the standard error stream (sys.stderr).
        Derived classes should override this to provide the appropriate
        output handling as needed.

        Note this routine is called three times when an exception is thrown
        1) to print the line starting with "Traceback"
        2) to print the actual traceback
        3) to print the final line starting with the exception error.

        Lest we are running with stdout buffered, first flush any stdout
        (which may fail to have a newline at the end,
        so if stderr data starts with "Traceback" then prefix a newline
        so that we get just the one necessary newline (just for case 1 above),
        and the client code can 'expect' "Traceback" to start at the beginning
        of the line.
        """

        self._stdout.flush()
        if data.startswith("Traceback"):
            data = '\n' + data
        code.InteractiveConsole.write(self, data)

def logging_interact(logfilename, local=None, log_file_exit_func=None):
    """
    Helper function providing a logged interactive console
    """

    # local is intended to be one of the python locals or globals dict, hence
    locals = local or {} # pylint: disable=redefined-builtin

    @contextlib.contextmanager
    def logging_console_active():
        real_stdout = sys.stdout
        # Open a logfile with the given name and create a LoggingStdout that
        # copies output to it
        logfile = open(logfilename, "w")
        logging_stdout = LoggingStdout(real_stdout, logfile)
        # Redirect stdout, both the built-in reference, and references held by
        # global_streams object
        sys.stdout = logging_stdout
        gstrm.replace_stream(real_stdout, logging_stdout)
        # Run the logging console
        yield LoggingConsole(
            logfile, logging_stdout, real_stdout, locals=locals)
        # Replace the redirection of stdout
        sys.stdout = real_stdout
        gstrm.replace_stream(logging_stdout, real_stdout)
        # Close the logfile and perform clean-up actions
        logfile.close()
        log_file_exit_func()

    with logging_console_active() as console:
        
        console.interact()

def daemonised_console_interact(local=None):
    """
    Helper function providing an interactive console
    using DaemonisedConsole instead of LoggingConsole.
    """

    # local is intended to be one of the python locals or globals dict, hence
    locals = local or {} # pylint: disable=redefined-builtin
    locals['console'] = DaemonisedConsole(sys.stdout, locals=locals)
    locals['console'].interact()
