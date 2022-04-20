############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import os
import sys
import code
import re
from datetime import datetime
import time
import atexit
from csr.wheels.global_streams import iprint
from csr.wheels import get_earlier_date
from csr.wheels.cmdline_options import CmdlineOption, CmdlineOptions

from .interact import logging_interact, daemonised_console_interact

if sys.version_info < (3,):
    from csr.wheels.py2_compatibility import _exec_cmd
else:
    def _exec_cmd(cmd, globals_dict, locals_dict=None):
        '''
        workaround to SyntaxError: unqualified exec is not allowed in function
        'main_wrapper' it contains a nested function with free variables
        '''
        #pylint: disable=exec-used
        if locals_dict is not None:
            exec(cmd, globals_dict, locals_dict)
        else:
            exec(cmd, globals_dict)

class BaseFrontEnd(object):

    cmdline_options = [
        CmdlineOption("-s", "--script-args",
                          help="Argument string to pass to script. *Note: this must be quoted*",
                          dest="script_args", default="",
                          is_multitoken=True),

        CmdlineOption("-x", "--execute",
                          help="Python program passed in as string. "
                               "*Note: this must be quoted*, "
                               "script_args is not passed to it. "
                               "Executed before python_script_file list.",
                          dest="execute",
                          default=None,
                          is_multitoken=True),

        ]

    @classmethod
    def cmdline_defaults(cls):
        
        return CmdlineOptions(cls.cmdline_options, custom_cmdline=[]).namespace
        

    @classmethod
    def _go_interactive(cls, shell=None):
        """\
        Drop into interactive python shell in the manner prescribed by the
        mixins included in the concrete subclass
        """
        
        cls.setup_cmdline(shell=shell)
        cls.interact(shell=shell)

    @staticmethod
    def execute_command(command, shell):
        '''
        Executes a user-supplied command-line string as a python command.
        '''
        locals_dict = {"__name__" : "__main__"}
        # note no definition of __file__
        iprint('Executing command line string "{}"'.format(command))
        _exec_cmd(command, shell, locals_dict)

    @classmethod    
    def main_wrapper(cls, shell=globals(), version=None):
        """
        Wrap main() with general exception handling and supply external inputs,
        i.e. the command line arguments and the interactive global shell.
        """
        
        try:
            cmdline_options = CmdlineOptions(cls.cmdline_options)
            
            interactive = False if cmdline_options.unknown_args else True
            if interactive and version is not None:
                iprint("This is Pydbg %s" % version)
            setup_ok = cls.main(cmdline_options, interactive, shell=shell)

            options = cmdline_options.values
            args = cmdline_options.unknown_args
            if setup_ok and (args or options["execute"]):
                # Run/Import any pydbg scripts specified on command line.
                #
                orig_argv = sys.argv
                orig_name = shell.get("__name__")
                orig_file = shell.get("__file__")
                # Create a function that can update "shell" (from this
                # scope) with an arbitrary namespace before invoking
                # the interactive interpeter 
                def go_interactive(local_shell=None):
                    if local_shell is not None:
                        shell.update(local_shell)
                    del shell["__name__"]
                    del shell["__file__"]
                    del shell["go_interactive"]
                    if orig_name is not None:
                        shell["__name__"] = orig_name
                    if orig_file is not None:
                        shell["__file__"] = orig_file

                    cls._go_interactive(shell=shell)
        
                if options["execute"]:
                    cls.execute_command(options["execute"], shell)
                for script in args:
                    _, ext = os.path.splitext(script)
                    if not ext:
                        # Reset the command line options
                        sys.argv = [script] + options["script_args"].split()
                        iprint("Running script: %s" % script)
                        if False:
                            try:
                                __import__('pydbg_scripts.' + script)
                            except Exception as e:
                                # Report all exceptions in scripts - but keep going
                                eprint("Error running '%s': %s" % (script, str(e)))
                        else:
                            __import__('pydbg_scripts.' + script)
                    elif ext == ".py":
                        # Make this function local to the execfile'd script
                        # and ensure run as if the main script when packaged
                        locals_dict = {"go_interactive" : go_interactive,
                                       "__name__" : "__main__",
                                       "__file__" : os.path.abspath(script)}
                        if options["script_args"]:
                            sys.argv = [script] + options["script_args"].split()
                        else:
                            sys.argv = [script]

                        shell.update(locals_dict)
                        _exec_cmd(compile(open(script).read(), script, 'exec'), shell)
                        
                        
                # Restore original arguments just to be neat and tidy
                sys.argv = orig_argv
                
            else:
                cls._go_interactive(shell=shell)
                
        except OSError as e:
            iprint("OS error: %s" % str(e)) 
            sys.exit(1)

