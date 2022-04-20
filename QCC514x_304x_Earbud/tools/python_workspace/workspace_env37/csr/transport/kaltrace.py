# Copyright(c) 2016-18 Qualcomm Technologies International, Ltd.
# All Rights Reserved.
# Qualcomm Technologies International, Ltd.Confidential and Proprietary.

# Python bindings for the kaltrace library.
# $Change: 3680160 $

from ctypes import byref, c_bool, c_char_p, c_double, c_int16, c_uint16, c_uint32, c_uint8, c_void_p, cdll, POINTER, Structure
import os
import sys
import platform


class KalTrace:
    """Python wrapper of kaltrace library."""
    
    # Indicates that there is no branch for a field that would otherwise store a branch target.
    NO_TARGET = 0xFFFFFFFF

    class InstructionType(Structure):
        """Describes a single instruction within a KalTraceResult."""
        _fields_ = [
                    ("instruction",         c_uint32),  # MaxiM-decoded version of the instruction
                    ("prefix",              c_uint32),  # MaxiM-decoded version of the prefix (if appropriate; 0 otherwise)
                    ("branch_target",       c_uint32),  # Target of a jump or call (if present, otherwise NO_TARGET)
                    ("doloop_start_target", c_uint32),  # Target of an end-of-do-loop branch (calculated from beginning of do loop); NO_TARGET if invalid
                    ("instruction_category",c_uint16),  # Bitfield (since we can at once have a conditional call at the end of a do loop that goes to interrupt)
                    ("call_stack_depth",    c_int16),   # Depth in call stack (integer, normalised so lowest entry at 0)
                    ("pc_increment",        c_uint8)    # PC increment (expected increment to next instruction)
                   ]
                   
    class InstructionTypesForRange(Structure):
        """Python equivalent of the instruction_types_for_range_struct type in kaltrace.h."""
        pass    # _fields_ is set below, once KalTrace is defined and hence KalTrace.InstructionType referencable.

    class TraceResult(Structure):
        """Python equivalent of the trace_result type in kaltrace.h."""
        pass    # _fields_ is set below, once KalTrace is defined and hence KalTrace.InstructionTypesForRange referencable.
        
    class _InstructionFrequency(Structure):
        """Describes the frequency of use of a single instruction, for internal use with the kaltrace DLL interface."""
        _fields_ = [
                    ("instruction_address", c_uint32),      # Address (PC value) of instruction.
                    ("count",               c_uint32)       # Number of times this instruction was executed.
                   ]
                   
    class InstructionFrequency:
        """Describes the frequency of use of a single instruction.
           Field pc holds the address of the instruction and field count holds the number of times it was executed."""
        def __init__(self, pc, count):
            self.pc = pc
            self.count = count

    def _load_kaltrace_dll(self):
        """Finds and loads the kaltrace library. Stores it as self._kaltrace_dll."""

        # Find the absolute path of this script
        mydir = os.path.abspath(os.path.dirname(__file__))

        # Load the C++ library. Give a bit of support to try to ease troubleshooting of common problems.
        if sys.platform.startswith('linux'):
            try:
                self._kaltrace_dll = cdll.LoadLibrary(os.path.join(mydir, "libkaltrace_shared.so"))
            except Exception as ex:
                message = ("Could not find or load libkaltrace.so (or one of its dependencies).\n"
                           "If the library is present, check that your Python installation type (32/64-bit) matches "
                           "the architecture of the kaltrace shared library (e.g. via the 'file' command)."
                           "The LD_LIBRARY_PATH used for the search was:\n    ")
                message += "\n    ".join(os.environ.get('LD_LIBRARY_PATH', '').split(":"))
                message += "\n\nInner Python exception : %r" % ex
                raise OSError(message)

        elif sys.platform.startswith('cygwin') or sys.platform.startswith('win32'):
            # On Cygwin, path elements are separated by colons, but on win32 it's a semi-colon.
            path_element_separator = ":" if sys.platform.startswith('cygwin') else ";"
            
            arch,_ = platform.architecture()
            subdir = {"64bit" : "win64", "32bit" : "win32"}[arch]
            dll_dir = os.path.join(mydir, subdir)
            # Add the absolute path of the DLL to the system path
            if os.environ.get("PATH", "").find(dll_dir) == -1:
                os.environ["PATH"] = dll_dir + path_element_separator + os.environ.get("PATH", "")

            try:
                self._kaltrace_dll = cdll.LoadLibrary("kaltrace.dll")
            except Exception as ex:
                message = ("Could not find or load kaltrace.dll (or one of its dependencies).\n"
                           "If the library is present, check that your Python installation type (32/64-bit) matches "
                           "the kaltrace DLL.\n"
                           "Sometimes this error can be fixed by installing a Visual C++ Redistributable package.\n"
                           "The system PATH used for the search was:\n    ")
                message += "\n    ".join(os.environ.get('PATH', '').split(path_element_separator))
                message += "\n\nInner Python exception : %r" % ex
                raise OSError(message)
        else:
            raise OSError("Cannot load the kaltrace library. The system '%s' you are using is not supported."
                          % sys.platform)
    
    def _add_cfunc(self, name, result_type, arg_types):
        """Adds the described function to those stored in _cfuncs. Implementation method for _init_cfuncs."""
        dll_function = getattr(self._kaltrace_dll, name)
        if dll_function is None:
            raise Exception("Failed to find function {0} in the loaded kaltrace library.".format(name))
        dll_function.argtypes = arg_types
        dll_function.restype = result_type
        self._cfuncs[name] = dll_function
        
    def _init_cfuncs(self):
        """Create ctypes declarations of the functions we need from kaltrace.dll."""
        self._cfuncs = {}
        self._add_cfunc('kaltrace_decode_trace_hardware',        POINTER(KalTrace.TraceResult),           [POINTER(c_uint32), c_uint32, c_char_p])  # (trace data, trace data size in uint32s, elf filename) -> TraceResult allocated with malloc
        self._add_cfunc('kaltrace_decode_trace_with_timestamps', POINTER(KalTrace.TraceResult),           [POINTER(c_uint32), c_uint32, c_char_p, POINTER(c_uint32), c_uint32])  # (trace data, trace data size in uint32s, elf filename, timestamp data, timestamp data size in uint64s) -> TraceResult allocated with malloc
        self._add_cfunc('kaltrace_decode_trace_kalsim',          POINTER(KalTrace.TraceResult),           [c_char_p, c_char_p])                     # (trace file path, elf filename) -> TraceResult allocated with malloc
        self._add_cfunc('kaltrace_destroy_trace',                None,                                    [POINTER(KalTrace.TraceResult)])          # (pointer to trace to free)
        self._add_cfunc('kaltrace_context_create_hardware',      c_void_p,                                [POINTER(c_uint32), c_uint32, c_char_p])  # (trace data, trace data size in uint32s, elf filename) -> kaltrace context
        self._add_cfunc('kaltrace_context_create_hardware_ts',   c_void_p,                                [POINTER(c_uint32), c_uint32, POINTER(c_uint32), c_uint32, c_char_p])  # (trace data, trace data size in uint32s, timestamp data, timestamp data size in uint64s, elf filename) -> kaltrace context
        self._add_cfunc('kaltrace_context_create_kalsim',        c_void_p,                                [c_char_p, c_char_p])                     # (trace file path, elf filename) -> kaltrace context
        self._add_cfunc('kaltrace_context_destroy',              None,                                    [c_void_p])                               # (kaltrace context)
        self._add_cfunc('kaltrace_decode_begin',                 c_uint32,                                [c_void_p])                               # (kaltrace context) -> first PC
        self._add_cfunc('kaltrace_decode_next',                  c_uint32,                                [c_void_p])                               # (kaltrace context) -> next PC
        self._add_cfunc('kaltrace_decode_end',                   c_bool,                                  [c_void_p])                               # (kaltrace context) -> bool (is at end?)
        self._add_cfunc('kaltrace_decode_ts_begin',              c_uint32,                                [c_void_p, POINTER(c_uint32)])            # (kaltrace context, out timestamp) -> first PC
        self._add_cfunc('kaltrace_decode_ts_next',               c_uint32,                                [c_void_p, POINTER(c_uint32)])            # (kaltrace context, out timestamp) -> next PC
        self._add_cfunc('kaltrace_count_number_of_instructions', c_uint32,                                [c_void_p])                               # (kaltrace context) -> number of instructions
        self._add_cfunc('kaltrace_get_instruction_frequencies',  POINTER(KalTrace._InstructionFrequency), [c_void_p, POINTER(c_uint32)])            # (kaltrace context, output instruction count) -> kaltrace_instruction_frequency array (with output instruction count elements)
        self._add_cfunc('kaltrace_free_instruction_frequencies', None,                                    [POINTER(KalTrace._InstructionFrequency)])# (pointer returned by kaltrace_get_instruction_frequencies to free)
        self._add_cfunc('kaltrace_start_logging',                None,                                    [c_char_p, c_bool])                       # (filename, overwrite?)
        self._add_cfunc('kaltrace_stop_logging',                 None,                                    [])
        
    
    def __init__(self):
        self._load_kaltrace_dll()
        self._init_cfuncs()
        self._kept_trace_data = {}  # Keeps a mapping from contexts returned by context_create to arrays that must live as long as those contexts. 
        self._kept_timestamp_data = {}  # Analagous to _kept_trace_data, but for timestamp data.
        
    def decode_trace(self, trace_data, elf_filename):
        """Decodes the given trace data, assuming it was generated from the named ELF file. 
           The trace_data argument should be a sequence of 32-bit unsigned integer values holding the encoded trace.
           Returns a POINTER(TraceResult) object which should be passed to free_trace when finished with."""
        trace_data_num_elements = len(trace_data)
        trace_data_as_uint32_array = (c_uint32*trace_data_num_elements)()
        trace_data_as_uint32_array[:] = trace_data
        rv = self._cfuncs['kaltrace_decode_trace_hardware'](trace_data_as_uint32_array, trace_data_num_elements, elf_filename)
        if not rv:
            raise Exception("Failed to decode trace.")
        return rv
        
    def decode_trace_with_timestamps(self, trace_data, time_data, elf_filename):
        """Decodes the given trace data, assuming it was generated from the named ELF file. 
           The trace_data argument should be a sequence of 32-bit unsigned integer values holding the encoded trace.
           The time_data argument should be a matching sequence of 32-bit unsigned integer values
           holding timestamps for the trace.
           Returns a POINTER(TraceResult) object which should be passed to free_trace when finished with."""
        trace_data_num_elements = len(trace_data)
        trace_data_as_uint32_array = (c_uint32*trace_data_num_elements)()
        trace_data_as_uint32_array[:] = trace_data

        time_data_num_elements = len(time_data)
        time_data_as_uint64_array = (c_uint32*time_data_num_elements)()
        time_data_as_uint64_array[:] = time_data

        rv = self._cfuncs['kaltrace_decode_trace_with_timestamps'](trace_data_as_uint32_array, 
                                                                   trace_data_num_elements, 
                                                                   elf_filename,
                                                                   time_data_as_uint64_array,
                                                                   time_data_num_elements)
        if not rv:
            raise Exception("Failed to decode trace.")
        return rv

    def decode_kalsim_trace(self, trace_filename, elf_filename):
        """Decodes the given trace data. The trace_filename argument should be the path of a trace file generated by
           kalsim. elf_filename should be the filename (and path if necessary) of the ELF the trace was generated for.
           Returns a POINTER(TraceResult) object which should be passed to free_trace when finished with."""
        rv = self._cfuncs['kaltrace_decode_trace_kalsim'](trace_filename, elf_filename)
        if not rv:
            raise Exception("Failed to decode trace.")
        return rv
        
    def free_trace(self, trace):
        self._cfuncs['kaltrace_destroy_trace'](trace)
        
    # The context_* methods are quite low level. It should be easy to build a higher level wrapper object around
    # the context that provides automatic destruction and iteration through the list of PCs. The methods are written
    # with this in mind, for example context_iteration_next throws StopIteration to indicate the end of data being
    # reached rather 
        
    def context_create(self, trace_data, elf_filename, for_kalsim = False, timestamp_data = None):
        """Creates and returns an iteration context that may be used with the methods with names beginning context_
           below. Raises an exception if creation fails.
           The context_destroy method must be called and passed the return value of this method to avoid leaks.
           For hardware traces, the trace_data argument should be a sequence of 32-bit unsigned integer values holding
           the encoded trace and for_kalsim should be False.
           For kalsim traces, trace_data should be the path of a trace file generated by kalsim and for_kalsim should
           be True.
           elf_filename should be the filename (and path if necessary) of the ELF file the trace was generated for.
           timestamp_data should be a sequence of 32-bit unsigned integer values holding timestamp data for the trace
           if it is to be used, and otherwise should be None."""
        if for_kalsim:
            trace_data_as_uint32_array = None
            time_data_as_uint32_array = None
            rv = self._cfuncs['kaltrace_context_create_kalsim'](trace_data, elf_filename)
        elif timestamp_data is None:
            trace_data_num_elements = len(trace_data)
            trace_data_as_uint32_array = (c_uint32*trace_data_num_elements)()
            trace_data_as_uint32_array[:] = trace_data
            time_data_as_uint32_array = None
            rv = self._cfuncs['kaltrace_context_create_hardware'](trace_data_as_uint32_array, trace_data_num_elements,
                                                                  elf_filename)
        else:
            trace_data_num_elements = len(trace_data)
            trace_data_as_uint32_array = (c_uint32*trace_data_num_elements)()
            trace_data_as_uint32_array[:] = trace_data
            time_data_num_elements = len(trace_data)
            time_data_as_uint32_array = (c_uint32*time_data_num_elements)()
            time_data_as_uint32_array[:] = timestamp_data
            rv = self._cfuncs['kaltrace_context_create_hardware_ts'](trace_data_as_uint32_array, trace_data_num_elements,
                                                                     time_data_as_uint32_array, time_data_num_elements,
                                                                     elf_filename)
        if not rv:
            raise Exception("Failed to create kaltrace context.")
        self._kept_trace_data[rv] = trace_data_as_uint32_array
        self._kept_timestamp_data[rv] = time_data_as_uint32_array
        return rv
        
    def context_destroy(self, context):
        """Destroys and frees resources associated with the given context created by the context_create method."""
        self._cfuncs['kaltrace_context_destroy'](context)
        if self._kept_trace_data[context] != None:
            del self._kept_trace_data[context]
        if self._kept_timestamp_data[context] != None:
            del self._kept_timestamp_data[context]
        
    def context_iteration_begin(self, context):
        """Begins iteration through the instructions in the trace loaded in a given context.
           If the context was created with timestamp data, returns a tuple of the PC value for the
           first visited instruction in the trace and the timestamp for that visit. If no timestamp
           data was provided, returns just the PC.
           This method must only be called once for a given context.
           Raises StopIteration if the trace is empty."""
        if self._kept_timestamp_data[context] is None:
            rv = self._cfuncs['kaltrace_decode_begin'](context)
        else:
            timestamp = c_uint32()
            pc = self._cfuncs['kaltrace_decode_ts_begin'](context, timestamp)
            rv = (pc, timestamp.value)     
        if self._cfuncs['kaltrace_decode_end'](context):
            raise StopIteration("Empty trace.")
        return rv
        
    def context_iteration_next(self, context):
        """Gets the next PC in a context being iterated through, or raises StopIteration if no more
           instructions remain in the trace. If the context was created with timestamp data, a tuple
           of PC and timestamp are returned, otherwise just the PC value is returned. 
           The given context must have successfully been passed to context_iteration_begin before
           calling this method."""
        if self._kept_timestamp_data[context] is None:
            rv = self._cfuncs['kaltrace_decode_next'](context)
        else:
            timestamp = c_uint32()
            pc = self._cfuncs['kaltrace_decode_ts_next'](context, timestamp)
            rv = (pc, timestamp.value)
        if self._cfuncs['kaltrace_decode_end'](context):
            raise StopIteration("Reached end of trace.")
        return rv
        
    def get_instruction_count(self, context):
        """Returns the number of instructions executed in the given trace. 
           This method is safe to call on a context that may no longer safely be iterated over, but the given
           context will not be left in a state that is safe to iterate over even if it was previously."""
        return self._cfuncs['kaltrace_count_number_of_instructions'](context)          
           
    def get_instruction_frequencies(self, context):
        """Returns a list of instruction addresses and the number of times each is called in the given context.
           Currently the context may not safely be iterated by the kaltrace_decode_* functions after calling
           this method. The given context must be safe to iterate before calling this method."""
        num_instructions = c_uint32(0)
        frequencies = self._cfuncs['kaltrace_get_instruction_frequencies'](context, byref(num_instructions))
        rv = []
        if frequencies != None and num_instructions.value > 0:
            for n in range(0, num_instructions.value):
                rv.append(KalTrace.InstructionFrequency(frequencies[n].instruction_address, frequencies[n].count))
        self._cfuncs['kaltrace_free_instruction_frequencies'](frequencies)
        return rv

    def start_logging(self, filename, overwrite = False):
        """Starts logging to the specified file. Stops any current logging in progress first.
           Appends to any existing file if overwrite is False, replaces it if overwrite is True."""
        self._cfuncs['kaltrace_start_logging'](filename, overwrite)

    def stop_logging(self):
        """Stops any logging in progres and closes the log file.
           Does nothing if there is no logging in progress."""
        self._cfuncs['kaltrace_stop_logging']()


