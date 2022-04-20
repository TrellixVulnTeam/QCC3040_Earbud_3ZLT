############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.fw.firmware import NoEnvAttributeError, Firmware
from csr.dev.fw.meta.i_firmware_build_info import IFirmwareBuildInfo, get_network_homes
import sys
import os
import functools
import inspect
import io

if sys.version_info >= (3, 0):
    # Py3, file built-in removed so we chack against IOBase
    file_type = io.IOBase
    from io import StringIO
else:
    file_type = file
    from StringIO import StringIO


def command_wrapper (command, core_name, name):
    """\
    Wraps a core command and translates structured return to plain text.
    Adds an output_file argument to wrapped core commands which allows
    redirection of output to either a file or to return a string.
    
    Future:
    - Consider allowing ansi adaptor option here.
    """
    @functools.wraps(command)
    def _wrapped_command(*args, **kwargs):
        if command is None:
            return
        
        close_file = False
        return_as_string = False
        try:
            # Determine how function output is to be handled
            output_file = kwargs.pop("output_file", None)
            copy_to_stdout = kwargs.pop("copy_to_stdout", None)
            if output_file is not None:
                if isinstance(output_file, file_type) and output_file.closed == False:
                    if copy_to_stdout:
                        out_streams = (output_file, gstrm.iout)
                    else:
                        out_streams = (output_file,)
                elif output_file == False:
                    # in-memory-file, supporting unicode or bytes;
                    # getvalue() will raise UnicodeError if top-bit set 8-bit bytes used
                    out_streams = (StringIO(),)
                    return_as_string = True
                elif isinstance(output_file,str):
                    out_file_stream = open(output_file, "w")
                    close_file = out_file_stream
                    iprint("(Copying output to '%s')" % os.path.realpath(output_file))
                    # When a filename has been passed, copy to stdout unless this
                    # has been explicitly rejected
                    if copy_to_stdout is None:
                        copy_to_stdout = True
                    if copy_to_stdout:
                        out_streams = (out_file_stream, gstrm.iout)
                    else:
                        out_streams = (out_file_stream,)
                else:
                    out_streams = (output_file,)
            else:
                out_streams = (gstrm.iout,)
    
            if inspect.isgeneratorfunction(command):
                try:
                    for structured_result in command(*args, **kwargs):
                        for out in out_streams:
                            TextAdaptor(structured_result, out)
                    for out in out_streams:
                        if isinstance(out, (StringIO, io.BytesIO)) and return_as_string:
                            return out.getvalue()
                except KeyboardInterrupt:
                    gstrm.iout.write("\nCommand interrupted by user\n")
                except IOError:
                    gstrm.iout.write("\nCommand interrupted by IO error (SPI disconnect?)")
            else:
                #Just a normal one-shot command
                structured_result = command(*args, **kwargs)
                out_string_io = None
                for out in out_streams:
                    if isinstance(out, (StringIO, io.BytesIO)) and return_as_string:
                        TextAdaptor(structured_result, out)
                        out_string_io = out.getvalue()
                    else:
                        TextAdaptor(structured_result, out)
                if out_string_io:
                    return out_string_io
        finally:
            if close_file:
                close_file.close()

    # Create a tweaked version of the wrapped command's argspec, which removes
    # "self" if it appears at the start of the argument names list.
    arg_spec = inspect.getargspec(command)
    arg_names = arg_spec[0]
    if arg_names and arg_names[0] == "self":
        arg_names = arg_names[1:]
    arg_spec = type(arg_spec)(arg_names, *arg_spec[1:])
    # Convert this into a core attribute call signature and make a useful 
    # docstring out of that
    argument_string = inspect.formatargspec(*arg_spec)
    # Add in output_file argument
    argument_string = argument_string[:-1] + ", output_file=None)"
    sig_string = "%s.%s%s" % (core_name, name, argument_string)
    marker = "*" * (len(sig_string) + len("Call as: "))
    docstring= """
Additional arguments provided as a core command:
:output_file (file, boolean) Pass an open file object to redirect any stdout/stderr output
to the given file. Set to False for the function to return a raw string containing the output.
This function will not close a passed file object.

(inserted into %s core command set as '%s')

%s
Call as: %s
%s
""" % (core_name, name, marker, sig_string, marker)
    # Reset the docstring of the decorated function to this
    if _wrapped_command.__doc__:
        _wrapped_command.__doc__ += docstring
    else:
        _wrapped_command.__doc__ = docstring
            
    return _wrapped_command
    
