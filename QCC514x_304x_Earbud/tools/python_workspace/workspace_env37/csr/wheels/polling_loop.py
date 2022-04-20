############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels import gstrm
from csr.wheels.bitsandbobs import detect_ctrl_z
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
import sys

_funcs_to_add = []
_funcs_to_remove = []

def poll_loop(show_backtrace_on_halt=False):
    '''
    Utility to call multiple polling loops in different modules so that
    they don't have to run exclusively. This can be used to run daemons
    that use test tunnel or appcmd or to get debug logs without problems
    of multiple threads accessing the same object.
    Polling functions should be registered with add_poll_function() and can
    be removed when finished with a call to \c remove_poll_function(). The
    loop can be stopped (with a Ctrl-Z) and restarted with another call
    to this poll_loop() function without nedeing clients to re-register.
    '''
    global poll_loop_functions

    _process_adds_and_removals()
    
    try:
        poll_loop_functions
    except NameError:
        return
        
    while True:
        if not poll_loop_functions:
            break
        # Create a list of values up front to ensure that things being added
        # or removed from poll_loop_functions during a loop doesn't cause
        # any problems.
        for poll_fn in list(poll_loop_functions.values()):
            try:
                output = poll_fn()
            except KeyboardInterrupt:
                if show_backtrace_on_halt:
                    raise
                return
            if isinstance(output, interface.Code):
                TextAdaptor(output, gstrm.iout)
            detect_ctrl_z()
        _process_adds_and_removals()


def _process_adds_and_removals():
    """
    Private function to edit the set of polling functions at the end of each
    loop through
    """
    global _funcs_to_add
    global _funcs_to_remove
    global poll_loop_functions
    try:
        poll_loop_functions
    except NameError:
        poll_loop_functions = dict()
    for name in _funcs_to_remove:
        try:
            del poll_loop_functions[name]
        except KeyError:
            pass
    for name, fn in _funcs_to_add:
        poll_loop_functions[name] = fn
    _funcs_to_add = []
    _funcs_to_remove = []


def add_poll_function(name, function):
    '''
    Add a polling function to the loop. Should be followed (at some point)
    by a call to poll_loop() to actually call the function. The name
    parameter is used to identify the instance so it may be later removed
    by a call to remove_poll_function().
    '''
    global _funcs_to_add
    _funcs_to_add.append((name, function))

def remove_poll_function(name):
    '''
    Remove a function by name from the list of functions that are polled
    by the poll_loop().
    '''
    global _funcs_to_remove
    _funcs_to_remove.append(name)
