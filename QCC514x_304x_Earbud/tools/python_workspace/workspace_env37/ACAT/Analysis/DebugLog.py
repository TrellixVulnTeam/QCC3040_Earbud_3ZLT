############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Debug Log Analysis.

Analysis to read debug logs form the chip.
"""
import collections
import logging
import math
import os
import re
import sys
import threading
import time as import_time
from collections import deque
from functools import wraps

from . import Analysis
from ACAT.Core import Arch
from ACAT.Core import CoreUtils as cu
from ACAT.Core.exceptions import (
    UsageError, AnalysisError, DebugInfoNoVariableError,
    DebugLogFormatterStringError, DebugLogNotFound
)

try:
    # pylint: disable=redefined-builtin
    from future_builtins import hex
except ImportError:
    pass

# Strict are the ones that are definitely called within method, not strict
# may or may not be called
VARIABLE_DEPENDENCIES = {
    'not_strict': (
        '$_debugLogLevel', '$_debugBufferSizeMask', 'L_debugBufferPos'
    ),
    'one_strict': (('L_debugBuffer', 'L_debugBuffer1'),)
}

logger = logging.getLogger(__name__)


class DebugLogBuffer(collections.abc.Iterator):
    """
    Encapsulate a Debug Log buffer.

    Args:
        debuginfo: DebugInfo adapter instance.
        chipdata: ChipData instance.

    Raises:
        DebugLogNotFound: When the debug log variables are unavailable.
    """
    BUFFER_SIZE_MASK_ADDRESS = '$_debugBufferSizeMask'
    P0_POINTER_ADDRESS = 'L_debugBuffer'
    P1_POINTER_ADDRESS = 'L_debugBuffer1'
    WRITE_POINTER_INDEX_ADDRESS = 'L_debugBufferPos'

    DEBUG_STRINGS_START = '$DEBUG_STRINGS_START'
    DEBUG_STRINGS_END = '$DEBUG_STRINGS_END'

    def __init__(self, debuginfo, chipdata):

        self._chipdata = chipdata
        self._debuginfo = debuginfo

        try:
            if self._chipdata.processor == 0:
                self._pointer_address = self._debuginfo.get_var_strict(
                    self.P0_POINTER_ADDRESS
                ).address

            else:
                self._pointer_address = self._debuginfo.get_var_strict(
                    self.P1_POINTER_ADDRESS
                ).address

            size_mask_address = self._debuginfo.get_var_strict(
                self.BUFFER_SIZE_MASK_ADDRESS
            ).address
            # Buffer size has to be converted to addressable units since
            # ``get_data`` takes the size in addr units. ``debugBufferSizeMask``
            # is ``size - 1``.
            self._buffer_size = Arch.addr_per_word * (
                    self._chipdata.get_var_strict(size_mask_address).value + 1
            )

        except DebugInfoNoVariableError:
            raise DebugLogNotFound(
                "The firmware doesn't have debug log compiled in."
            )

        # The ``_write_pointer_index`` is pointing to a location in the buffer
        # which is about to be written by the firmware. However,
        # ``_current_index`` instance variable points to the last read location
        # by the DebugLogBuffer instance.
        self._current_index = None
        self._write_pointer_index = None

        self._started = False
        self._buffer = []

        try:
            self._dbg_str_start = self._debuginfo.get_constant_strict(
                self.DEBUG_STRINGS_START
            )
            self._dbg_str_end = self._debuginfo.get_constant_strict(
                self.DEBUG_STRINGS_END
            )
        except DebugInfoNoVariableError:
            self._dbg_str_start = None
            self._dbg_str_end = None

    def refresh(self):
        """
        Updates the buffer content and its write pointer location.
        """
        self._read_buffer()

    def __next__(self):
        """
        Iterate over the buffer content and report the content one by one.

        Raises:
            StopIteration: When there is no next item available.
            DebugLogFormatterStringError: When the content of the buffer is
                erroneous.
        """
        format_string = self._get_format_string()

        arguments = []
        for _ in range(format_string.count('%')):
            self._next()
            try:
                value = self._get_value(self._buffer[self._current_index])
                arguments.append(value)

            except DebugLogFormatterStringError:
                # Assume it is just a big number.
                arguments.append(self._buffer[self._current_index])

        return self._format_string(format_string, arguments)

    def _next(self):
        if self._current_index is None:
            next_index = self._write_pointer_index

        else:
            next_index = self._current_index + 1

        # Check for the buffer wrap.
        if next_index > (len(self._buffer) - 1):
            # Wrapped. Start from the beginning of the buffer.
            next_index = 0

        if next_index == 0 and self._current_index == 0:
            raise StopIteration("Buffer is empty.")

        if self._write_pointer_index == next_index and self._started:
            raise StopIteration("Nothing new in the buffer.")

        self._current_index = next_index
        self._started = True

    def _get_format_string(self):
        self._next()

        if self._buffer[self._current_index] == 0:
            self._next()

        value = self._get_value(self._buffer[self._current_index])
        if isinstance(value, str):
            return value

        else:
            # This is just to handle the first time read-forward of the buffer.
            return self._get_format_string()

    def _get_value(self, value):
        """Find the underlying value of the given value/address."""
        region = Arch.get_dm_region(value, False)

        if region == 'DBG_DWL':
            return self._read_from_downloadable(value)

        if region == 'DBG_PTCH':
            return self._read_from_patch(value)

        if region == 'DEBUG' and self._is_debug_string(value):
            return self._read_from_kymera(value)

        return value

    def _is_debug_string(self, address):
        if self._dbg_str_start is None or self._dbg_str_end is None:
            # The build doesn't have the specified constant, so we can't
            # meaningfully decide whether the address is in the Kymera's
            # debug string section. Returning True for backward
            # compatibility.
            return True


        if self._dbg_str_start.value <= address < self._dbg_str_end.value:
            return True

        return False

    def _read_from_downloadable(self, address):
        for download_bundle in self._debuginfo.get_downloadable_debuginfos():
            # The linker will put the debug messages for downloadable
            # capabilities to 0x15500000, but the elf for some reason
            # leaves it in 0x13500000. Count for the difference
            # 0x15500000 -  0x13500000 = 0x2000000#
            try:
                return download_bundle.debug_strings[address - 0x2000000]
            except KeyError:
                pass

        raise DebugLogFormatterStringError(
            "Cannot find {} debug string in any of the loaded "
            "downloadables.".format(hex(address))
        )

    def _read_from_patch(self, address):
        # Look at the patch elf.
        # No address conversation is needed; the patch is directly mapped.
        patch = self._debuginfo.get_patch_debuginfo()
        if patch is None:
            raise DebugLogFormatterStringError(
                "Cannot find {} debug string because the patch is not"
                "loaded.".format(hex(address))
            )

        else:
            try:
                # The linker will put the debug messages for the patch
                # to 0x14500000, but in the elf it will be in 0x13500000.
                # Count for the difference:
                #   0x14500000 -  0x13500000 = 0x1000000
                return patch.debug_strings[address - 0x1000000]
            except KeyError:
                raise DebugLogFormatterStringError(
                    "Cannot find {} debug string in the patch.".format(
                        hex(address)
                    )
                )

    def _read_from_kymera(self, address):
        try:
            kymera = self._debuginfo.get_kymera_debuginfo()
            return kymera.debug_strings[address]

        except KeyError:
            # Invalid pointer. This is probably caused by a buffer tear.
            raise DebugLogFormatterStringError(
                "Cannot find {} debug string.".format(hex(address))
            )

    def _format_string(self, formatter, arguments):
        """Searches for the formatter string in the debug information.

        Also checks the downloadable capabilities.

        Args:
            formatter: A formatter string.
            arguments: Arguments for the format string.
        """
        try:
            # python can't handle the '%p' format specifier, apparently.
            formatter = re.sub(r'%([0-9]*)p', r'0x%\g<1>x', formatter)
            formatted_str = self.format_c_string(
                formatter,
                arguments
            )
        except TypeError as error:
            # TypeError: %d format: a number is required, not str
            # Nasty special case: if we incorrectly interpreted a large number
            # as a string, the format specifier won't match here.
            if re.search("arguments number is required", str(error)):
                specifiers = re.findall(r"%([\d\w]+)", formatter)
                # if len(specifiers) != len(arguments) bad case
                for idx, val in enumerate(specifiers):
                    # If the specifier isn't a string, but we have a string in
                    # args, try to correct our mistake.
                    cs_available = re.search("[cs]", val)
                    if not cs_available and isinstance(arguments[idx], str):
                        # Replace the string with the original large number.
                        # That requires a search through all the debug strings
                        # to work out what the number was.
                        debug_strings = self._debuginfo.get_kymera_debuginfo().debug_strings
                        for addr, string in debug_strings.items():
                            if string == arguments[idx]:
                                arguments[idx] = addr
                                formatted_str = self.format_c_string(
                                    formatter,
                                    arguments
                                )
            else:
                formatted_str = (
                        "\n@@@@ ERROR:" + str(error) + "\n" +
                        "     formatter string: %s\n" % (formatter) +
                        "     arguments: %s\n" % (cu.list_to_string(arguments))
                )
        return formatted_str

    @staticmethod
    def format_c_string(formatter, arguments):
        """Format a C string formatter to Python string.

        Args:
            formatter (str): A formatter in C language.
            arguments (lst): List of arguments.

        Returns:
            str: Formatted string.
        """
        # Regular expression to match all the placeholders in a string formatter
        # written in C.
        c_formatters = r'(%(?:(?:[-+0 #]{0,5})(?:\d+|\*)?(?:\.(?:\d+|\*)\
        )?(?:h|l|ll|w |I|I32|I64)?[cCdiouxXeEfgGaAnpsSZ])| %%)'
        specifiers = tuple(
            m.group(1) for m in re.finditer(c_formatters, formatter, flags=re.X)
        )

        for specifier_pattern in (r'^\%\d*d$', r'^\%\d*i$'):
            # Converts unsigned integer to signed integer for %d and %i
            # specifiers.
            for index, specifier in enumerate(specifiers):
                if re.match(specifier_pattern, specifier):
                    arguments[index] = cu.u32_to_s32(arguments[index])

        return formatter % tuple(arguments)

    def _read_buffer(self):
        self._write_pointer_index = self._chipdata.get_data(
            self._debuginfo.get_var_strict(
                self.WRITE_POINTER_INDEX_ADDRESS
            ).address
        )

        self._buffer = self._chipdata.get_data(
            self._pointer_address,
            self._buffer_size
        )


class LogController(object):
    """Control the debuglog enable/disable with context manager.

        With this class the debuglog enable/disable can be encapsulated in
        a "with" statement.
    """

    def __init__(self, analysis, sleep_time=0):
        # init function of the controller.
        self.debuglog_was_enabled = False
        self.analysis = analysis
        self.sleep_time = sleep_time

    def __enter__(self):
        # Debug logging can cause erroneous profiling in many ways*, for
        # simplicity always disable it. The SPI can wake up the chip or
        # the SPI bandwidth comes to a limit.
        if self.analysis.interpreter.get_analysis(
                "debuglog", self.analysis.chipdata.processor
            ).logging_active():
            print("Disable logging for the profiling")
            self.debuglog_was_enabled = True

            self.analysis.interpreter.get_analysis(
                "debuglog", self.analysis.chipdata.processor
            ).stop_polling()

            if self.sleep_time != 0:
                print("Sleep for %d" % self.sleep_time)
                import_time.sleep(self.sleep_time)

        # we don't need to return any resource
        return None

    def __exit__(self, *_):
        # re- enable debuglog if it was disabled.
        if self.debuglog_was_enabled:
            print("Re-enable logging for the profiling")
            self.analysis.interpreter.get_analysis(
                "debuglog", self.analysis.chipdata.processor
            ).poll_debug_log()


def suspend_log_decorator(sleep_time):
    """Creates a function decorator.

        It disables and re-enables debug logging (see below) and waits for
        a predefined time before the decorated function call.

    Args:
        sleep_time (int) Time to sleep before the decorated function will
        be called.
    """
    def decorator(func):
        """Disables and re-enables debug logging for the function call.

        Only does this if debuglog was enabled.

        Args:
            func
        """

        @wraps(func)
        def _new_method(self, *arg, **kws):
            with LogController(self, sleep_time):
                # call the actual function.
                return func(self, *arg, **kws)

        return _new_method

    return decorator


def is_debuglog_enabled_decorator(func):
    """Only calls the decorated function if debuglog is enabled.

    Args:
        func
    """

    @wraps(func)
    def _new_method(self, *arg, **kws):

        if self.is_debuglog_enabled:
            # call the actual function.
            return func(self, *arg, **kws)
        else:
            # displaying alerts for each function is a bit overkill but is
            # good for testing.
            # self.formatter.alert(
            #    "DebugLog is disabled in the build. debuglog.%s"
            #    " will have no affect."%(func.__name__)
            # )
            pass

    return _new_method


class DebugLog(Analysis.Analysis):
    """Encapsulates an analysis for Debug Log decode.

    Args:
        *arg: Variable length argument list.
        **kws: Arbitrary keyword arguments.
    """

    def __init__(self, **kwarg):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwarg)
        try:
            self.chipdata.get_var_strict('$_debugLogLevel')
            self.is_debuglog_enabled = True
            self._stop_event = threading.Event()
            self._stop_event.clear()
            self._polling = threading.Event()
            self._polling.clear()
            self._debug_logging = Logging(
                self.chipdata, self.debuginfo, self._polling, self._stop_event,
                self.formatter
            )
        except DebugInfoNoVariableError:
            # Debug log is disabled for the current build. We still need the
            # analysis because the profiler and fats analyses are calling it.
            self.is_debuglog_enabled = False
            self.formatter.alert("DebugLog is disabled in the build")

    def __del__(self):
        if self.is_debuglog_enabled:
            # Need to make sure we stop polling if the object is destroyed.
            self._polling.clear()
            self._stop_event.set()
            if self._debug_logging.is_alive():
                self._debug_logging.join()

    @property
    def log_level(self):
        """Debug Log Level Variable."""
        return self.chipdata.get_var_strict('$_debugLogLevel')

    @is_debuglog_enabled_decorator
    def run_all(self):
        """Outputs the contents of the debug log buffer."""
        self.formatter.section_start('Debug Log')
        self.analyse_debug_log()
        self.formatter.section_end()

    @is_debuglog_enabled_decorator
    def get_debug_log(self):
        """Gets the (decoded) contents of the debug_log buffer.

        This could raise a number of different exception types, including
        AnalysisError.

        Returns:
            A list of debug strings, ordered from oldest to newest.

        Raises:
            AnalysisError: The firmware does not have Debug Log symbols.
            UsageError: Debug Log is being polled.
        """

        if not self._debug_logging.debuginfo_present:
            raise AnalysisError(
                "This firmware does not have symbols for the Debug Log.\n"
                "Most likely it's not compiled in."
            )

        # Don't perform this action if polling is occurring
        if self._polling.is_set():
            raise UsageError(
                "Debug Log is being polled; "
                "you must stop polling to make this call!"
            )

        return self._debug_logging.decode_log()

    @is_debuglog_enabled_decorator
    def re_get_debug_log(self):
        """Re read the contents of the debug_log buffer.

        The method is similar to ``get_debug_log``, with the difference that
        it will re-read the debug buffer.

        This could raise a number of different exception types, including
        AnalysisError.

        Returns:
            A list of debug strings, ordered from oldest to newest.

        Raises:
            AnalysisError: The firmware does not have Debug Log symbols.
            UsageError: Debug Log is being polled.
        """

        if not self._debug_logging.debuginfo_present:
            raise AnalysisError(
                "This firmware does not have symbols for the Debug Log.\n"
                "Most likely it's not compiled in."
            )

        # Don't perform this action if polling is occurring
        if self._polling.is_set():
            raise UsageError(
                "Debug Log is being polled; "
                "you must stop polling to make this call!"
            )

        return self._debug_logging.re_decode_log()

    @is_debuglog_enabled_decorator
    def set_debug_log_level(self, set_level):
        """Sets the debug log level to use.

        Note:
            This is only available on a live chip.

        Args:
            set_level (int)
        """
        if not self.chipdata.is_volatile():
            raise UsageError(
                "The debug log level can only be set on a live chip.")

        # Force to int, just in case the user supplied a string
        set_level = int(set_level)
        if set_level > 5 or set_level < 0:
            raise UsageError("Level must be in the range 0-5\n")
        else:
            # for the 32-bit architecture, the memory location where log_level
            # is might contain other values, therefore set_data() cannot be
            # used without any algorithm to preserve the other values since it
            #  overwrites the whole word the offset of log_level in the word
            offset = self.log_level.address % Arch.addr_per_word
            # the mask to be applied to get the value of log_level from the
            # word
            mask = (
                int(math.pow(2, 8 * self.log_level.size) - 1)
            ) << (8 * offset)
            value = self.chipdata.get_data(self.log_level.address)
            # set the log_level to 0, while preserving the other bits in the
            # word
            value &= (~mask)
            # add the set_level variable in the location of log_level in the
            # word
            value |= (set_level << (8 * offset))
            self.chipdata.set_data(self.log_level.address, [value])
            self.formatter.alert(
                "Debug log level set to " + str(set_level) + "\n"
            )

    @is_debuglog_enabled_decorator
    def print_log_level(self):
        """Prints the current debug log level."""
        # update the log level variable
        self.formatter.alert(
            "Debug log level is currently set to %s \n" % str(
                self.log_level.value
            )
        )

    @is_debuglog_enabled_decorator
    def poll_debug_log(self, wait_for_keypress=None):
        """Polls the debug_log buffer.

        If the contents of the buffer changes then it prints out the new
        extra contents. This function spawns a new thread to perform the
        polling operation.

        Note:
            This is only available on a live chip.

        Raises:
            UsageError: Invoked on something other than a live chip.
            UsageError: Debug Log is being polled.
            AnalysisError: The firmware does not have Debug Log symbols.
        """
        if wait_for_keypress is None:
            if os.name == 'nt':
                wait_for_keypress = False
            else:
                wait_for_keypress = True
        if not self.chipdata.is_volatile():
            raise UsageError(
                "ERROR: This functionality is only available on a live chip.\n"
                "Use get_cu.debug_log()"
            )

        if not self._debug_logging.debuginfo_present:
            raise AnalysisError(
                "This firmware does not have symbols for the Debug log.\n"
                "Most likely it's not compiled in."
            )

        # If we're already polling don't do it again
        if self._polling.is_set():
            raise UsageError(
                "Debug log is being polled; "
                "you must stop polling to make this call!"
            )

        # Use 'alert' to differentiate command responses from the debug log
        # contents
        self.formatter.alert("Polling debug log...")

        # start polling in s separate thread
        if not self._debug_logging.is_alive():
            self._debug_logging.start()
        self._polling.set()

        if wait_for_keypress:
            self.formatter.alert("Press any key to stop.")
            # wait until a key is pressed.
            sys.stdin.read(1)
            self.stop_polling()

    @is_debuglog_enabled_decorator
    def stop_polling(self):
        """Stops the polling.

        Sets a flag that the polling thread can see telling it to stop.
        """
        self._polling.clear()
        self._debug_logging.inactive.wait()
        # Use 'alert' to differentiate command responses from the debug log
        # contents
        self.formatter.alert("Polling debug log terminated")

    def logging_active(self):
        """Check if the ACAT is actively logging the debug log.

        Returns:
            bool: True if the ACAT is actively logging the debuglog, False
                otherwise.
        """
        if self.is_debuglog_enabled:
            return self._polling.is_set()

        return False

    @is_debuglog_enabled_decorator
    def set_formatter(self, formatter):
        """Sets how the result is being formatted.

        Args:
            formatter (:obj:`Formatter`)
        """
        Analysis.Analysis.set_formatter(self, formatter)
        self._debug_logging.formatter = self.formatter

    #######################################################################
    # Analysis methods - public since we may want to call them individually
    #######################################################################

    @is_debuglog_enabled_decorator
    def analyse_debug_log(self):
        """Outputs the contents of the debug_log buffer.

        If something goes wrong (Debug Log not compiled in, Debug Log is
        being polled etc.), an exception is thrown.
        """
        self.formatter.output('Debug log contents is: ')
        self.formatter.output_list(self.get_debug_log())

    @is_debuglog_enabled_decorator
    def re_analyse_debug_log(self):
        """Outputs the contents of the debug_log buffer.

        This method is similar to ``analyse_debug_log`` but it re-reads
        the debuglog buffer.

        If something goes wrong (Debug Log not compiled in, Debug Log is
        being polled etc.), an exception is thrown.
        """
        self.formatter.output('Debug log contents is: ')
        self.formatter.output_list(self.re_get_debug_log())

class Logging(threading.Thread):
    """Creates a separate thread for logging debug information.

    Attributes:
        debuginfo
        formatter
        chipdata
        decode_lock
        inactive
        debug_log
        debuginfo_present

    Args:
        chipdata
        debuginfo
        polling
        stop_event
        formatter
    """

    def __init__(self, chipdata, debuginfo, polling, stop_event, formatter):
        threading.Thread.__init__(self)
        self._stop_event = stop_event
        self._polling = polling
        self.debuginfo = debuginfo
        self.formatter = formatter
        self.chipdata = chipdata

        self.decode_lock = threading.Lock()
        # This event can give a feedback about the state of the debuglog
        # thread.
        self.inactive = threading.Event()
        self.inactive.set()

        # Our list of formatted debug strings. Set every time we call
        # decode_log.decode_log is a limited queue
        self.debug_log = deque(maxlen=10000)

        try:
            self._buffer = DebugLogBuffer(self.debuginfo, self.chipdata)
            self.debuginfo_present = True

        except DebugLogNotFound:
            self._buffer = None
            self.debuginfo_present = False

    def decode_log(self):
        """Decodes the contents of the debug_buffer and returns it.

        This could raise a number of different exception types, including
        AnalysisError. This function could be called from outside of this
        thread. A lock is used to makes sure it is only running on one
        thread.
        """
        with self.decode_lock:
            return self._decode_log()

    def re_decode_log(self):
        """Re establishes the buffer and decode logs.

        The function is similar to ``decode_log`` method, but also
        re-instantiate the buffer to start from the beginning.
        """
        with self.decode_lock:
            self._buffer = DebugLogBuffer(self.debuginfo, self.chipdata)
            return self._decode_log()

    def _decode_log(self):
        """Same as decode_log, but is not thread safe."""
        if self._buffer is None:
            logger.error("Debug info is not present!")
            return

        self._buffer.refresh()

        tmp_debug_logs = []
        while True:
            try:
                tmp_debug_logs.append(next(self._buffer))

            except DebugLogFormatterStringError as error:
                # For some reasons the formatter message can not be
                # retrieved. This means the accumulated arguments should be
                # reset for the next up coming debug log message.
                tmp_debug_logs.append(
                    'Warning: A debug message is removed. {}'.format(
                        error
                    )
                )
            except StopIteration:
                break

        if len(tmp_debug_logs):
            return tmp_debug_logs
        else:
            return None

    def run(self):
        """Polls the debug log buffer repeatedly.

        This is called in a separate thread so that command line input
        still works. This could raise a number of different exception
        types, including AnalysisError.
        """
        while not self._stop_event.is_set():
            self.inactive.set()
            self._polling.wait()
            self.inactive.clear()
            try:
                new_debug_log = self.decode_log()
                # new_debug_log can be None
                if isinstance(new_debug_log, list):
                    self.debug_log += new_debug_log
                    self.formatter.output_list(new_debug_log)
            except (TypeError, IndexError, AnalysisError) as excep:
                # It's possible if the buffer tears.
                self.formatter.output("|%s| in Logging.run()" % (str(excep)))