class NullCmdlineMixin(object):
    """
    Mixin that add no command line management support beyond what's available in
    the Python interpreter by default
    """
    @classmethod
    def setup_cmdline(cls, shell=None):
        """
        Don't provide any special command line environment
        """
        pass
    
def _get_user_home():
    """
    Return the user's home directory, as indicated in the environment
    """
    if "HOME" in os.environ:
        home = os.environ["HOME"] # unix-style
        if os.path.isdir(home):
            return home
    if "USERPROFILE" in os.environ:
        home = os.environ["USERPROFILE"] # windows-style
        if os.path.isdir(home):
            return home
    if "HOMEPATH" in os.environ:
        home = os.environ["HOMEPATH"] # CSR-style
        if os.path.isdir(home):
            return home
    return None
    
class ReadlineCmdlineMixin(object):
    """
    Setup history and tab completion using readline
    """
    @classmethod
    def setup_cmdline(cls, shell=None):
        # If user has installed readline then give them access to it.
        #
        try:
            # Command line editing
            #
            import readline
        except ImportError:
            pass
        else:
            # Command history
            #
            home = _get_user_home()
            if home is None:
                iprint("WARNING: Couldn't load command history: invalid home "
                      "directory. Check HOME/USERPROFILE setting in your environment.")
                return

            histfile = os.path.join(home, ".device-debug-history")
            try:
                readline.read_history_file(histfile)
            except IOError:
                pass
            atexit.register(readline.write_history_file, histfile)
            del histfile
            
            # Tab completion
            #
            import rlcompleter
            if shell:
                readline.set_completer(rlcompleter.Completer(shell).complete)
            readline.parse_and_bind("tab: complete")

class PythonInspectInteractionMixin(object):
    """Mixin class for the PYTHONINSPECT method of dropping into an interactive
    session"""
    @classmethod
    def interact(cls, shell=None):
        """ Invoke interactive interpreter.
        
        ...by setting inspect mode then calling exit!
        
        This is a rather esoteric trick. Alternatives would be launch a subprocess
        like "python -i setupdebugenv" or find some way to explicitly invoke the
        interactive interpreter.
        This works with ipython.
        """
        os.environ['PYTHONINSPECT'] = '1'   


_logfile_regex = re.compile(r"session_(?P<year>\d{4})-(?P<month>\d{2})-"
                 r"(?P<day>\d{2})-(?P<hour>\d{2})(?P<minute>\d{2})_\d+.log")
_open_logfile_fmt = "session_{}.log"
_closed_logfile_fmt = "session_{year:0>4}-{month:0>2}-{day:0>2}-{hour:0>2}{min:0>2}_{pid:0>5}.log"
    
def handle_logfile_retention(logging_dir, now):
    """
    Implement the current logfile retention policy based on the environment,
    defaulting to "1week".
    Return True if logging should be enabled for this session, False if not.
    """
    log_rotation_policy_string = os.getenv("PYDBG_SESSION_LOG_RETENTION", 
                                           "1week").lower()
    match = re.match(r"((\d+)?(day|week|month|year)s?|never|forever)s?", 
                     log_rotation_policy_string)
    if not match:
        raise ValueError("Unrecognised value for PYDBG_SESSION_LOG_RETENTION "
                         "'%s'. Must be of form <n>day|week|month|year(s) or "
                         "'forever' or 'never'" % log_rotation_policy_string)

    to_delete = set()
    try:
        files_in_logging_dir = os.listdir(logging_dir)
    except (OSError, IOError):
        files_in_logging_dir = []
    if match.group(1) not in ("never", "forever"):
        time_unit = match.group(3)
        num_units = int(match.group(2)) if match.group(2) else 1
        cutoff = get_earlier_date(now, time_unit, num_units)
        # Look for files that are older than the cutoff time
        for f in files_in_logging_dir:
            logfile = os.path.join(logging_dir, f)
            
            m = re.match(_logfile_regex, f)
            if m:
                file_date = datetime(int(m.group("year")),
                                     int(m.group("month")),
                                     int(m.group("day")),
                                     int(m.group("hour")),
                                     int(m.group("minute")))
                if file_date < cutoff:
                    to_delete.add(logfile)
    elif match.group(1) == "never":
        # Delete all complete logs in the directory
        for f in files_in_logging_dir:
            logfile = os.path.join(logging_dir, f)
            
            m = re.match(_logfile_regex, f)
            if m:
                to_delete.add(logfile)
                    
    for logfile in to_delete:
        try:
            os.remove(logfile)
        except (IOError, OSError) as exc:
            iprint("WARNING: couldn't remove out-of-date log file '%s': %s" % 
                                                                (logfile, exc))
    
    return match.group(1) != "never"

