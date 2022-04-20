############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""\
Embedded Firmware Debug Log Component

These components provide convenient interfaces to firmware log level control(s)
and the raw log data stream. They do not perform any interpretation of the log
data.

Example application design:- (Key: '-->' = Uses)

                tui/gui
                   |
                   V
            DebugLogMonitor
            |             |
            V             V
   DebugLogReader     DebugLogDecoder
         |                   |
         V                   V
      DebugLog       FirmwareMetaData

Several DebugLog variants are supported:-

-- ClassicHydraLog    Single global threshold "debugLogLevel".
-- PerModuleHydraLog  Per-module thresholds.
-- GlobalHydraLog     As above but built with per-module levels disabled.

The variants support self-detection so they can be used with the generic
"FirmwareComponent.create_subtype_variant" algorithm.

Future:

-- Factor log threshold control variation out of main HydraLog class,
and into sub-component (and match with ui/command-line classes).
"""

from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import PureVirtualError, TypeCheck, \
                            AnsiColourCodes, create_reverse_lookup, to_signed, \
                            detect_keypress, add_colours
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.fw.slt import FakeSLTAttributeError
import re, time
from csr.dev.fw.meta.i_firmware_build_info import IGenericHydraFirmwareBuildInfo
from csr.dev.model import interface
from csr.dwarf.read_dwarf import DwarfNoSymbol
from csr.wheels.polling_loop import  add_poll_function
from csr.dev.hw.address_space import AddressSpace
from csr.dev.hw.address_space import AddressMultiRange
from csr.dev.hw.port_connection import ReadFailureDUTInaccessible, \
ReadFailureSubsystemInaccessible, ReadFailureDeviceChanged
import csr.dev
from csr.dev.env.env_helpers import _Variable
from csr.dev.framework.meta.elf_firmware_info import BadPCHigh
from time import sleep
import string
import platform
import os
import sys
import threading
import functools
from collections import OrderedDict
from datetime import datetime
from csr.wheels.bitsandbobs import CLang
from csr.dev.hw.memory_pointer import MemoryPointer
from csr.dev.fw.meta.elf_code_reader import NotInLoadableElf
import traceback

def log_livener_thread(report_func, sleep_time=0, exit_check=None, ignore_exc=True,
                       reader=None, resume_cb=None, **kwargs):
    """
    Call a log report function repeatedly, sleeping for a given time between
    calls (none by default), with "q<enter>" or Ctrl-C terminating the 
    loop. 
    
    This implementation uses a thread to poll sys.stdin for characters.  It's
    potentially a bit fragile around the 
    """
    kill_look_for_exit_char = False
    
    def look_for_exit_char(exit_chars):
        """
        Function intended to run in a separate thread
        """
        while not kill_look_for_exit_char:
            input = sys.stdin.read(1)
            if input in exit_chars:
                return
            # Sleep briefly to give the parent thread a chance to set 
            # kill_look_for_exit_char if it wants to.
            time.sleep(0.1)

    real_time = False
    quit_key = "q"
    exit_poller = threading.Thread(target=look_for_exit_char, 
                                   args=((quit_key,),))
    exit_poller.daemon = True
    exit_poller.start()
    try:
        successive_exceps = 0
        try_to_resume = False
        while True:
            try:
                report = report_func(real_time, **kwargs)
            except Exception as exc:
                if ignore_exc:
                    if isinstance(exc, ReadFailureDeviceChanged):
                        report = interface.Code("Device changed (" + str(exc) + "): continuing")
                        # The transport gets reset in the code below to let it continue 
                        # the log with the new device
                    elif successive_exceps < 2:
                        report = interface.Code("Saw '" + str(exc) + "': retrying")
                    else:
                        report = None
                    successive_exceps += 1
                    if successive_exceps == 2:
                        # Probably a reset.  Stop reporting errors, and reset
                        # the log reader's offset to 0
                        msg = "Probable reset: suppressing further errors"
                        if reader is not None:
                            msg += " and preparing to read from start of buffer"
                        report = interface.Code(msg)
                    if reader is not None:
                        transport_needs_reset = not isinstance(exc, (ReadFailureDUTInaccessible,
                                                                     ReadFailureSubsystemInaccessible))
                        reader.reset(reset_transport=transport_needs_reset)
                    
                else:
                    raise
            else:
                successive_exceps = 0
                if report is None:
                    try_to_resume = True
                    
                if try_to_resume:
                    try_to_resume = not resume_cb()

            if report is False:  # indicates the processor is no longer running
                break
            if report is not None:
                yield report
            real_time = True
            sleep(sleep_time)
            if not exit_poller.is_alive():
                break
            if exit_check and exit_check():
                break
    except KeyboardInterrupt:
        # On Windows it appears that a Ctrl-C makes sys.stdin.read(1) return with
        # an empty string, which is handy for kicking the polling thread out of
        # its loop.  We just have to make sure we give time for the kill flag to
        # take effect.
        kill_look_for_exit_char = True
        if exit_poller.is_alive():
            # If it's still running, give it a chance to stop
            time.sleep(0.2)
            if exit_poller.is_alive():
                # It's had long enough: it must still be blocking in 
                # sys.stdin.read(1), i.e. the KeyboardInterrupt didn't cause it
                # to jump out after all. Try soliciting user input instead.
                iprint("Keyboard interrupt: <enter> to continue")
    finally:
        if exit_poller.is_alive():
            # Kill the exit_poller thread asap.
            kill_look_for_exit_char = True

def log_livener_wincrt(report_func, sleep_time=0, exit_check=None, 
                       ignore_exc=True, reader=None, resume_cb=None, **kwargs):
    """
    Call a log report function repeatedly, sleeping for a given time between
    calls (none by default), with "q" (Windows only) or Ctrl-C terminating the 
    loop. 
    """
    real_time = False
    quit_key = ord("q") if platform.system() == "Windows" else False
    try:
        successive_exceps = 0
        try_to_resume = False
        while True:
            try:
                report = report_func(real_time, **kwargs)
            except Exception as exc:
                if ignore_exc:
                    if isinstance(exc, ReadFailureDeviceChanged):
                        report = interface.Code("Device changed (" + str(exc) + "): continuing")
                        # The transport gets reset in the code below to let it continue 
                        # the log with the new device
                    elif successive_exceps < 2:
                        report = interface.Code("Saw '" + str(exc) + "': retrying")
                    else:
                        report = None
                    successive_exceps += 1
                    if successive_exceps == 2:
                        # Probably a reset.  Stop reporting errors, and reset
                        # the log reader's offset to 0
                        msg = "Probable reset: suppressing further errors"
                        if reader is not None:
                            msg += " and preparing to read from start of buffer"
                        report = interface.Code(msg)
                    if reader is not None:
                        # These two exceptions imply an incompatible chip state,
                        # e.g. subsystem powered off, there's no reason to try
                        # a transport reset in this case.
                        try_transport_reset = not isinstance(exc, (ReadFailureDUTInaccessible,
                                                                     ReadFailureSubsystemInaccessible))
                        reader.reset(reset_transport=try_transport_reset)
                else:
                    raise
            else:
                successive_exceps = 0
                if report is None:
                    try_to_resume = True
                    
                if try_to_resume:
                    try_to_resume = not resume_cb()
 
            if report == False:  # indicates the processor is no longer running
                break
            if report is not None:
                yield report
            real_time = True
            sleep(sleep_time)
            if quit_key and detect_keypress(quit_key):
                break
            if exit_check and exit_check():
                break
    except KeyboardInterrupt:
        pass

if os.getenv("PYDBG_RUNNING_IN_SUBPROCESS"):
    log_livener = log_livener_thread
else:
    log_livener = log_livener_wincrt

class DebugLog (FirmwareComponent):
    """\
    Embedded Firmware Debug Log (Abstract Base)

    Common interfaces & implementations for all known firmware debug logs
    (including non-hydra).
    """
    def __init__(self, fw_env, core, parent):
        """
        Construct DebugLog.
        """
        FirmwareComponent.__init__(self, fw_env, core, parent)

    # FirmwareComponent compliance

    @property
    def title(self):
        return "Log"

    # Extensions

    @property
    def buffer(self):
        """\
        The raw circular log buffer.
        """
        raise PureVirtualError(self)

    @property
    def pos(self):
        """\
        The current buffer write/head index value.
        """
        raise PureVirtualError(self)

    def clear(self):
        """
        Clear the log buffer.
        """
        raise PureVirtualError(self)


class HydraLog (DebugLog):
    """\
    Hydra Debug Log  (Base)

    Common base for Hydra (common & curator) DebugLogs.
    """
    def __init__(self, fw_env, core, parent, buf_to_use = 0):
        """
        Construct HydraLog component for the specified hydra firmware.

        Raises:
        -- NotDetected: if the environment does not appear to contain a
        hydra log of this (sub)type.
        """
        DebugLog.__init__(self, fw_env, core, parent)

        if buf_to_use == 0:
            # Create buffer proxy
            self.__buffer = _Buffer.create(fw_env, core)

            # Check for, and cache, IVariables common to all subtypes...
            cu=None
            try:
                fw_env.build_info._log_firm_basename
            except AttributeError:
                cu = self.env.cus["hydra_log_firm.c"]
                self.__buf = cu.localvars["debugBuffer"]
                self.__pos = cu.localvars["debugBufferPos"]
            else:
                try:
                    # a subsystem may provide us a list of alternative CU names
                    if isinstance(fw_env.build_info._log_firm_basename, str):
                        cu = self.env.cus[fw_env.build_info._log_firm_basename]
                    else:
                        for firm_basename in fw_env.build_info._log_firm_basename:
                            try:
                                cu = self.env.cus[firm_basename]
                                break
                            except KeyError:
                                pass
                except KeyError:
                    pass
            try:
                if cu is not None:
                    try:
                        self.__buf = cu.localvars[parent._get_debug_buffer_name()]
                    except AttributeError:
                        self.__buf = cu.localvars["debugBuffer"]
                    self.__pos = cu.localvars["debugBufferPos"]
                else:
                    # Finally, they might appear to be globals, due to the effects of ELF
                    # file filtering
                    self.__buf = fw_env.globalvars["debugBuffer"]
                    self.__pos = fw_env.globalvars["debugBufferPos"]
            except KeyError:
                raise self.NotDetected()

        self._printf_log = DebugPrintfLog(fw_env, core)
        
    def add_exclude_pattern(self, pattern):
        '''
        Add a log line exclusion pattern (a regexp)
        '''
        self.decoder.add_exclude_pattern(pattern)
        
    def clear_exclude_patterns(self):
        '''
        Clear all log line exclusions patterns
        '''
        self.decoder.clear_exclude_patterns()
        
    def show_exclude_patterns(self):
        '''
        Print all log line exclusion patterns.
        '''
        self.decoder.show_exclude_patterns()

    def add_include_pattern(self, pattern):
        '''
        Add a log line inclusion pattern (a regexp).  If any inclusion patterns 
        have been added, only lines matching an inclusion pattern will be shown.
        '''
        self.decoder.add_include_pattern(pattern)
        
    def clear_include_patterns(self):
        '''
        Clear all inclusion patterns
        '''
        self.decoder.clear_include_patterns()
        
    def show_include_patterns(self):
        '''
        Print all log line inclusion patterns.
        '''
        self.decoder.show_include_patterns()

    # DebugLog compliance

    @property
    def buffer(self):
        # Potential extension:: reinstate Variable Sized buffer log
        return self.__buf

    @property
    def pos(self):
        try:
            return self.__pos.value
        except AttributeError:
            #Obviously we don't have DWARF info so we can't access type-specific
            #_Variable attributes
            return self.__pos.mem[0]

    @property
    def pos_memory_address(self):
        ''' Return the address in memory of the buffer position variable. '''
        return self.__pos.address
    
    def clear(self):
        self.__buffer.clear()

    # Extensions

    @property
    def reader(self):
        try:
            self._reader
        except AttributeError:
            self._reader = DebugLogReader(self)
        return self._reader

    def _on_reset(self):
        try:
            self._reader
        except AttributeError:
            pass
        else:
            self._reader.reset()

    def reread(self):
        self.reader.reset()
        return self.generate_decoded_event_report(False)

    @property
    def decoder(self):
        return self.parent.debug_log_decoder

    def generate_decoded_event_report(self, real_time = False, return_str=False,
                                      check_running=False, time_fmt=None,
                                      raw_log_file=None):
        """
        Read new messages in the debug log and run them through the decoder.
        :real_time (boolean) If True, print time.time() against this tranche of
        entries
        :return_str (boolean) If True, return the decoded events as a plain string;
        otherwise return them as a formatting element, interface.Code.
        :check_running (boolean) If True, whenever no new entries are found in
        the buffer, check whether the process is still running (via the
        "is_running" property of the core; if no such property exists, it's
        assumed to be running).  If not, return False.  This return value causes
        the live_log loop to return.
        :raw_log_file: If not None, open a file with the given name and read the
        data out assuming it is binary data in the same layout as found in 
        RAM.
        """
        
        if raw_log_file is None:
            data = self.reader.read()
        else:
            raw_bytes = open(raw_log_file,"rb").read()
            if bytes is str:
                # In Py2, binary data is treated as strings, not numbers, so
                # convert explicitly.
                raw_bytes = [ord(b) for b in raw_bytes]
            data = list(self._core.info.layout_info.word_stream_from_octet_stream(raw_bytes))
        if not data and check_running:
            # No new data: is the processor still running?
            try:
                if not self._core.is_running:
                    # nope
                    iprint("Processor not running")
                    return False # tells the log_livener to stop spinning
            except AttributeError:
                # Core doesn't have the appropriate interface; just carry on
                pass
            
        if real_time:
            time_stamp = time.time()
        else:
            time_stamp = None
        events = self.decoder.decode(data, timestamp = time_stamp, 
                                     printf_buf=self._printf_log, time_fmt=time_fmt)
        text = ''
        for event in events:
            text += event + '\n'
        
        if return_str:
            return text
        return interface.Code(text)

    def live_log(self, sleep_time=0, exit_check=None, persistent=True,
                 time_fmt=None):
        """
        Run the log in continuous polling mode.  Exit with Ctrl-C or q
        
        :param sleep_time, optional: number of seconds to wait between polls (can be
         fractional). Default is 0.
        :param exit_check, optional: Callable that can make the logger exit by returning False
        :param persistent: has two effects: 
          # Ignore exceptions other than KeyboardInterrupt which occur while
           retrieving data.  This avoids exit of the poller over a reset. 
          # Don't check if the processor is still running when 
           there is no new data in the buffer; with this False the logger will 
           exit on spotting that the processor isn't running, but note that it 
           will only check this if there is no new data available to avoid a 
           costly and pointless check that would increase the risk of buffer 
           wrapping by slowing the scraping down.
        :param time_fmt, optional: A format string for the timestamps added to
         the logger, or True.  If True, this is replaced with a default format 
         "%H:%M:%S:%f ".  Otherwise this argument is passed unchanged to datetime.
        """
        decoder = functools.partial(self.generate_decoded_event_report,
                                    check_running= not persistent,
                                    time_fmt = time_fmt)
        
        for block in log_livener(decoder, 
                                 sleep_time=sleep_time,
                                 exit_check=exit_check,
                                 ignore_exc=persistent,
                                 reader=self.reader):
            yield block

    def start_live_log(self):
        core = self._core
        log_name = " ".join([core.subsystem.title, core.title, "fw_log"])
        add_poll_function(log_name, self._log_polling_function)

    def trb_live_log(self, sleep_time=0, exit_check=None):
        """
        Run trb logging in continuous polling mode. Exit with Ctrl-C or q
        
        :param sleep_time, optional: number of seconds to wait between polls (can be
         fractional). Default is 0.
        :param exit_check, optional: Callable that can make the logger exit by returning False
        """

        trb_log = self._core.subsystem.chip.trb_log
        
        def _trb_log_show(real_time, **kwargs):
            
            trb_chunk = trb_log.show(report=True)
            trb_log.clear()

            return trb_chunk if trb_chunk.text else None

        def _trb_log_resume():
            trb_log.attempt_restart(self._core.nicknames[0])

        already_enabled = trb_log.start(self._core.nicknames[0])[0]
        trb_log.clear()
        for block in log_livener(_trb_log_show, sleep_time=sleep_time, exit_check=exit_check,
                                 resume_cb=_trb_log_resume):

            yield block

        # If already enabled we asssume that this was done deliberately so leave it alone.
        if not already_enabled:
            trb_log.stop(self._core.nicknames[0])

    def _log_polling_function(self):
        return self.generate_decoded_event_report(True)
                
    def log_level(self, value):
        raise NotImplementedError

    # Protected / DebugLog compliance

    @staticmethod
    def _analyse(events):
        from csr.dev.adaptor.text_adaptor import StringTextAdaptor
        tmp_s = StringTextAdaptor(events)

        #The regexp used on the old HydraLog.pm
        pattern = r"""
        (?<!\w)
        (?:subsystem_)?
        (?:
          crash(?:ed)?|
          panic|
          emergency|
          fault|
          ss_watchdog
        )
        (?:_halt)?
        (?:_waiting)?
        (?:_ind)?
        (?:_req)?
        (?!\w)"""

        event_check = re.compile(pattern, re.I|re.VERBOSE)

        #Analyse line by line
        issues = []
        for line in tmp_s.splitlines():
            match = event_check.search(line)
            if match:
                if not issues:
                    issues = interface.Group()
                issues.append(interface.Warning('Event: {} '.format(line)))

        result = []

        if issues:
            result.append(issues)

        result.append(events)
        return result

    def _generate_report_body_elements(self):
        ret = self.reread()
        return self._analyse(ret)


class ClassicHydraLog (HydraLog):
    """\
    Classic Hydra Log Proxy

    Has single global threshold "debugLogLevel".
    """

    def __init__(self, fw_env, core, parent):

        HydraLog.__init__(self, fw_env, core, parent)

        # Check for, and cache, variables specific to this subtype...
        try:
            self.__threshold = fw_env.globalvars["debugLogLevel"]
        except KeyError:
            raise self.NotDetected()

    # Extensions

    @property
    def threshold(self):
        """\
        Global log level threshold (IVariable)
        """
        return self.__threshold

    def log_level(self, *args):
        if len(args) == 1:
            #We are setting the log level
            value = args[0]
            try:
                self.__threshold.value = value
            except AttributeError:
                #Obviously we don't have DWARF info so we can't access type-specific
                #_Variable attributes
                self.__threshold.mem[0] = value
        else:
            #We are getting the log level
            return interface.Code("%i" % self.__threshold.value)

class PerModuleHydraLog (HydraLog):
    """\
    Hydra Log with per-module thresholds (Proxy)

    Module thresholds are held as "hydra_log.levels.<modulename>_threshold"

    The module population may vary with build options.
    """

    def __init__(self, fw_env, core, parent):

        HydraLog.__init__(self, fw_env, core, parent)

        # Check for, and cache, variables specific to this subtype...
        try:
            self.__log = self.env.globalvars["hydra_log"]
            self.__log_levels = self.__log["levels"]
        except KeyError:
            #There's no "hydra_log" global variable
            raise self.NotDetected()
        except TypeError:
            #There's no DWARF info, so "hydra_log" returns a blob variable, not
            #a subscriptable _Structure
            raise self.NotDetected()

    # Extensions

    @property
    def thresholds(self):
        """\
        Dictionary of log thresholds (IEnums) keyed by module name.
        """
        try:
            self.__thresholds
        except AttributeError:
            self.__init_thresholds()

        return self.__thresholds

    def log_level(self, *args):
        if len(args) == 2:
            #We are setting a specific modules log level
            self.thresholds[args[0]].value = args[1]
        elif len(args) == 1:
            if type(args[0]) is int:
                #We are settings all modules log levels to the same value
                for dummy, thresh in self.thresholds.items():
                    thresh.value = args[0]
            elif type(args[0]) is str:
                #We are getting a named modules log level
                return interface.Code("%i" % self.thresholds[args[0]].value)
        elif len(args) == 0:
            # List of all modules with their thresholds
            list_of_modules = []
            max_length = 0

            for entry in self.thresholds:
                list_of_modules.append((entry, self.thresholds[entry].value))
                if len(entry) > max_length:
                    max_length = len(entry) 

            list_of_modules.sort()
            max_length += 1

            for entry in list_of_modules:
                iprint("%s: %u" %(entry[0].ljust(max_length), entry[1]))

        else:
            raise TypeError("Wrong number of arguments to log_level!")

    def level_mib_key(self, *args):
        '''
        Returns the string to place in the .htf file to set the
        module log levels on subsystem boot. Takes a sequence of tuples
        containing the module name and the log level.
        e.g. level_mib_key(("transport_bt", 2), ("sched_oxygen", 4))
        or
        level_mib_key(("sched_oxygen", 4))
        '''
        value_str = " ".join(["%02X" % a for a in 
                        self._level_mib_key_values(args)])
        return "InitialLogLevels = [ " + value_str + " ]"
        
    def _level_mib_key_values(self, module_level_tuple_list):
        '''
        Returns a list of values for the InitialLogLevels MIB key to set
        the module log levels on subsystem boot. Takes a list of tuples
        containing the module name and the log level.
        e.g. level_mib_key_values(("transport_bt", 2), ("sched_oxygen", 4))
        '''
        module_loglevel_values = []
        for entry in module_level_tuple_list:
            if len(entry) == 2:
                module_name = entry[0]
                log_level = entry[1]
                addr_offset = self.thresholds[module_name].address - \
                                                self.__log_levels.address
                module_loglevel_values += [addr_offset, log_level] 
            else:
                raise TypeError("Wrong arguments provided!")
        return module_loglevel_values
        
    # Private

    def __init_thresholds(self):
        """\
        Initialise threshold dictionary.
        """
        import re

        self.__thresholds = {}
        for var_name, threshold in self.__log_levels.members.items():
            # Use <modulename> from "<modulename>_threshold" as key.
            module_name = re.sub("_threshold", "", var_name)
            self.__thresholds[module_name] = threshold


class GlobalHydraLog (HydraLog):
    """\
    14a Hydra Log with global threshold (Proxy)

    14a Introduced build option to have per-module levels (like
    hydra_log.levels.<module_name>) or global level (hydra_log.level).

    This class is proxy for the single _global_ level case.
    """

    def __init__(self, fw_env, core, parent):

        HydraLog.__init__(self, fw_env, core, parent)

        # Check for, and cache, variables specific to this subtype...
        try:
            self.__threshold = self.env.globalvars["hydra_log"]["level"]
        except (KeyError, TypeError):
            raise self.NotDetected()

    # Extensions

    @property
    def threshold(self):
        """\
        Global log level threshold (IEnum).
        """
        return self.__threshold

    def log_level(self, *args):
        """
        Set or get the Global log level depending if exactly one integer argument is given. If no argument
        provided current log level is displayed, it is not returned. Raises TypeError exception if more 
        than one argument provided or argument is not an integer.
        """
        if len(args) == 1:
            try:
                self.threshold.value = args[0]
            except TypeError:
                raise TypeError("Global log level must be an integer not {}".format(type(args[0])))
        elif len(args) == 0:
            return interface.Code("%i" % self.__threshold.value)
        else:
            raise TypeError("Wrong number of arguments provided for global log level!")

class PerModuleApps1Log (HydraLog):
    """\
    Hydra Log with P1 apps style per-module log levels
    Module thresholds are held as "debug_log_level_<modulename>" and the global
    level in "debug_log_level__global".
    The module population may vary with build options.
    """

    def __init__(self, fw_env, core, parent):
        HydraLog.__init__(self, fw_env, core, parent)
        # Check for, and cache, variables specific to this subtype...
        self._prefix = "debug_log_level_"
        self.global_name = self._prefix + "_global"
        try: 
            self.level_enum = self.env.enum.debug_log_level_t
            self.global_level_var = self.env.globalvars[self.global_name]
        except (KeyError, DwarfNoSymbol, AttributeError):
            #There's no enum or no global log level variable
            raise self.NotDetected()
        except TypeError:
            #There's no DWARF info, so "hydra_log" returns a blob variable, not
            #a subscriptable _Structure
            raise self.NotDetected()
        self.modules = [a[0][len(self._prefix):] for a in 
                        self.env.globalvars.keys() 
                        if a[0].startswith(self._prefix) and
                        a[0] != self.global_name and not self.env.vars.symbol_is_garbage(a[0])]
        self.modules.sort()
        # Create a lookup returning just the varying part of the enum
        # so it displays better
        self.level_name = dict([(value,name[len("DEBUG_LOG_LEVEL_"):]) 
                                for name,value in self.level_enum.items()])
        # Make a lookup that works both for the full enum name and just the
        # varying part of it
        self.level_value = create_reverse_lookup(self.level_name)
        self.level_value.update(self.level_enum)

        self.global_level_name = "default"
        self.level_var = dict([(mod,self.env.globalvars[self._prefix+mod]) 
                                for mod in self.modules])
        self.level_var[self.global_level_name] = self.global_level_var 
        
    def log_level(self, *args):
        if len(args) == 2:
            #We are setting a specific modules log level (or the global one)
            level = self.level_value[args[1]] if isinstance(args[1], str) else args[1]
            self.level_var[args[0]].value = level
        elif len(args) == 1:
            if isinstance(args[0], str) and args[0] not in self.level_value:
                #We are getting a named modules log level
                return self.level_name[self.level_var[args[0]].value]
            else:
                #We are settings all modules log levels to the same level
                level = self.level_value[args[0]] if isinstance(args[0], str) else args[0]
                for thresh in self.level_var.values():
                    thresh.value = level
        elif len(args) == 0:
            # List of all modules with their levels
            max_length = max([len(a) for a in self.modules] + [len(self.global_name)])

            for mod in [self.global_level_name]+self.modules:
                iprint("%s: %s" %(mod.ljust(max_length+1), 
                                 self.level_name[self.level_var[mod].value]))
        else:
            raise TypeError("Wrong number of arguments to log_level!")

class PrimHydraLog (HydraLog):
    """\
    Hydra Log for prim logging
    """

    def __init__(self, fw_env, core, parent):
        HydraLog.__init__(self, fw_env, core, parent, 1)

        try:
            self.__buf = self.env.globalvars["bsif_prim_log"]
            self.__pos = self.env.globalvars["bsif_prim_log_pos"]
        except KeyError:
            # First fallback, because this is not P0's env: try the P0 SLT
            try:
                self.__buf = fw_env.cast(
                    core.subsystem.p0.fw.slt.bsif_prim_log,
                    "uint8",
                    array_len=core.subsystem.p0.fw.slt.bsif_prim_log_size)
                self.__pos = fw_env.cast(
                    core.subsystem.p0.fw.slt.bsif_prim_log_pos,
                    "uint16")
            except FakeSLTAttributeError:
                # Second fallback, because the P0 SLT is a FakeSLT from a 
                # coredump: try P0's env, which may be available instead.
                env = core.subsystem.p0.fw.env
                try:
                    self.__buf = env.globalvars["bsif_prim_log"]
                    self.__pos = env.globalvars["bsif_prim_log_pos"]
                except KeyError:
                    raise self.NotDetected()

    @property
    def buffer(self):
        # Potential extension:: reinstate Variable Sized buffer log
        return self.__buf

    @property
    def pos(self):
        try:
            return self.__pos.value
        except AttributeError:
            #Obviously we don't have DWARF info so we can't access type-specific
            #_Variable attributes
            return self.__pos.mem[0]

    def reread(self):
        try:
            del self._prim_log_reader
        except AttributeError:
            pass
        return self.generate_decoded_event_report(False)

    @property
    def decoder(self):
        try:
            self._prim_log_decoder
        except AttributeError:
            self._prim_log_decoder = PrimLogDecoder(self, self._core)
        return self._prim_log_decoder

    @property
    def reader(self):
        try:
            self._prim_log_reader
        except AttributeError:
            # Get the current start location in the bsif_prim_log
            try:
                start = self.env.globalvars["bsif_prim_log_start"].value
            except KeyError:
                start = self.env.cast(
                    self._core.subsystem.p0.fw.slt.bsif_prim_log_start,
                    "uint16").value

            # Current start location obtained. Use it to indicate to
            # DebugLogReader where to start reading primitives from.
            self._prim_log_reader = DebugLogReader(self, start)
        return self._prim_log_reader

    def generate_decoded_event_report(self, real_time=True,
        return_str=False, xml=False):

        # Just stuff flat event text into preformatted code element.
        data = self.reader.read()
        if real_time:
            time_stamp = time.time()
        else:
            time_stamp = None
        events = self.decoder.decode(data, time_stamp, xml = xml)
        text = ''
        for event in events:
            text += "\n".join(event) + "\n\n"
        if return_str:
            return text
        return interface.Code(text)

    def generate_decoded_event_report_xml(self, real_time = False,
        return_str=False):

        return self.generate_decoded_event_report(real_time, return_str,
            xml = True)

    def live_log(self, sleep_time = 0.5, exit_check=None):
        """
        Run Bluestack primitive log retrieval in continuous polling mode.  Exit 
        with Ctrl-C.
        
        :param sleep_time, optional: number of seconds to wait between polls (can be
         fractional)
        :param exit_check, optional: Callable that can make the logger exit by returning False
        """
        for block in log_livener(self.generate_decoded_event_report,
                                 sleep_time=sleep_time, exit_check=exit_check):
            yield block

    def live_log_xml(self, sleep_time = 0.5, exit_check=None):
        """
        Run Bluestack primitive log retrieval in continuous polling mode, 
        generating XML format output.  Exit with Ctrl-C.
        
        :param sleep_time, optional: number of seconds to wait between polls (can be
         fractional)
        :param exit_check, optional: Callable that can make the logger exit by 
         returning False
        """
        for block in log_livener(self.generate_decoded_event_report,
                                 sleep_time=sleep_time, xml=True,
                                 exit_check=exit_check):
            yield block

    # Backwards compatibility
    prim_live_log = live_log
    prim_log_log_xml = live_log_xml

    def start_prim_log(self):
        """
        Use the polling function to log the primitives.
        """
        core = self._core
        log_name = " ".join([core.subsystem.title, core.title, "prim_log"])
        add_poll_function(log_name, self._log_polling_function)

    def _log_polling_function(self):
        return self.generate_decoded_event_report(True)

class TrapHydraLog (HydraLog):
    """\
    Hydra Log for trap api logging
    """

    def __init__(self, fw_env, core, parent):
        HydraLog.__init__(self, fw_env, core, parent, 2)
        # Create buffer proxy - the type is known for prim_buf
        self.__buffer = _VariableSizeBuffer(fw_env, core, parent, 2)

        try:
            cu = self.env.cus["trap_api_message_log.c"]
            self.__buf = cu.localvars["trap_msg_log"]
            self.__pos = cu.localvars["trap_msg_log_pos"]
        except KeyError:
            raise self.NotDetected()

    @property
    def buffer(self):
        # Potential extension:: reinstate Variable Sized buffer log
        return self.__buf

    @property
    def pos(self):
        try:
            return self.__pos.value
        except AttributeError:
            #Obviously we don't have DWARF info so we can't access type-specific
            #_Variable attributes
            return self.__pos.mem[0]

    @property
    def decoder(self):
        try:
            self.__trap_log_decoder
        except AttributeError:
            self.__trap_log_decoder = TrapLogDecoder(self.env, self._core)
        return self.__trap_log_decoder

    @property
    def reader(self):
        try:
            self.__trap_log_reader
        except AttributeError:
            # Get the current start location in the trap_msg_log
            cu = self.env.cus["trap_api_message_log.c"]
            start = cu.localvars["trap_msg_log_start"].value
            # Current start location obtained. Use it to indicate to
            # DebugLogReader where to start reading trap messages from.
            self.__trap_log_reader = DebugLogReader(self, start)
        return self.__trap_log_reader

    def generate_decoded_event_report(self, real_time = False,
        return_str=False, xml=False):

        # Just stuff flat event text into preformatted code element.
        data = self.reader.read()
        if real_time:
            time_stamp = time.time()
        else:
            time_stamp = None
        events = self.decoder.decode(data, time_stamp, xml = xml)
        text = ''
        for event in events:
            text += "\n".join(event) + "\n"

        # If leave the trap buffer too long it will wrap and we won't
        # re-evaluate start unless we delete the reader, forcing it to be
        # re-instatiated the next time around.
        if real_time == False:
            del self.__trap_log_reader

        if return_str:
            return text
        return interface.Code(text)

    def generate_decoded_event_report_xml(self, real_time = False,
        return_str=False):

        return self.generate_decoded_event_report(real_time, return_str,
            xml = True)

    def live_log(self, sleep_time = 0, exit_check=None):
        """
        Run Bluestack primitive log retrieval in continuous polling mode.  Exit 
        with Ctrl-C.
        
        :param sleep_time, optional: number of seconds to wait between polls (can be
         fractional)
        :param exit_check, optional: Callable that can make the logger exit by returning False
        """
        for block in log_livener(self.generate_decoded_event_report,
                                 sleep_time=sleep_time, exit_check=exit_check):
            yield block
        del self.__trap_log_reader

    def live_log_xml(self, sleep_time = 0, exit_check=None):
        """
        Run Bluestack primitive log retrieval in continuous polling mode.  Exit 
        with Ctrl-C.
        
        :param sleep_time, optional: number of seconds to wait between polls (can be
         fractional)
        :param exit_check, optional: Callable that can make the logger exit by returning False
        """
        for block in log_livener(self.generate_decoded_event_report,
                                 sleep_time=sleep_time, xml=True,
                                 exit_check=exit_check):
            yield block
        del self.__trap_log_reader

class _Buffer (FirmwareComponent):
    """\
    HydraLog buffer base.
    """

    @staticmethod
    def create(fw_env, core):
        """\
        Create HydraLog buffer proxy of appropriate type.
        """
        subtypes = (_FixedSizeBuffer, _VariableSizeBuffer)
        return FirmwareComponent.create_component_variant(subtypes, fw_env,
                                                          core)

    def __init__(self, fw_env, core, parent, buf_to_use = 0):

        FirmwareComponent.__init__(self, fw_env, core, parent)
        if buf_to_use == 0:
            # Check for, and cache, IVariables common to all log buffers...
            #
            cu = None
            try:
                fw_env.build_info._log_firm_basename
            except AttributeError:
                cu = self.env.cus["hydra_log_firm.c"]
            else:
                try:
                    # a subsystem may provide us a list of alternative CU names
                    if isinstance(fw_env.build_info._log_firm_basename, str):
                        cu = self.env.cus[fw_env.build_info._log_firm_basename]
                    else:
                        for firm_basename in fw_env.build_info._log_firm_basename:
                            try:
                                cu = self.env.cus[firm_basename]
                                break
                            except KeyError:
                                pass

                except KeyError:
                    pass 
            try:
                if cu is not None:
                    self.__buf = cu.localvars["debugBuffer"]
                    self.__pos = cu.localvars["debugBufferPos"]
                else:
                    # If we have a stripped ELF we won't have any matching CU but
                    # the variables we want may appear as globals
                    self.__buf = fw_env.globalvars["debugBuffer"]
                    self.__pos = fw_env.globalvars["debugBufferPos"]
            except KeyError:
                raise self.NotDetected()
        elif buf_to_use == 1:
            try:
                self.__buf = self.env.globalvars["bsif_prim_log"]
                self.__pos = self.env.globalvars["bsif_prim_log_pos"]
            except KeyError:
                raise self.NotDetected()
        elif buf_to_use == 2:
            try:
                cu = self.env.cus["trap_api_message_log.c"]
                self.__buf = cu.localvars["trap_msg_log"]
                self.__pos = cu.localvars["trap_msg_log_pos"]
            except KeyError:
                raise self.NotDetected()
        else:
            raise ValueError("%d" % buf_to_use)

    def clear(self):
        """\
        Clear the buffer memory.
        """
        self._buf_mem[:] = [0] * len(self._buf_mem)

    # Protected/Provided

    @property
    def _buf_var(self):
        """\
        The Buffer IVariable
        """
        return self.__buf_var

    # Protected/Required

    @property
    def _buf_mem(self):
        """\
        This Buffer's _effective_ IMemoryRegion.

        N.B. In case of variable sized buffers this is not necessarily
        the same as IArray.mem!
        """
        raise NotImplementedError()


class _FixedSizeBuffer (_Buffer):
    """\
    HydraLog non-resizable buffer.

    The buffer size can be determined and accessed in the "normal" way via
    the debugBuffer IVariable (c.f. _VariableSizeBuffer)
    """

    def __init__(self, fw_env, core, parent):

        _Buffer.__init__(self, fw_env, core, parent)

        # _Don't_ expect to find patchable buffer mask...
        try:
            self.env.globalvars["debugBufferSizeMask"]
            raise self.NotDetected()
        except KeyError:
            pass

    # Protected/Required

    @property
    def _buf_mem(self):

        # IArray.mem is just fine for fixed sized buffers.
        #
        return self._buf_var.mem


class _VariableSizeBuffer (_Buffer):
    """\
    HydraLog re-sizable buffer.

    The buffer size is defined by debugBufferPosMask variable - which
    can be changed on the fly (e.g. by patch)
    """

    def __init__(self, fw_env, core, parent, buf_to_use = 0):

        _Buffer.__init__(self, fw_env, core, parent, buf_to_use)

        if buf_to_use == 0:
            # Expect to find patchable buffer mask...
            try:
                self.__mask_var = self.env.globalvars["debugBufferSizeMask"]
            except KeyError:
                raise self.NotDetected()
        
    # Protected/Required

    @property
    def _buf_mem(self):

        # This buffer's MemorySubregion must be re-computed on demand as the
        # size can be patched on the fly. (The region and offset still come
        # from IArray.mem but length must come from the mask variable).
        #
        return MemorySubregion(self._buf_var.mem.parent,
                               self._buf_var.mem.offset,
                               self.__mask_var.value + 1)


# -----------------------------------------------------------------------------
# -----------------------------------------------------------------------------

class DebugLogReader (object):
    """\
    Reads raw data from a DebugLog.

    This class specialises in reading raw data from a DebugLog. It does not get
    involved in interpreting the data.

    The reader is passive. It must be polled frequently. An event driven
    application might wrap this with a thread dedicated to polling it and
    emitting new data via an Event or Queue, depending on application.

    Future:
    - Reinstate crude overrun detection.
    """
    def __init__(self, log, tail = None):

        TypeCheck(log, DebugLog)
        self._log = log
        self._tail = tail

    def read(self):
        """\
        Reads any raw data arrived in DebugLog buffer since last read.

        The entire buffer is read on first read, after that only new data.

        If no new data available then empty array is returned.
        """
        tail = self._tail
        log = self._log
        buf = log.buffer
        buf_len = len(buf)
        head = log.pos % buf_len # modulo here incase fw has not yet (race).

        # Force read of entire buffer first time around
        if tail is None:
            tail = (head + 1) % buf_len

        # Grab all new data
        data = []
        if tail < head:
            data += buf[tail:head].value
        elif tail > head:
            data += buf[tail:].value
            if head > 0:
                data += buf[:head].value

        # Bump tail
        self._tail = head

        return data

    def reset(self, reset_transport=True):
        self._tail = None
        if reset_transport:
            try:
                # If we're trying to respond to a reset, we might need to kick the
                # driver to get us a new stream pointer
                self._log._core.subsystem.chip.device.transport.reset()
            except Exception:
                # We don't want failure here to scupper the logging loop
                pass

class DebugLogEventInfo (object):
    """\
    Debug Log Event Information Database

    Parses special debug_strings section from the ELF file and builds some
    handy indexes.

    Known users:
    - DebugLogDecoder
    """
    def __init__(self, fw_env):

        self._strings = {}
        self._arg_counts = {}
        self._arg_types = {}
        self._args_regexp = re.compile(r'%[ +-]?[#]?[0-9]*[,]?[.]?[0-9]*[l]*[h]*([udioDxXpPsSc])')

        self._add_strings_from_elf(fw_env.build_info)

    class ArgType:
        SIGNED_INTEGER = 0
        UNSIGNED_INTEGER = 1
        STRING = 2

    @property
    def strings(self):
        """Debug Strings indexed by string address"""
        return self._strings

    @property
    def arg_counts(self):
        """Argument counts indexed by string address"""
        return self._arg_counts

    @property
    def arg_types(self):
        """
        Argument type list indexed by string address.
        No entry for addresses of strings with no args (arg_count == 0).
        """
        return self._arg_types

    # Private

    def _add_strings_from_elf(self, build_info):
        """
        Reads debug strings from special debug_strings section in ELF file.
        (Not to be confused with standard debug_str section!)
        """
        self._strings = build_info.debug_strings
        for string_table_addr, elementString in self._strings.items():
            self._add_string(string_table_addr, elementString)

    def _add_string(self, string_table_addr, elementString):
        ''' Parse a string from the firmware and add it to the databases of strings,
        argument counts and argument types
        '''
        argsList = self._args_regexp.findall(elementString.replace("%%",""))
        self._arg_counts[string_table_addr] = len(argsList)
        if argsList:
            elementArgsType = []
            for argsType in argsList:
                if argsType == 's': # string
                    elementArgsType += [self.ArgType.STRING]
                elif argsType == 'd' or argsType == 'i':
                    elementArgsType += [self.ArgType.SIGNED_INTEGER]
                else: # the rest
                    elementArgsType += [self.ArgType.UNSIGNED_INTEGER]
            self._arg_types[string_table_addr] = elementArgsType

    def add_fmt_string(self, string_table_addr, elementString):
        ''' Extend the debug format strings with the supplied string. This
        is used for adding strings from the const section of the ELF '''
        self._strings[string_table_addr] = elementString
        self._add_string(string_table_addr, elementString)

class DebugLogDecoder (object):
    """
    Decodes raw DebugLog data into textual descriptions.

    Specialisations may add additional, message specific information
    by overriding the parse_debug_string() method.
    """
    def __init__(self, fw, verbose=False, reduced=False):

        self._db = DebugLogEventInfo(fw.env)
        self.event_key_top_byte = next(iter(self._db.strings)) & 0xFF000000
        self._int_width_bits = fw.env.layout_info.data_word_bits
        self._colour_dict = dict()
        self._colour = AnsiColourCodes()
        self._default_colour = None
        self.verbose = verbose
        self._default_prefix = "%8s: " % fw._core.nicknames[0]
        self.clear_prefix() 
        self.default_colour_set = False # whether client has set default colour manually
        self.clear_exclude_patterns()
        self.clear_include_patterns()
        self.env = fw.env
        self._reduced = reduced
        self.has_line_count = "debugBufferLineCount" in self.env.vars
        self._event_key_mask = 0xffffff if self.has_line_count else 0xffffffff
        # Build a lookup of const variable addresses from masked event key to cope
        # with debug strings coming from the const section
        self._masked_const_var_addresses = dict()
        for name,section_addr, section_len, _ in self.env.elf.get_section_table():
            if name == ".const":
                self._masked_const_var_addresses.update({
                        (a.address & self._event_key_mask) : a.address
                        for a in self.env.elf.vars.values() 
                        if section_addr < a.address < section_addr + section_len})

    def add_exclude_pattern(self, pattern):
        '''
        Add a log line exclusion pattern (a regexp)
        '''
        self._exclude_patterns.append(pattern)
        self._exclude_regexp = "|".join(self._exclude_patterns)
        
    def clear_exclude_patterns(self):
        '''
        Clear all log line exclusions patterns
        '''
        self._exclude_patterns = []
        self._exclude_regexp = ""
        
    def show_exclude_patterns(self):
        '''
        Print all log line exclusion patterns.
        '''
        for pattern in self._exclude_patterns:
            iprint("\"%s\"" % pattern)

    def add_include_pattern(self, pattern):
        '''
        Add a log line inclusion pattern (a regexp).  If any inclusion patterns 
        have been added, only lines matching an inclusion pattern will be shown.
        '''
        self._include_patterns.append(pattern)
        self._include_regexp = "|".join(self._include_patterns)
        
    def clear_include_patterns(self):
        '''
        Clear all inclusion patterns
        '''
        self._include_patterns = []
        self._include_regexp = ""
        
    def show_include_patterns(self):
        '''
        Print all log line inclusion patterns.
        '''
        for pattern in self._include_patterns:
            iprint("\"%s\"" % pattern)
    
    def set_colour_pattern(self, pattern, colour):
        """
        Set the colour of text for strings matching the regular
        expression given in "pattern". Examples:
        -> pattern="config", colour='green' to apply green colour to
         each ocurrence of the word "config" in the string.
        -> pattern="[eE]rror", colour='brightmagenta' to apply bright
         magenta colour to each occurence of the words "Error" and "error".
        -> pattern=".*(?i)panic.*", colour='darkred' to apply dark red
         colour to the entire line if the word "panic" is found, in any
         combination of lowercase and uppercase letters.
        The "colour" parameter is a string with one of the following:
        'black' 'blue' 'cyan' 'green' 'magenta' 'red' 'white' 'yellow'
        optionally prefixed with 'bright' or 'dark'.
        Note that this needs the "colorama" python module installing to
        work on Windows:
        Download from https://pypi.python.org/pypi/colorama, e.g.
        https://pypi.python.org/packages/source/c/colorama/colorama-0.3.3.tar.gz
        and unzip into a folder.
        cd colorama-0.3.3
        python setup.py install      
        """
        self._colour_dict[pattern] = self._get_colour_attr(colour)

    def remove_colour_pattern(self, pattern):
        """Remove the given colour pattern from the list."""
        del self._colour_dict[pattern]

    def reset_colour_patterns(self):
        """Remove all colour patterns from the list."""
        self._colour_dict = OrderedDict()

    def _set_default_colour(self, colour):
        self._default_colour = self._get_colour_attr(colour)
    def set_default_colour(self, colour):
        self._set_default_colour(colour)
        self.default_colour_set = True
    def clear_default_colour(self):
        self._default_colour = None
        self.default_colour_set = False
                
    def _get_colour_attr(self, colour):
        colour = colour.lower()
        try:
            return self._colour.code[colour]
        except KeyError:
            raise AttributeError("Valid colours are: %s" % self._colour.names)

    def _set_prefix(self, prefix, exact=False):
        self._prefix = (prefix if exact else "%8s: " % prefix) if prefix else ""
        
    def set_prefix(self, prefix, exact=False):
        self._set_prefix(prefix, exact=exact)
        self.prefix_set = True

    def clear_prefix(self):
        self._set_prefix(self._default_prefix, exact=True)
        self.prefix_set = False

    def _enum_decoder(self, match):
        """
        Fallback decoder matching the generic text enum:type:value where enum
        is the literal string "enum", type is the name of the type and value
        is a decimal or (prefixed) hexadecimal encoding of the value.

        This decoder runs only if parse_debug_string doesn't provide any
        extra information.

        The net result of this is that any hydra log message that uses the
        correct format (enum:type:value) will now have its enum decoded
        without needing to write a specific handler for the message in Pylib.
        """
        enum = match.group(1)
        dec_match = match.group("dec")
        hex_match = match.group("hex")
        value = int(dec_match) if dec_match is not None else int(hex_match,16)
        try:
            decode_enum = self.env.enums[enum]
        except DwarfNoSymbol:
            # Try looking up non-typedef'd version of name
            try:
                decode_enum = self.env.enums["enum " + enum]
            except DwarfNoSymbol:
                return match.group(0)
        try:
            decode = decode_enum[value]
        except KeyError:
            return match.group(0)
        if hex_match:
            return "%s=0x%x" % (decode, value)
        return "%s=%d" % (decode, value)

    def const_strings(self, addr):
        ''' Return the string from the ELF or None if it can't be found '''
        elf_data_space = self.env.build_info.elf_code.data
        try:
            return CLang.get_string(MemoryPointer(elf_data_space, addr))
        except NotInLoadableElf:
            # bad addr or ran off the end before seeing null terminator
            return None

    def const_fmt_strings(self, addr):
        ''' Return a printf const string from the ELF if it exists.
        This allows for the top byte of the address being used as a
        debug log line counter. Because of the ambiguity we check that
        there is a symbol at the given address before extracting the
        string from the ELF data.
        '''
        try:
            return self.const_strings(self._masked_const_var_addresses[addr & self._event_key_mask])
        except KeyError:
            return None

    def decode(self, buf,  timestamp=None, printf_buf=None, time_fmt=None):
        """
        Consumes all completed event data (key + args) from head of buf and
        returns list of descriptions for those events.
        
        Any incomplete event data at tail end is left in the buf.
        
        Can return an empty list if all events were dropped by filters.
        
        If none of the events were decoded successfully, returns "None".
        
        pywinge: I'd have called a buffer a buffer but the pypolice issued me a
        formal caution for trying.
        """

        db = self._db

        descriptions = []

        putchar_message = ""
        
        if timestamp == False:
            timestr = ""
        elif time_fmt is None:
            if timestamp is None:
                timestr = "----.--- "
            else:
                timestr = str("%04.3f " % (timestamp % 3600))
        else:
            if time_fmt == True:
                time_fmt = "%H:%M:%S:%f "
            if timestamp is None:
                # Py 3 won't accept times on 1st Jan 1970 on Windows so start on
                # 2nd for our arbitrary timestamp.
                dummy_timestr = datetime.fromtimestamp(86400).strftime(time_fmt)
                timestr = re.sub(r"(\d|\w)", "-", dummy_timestr)
            else:
                timestr = datetime.fromtimestamp(timestamp).strftime(time_fmt)
        
        ientry = 0
        while ientry < len(buf):
            # Assume head holds an event key, if not just skip it
            if self.has_line_count:
                # event_key has a line count in the top bits so extract it and
                # replace it with the real top bits from the first entry in the
                # database
                event_key = buf[ientry] & 0xFFFFFF | self.event_key_top_byte
                line_number_str = "%02X: " % (buf[ientry] >> 24)
            else:
                event_key = buf[ientry]
                line_number_str = ""
            ientry += 1

            try:
                argCount = db.arg_counts[event_key]
            except KeyError:
                fmt_str = self.const_fmt_strings(event_key)
                if fmt_str is not None:
                    self._db.add_fmt_string(event_key, fmt_str)
                    argCount = db.arg_counts[event_key]
                else:
                    if event_key & self._event_key_mask != 0 and not self._reduced:
                        iprint("Couldn't find a log string at address 0x%0x" % event_key)
                    continue
            event_length = 1 + argCount

            # If some args haven't arrived yet then were all done here.
            # event_length is useful to identify problems in the code
            # like DBG_MSG("my arg:%d,  <= should be DBG_MSG1
            if ientry + argCount > len(buf):
                if self.verbose:
                    iprint("event_length too long",argCount+1)
                break

            # Lookup the fixed/boilerplate event text and decode all events
            # with boilerplate that matches pattern.
            boilerplate = db.strings[event_key].replace("%p","%x")

            if boilerplate == "putchar string 0x%x" and printf_buf:
                try:
                    description = printf_buf.read_from_arg(buf[ientry])
                except AddressSpace.NoAccess:
                    description = "printf message not available"

                extra_info = self.parse_debug_string(description, [])
                ientry += 1
            elif self._reduced and not boilerplate.replace("#",""):
                # The line has been redacted, so don't display it
                continue
            else:
                # Decode args1
                args = []
                if argCount:
                    arg_types = db.arg_types[event_key]
                    for i in range (argCount):
                        arg_type = arg_types[i]
                        arg = buf[ientry+i]
                        # If its a string arg type use integer arg to lookup
                        if arg_type == db.ArgType.STRING:
                            try:
                                arg = db.strings[arg]
                            except KeyError:
                                arg = self.const_strings(arg)
                                if arg == None:
                                    arg = "<BadStr>"
                        elif arg_type == db.ArgType.SIGNED_INTEGER:
                            arg = to_signed(arg, bits=self._int_width_bits)
                        args.append(arg)

                    ientry += argCount

                # Format basic event info.
                try:
                    basic_info = boilerplate % tuple(args)
                except ValueError:
                    basic_info = boilerplate + " % " + str(args)

                # Check if it's a putchar(character) event:
                putchar_match = re.match(r"^putchar\((.|\n)?\)$",basic_info)

                if putchar_match:
                    # Store the character in the putchar message.
                    putchar_message += putchar_match.group(1)

                    if '\n' in putchar_message:
                        description = putchar_message
                        putchar_message = ""
                    else:
                        continue

                else:
                    if putchar_message:
                        # Putchar message followed by
                        # a non-putchar event:
                        # Insert a new-line in between them.
                        description = putchar_message + '\n'+ timestr + self._prefix + line_number_str + basic_info
                        putchar_message = ""
                    else:
                        # Non-putchar message
                        description = basic_info

                # Optional/Extra info decode.
                extra_info = self.parse_debug_string(boilerplate, args)

            # Extend the description with the extra info
            if extra_info:
                description = description + " (" + str(extra_info) + ")"
            else:
                description = re.sub(r"\benum:(\w+):((?P<hex>(0x[\da-fA-f]+))|(?P<dec>(\d+)))", self._enum_decoder,
                                     description)

            description = self.apply_filters_and_colours(description, line_number_str)
            if description is None:
                # this line was filtered out
                continue
            
            # Add the timestamp string and prefix to the final string
            description = "".join([timestr, description])

            descriptions.append(description)

        return descriptions
    
    def apply_filters_and_colours(self, description, line_number_str=""):
        """
        Apply include/exclude filtering and colouration to the given log message.
        """
        # Apply exclusion filter
        if self._exclude_regexp and \
              re.search(self._exclude_regexp, description):
            return None
        # Apply inclusion filter
        if self._include_regexp and \
              not re.search(self._include_regexp, description):
            return None
            
        # Apply colours (if any)
        colour_patterns = OrderedDict()
        # Default colour
        if self._default_colour:
            colour_patterns["^.*$"] = self._default_colour
        # Patterns
        colour_patterns.update(self._colour_dict)
        # Add the colour codes for the default colour and the patterns
        if colour_patterns:
            description = add_colours(colour_patterns, description)
        return "".join([self._prefix, line_number_str, description])

    # Protected / Overridable

    def parse_debug_string(self, string, args):
        '''\
        Virtual function to be overridden to add extra parsing for each
        debug string that is printed.
        /param string - The debug string to be printed
        /param args - a list of the argument values
        /returns a string object that will be appended to the debug string
                or None if no further information is provided
        '''
        return None


class GenericHydraLogDecoder (DebugLogDecoder):
    '''\
    DebugLogDecoder with specialisation common to GenericHydra and Curator log
    interpretation.
    '''
    def __init__(self, fw):

        fw_info = fw.env.build_info

        TypeCheck(fw_info, IGenericHydraFirmwareBuildInfo)

        DebugLogDecoder.__init__(self, fw)
        self._fw_info = fw_info
        self._fw_env = fw.env

        # Create dictionaries to lookup interesting event arg names.
        #
        self._ccp_signals = self._lookup_dict_from_enum('CCP_SIGNAL_ID', True)
        self._reboot_reason_enum = self._lookup_dict_from_enum('CCP_REBOOT_REASON_TYPE', True)
        self._ftp_signals = self._lookup_dict_from_enum('FTP_SIGNAL_ID', True)
        self._isp_ports = self._lookup_dict_from_enum('ISP_PORT_MAPPING_ENUM')
        self._resources = self._lookup_dict_from_enum("CCP_RESOURCE_ID", True)
        
        self._mibs = {}
        if fw_info.mibdb:
            for name, mib in fw_info.mibdb.items():
                self._mibs[mib.psid()] = name
            
        self._panics = fw.env.enums["panicid"]
        self._faults = fw.env.enums["faultid"]

    # Protected / provided
    @property
    def _subserv_op_enum(self):
        # Construct lazily...
        try:
            self._subserv_op_enum_internal
        except AttributeError:
            self._subserv_op_enum_internal = (
                        self._fw_env.cus["subserv_init.c"].enums["SUBSERV_OP"])
        return self._subserv_op_enum_internal

    def _lookup_dict_from_enum(self, enum_name, lower_case=False,
                               enum_starts_with=""):
        ''' Create a dictionary for translating numbers into enum strings
        if possible. Otherwise return an empty dictionary (silent fail) '''
        lookup = dict()
        try:
            for name,value in self._fw_env.enums[enum_name].items():
                new_name = name
                if enum_starts_with:
                    ''' Take only those enums that start with the pattern,
                        remove pattern from enum name. '''
                    if not name.startswith(enum_starts_with):
                        continue
                    new_name = name.split(enum_starts_with + "_")[1]
                if name.startswith(enum_name):
                    new_name = name.split(enum_name + "_")[1]
                if lower_case:
                    new_name = new_name.lower()
                lookup[value] = new_name
        except DwarfNoSymbol:
            pass
        return lookup
        
    def _lookup_types_from_elf(self, enum_list):
        ''' If the interface xml isn't available then we can use
        symbols from the Elf instead for most cases. These have slightly 
        different names to the xml because of the way the prim files
        are autogenerated so this method does some attempt at translating
        them back so they look alike.
        '''
        interface= dict()
        for enum_name in enum_list:
            try:
                enum_dict = dict(self._fw_env.enums[enum_name])
                for key in list(enum_dict.keys()):
                    if key.startswith(enum_name):
                        enum_dict[key.split(enum_name + "_")[1]] = enum_dict[key]
            except DwarfNoSymbol:
                enum_dict = dict()
            interface[enum_name] = enum_dict
        return interface
        

class Apps1LogDecoder(DebugLogDecoder):
    """
    Apps subsystem-specific decoding
    """
    msg_regexp = re.compile(r"(MSG|MESSAGE):((?P<enum_name>\w+):)?((?P<hex_msg_id>(0x)[0-9a-fA-F]+)|(?P<dec_msg_id>\d+))")

    def __init__(self, fw):
        DebugLogDecoder.__init__(self, fw)
        self._fw_env = fw.env
        self._panics = fw.env.enums["panicid"]
        self._faults = fw.env.enums["faultid"]

        self._trap_decoder = TrapLogDecoder(fw.env, None)
        self.message_id_strings = ["AVRCP_MSG id=",
                                   "Unhandled CL msg[",
                                   "CL = [",
                                   "HFP = [",
                                   "CL[",
                                   "HFP[",
                                   "UE unhandled!! [",
                                   "HS : LEDEvCmp[",
                                   "HS : UE[",
                                   "UE[" ]

        # Things to strip off the end of print strings to leave the message ID
        # number we are interested in
        self.trailing_chars = " :]"

        self._msg_enums = fw.msg_enums
    
        self._msg_enums = fw.msg_enums

    def parse_debug_string(self, print_string, args):
        for msg_str in self.message_id_strings:
            if msg_str in print_string:
                try:
                    hex_str = print_string.split(msg_str)[1].strip(self.trailing_chars)
                    msg_id = int(hex_str,16)
                    return self._trap_decoder.get_id_name(0, msg_id)
                except IndexError:
                    pass
                except ValueError:
                    pass
        if self._msg_enums is not None:
            interp_string = print_string % tuple(args)
            m = self.msg_regexp.search(interp_string)
            if m:
                enum_name = m.group("enum_name")
                if m.group("hex_msg_id"):
                    msg_id = int(m.group("hex_msg_id"),16)
                else:
                    msg_id = int(m.group("dec_msg_id"))
                msg_name = self._msg_enums.name_from_id(msg_id, enum_name=enum_name,
                                                        no_exceptions=True)
                if msg_name is not None:
                    return msg_name

        try:
            if (print_string.startswith("DM PRIM") or print_string.startswith("DM EVT")) and len(args) > 0:
                return self._fw_env.enums["DM_PRIM_T"][args[0]].split("ENUM_")[1]
            elif (print_string.startswith("L2CAP PRIM") or print_string.startswith("L2CAP EVT")) and len(args) > 0:
                return self._fw_env.enums["L2CAP_PRIM_T"][args[0]].split("ENUM_")[1]
            elif (print_string.startswith("ATT PRIM") or print_string.startswith("ATT EVT")) and len(args) > 0:
                return self._fw_env.enums["ATT_PRIM_T"][args[0]].split("ENUM_")[1]
            elif (print_string.startswith("RFCOMM PRIM") or print_string.startswith("RFCOMM EVT")) and len(args) > 0:
                return self._fw_env.enums["RFC_PRIM_T"][args[0]]
            elif (print_string.startswith("SDP PRIM") or print_string.startswith("SDP EVT")) and len(args) > 0:
                return self._fw_env.enums["SDS_PRIM_T"][args[0]]
            elif print_string.startswith("PANIC"):
                return self._decode_panic_diatribes(args)
            elif print_string.startswith("FAULT"):
                return self._faults[args[0]]
        except KeyError:
            #We don't know about this message
            pass
        except IndexError:
            # The parameters we were expecting for this string aren't there so
            # we must have matched it unintentionally - leave as a raw number
            pass

    def _decode_panic_diatribes(self, args):
        '''
        Repository of ad hoc code to decode diatribes.  Please extend!
        '''

        # With a branch through zero the diatribe contains the return
        # address at the time of the dodgy call, which we can decode.
        # If it was a plain branch rather than a function call, we'll be
        # reporting one level higher than we want up the stack, but
        # it'll still help.
        if self._panics[args[0]] == "PANIC_HYDRA_BRANCH_THROUGH_ZERO":
            ret_addr = args[1]
            try:
                func_name = self._fw_env.functions[ret_addr]
                func_sym = self._fw_env.dwarf.get_function(func_name)
                offset = ret_addr - self._fw_env.functions[func_name]
                _, line_no = func_sym.get_srcfile_and_lineno(offset-2)
                return ('%s from %s, line %d' %
                        (self._panics[args[0]], func_name, line_no))
            except (KeyError, DwarfNoSymbol):
                pass
        elif self._panics[args[0]] == "PANIC_KALIMBA_MEMORY_EXCEPTION":
            excep_type = args[1]
            excep_name = self._fw_env.enums["exception_type"][excep_type]
            return '%s: %s' % (self._panics[args[0]], excep_name)

        return self._panics[args[0]]

class HCILogDecoder(DebugLogDecoder):
    """
    Placeholder for proper decoding of the HCI log
    """
    def __init__(self, fw):
        DebugLogDecoder.__init__(self, fw, reduced=True)



class PrimLogDecoder(DebugLogDecoder):
    """
    Primitive logger specific decoding
    """
    def __init__(self, fw, core):
        DebugLogDecoder.__init__(self, fw)

        def make_enum_ids_dict(prim_type_name):
            ''' Convert prim enum dict (keyed by name) into dictionary that is
                keyed on the enum values and holds the names '''
            enum = core.subsystem.p1.fw.env.enums[prim_type_name + "_PRIM_T"]
            return {id : name for name, id in enum.items()}

        def make_prim_dict(prim_type_name_list):
            ''' Make dict containing the protocol name and dict of prim enums
                keyed by value '''
            d = dict()
            d['ids_to_names'] = dict()
            prim_types_found = []
            for prim_type_name in prim_type_name_list:
                try:
                    d['ids_to_names'].update(make_enum_ids_dict(prim_type_name))
                    prim_types_found.append(prim_type_name)
                except DwarfNoSymbol:
                    pass
            d['protocol_name'] = '|'.join(n + "_PRIM" for n in prim_types_found)
            return d

        try:
            # Dict containing prim info dicts keyed by protocol ID
            self._prim_dict = dict()
            self._prim_dict[4] = make_prim_dict(["DM"])
            self._prim_dict[8] = make_prim_dict(["L2CAP"])
            self._prim_dict[9] = make_prim_dict(["RFC"])
            self._prim_dict[10] = make_prim_dict(["SDC", "SDS", "ATT", "MDM", "VSDM"])
            self._enums_available = True
        except AttributeError:
            self._enums_available = False

        self._fw_env = fw.env
        self._core = core

    def decode_primitive(self, prim_len, protocol, direction, buf, xml):
        display_lines = []
        deserialise = self._core.info.layout_info.deserialise
        # Determmine the type of the primitive from the first uint16
        id = deserialise(buf[0:2])

        # Is the protocol recognised?
        protocol_name = None
        try:
            protocol_name = self._prim_dict[protocol]['protocol_name']
        except KeyError:
            pass

        if protocol_name != None:
            # The protocol is recognised
            # Get the enum identifier from the id value
            id_name = None
            try:
                id_name = self._prim_dict[protocol]['ids_to_names'][id]
            except KeyError:
                pass

            if id_name != None:
                # The enum identifier has been obtained
                # Derive the structure identifier from the enum identifier
                struct_name = id_name
                if struct_name.startswith("ENUM_"):
                    # All but RFC_PRIM_T have enum identifiers starting with
                    # "ENUM_" which needs to be removed if present
                    struct_name = struct_name[len("ENUM_"):]
                # Then the structure identifier is derived by simply adding
                # "_T" to the end of what then remains from the enum identifier
                struct_name = struct_name + '_T'

                # Get the structure type from the structure identifier
                try:
                    struct_type = self._fw_env.types[struct_name]
                except DwarfNoSymbol:
                    struct_type = None

                if struct_type != None:
                    # The structure type is known
                    
                    if xml == True:
                        prefix = ""
                        if direction == 0:
                            # The "to app" direction
                            dir_str = "up"
                        elif direction == 1:
                            # The "from app" direction
                            dir_str = "down"
                        else:
                            raise ValueError("%d" % direction)
                    else:
                        if direction == 0:
                            # The "to app" direction
                            prefix = "<<"
                        elif direction == 1:
                            # The "from app" direction
                            prefix = ">>"
                        else:
                            raise ValueError("%d" % direction)

                    byte_array = buf

                    # Convert the array of bytes into a structure
                    struct = _Variable.create_from_type(
                        struct_type,
                        0,
                        byte_array,
                        self._core.info.layout_info,
                        ptd_to_space=False)

                    if xml == True:
                        display_lines += \
                            ['<bs_prim direction="%s">' % dir_str]
                            
                    # Get the displayable form of the structure
                    display_lines += struct.display("BS prim", prefix, [], [])

                    if xml == True:
                        display_lines += ['</bs_prim>']
                else:
                    # The structure type is not known
                    display_line = "Unknown structure %s for prim id 0x%04x " \
                        % (struct_name, id)
                    display_line += "(%s) for protocol %d (%s)\n" \
                        % (id_name, protocol, protocol_name)
                    display_lines.append(display_line)
            else:
                # The enum identifier is not known
                display_line = "Unknown prim id 0x%04x for protocol %d (%s)\n" \
                    % (id, protocol, protocol_name)
                display_lines.append(display_line)
        else:
            # The protocol is not recognised
            display_line = "Prim id 0x%04x for unknown protocol %d\n" \
                % (id, protocol)
            display_lines.append(display_line)

        return display_lines

    def decode(self, buf, timestamp=None, printf_buf=None, xml=False):
        """\
        Consumes primitive logs.
        """

        if timestamp is None:
            prim_timestr = "----.--- "
        elif timestamp == False:
            prim_timestr = ""
        else:
            prim_timestr = str("%04.3f " % (timestamp % 3600))

        descriptions = []
        if self._enums_available:
            while buf:

                try:
                    prim_len = (buf[1] * 256) + buf[0]
                    protocol = buf[2]
                    direction = buf[3]
                    display_lines = self.decode_primitive(
                        prim_len, protocol, direction, buf[4:prim_len+4], xml)

                    if prim_timestr:
                        timestamped_display_lines = []
                        timestrs = [" " * len(prim_timestr)] * len(display_lines)
                        timestrs[0] = prim_timestr
                        
                        for timestr, display_line in zip(timestrs, display_lines):
                            timestamped_display_lines.append(timestr + " " + display_line)
                        descriptions.append(timestamped_display_lines)
                    else:
                        descriptions.append(display_lines)
                    del buf[:prim_len+4]
                except IndexError:
                    # Don't let a problem with an incomplete primitive cause
                    # a crash that will prevent decoding of further primitives.
                    break
        else:
            display_line = ["No apps1 primitive enumeration information available. Cannot decode."]
            descriptions.append(display_line)

        return descriptions

class TrapLogDecoder(object):
    """
    Trap api message logger specific decoding
    """

    _type_names = {0: "Send    ", 1: "Deliver ", 2: "Free    ", 3: "Cancel   "}
    
    def __init__(self, fw_env, core):

        self._fw_env = fw_env
        self._core = core

        self._ids_to_names = {}
        # Get the various enumerations keyed on id in self._ids_to_names
        self._add_enumeration('sinkEvents_t')
        self._add_enumeration('UpgradeMsgFW')
        self._add_enumeration('ConnectionMessageId')
        self._add_enumeration('NFC_VM_MSG_ID')
        self._add_enumeration('NFC_CL_MSG_ID')
        self._add_enumeration('HfpMessageId')
        self._add_enumeration('PowerMessageId')
        self._add_enumeration('PbapcMessageId')
        self._add_enumeration('PbapcAppMsgId')
        self._add_enumeration('MapcMessageId')
        self._add_enumeration('MapcAppMessageId')
        self._add_enumeration('SinkAvrcpMessageId')
        self._add_enumeration('AvrcpMessageId')
        self._add_enumeration('avrcp_ctrl_message')
        self._add_enumeration('A2dpMessageId')
        self._add_enumeration('audio_plugin_upstream_message_type_t')
        self._add_enumeration('audio_plugin_interface_message_type_t')
        self._add_enumeration('usb_device_class_message')
        self._add_enumeration('GaiaMessageId')
        self._add_enumeration('display_plugin_upstream_message_type_t')
        self._add_enumeration('SwatMessageId')
        self._add_enumeration('fm_plugin_upstream_message_type_t')
        self._add_enumeration('upgrade_application_message')
        self._add_enumeration('SYSTEM_MESSAGE_ENUM')

        try:
            temp_names_to_ids = fw_env.enums["CL_INTERNAL_T"]
            self._CL_INTERNAL_T_ids_to_names = dict((id, name) for (name, id) in 
                                          temp_names_to_ids.items())
        except (AttributeError, DwarfNoSymbol):
            pass

        try:
            temp_names_to_ids = fw_env.enums["HFP_INTERNAL_T"]
            self._HFP_INTERNAL_T_ids_to_names = dict((id, name) for (name, id) in 
                                          temp_names_to_ids.items())
        except (AttributeError, DwarfNoSymbol):
            pass

        try:
            temp_names_to_ids = fw_env.enums["AVRCP_INTERNAL_T"]
            self._AVRCP_INTERNAL_T_ids_to_names = dict((id, name) for (name, id) in 
                                          temp_names_to_ids.items())
        except (AttributeError, DwarfNoSymbol):
            pass

        try:
            temp_names_to_ids = fw_env.enums["A2DP_INTERNAL_T"]
            self._A2DP_INTERNAL_T_ids_to_names = dict((id, name) for (name, id) in 
                                          temp_names_to_ids.items())
        except (AttributeError, DwarfNoSymbol):
            pass

        self._sizeof_pointer = self._fw_env.types['Task']['byte_size']

    def _add_enumeration(self, enumeration):
        try:
            # Get the _EnumLookup keyed by name
            temp_names_to_ids = self._fw_env.enums[enumeration]
            # Convert it into a dictionary keyed by id
            temp_ids_to_names = dict((id, name) for (name, id) in 
                                          temp_names_to_ids.items())
            # Add that into the dictornary collected so far
            self._ids_to_names.update(temp_ids_to_names);
        except DwarfNoSymbol:
            pass

    def _get_minimal_name(self, full_key, container):
        if full_key is None:
            return "?"
            
        minimal_key_cmpts = container.minimal_unique_subkey(full_key,
                                                            join=False)
        if "." in minimal_key_cmpts[0]:
            cu = self._fw_env.cus.minimal_unique_subkey(minimal_key_cmpts[0], join=True)
            return "::".join(minimal_key_cmpts[1:]) + " (%s)" % cu
        else:
            return "::".join(minimal_key_cmpts)

    def _get_handler_name(self, handler_address):
        if handler_address is None:
            handler_desc = "[NONE]"
        elif handler_address == 0:
            handler_desc ="[NULL]"
        else:
            try:
                handler_desc = self._get_minimal_name(self._fw_env.functions.find_by_address(handler_address),
                                                self._fw_env.functions)
            except (AttributeError, KeyError, IndexError, AddressSpace.NoAccess, BadPCHigh):
                handler_desc = "[0x%08X]" % handler_address

        return handler_desc

    def _get_task_name(self, task):
        # Validate the task
        # The task must be in P1 RAM or P1 const space
        self._valid_task_ranges = AddressMultiRange(self._core.dm.map.select_subranges("P1_DATA_RAM", 
                                                    "P1_CACHED_SQIF_FLASH_0"))
        if task == 1:
            task_name = "[invalidated]"
        if task not in self._valid_task_ranges:
            task_name = "[invalid]"
        else:
            task_key = self._fw_env.vars.find_by_address(task)
            if task_key is None:
                task_name = "[dynamic]"
            else:
                try:
                    task_name = self._get_minimal_name(self._fw_env.vars.find_by_address(task),
                                                self._fw_env.vars)
                except (AttributeError, KeyError, IndexError, AddressSpace.NoAccess, BadPCHigh):
                    task_name = "[invalid]"

        return task_name

    def _handler_from_task(self, task):
        try:
            task_data = self._fw_env.cast(task, "TaskData")
            # Get the function pointer
            handler_address = task_data.handler.value
            return handler_address
        except (AttributeError, KeyError, IndexError, AddressSpace.NoAccess, BadPCHigh):
            return None

    def get_task_name(self, task):
        # Old-style reporting of both task and handler
        return ", ".join([self._get_task_name(task),self._get_handler_name(self._handler_from_task(task))])

    def get_id_name(self, task, id):
        # Get the enum identifier from the id value if possible
        try:
            return self._ids_to_names[id]
        except (AttributeError, KeyError):
            try:
                if self._get_handler_name(self._handler_from_task(task)) == 'LedsMessageHandler':
                    # LedsMessageHandler does not use an enum for id but rather
                    # uses the LED number. 0x1000 is masked in to indicate a
                    # "Dimming LED Update message"
                    return "LED %d" % (id % 0x1000)
                elif self._get_handler_name(self._handler_from_task(task)) == 'connectionBluestackHandler':
                    try:
                        return self._CL_INTERNAL_T_ids_to_names[id]
                    except (AttributeError, KeyError):
                        pass
                    return "LED %d" % (id % 0x1000)
                elif self._get_handler_name(self._handler_from_task(task)) == 'hfpProfileHandler':
                    try:
                        return self._HFP_INTERNAL_T_ids_to_names[id]
                    except (AttributeError, KeyError):
                        pass
                elif self._get_handler_name(self._handler_from_task(task)) == 'avrcpInitHandler' or \
                     self._get_handler_name(self._handler_from_task(task)) == 'avrcpProfileHandler':
                    try:
                        return self._AVRCP_INTERNAL_T_ids_to_names[id]
                    except (AttributeError, KeyError):
                        pass
                elif self._get_handler_name(self._handler_from_task(task)) == 'a2dpProfileHandler':
                    try:
                        return self._A2DP_INTERNAL_T_ids_to_names[id]
                    except (AttributeError, KeyError):
                        pass
            except KeyError:
                pass
        return None

    def decode_trap_message(self, delimiter, rec_len, mcast, buf, xml):
        display_lines = []
        task_list = []

        deserialise = self._core.info.layout_info.deserialise
        posn = 0
        # Sequence number is uint16 or uint32 depending on format
        if delimiter == 0x10910900:
            seq = deserialise(buf[posn:posn+2])
            posn += 2
        elif delimiter == 0xc001d00d:
            seq = deserialise(buf[posn:posn+4])
            posn += 4

        try:
            if seq > (self._seq + 1):
                display_lines += ['<!-- %d record(s) missed >' 
                    % (seq - (self._seq + 1))]
            self._seq = seq
        except AttributeError:
            self._seq = seq
            
        # The timestamp is uint32
        now = deserialise(buf[posn:posn+4])
        posn += 4

        # The type is uint8.
        type = buf[posn]
        posn += 1

        if type > 3:
            display_lines += \
                ['<!-- invalid type %d: skipping record %d>' % (type, seq)]
            return display_lines

        if mcast:
            num_task_entries = mcast
        else:
            num_task_entries = 1

        # Read the task/handler pairs (or just task if old format)
        while num_task_entries:
            # Read the task and handler from the log

            # The task is a pointer
            task = deserialise(buf[posn:posn+self._sizeof_pointer])
            posn += self._sizeof_pointer

            if delimiter == 0x10910900:
                # The handler is in the log
                handler = deserialise(buf[posn:posn+self._sizeof_pointer])
                posn += self._sizeof_pointer
            elif delimiter == 0xc001d00d:
                # The handler needs generating from the task
                handler = self._handler_from_task(task)

            task_list.append("Task 0x{:05x} ({}, {})".format(task, self._get_task_name(task), self._get_handler_name(handler)))
            num_task_entries -= 1

        # Message id is a uint16
        id = deserialise(buf[posn:posn+2])
        posn += 2

        condition_addr = deserialise(buf[posn:posn+self._sizeof_pointer])
        posn += self._sizeof_pointer

        # Due timestamp is a uint32. We will print it as a delta later.
        due = deserialise(buf[posn:posn+4])
        posn += 4

        msg_len = deserialise(buf[posn:posn+2])
        posn += 2

        if xml == True:
            display_lines += ['<trap>']

        # We've got all the info we need, so buildup the output
        display_line = "SN: {:4d}, Time:{:10d}, {:8s}in {:5d}ms (at {:d}), ".format(seq, now, self._type_names[type], due-now, due)
        pre_task_len = len(display_line)

        # First task is inline
        display_line += task_list[0]

        display_line += ", Id 0x%04X" % id

        id_name = self.get_id_name(task, id)
        if id_name:
            display_line += " (%s)" % id_name
                
        if condition_addr != 0:
            display_line += ", Conditional (0x%08x) " % condition_addr

        if msg_len > 0:
            display_line += ", Msg hex:"
            index = 0
            if (posn+msg_len) <= len(buf):
                while index < msg_len:
                    display_line += "%02X " % buf[posn+index]
                    index += 1
            else:
                display_line += \
                    ("\n<!-- msg_len {} too large, posn {}, len(buf) {} >"). \
                            format(msg_len, posn, len(buf))

        display_lines.append(display_line)

        # Here, do extra lines for multicast task lists
        i = 1
        while i < mcast:
            display_line = " "*(pre_task_len)
            display_line += task_list[i]
            display_lines.append(display_line)
            i += 1
        
        if xml == True:
            display_lines += ['</trap>']

        return display_lines


    def decode(self, buf, timestamp=None, printf_buf=None, xml=False):
        """\
        Consumes trap api message logs.
        """

        deserialise = self._core.info.layout_info.deserialise

        descriptions = []
        while buf:

            if len(buf) < 28:
                # There is not the minimum in the buf for the shortest record
                display_lines = ["<!-- Insufficient buffer left for record: %d bytes >" % len(buf)]
                descriptions.append(display_lines)
                break

            # There have been cases where things have not been found were
            # expected in buf, so a 32-bit delimiter of 0xc001d00d (3221344269)
            # or 0x109109mm (where mm is the number of mcast recipients)
            # has been used to ensure alignment of location. This immediately
            # follows a 16-bit rec_len, so should start at buf[2] if everything
            # is as it should be. It that is not the case, scan until found,
            # if there is one.
            for posn in range(2, len(buf) - 4):
                delimiter = deserialise(buf[posn:posn+4])

                if delimiter == 0xc001d00d:
                    mcast = 0
                    break
                if delimiter & 0xffffff00 == 0x10910900:
                    mcast = delimiter & 0x000000ff
                    delimiter = 0x10910900
                    break

            if (delimiter != 0xc001d00d) and (delimiter != 0x10910900):
                display_lines = ["<!-- Unable to find delimiter in %d bytes >" % len(buf)]
                descriptions.append(display_lines)
                break

            if posn > 2:
                display_lines = ["<!-- Skipped %d bytes >" % (posn - 2)]
                descriptions.append(display_lines)
                del buf[:posn-2]

            # Having found the delimiter, the rec_len should be just before it
            rec_len = deserialise(buf[0:2])

            if rec_len < 26:
                # There is not the minimum in the buf for the shortest record
                display_lines = ["<!-- Insufficient rec_len for minimum record: %d bytes >" % rec_len]
                descriptions.append(display_lines)
                break

            if (rec_len + 2) > len(buf):
                display_lines = \
                    ["<!-- Record length %d is greater than space remaining %d >" % \
                        (rec_len + 2, len(buf))]
                descriptions.append(display_lines)
                break

            # A valid record. Decode from after the delimiter
            display_lines =  self.decode_trap_message(
                    delimiter, rec_len, mcast, buf[6:rec_len+2], xml)
            if len(display_lines) > 0:
                descriptions.append(display_lines)
            del buf[:rec_len+2]

        return descriptions


class DebugPrintfLog(object):
    def __init__(self, fw_env, core):
        self._fw_env = fw_env
        try:
            self.char_buffer_addr = fw_env.globalvars["debugCharBuffer"].address
            self.char_buffer_size = fw_env.globalvars["debugCharBuffer"].size
            self.char_buffer_pos_addr = fw_env.globalvars["debugCharBufferPos"].address
            self._mem = core.dm
        except (KeyError, AttributeError):
            pass
  
    def read(self, offset, length):
        try:
            self.char_buffer_addr
        except AttributeError:
            return None

        # Read an extra character prior to the offset we are asked for so we 
        # can check it is a delimiter        
        if offset + length < self.char_buffer_size:
            mem = self._mem[self.char_buffer_addr + offset - 1:
                            self.char_buffer_addr + offset + length]
        else:
            remaining_len = (offset + length) - self.char_buffer_size
            mem = (self._mem[self.char_buffer_addr + offset - 1:
                           self.char_buffer_addr + self.char_buffer_size] +
                  self._mem[self.char_buffer_addr: 
                           self.char_buffer_addr + remaining_len])
        raw_string = bytearray(mem)
        for a in range(len(raw_string)):
            if chr(raw_string[a]) not in string.printable:
                raw_string[a] = ord(".")

        print_string = str(raw_string.decode())
        
        # If offset is zero then assume it is a good start of a string
        # Otherwise check that the previous character was a newline
        prev_char_is_delimiter = print_string[0] == '\n' if offset else True
        # Trim the previous character off now
        print_string = print_string[1:]
        # The firmware separates strings at the \n so we can use that as
        # a test for buffer overruns
        if print_string[-1] == '\n' and not '\n' in print_string[:-1] and \
                                                    prev_char_is_delimiter:
            return print_string[:-1]
        return "String len %d (characters not available)" % len(print_string)

    def read_from_arg(self, arg):
        end_offset = arg & 0xffff
        str_len = arg >> 16
        start_offset = end_offset + 1 - str_len 
        if start_offset < 0:
            start_offset += self.char_buffer_size
            
        return self.read(start_offset, str_len)