def _func_unavailable(name, unavail_msg):
    """
    Placeholder for function that is unavailable in this context.  Simply 
    prints a message to say that the function can't be used. 
    The nane of the real function that isn't available and the same message are
    inserted into the function attributes so that help() will say the right
    thing.
    """
    def function_unavailable(*args, **kwargs):
        iprint(unavail_msg)
    function_unavailable.__name__ = name
    function_unavailable.__doc__ = unavail_msg
    return function_unavailable
    
class CoreCommandSetManager (object):
    """\
    Manages population of a shell dictionary with objects (mostly debugging
    commands) published by device CPU cores.
    
    Reacts to changes in debugging focus (core/project) by changing the
    population of objects registered in the shell dictionary.
    
    Takes some care to avoid overwriting synonymous commands or deleting names
    that get rebound to other objects whilst it wasn't looking.
    
    This class is designed/constrained to replace the xap2emus/TargetManager in
    xap2emu.py to facilitate porting of xap2emus out of xIDE and new/refactored
    commands back in!
    
    It must work within and without xIDE. Hence it does not create a control or
    command to allow the user to change focus but reacts to such a command from
    elsewhere.
    """            
    def __init__(self, shell, gbl_prefix = None):
        """\
        Params:-
        - chip : The current Chip under debug. This determines the population
        of commands.
        - shell : the interactive command line shell dictionary - this manager
        will add/remove commands to/from this as it sees fit (e.g. as project 
        focus changes etc.). 
        
        Future:-
        - Pass Device (or DeviceManager) instead of Chip.        
        """
        self._shell = shell
        self._prefix = gbl_prefix
                
        # Dictionary of extensions currently installed to shell. 
        # Used to remove the exact same set we added and check shell names
        # weren't rebound by someone else!
        #
        self._installed_commands  = {}

            
    def change_focus(self, core=None):
        """\
        Removes currently installed core command set and installs new one.
        
        In xide should be hooked into xide project change event.
        """
        self._uninstall_commands()
        self._install_commands(core)

    # Private
    def _install_commands(self, core):
        """\
        Install the specified command set to the shell.
        
        ...and remember the set so we remove the exact same ones when 
        done with even if some dynamic magic happens to the command set
        in the interim (this is python).
        """
        assert not self._installed_commands

        # Set current core alias in shell
        self._shell['core'] = core
                
        if not core:
            return
        
        # Get a name for the core to use in docstrings
        try:
            core_name = core.nicknames[0]
        except (AttributeError, KeyError):
            core_name = "core"

        core_commands, exceps = core.core_commands
        
        for x_name, x_obj in core_commands.items():
            global_name = x_name
            if self._prefix:
                global_name = "_".join([self._prefix,global_name]) 

            # Trap command  already in shell
            if global_name in self._shell:
                continue

            if isinstance(x_obj, str):
                def getter_factory(name, code_string, exptd_exceps):
                    def getter(core):
                        """
                        Return a suitable callable: either the supplied string
                        successfully eval'd, or a generic "this function isn't
                        available" function that prints a suitable message and
                        then returns.
                        """
                        try:
                            self = core

                            return command_wrapper(eval(code_string),
                                                   core_name, name)
                        except Exception as e:
                            expected = [etype for etype in exptd_exceps
                                                        if isinstance(e, etype)]
                            if expected:
                                if isinstance(e, NoEnvAttributeError):
                                    # Special type of AttributeError raised by the firmware metaclass when
                                    # an attribute needs an env, but env creation failed
                                    return _func_unavailable(name, "'{}' unavailable: "
                                 "no matching ELF was found during auto-lookup".format(name))
                                elif (isinstance(e, AttributeError) and 
                                      ".fw." in code_string and 
                                      not isinstance(core.fw, Firmware)):
                                    # This is an attempt to run a command defined as a Firmware attribute 
                                    # with only a DefaultFirmware instance available
                                    try:
                                        get_network_homes(check=True)
                                    except IFirmwareBuildInfo.LookupDisabledException as exc:
                                        unavailable_msg = "auto-lookup is not available: {}".format(str(exc))
                                    else:
                                        unavailable_msg = "auto-lookup is not available"
                                    
                                    return _func_unavailable(name, "'{}' unavailable: "
                                    "no ELF was provided".format(name))
                                return _func_unavailable(name,
                                            "'%s' unavailable: %s" % (name, e))
                            else:
                                raise
                    return getter
                            
                # Insert a property that will call the function getter above
                # when the attribute on the core is invoked.
                if hasattr(type(core),"x_name"):
                    raise TypeError("Trying to insert core command '%s' into "
                                    "'%s', but class '%s' already has an "
                                    "attribute of that name!")
                setattr(type(core),x_name,
                        property(getter_factory(x_name, x_obj, exceps)))
            elif hasattr(x_obj, '__call__'):
                x_obj = command_wrapper(x_obj)
                self._shell[global_name] = x_obj
                self._installed_commands[global_name] = x_obj
    
    
    def _uninstall_commands(self):
        """\
        Remove previously installed commands from the shell.
        
        Some care is taken as previously registered extensions may have been 
        deleted or rebound (e.g. by users typing stuff at the command shell).
        """
        for x_name, x_obj in self._installed_commands.items():

            # Trap name no longer bound at all!
            try:
                shell_obj = self._shell[x_name]
            except KeyError:
                iprint("! Command %s already unbound !" % x_name)
                continue
                            
            # Trap name got rebound to some other object!
            if shell_obj is not x_obj:
                iprint("! Command  %s rebound !" % x_name)                
                continue
            
            # Safe to unbind it
            del self._shell[x_name]
        
        # Forget the lot
        self._installed_commands = {}
        
        