KalTrace.InstructionTypesForRange._fields_ = [
        ("begin_address",   c_uint32),      # Address at which this range starts.
        ("end_address",     c_uint32),      # First address after the end of this range.
        # The instructions in this range, indexed by address minus begin_address.
        ("instructions",    POINTER(KalTrace.InstructionType)),
        # Next range in a linked list, or null if this is the last range.
        ("next",            POINTER(KalTrace.InstructionTypesForRange))    
    ]
KalTrace.TraceResult._fields_ = [
        ("pc_listing",              POINTER(c_uint32)),                 # An array of all PC values visited in the trace.
        ("num_instructions",        c_uint32),                          # The number of instructions visited in the trace.
        ("bits_per_pc",             c_double),                          # Number of trace bits per visited instruction.
        # The lowest PM address that falls after all address ranges in which valid PM data is
        # loaded from the ELF file. Useful for quick validity or statistics checks, but not a guide
        # to how much PM memory was actually used by the ELF because there may be large blocks of
        # unused PM memory below this address.
        ("pm_address_limit",        c_uint32),                          
        # Array of information about the instructions at a range of PM addresses, including every
        # address in pc_listing. May be NULL if decoding failed or used PM memory was too large.
        ("instruction_type_by_address",  POINTER(KalTrace.InstructionTypesForRange)),
        # The timestamp associated with each instruction. 
        ("times_listing",                POINTER(c_uint32)),
        # The number of valid timestamps in times_listing. Always zero if timestamp data was not analysed in the trace.
        ("num_instructions_with_times",  c_uint32)
    ]


def get_range_containing_address(trace_result, address):
    """Given a Kaltrace.TraceResult object and an address, returns the range in the trace's
       instruction_type_by_address linked list that contains the given address.
       Returns None if no range contains the address."""
    ptr = trace_result.instruction_type_by_address
    while ptr:
        if ptr[0].begin_address <= address and ptr[0].end_address > address:
            return ptr[0]
        ptr = ptr[0].next
    return None