def set_up_session_logfile(logging_dir):
    """
    Construct a name for the logfile based on the PID, and register it to be
    renamed to a fully timestamped name when the session exits.  The timestamp
    encoded in the name is what is used to determine which logfiles to delete
    when using time-bound retention policies.
    """
    open_logfile_name = os.path.join(logging_dir, _open_logfile_fmt.format(os.getpid()))
    # Register a function to close the logfile at exit and rename it to
    # show that it is historical
    def mark_logfile_closed():
        exit_time = datetime.fromtimestamp(time.time())
        closed_logfile_name = os.path.join(logging_dir,
                                           _closed_logfile_fmt.format(year=exit_time.year, 
                                                                      month=exit_time.month, 
                                                                      day=exit_time.day, 
                                                                      hour=exit_time.hour, 
                                                                      min=exit_time.minute,
                                                                      pid=os.getpid()))
        try:
            os.rename(open_logfile_name, closed_logfile_name)
        except (IOError, OSError) as exc:
            iprint("WARNING: couldn't rename logfile: %s" % str(exc))
        else:
            iprint("Saved session log as {}".format(closed_logfile_name))


    # Return the name we want the logfile to have, plus the exit-time function.
    # This will be invoked by the logging console as part of its clean-up, after 
    # ensuring that the destination of iprint is no longer associated with the 
    # logfile.
    return open_logfile_name, mark_logfile_closed

def handle_logging():
    """
    Sort out session logging in preparation for a logged interactive session.
     - Create the .pydbg_logs area, if necessary
     - If it already existed, clean up logfiles contained in it, according to the
     current PYDBG_SESSION_LOG_RETENTION policy
     - Create a new session logfile name and register it to be re-named to
     include the session close timestamp on exit from the session.
    """
    now = datetime.fromtimestamp(time.time())
    home = _get_user_home()
    if home is None:
        # Couldn't get a home dir.  Point this out unless the user has said they
        # don't want logging.
        if os.getenv("PYDBG_LOGFILE_RETENTION_POLICY") != "never":
            iprint("WARNING: Couldn't set up session logging: invalid home directory. "
                  "Check HOME/USERPROFILE setting in your environment.")
        return
    logging_dir = os.path.join(home, ".pydbg_logs")
    if not os.path.exists(logging_dir):
        os.makedirs(logging_dir)
    log_this_session = handle_logfile_retention(logging_dir, now)
    return set_up_session_logfile(logging_dir) if log_this_session else None
    

class CodeModuleInteractionMixin(object):
    """
    Mixing class for the code.interact method of dropping into an interactive
    session
    """
    @classmethod
    def interact(cls, shell=None):
        """
        Call code.interact with the given shell as the local namespace
        """
        logfile_name_and_exit_func = handle_logging()

        if logfile_name_and_exit_func is not None:
            logfile_name, exit_func = logfile_name_and_exit_func
            logging_interact(logfile_name, local=shell, log_file_exit_func=exit_func)
        else:
            code.interact(local=shell)
            
class DaemoniserInteractionMixin(object):
    """
    Mixing class for the daemoniser method of dropping into an interactive
    session
    """
    @classmethod
    def interact(cls, shell=None):
        """
        Call code.interact with the given shell as the local namespace
        """
        daemonised_console_interact(local=shell)