class XideCoreCommandSetManager(CoreCommandSetManager):
    """
    Overrides CoreCommandSetManager's __call__ method to ensure the _state_save
    and _state_restore functions are called
    """
    
    def __init__(self, xide_target, shell, gbl_prefix = None):
        
        CoreCommandSetManager.__init__(self, shell, gbl_prefix)
        self._xide_target = xide_target
        
    class _CommandWrapper(object):
        
        def __init__(self, xide_target, cmd):
            
            self._cmd = cmd
            self._xide_target = xide_target
                
        def __call__(self, *args, **kwargs):
            
            _state = self._xide_target._state_save()
            try:
                if inspect.isgeneratorfunction(self._cmd):
                    try:
                        for structured_result in self._cmd(*args, **kwargs):
                            TextAdaptor(structured_result, gstrm.iout)
                    except KeyboardInterrupt:
                        gstrm.iout.write("\nCommand interrupted by user\n")
                    except IOError:
                        gstrm.iout.write("\nCommand interrupted by IO error (SPI disconnect?)")
                else:
                    #Just a normal one-shot command
                    structured_result = self._cmd(*args, **kwargs)
                    TextAdaptor(structured_result, gstrm.iout)
            finally:
                self._xide_target._state_restore(_state)
    
    def _command_wrapper(self, cmd):
        """
        Factory method returns the right sort of _CommandWrapper
        """
        
        return self._CommandWrapper(self._xide_target, cmd)

