############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Module providing access to decoded stack frames, primarily for the purpose of
displaying stack backtrace.

The module provides a class StackUnwinder, which provides a partial
implementation of the logic required to unwind a given stack, starting from
a given core.  Core-specific parts of this logic are delegated to subclasses;
at the time of writing there is just one of these - XapStackUnwinder.

A StackUnwinder returns a list of InterruptStacks (commonly containing just
one).  InterruptStack is a simple class which holds a list of StackFrames.
StackFrame is a more complex class which is capable of decoding itself to
give the values (where available) of arguments and local variables in that
frame.

The returned backtrace is a "snapshot" in the sense that if the program counter
changes, it immediately becomes stale.  On the other hand, the debugging
framework as a whole (in particular, the class representing code variables) is
orientated towards lazy evaluation, so it is not feasible to completely
evaluate the backtrace (i.e. fully compute the values of all the variables in
each frame up front).  To avoid inadvertent evaluation of stale StackFrames,
they are created with a record of the real program counter and will raise an
exception if they are evaluated for any other PC.
"""

# pylint: disable=too-many-lines
import os
from abc import ABCMeta, abstractmethod
# compatible with Python 2 and 3
ABC = ABCMeta('ABC', (object,), {'__slots__': ()})

from collections import OrderedDict
from csr.wheels import gstrm, autolazy
from csr.wheels.global_streams import iprint
from csr.dwarf.read_dwarf import DW_OP, \
                                 DwarfNoStackFrameInfo, DwarfNoSymbol,\
                                 Dwarf_Func_Symbol
from csr.wheels.bitsandbobs import build_le, PureVirtualError
from csr.dev.model import interface
from csr.dev.framework.meta.elf_firmware_info import StackVariableInfo, BadPC, \
                                                        BadPCHigh, BadPCLow
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.env.env_helpers import _Variable
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.hw.core.meta.io_struct_io_map_info import RegistersUnavailable
from csr.dev.hw.address_space import AddressSpace

DW_FRAME_CFA_COL3 = 1436 #This comes from the libdwarf header - it's a way
#of indicating that you want the CFA rule.


class NoLocationRules(RuntimeError):
    """
    Exception indicating that no location info was found for a parameter for
    the given program counter
    """

class ExtraText(object): # pylint: disable=too-few-public-methods
    """
    Utility class for adding extra text to the unwinder backtrace
    """
    def __init__(self, text):
        self.text = text
    def display(self):
        """Returns the text passed to the constructor"""
        return self.text

class NoLocationVariable(object):
    """
    Dummy variable passed to the display methods to indicate that there is no
    access via the DWARF to the variable's location, either because the
    variable is genuinely stale or because the DWARF is incomplete.
    """
    @property
    def value_string(self):
        """
        Dummy value_string property when location information is not available
        """
        return "<not available>"

    def display(self, *args, **kwargs):
        """Formats exception message for display"""
        # pylint: disable=no-self-use, unused-argument
        return "Value not available - no location info for current PC"


class UnknownLocationRule(RuntimeError):
    """
    Exception indicating that the location rule returned by the DWARF wasn't
    understood by the decoder
    """



class ProbableInfiniteLoop(RuntimeError):
    """
    Exception used to indicate that the backtrace is probably stuck in an
    infinite loop and needs to abort
    """
    THRESHOLD = 200

    def __str__(self):
        return ("Probable infinite loop: aborted after %d frames" %
                self.THRESHOLD)

class BacktraceNotAdvancing(RuntimeError):
    """
    Exception used to indicate that the backtrace has stuck at the
    same location.
    """
    # Allow this number of repeats
    THRESHOLD = 5
    def __str__(self):
        return ("Backtrace failed: Same SP/Frame {} times. "
                "Probably insufficient symbolic information or reached "
                "thread handler".format(self.THRESHOLD))

class UnknownLocationRuleVariable(object):
    """
    Dummy variable passed to the display methods to indicate that the location
    rule for the requested variable is not one that this module knows about.
    This isn't expected to occur very often.
    """
    @property
    def value_string(self):
        """
        Dummy value_string property when location information is unknown.
        """
        return "<Loc info not decoded>"

    def display(self, *args, **kwargs):
        # pylint: disable=no-self-use, unused-argument
        """Formats exception message for display"""
        return "Value not available - unrecognised location rule"


class ErrorParsingDwarfVariable(object):
    """
    Exception class for errors arising in the DWARF parsing library
    """
    @property
    def value_string(self):
        """
        Dummy value_string property when location information is not available
        due to an error during parsing.
        """
        return "<Error parsing DWARF>"

    def display(self, *args, **kwargs):
        # pylint: disable=no-self-use, unused-argument
        """Formats exception message for display"""
        return "Value not available - error parsing DWARF"


def inlined_call(calling_func, pc, dwarf): # pylint: disable=unused-argument
    """
    If this offset is inside an inlined function, return the offset into the
    inlined function and the Dwarf_Inlined_Func_Symbol representing it.
    Otherwise return None.
    """

    try:
        for iln_call in calling_func.inline_calls:
            try:
                if iln_call.is_in_range(pc):
                    return iln_call
            except RuntimeError:
                #iprint("Couldn't get anything useful for '%s'" % iln_call.name)
                pass
    except AttributeError:
        # non-DWARF function symbol - no inline calls
        pass
    return None

class InterruptStack(object):
    """
    Class representing a set of StackFrames in a single interrupt.  Consists
    essentially of a list of StackFrames and a string indicating details of
    the interrupt.  This class is generic, i.e. it is oblivious of the
    flavour of core that is being unwound.
    """

    def __init__(self, fw_env, core, unwinder):
        """
        Initialise the stack.  We store a reference to the unwinder so that
        we can access the working registers.
        """
        self._core = core
        self._fw_env = fw_env
        self._valid_pc = core.pc
        self._unwinder = unwinder
        self._frames = []
        self._heuristics_from = None

    def __repr__(self):
        return self.display()

    def __getitem__(self, index):
        return self.frame[index]

    def _get_function(self, pc, raise_exc=True):
        """
        Return the DWARF API object for the function we're in.  This may be an
        inlined instance.
        """
        _, _, func_obj = self._fw_env.functions.get_function_of_pc(pc)
        if not isinstance(func_obj, Dwarf_Func_Symbol) and raise_exc:
            raise DwarfNoSymbol
        return func_obj

    def add_frame(self, prev_base_ptr, pc, dwarf):
        """
        Construct a StackFrame based on the supplied info
        """

        # Work out what function the current program address is in
        try:
            func_obj = self._get_function(pc)
            full_func_obj = True
        except DwarfNoSymbol:
            func_obj = self._get_function(pc, raise_exc=False)
            full_func_obj = False

        try:
            if full_func_obj:
                iln_call = inlined_call(func_obj, pc, dwarf)
                if iln_call is not None:
                    self._frames.append(
                        self._unwinder.frame_type(
                            self._fw_env,
                            self._core,
                            self._unwinder, iln_call,
                            pc, prev_base_ptr, dwarf,
                            not self._frames,
                            self._valid_pc))

            frame = self._unwinder.frame_type(self._fw_env,
                                              self._core, self._unwinder,
                                              func_obj,
                                              pc, prev_base_ptr, dwarf,
                                              not self._frames,
                                              self._valid_pc)
        except (DwarfNoStackFrameInfo, DwarfNoSymbol):
            build_no_info_stack_frame = True
        else:
            # If we got an empty set of rules we need to try something different
            build_no_info_stack_frame = not frame.rules

        if build_no_info_stack_frame:
            # There's either no stack information available in the DWARF, or no
            # DWARF symbol at all
            func_addr, func_name, _ = (
                self._fw_env.functions.get_function_of_pc(pc))
            if isinstance(func_name, tuple):
                func_name, cuname = func_name
            else:
                cuname = None
            frame = NoUnwindInfoStackFrame(
                self._fw_env, self._core, func_name, cuname, func_addr,
                pc, prev_base_ptr, not self._frames, self._valid_pc)
        if len(self._frames) > ProbableInfiniteLoop.THRESHOLD:
            raise ProbableInfiniteLoop
        self._frames.append(frame)

    @property
    def latest_frame(self):
        """
        Get a reference to the most recently added stack frame
        """
        return self._frames[-1]

    @property
    def frame(self):
        """
        Public access to the list of frames, to allow clients to select a
        particular frame to examine in more detail
        """
        return self._frames

    def set_interrupt_info(self, info):
        """
        Store general information about the interrupt this stack corresponds
        to
        """
        self._info = info

    def display(self, source_context=0):
        """
        Print the stack backtrace to an interface object
        """

        try:
            text = "Interrupt info: %s\n" % self._info
        except AttributeError:
            text = ""
        heuristics_marker = ""
        for index, frame in enumerate(self._frames):
            if self._heuristics_from == index:
                heuristics_marker ="*"
                text += " ** {}. **\n (Heuristics used to recover: Frames marked '*' may be incorrect.)\n".format(self._heuristics_msg)
            text += "{}[{}] {}\n".format(heuristics_marker, index, frame.display(source_context))

        return text

    def _generate_report_body_elements(self):
        """
        Report the details of this stack frame for the general report
        """
        return interface.Code(self.display(source_context=2))

    def __len__(self):
        return len(self._frames)

    def note_heuristics(self, msg):
        self._heuristics_from = len(self._frames)
        self._heuristics_msg = msg

class StackFrame(ABC):
    """
    Generic class representing an individual function call frame.  Core-
    specific elements of the decoding process (specifically, the mapping of
    the DWARF location rules to actual registers/stack arrays) is left for
    specialised subclasses to fill in.
    """

    # pylint: disable=too-many-instance-attributes
    def __init__(# pylint: disable=too-many-arguments
            self, fw_env, core, func, addr,
            pc, is_bottom, valid_pc):
        self._core = core
        self._fw_env = fw_env
        self._func = func
        self._offset = pc - addr
        self._pc = pc
        self._is_bottom = is_bottom
        self._valid_pc = valid_pc

    def _retrieve_dwarf_info(self, func_obj):
        try:
            self._dwarf_info = func_obj.get_frame_info
        except AttributeError:
            self._dwarf_info = None
        else:
            self._dwarf_info = func_obj.get_frame_info(self._pc, 
                                        not self._is_bottom, self.reg_list)



    def __repr__(self):
        return self.display()

    #Standard interface
    @property
    @abstractmethod
    def info(self):
        """
        Return the suitably-configured DWARF info
        """
        raise NotImplementedError

    @property
    @autolazy
    def rules(self):
        try:
            return self.info.rules
        except AttributeError:
            return self._fw_env.dwarf.get_frame_info(self._pc, self.reg_list)

    @property
    def base_ptr(self):
        """
        Pointer to "base" of stack frame, in the DWARF sense (at the given PC
        offset).  This is known in DWARF jargon as the CFA (canonical frame address)
        """
        raise PureVirtualError

    @property
    def display_ptr(self):
        """
        Stack frame pointer to use for the output.  By default this is the FP/base_ptr/CFA,
        but in some cases we have traditionally output the stack pointer.
        """
        return self.base_ptr

    def _evaluate_loc_rule(self, rule, var_size):
        # pylint: disable=no-self-use, unused-argument
        """
        Return a list of words obtained by decoding the given rule.  We're
        assuming that our DWARF doesn't have any
        """
        raise PureVirtualError

    @property
    def _dwarf_pc(self):
        """
        If address munging is required to give the DWARF a PC it understands,
        this function should be overridden
        """
        return self._pc

    def _retrieve_raw_value(self, var, info):
        """
        Decode the location instructions and return a list of raw words where
        the variable starts at word 0.
        """

        loc_info = self.info.local_var_loc(var)
        if loc_info is None:
            raise NoLocationRules

        # Evaluate the location rules for this variable at this PC.  This
        # results in a list of data words which will subsequently be turned
        # into a Variable object
        words = []
        new_words = []
        for rule in loc_info:
            if rule[0] != DW_OP["piece"]:
                words += new_words
                # set new_words from a virtual routine
                # pylint: disable=assignment-from-no-return
                new_words = self._evaluate_loc_rule(rule, info.size)
            else:
                #The "piece" rule says how many *bytes* the last command
                #was supposed to extract from the given address.  (See DWARF 2
                #spec, sec 2.4.3.6, para. 1) So we may need to trim the output
                #of the previous command.
                arg = rule[1]
                # I *think* it's an error in XAP dwarf for the piece argument
                # ever to be one byte.  After all, there are no single-byte
                # spaces in the addressing scheme!  Unfortunately, this is
                # supposed to be generic code.
                if arg == 1:
                    arg = 2
                new_words = new_words[:arg // 2]

        words += new_words
        return words

    def _build_frame_var(self, local_var_sym):
        """
        Construct a _Variable object corresponding to the stack variable from
        the raw word(s) obtained by evaluating the location rules.
        """
        var_info = StackVariableInfo(local_var_sym, self._core.info.layout_info)
        try:
            data_space = self._retrieve_raw_value(local_var_sym, var_info)
            # The KCC DWARF has very dodgy local variable location info.  One
            # thing that can happen is that a variable can be indicated as
            # available in a caller-preserved (scratch) register, which results
            # in data_space having "None" in it here.  Treat this as if there
            # were no location data available.
            if None in data_space:
                raise NoLocationRules
            return _Variable.factory(var_info, data_space, self._core.data)

        except UnknownLocationRule as exc:
            iprint("Unknown location rule: %s" % exc)
            return UnknownLocationRuleVariable()
        except NoLocationRules:
            return NoLocationVariable()
        except RuntimeError:
            return ErrorParsingDwarfVariable()

    def _check_valid(self):
        """
        Check that this frame snapshot hasn't gone stale
        """
        pc = self._core.pc
        if pc != self._valid_pc:
            raise RuntimeError("Current PC (0x%x) doesn't match creation-time "
                               "PC (0x%x)! Unsafe to evaluate InterruptStack" %
                               (pc, self._valid_pc))

    @property
    def args_list(self):
        """
        Look up the formal parameters in the DWARF, construct corresponding
        variables, and return
        """
        self._check_valid()
        try:
            self.info.params
        except AttributeError:
            return {}
        else:
            return ((name, self._build_frame_var(symbol)) 
                                            for name, symbol in self.info.params)

    @property
    def args(self):
        """returns the args_list property as a dict"""
        try:
            self._args_dict
        except AttributeError:
            self._args_dict = dict(self.args_list)
        return self._args_dict

    @property
    def locals_list(self):
        """
        Look up the locals in the DWARF, construct corresponding variables, and
        return
        """
        self._check_valid()
        try:
            self.info.params
        except AttributeError:
            return tuple()
        else:
            return ((name, self._build_frame_var(symbol)) \
                                        for name, symbol in self.info.locals)

    @property
    def locals(self):
        """Accessor to the locals_list property as a dict"""
        try:
            self._locals_dict
        except AttributeError:
            self._locals_dict = dict(self.locals_list)
        return self._locals_dict

    @property
    def srcfile(self):
        """
        returns the DWARF source file for the Dwarf_Stack_Frame information
        (set in a derived class)
        """
        try:
            return self._dwarf_info.srcfile
        except AttributeError:
            return None

    @property
    def lineno(self):
        """
        returns the line number in the Dwarf_Stack_Frame information
        (set in a derived class)
        """
        try:
            return self._dwarf_info.lineno
        except AttributeError:
            return 0

    def source_code(self, context_lines=0):
        """
        Return the source code line corresponding to this frame's PC, plus the
        requested number of lines of context
        """
        if self.srcfile is None:
            return " <unknown source file>"

        default_reference = " %s:%d" % (self.srcfile, self.lineno)

        if context_lines == 0:
            return default_reference

        try:
            return self._fw_env.build_info.source_code(self.srcfile,
                                                       self.lineno,
                                                       context_lines)
        except (ValueError, AttributeError):
            # Couldn't do the source file path remapping
            # Attribute error at this stage means source_code() not supported
            return default_reference

    @property
    def name(self):
        """
        Return the (possibly cleaned-up) name of the function corresponding to
        this stack frame
        """
        return self._display_name_munge(self._func)

    def _display_name_munge(self, name): # pylint: disable=no-self-use
        """
        Hook for applying any adjustment to the names in the DWARF.  By default
        names aren't changed.
        """
        return name

    def display(self, source_context=0):
        """
        Construct a string containing the salient information about the frame
        for display.
        """
        self._check_valid()

        display = "0x%04x is 0x%06x:%s(" % (self.display_ptr,
                                            self._pc,
                                            self.name)

        for (name, var) in self.args_list:
            display += "%s=%s, " % (name, var.value_string)
        if display[-2:] == ", ":
            display = display[:-2]
        display += ") + 0x%x" % self._offset

        code = self.source_code(source_context)
        display += code

        return display

    def struct(self, var_name):
        """
        Display fully the data in either a local or function argument to this
        stack frame.
        """

        self._check_valid()

        if var_name in self.args:
            return self.args[var_name].display(var_name, "", [], [])
        if var_name in self.locals:
            return self.locals[var_name].display(var_name, "", [], [])

        raise ValueError("'%s' not found amongst args or locals in this "
                         "stack frame" % var_name)

    @property
    def _bytes_per_addr_unit(self):
        """
        Number of 8-bit units in an address unit.  To be defined by the
        concrete subclass
        """
        raise PureVirtualError

    @property
    def offset(self):
        """
        returns the offset oin self._offset
        """
        return self._offset

class NoUnwindInfoStackFrame(StackFrame):
    """
    Largely unimplemented StackFrame for cases where there's no frame unwind
    information available, e.g. because the frame corresponds to a function
    written in assmebly without DWARF instrumentation.

    Unwinder functions should handle this type of frame by using ad hoc
    knowledge to try to get a return address and hope that the next frame up
    has some DWARF info.  Note: you typically can't unwind through two
    successive frames like this because you can't unwind any of the registers
    the first time except the PC, so there's no way to unwind the PC the second
    time, unless you can find the RA on the stack without any help (a la xIDE
    stack_classic)
    """
    def __init__( # pylint: disable=too-many-arguments
            self, fw_env, core, func, cuname, func_addr, pc, prev_base_ptr,
            is_bottom, valid_pc):
        StackFrame.__init__(self, fw_env, core, func, func_addr, pc,
                            is_bottom, valid_pc)
        self._cuname = cuname
        self._prev_base_ptr = prev_base_ptr

    @property
    def base_ptr(self):
        return self._prev_base_ptr # it's the best we can do


    def display(self, source_context=0):
        """
        Construct a string containing the salient information about the frame
        for display.
        """
        self._check_valid()

        display_str = "0x%04x is 0x%06x:%s + 0x%x" % (self.base_ptr,
                                                      self._pc,
                                                      self._func,
                                                      self._offset)
        if self._cuname is not None:
            display_str += " (%s)" % self._cuname
        return display_str

    @property
    def info(self):
        return None

    @property
    def rules(self):
        return {}

class K32StackFrame(StackFrame):
    # pylint: disable=too-many-instance-attributes
    """
    Represents a stack frame on the Kalimba 32-bit processor
    """

    ELF_PM_BIT = 0x80000000

    DWARF_REG_NUMS = {
        "RMAC"      : 1,
        "R0"        : 2,
        "R1"        : 3,
        "R2"        : 4,
        "R3"        : 5,
        "R4"        : 6,
        "R5"        : 7,
        "R6"        : 8,
        "R7"        : 9,
        "R8"        : 10,
        "R9"        : 11,
        "R10"       : 12,
        "RLINK"     : 13,
        "FP"        : 14,
        "SP"        : 15,
        "RLINK_INT" : 17}


    dwarf_asm_name_munge = {
        "$M.interrupt.block" : "interrupt_block",
        "$M.interrupt.unblock" : "interrupt_unblock",
        "$M.interrupt.handler" : "interrupt_handler",
        "$M.safe_enable_shallow_sleep" : "safe_enable_shallow_sleep",
        "$M.crt0_rst" : "reset",
        "$M.appcmd_call_function" : "appcmd_call_function"}


    def __init__( # pylint: disable=too-many-arguments
            self, fw_env, core, unwinder, func_obj, pc, prev_base_ptr,
            dwarf, # pylint: disable=unused-argument
            is_bottom, valid_pc):

        self.reg_list = ([DW_FRAME_CFA_COL3] +
                        [self.DWARF_REG_NUMS[reg]
                        for reg in ("FP", "SP", "RLINK", "RLINK_INT")] +
                        [self.DWARF_REG_NUMS["R%d" % reg] for reg in range(11)] +
                        [self.DWARF_REG_NUMS["RMAC"]])

        # Potential extension: Perhaps munge the name *back* to the ELF name?
        StackFrame.__init__(self, fw_env, core, func_obj.name,
                            func_obj.address & ~self.ELF_PM_BIT,
                            pc, is_bottom, valid_pc)

        self._retrieve_dwarf_info(func_obj)

        #Now store this frame's register values as determined by the immediately
        # preceding unwind
        self._rlink = unwinder.rlink
        self._fp = unwinder.fp
        self._sp = unwinder.sp
        self._r = unwinder.r[:]
        self._rmac = unwinder.rmac

        try:
            cfa_rule = self.rules[DW_FRAME_CFA_COL3]
        except KeyError:
            raise DwarfNoStackFrameInfo

        if "offset" in cfa_rule:
            # We expect we'll usually get FP + 0 as the rule, since there's
            # an actual frame pointer register, but no harm in covering other
            # possibilities
            reg_num = cfa_rule["register"]
            if reg_num == K32StackFrame.DWARF_REG_NUMS["FP"]:
                reg = self._fp
            elif reg_num == K32StackFrame.DWARF_REG_NUMS["SP"]:
                reg = self._sp
            elif reg_num == K32StackFrame.DWARF_REG_NUMS["RMAC"]:
                reg = self._rmac
            elif (K32StackFrame.DWARF_REG_NUMS["R0"] <= reg_num <=
                  K32StackFrame.DWARF_REG_NUMS["R10"]):
                reg = self._r[reg_num-K32StackFrame.DWARF_REG_NUMS["R0"]]
            elif reg_num == K32StackFrame.DWARF_REG_NUMS["RLINK"]:
                reg = self._rlink
            elif reg_num == DW_FRAME_CFA_COL3:
                reg = prev_base_ptr
            else:
                raise RuntimeError("Unexpected DWARF register number %d in "
                                   "unwind rule" % reg_num)
            self._base_ptr = reg + cfa_rule["offset"]
        elif "same_value" in cfa_rule:
            self._base_ptr = prev_base_ptr
        else:
            raise RuntimeError("Unexpected unwind rule for "
                               "CFA: %s" % str(cfa_rule))

        # The interrupt handler is incompletely instrumented because the
        # rIntLink register isn't known to the DWARF.
        # We need this to be visible to the unwinder so it can do its thing
        self._is_interrupt = (self._func == "$M.interrupt.handler")

    def _retrieve_dwarf_info(self, func_obj):
        try:
            func_obj.get_frame_info
        except AttributeError:
            self._dwarf_info = None
        else:
            self._dwarf_info = func_obj.get_frame_info(self._pc | self.ELF_PM_BIT,
                                not self._is_bottom, self.reg_list)

    def _display_name_munge(self, name):
        return self.dwarf_asm_name_munge.get(name, name)

    @property
    def info(self):
        """
        Access to the Dwarf_Stack_Frame
        """
        return self._dwarf_info

    @property
    def _bytes_per_addr_unit(self):
        return 4

    @property
    def base_ptr(self):
        """
        Pointer to "base" of stack frame, in the DWARF sense (at the given PC
        offset)
        """
        return self._fp

    @property
    def _dwarf_pc(self):
        """
        Address munger for those times when we have to talk directly to the
        DWARF about PCs
        """
        return self._pc | 0x80000000

    @property
    def is_interrupt(self):
        """
        Is this a stack frame for the interrupt handler?  If so the unwinder
        has to do some magic.
        """
        return self._is_interrupt

    def _evaluate_loc_rule(self, rule, var_size):
        """
        Evaluate a single loc operation for the given variable
        """
        op = rule[0]
        arg = rule[1]

        # "Base reg" rules, i.e. the value is stored at given offset from the
        # address stored in the implied register
        if DW_OP["breg2"] <= op <= DW_OP["breg12"]:
            start_addr = self._r[op-DW_OP["breg2"]] + arg
            return self._core.data[start_addr:start_addr + var_size]
        if op == DW_OP["breg14"]:
            start_addr = self.base_ptr + arg
            return self._core.data[start_addr:start_addr + var_size]
        if op == DW_OP["fbreg"]:
            start_addr = self.base_ptr + arg
            return self._core.data[start_addr:start_addr + var_size]

        # "Register rules", i.e. the value is stored in the implied register

        # rMAC is DWARF reg 1
        if op == DW_OP["reg1"]:
            return [self._rmac]

        # R0-R10 are DWARF numbers 2-12. The DW_OP values are contiguous so it's
        # easy to compute the index into the local array of R registers
        if DW_OP["reg2"] <= op <= DW_OP["reg12"]:
            return [self._r[op - DW_OP["reg2"]]]

        # Explicit address rule, i.e. the value is stored at the given memory
        # address
        if op == DW_OP["addr"]:
            return self._core.data[arg:arg + var_size]
        raise UnknownLocationRule(rule)


class StackBacktrace(list):
    """
    A list of StackFrames that can print itself out nicely
    """
    def __repr__(self):
        text = ""
        for int_stack in self:
            text += int_stack.display()
        return text

    __str__ = __repr__


class StackUnwinder(FirmwareComponent):
    """
    Generic stack unwinder.  Produces a list of InterruptStacks, each of
    which consists of a list of StackFrames.  Most aspects of the stack
    unwinding operation must be overridden by child classes.
    """

    def __init__(self, fw_env, core, execution_state=None, stack_base=None, **init_regs):
        """
        Constructor
        """
        FirmwareComponent.__init__(self, fw_env, core, parent=None)
        self._fw_env = fw_env
        self._layout_info = core.info.layout_info
        self._verbose = False
        self._reg_containing_obj = execution_state or self._core
        self._stack_base = stack_base
        self._init_regs = init_regs

    #StackUnwinder virtual interface

    @property
    def frame_type(self):
        """
        Return the type of the associated StackFrame to allow InterruptStacks
        to generate them
        """
        raise PureVirtualError

    def _reset_working_registers(self):
        self._pc = self._init_regs.get("pc", self._reg_containing_obj.pc)

    @property
    def _user_mem_begin(self):
        raise PureVirtualError

    @property
    def _user_mem_end(self):
        raise PureVirtualError

    @property
    def _stack_begin(self):
        raise PureVirtualError

    @property
    def _stack_end(self):
        raise PureVirtualError

    @property
    def _current_stack_ptr(self):
        raise PureVirtualError

    def _get_prefetch_range(self):
        return (self._stack_begin, self._current_stack_ptr)


    @property
    def _instr_size(self):
        """
        How many addressing units in a single instruction?
        """
        raise PureVirtualError

    def _unwind(self, frame, backtrace):
        # pylint: disable=no-self-use, unused-argument
        raise PureVirtualError

    @property
    def pc(self):
        """
        Access to the working PC
        """
        return self._pc

    def _evaluate_unwind_rule(self, rule, cfa, current_value):
        """
        Evaluate unwind rule from read_dwarf based on the Arm ABI
        """
        if "offset" in rule:
            offset = rule["offset"]
            base = self.get_register(rule["register"], cfa)
            units = self._layout_info.addr_units_per_data_word
            return self._layout_info.deserialise(
                self._core.data[base + offset:base + offset + units])
        op = list(rule.keys())[0]
        arg = rule[op]
        if op == "same_value":
            return current_value
        if op == "register":
            return self.get_register(arg, cfa)
        if op == "undefined":
            return None
        raise RuntimeError("Unknown unwind rule '%s'" % op)

    def _backtrace_loop(self, backtrace, **kwargs):
        """
        Generic backtrace execution loop which repeatedly calls the _unwind
        method to produce more stack frames until _unwind indicates it's one.
        This method is wrapped by the public bt() which does things like error
        handling.

        Note: this method returns backtrace, but it is updated in-place, so this
        isn't strictly necessary.  But it might be useful in some contexts.
        """
        dwarf = self._fw_env.dwarf

        last_stack_pointer = None
        repeated_stack_pointer = 0
        while 1:
            current_stack = backtrace[-1]
            if self._verbose:
                iprint("------------------------------------------------")
                iprint("Backtracing from PC 0x%08x" % self.pc)
            try:
                # Create a new frame.
                # We make the safe assumption that the current stack pointer is
                # the same as the previous frame's base pointer.
                current_stack.add_frame(self._current_stack_ptr, self.pc,
                                        dwarf)
            except BadPC as exc:
                # No function symbol for the current PC: we've probably hit
                # the CRT entry function
                if self._verbose:
                    iprint("PC is not in a function")
                if isinstance(exc, (BadPCLow, BadPCHigh)):
                    # We're probably in the CRT
                    break
                # Something has gone wrong
                raise

            if self._current_stack_ptr == last_stack_pointer:
                repeated_stack_pointer = repeated_stack_pointer + 1
                if repeated_stack_pointer >= BacktraceNotAdvancing.THRESHOLD:
                    raise BacktraceNotAdvancing
            else:
                last_stack_pointer = self._current_stack_ptr
                repeated_stack_pointer = 0

            if not self._unwind(current_stack.latest_frame, backtrace,
                                **kwargs):
                if self._verbose:
                    iprint("Unwind signalled end of backtrace")
                # Couldn't unwind any further
                break
        return backtrace

    def bt(self, raise_exc=False, verbose=False):
        """
        Generate a generic backtrace by progressively finding functions at the
        latest unwound program counter and creating a stack frame for it.  The
        logic to determine return address, frame pointer adjustment or to
        restore the interrupt context (as required) is provided by the unwind
        method of specialised subclasses
        """
        self._verbose = verbose

        self._reset_working_registers()

        backtrace = StackBacktrace()
        backtrace.append(InterruptStack(self._fw_env, self._core, self))
        try:
            self._backtrace_loop(backtrace)
        except Exception as exc: # pylint: disable=broad-except
            class StackUnwinderError(object):
                # pylint: disable=too-few-public-methods
                """
                Provides for holding and displaying an error message
                for display during stack backtrace unwinding.
                """
                def __init__(self, msg):
                    self._msg = msg
                def display(self):
                    """Formats the error msg passed to constructor"""
                    return "ERROR unwinding: %s" % self._msg

            backtrace.append(StackUnwinderError(exc))

            if self._verbose or raise_exc:
                # In the verbose case the frames are printed as we go, so we
                # don't need to suppress the crash
                raise

        return backtrace


class K32StackUnwinder(StackUnwinder):
    # pylint: disable=too-many-instance-attributes

    # Potential extension:: These values are in the ELF, but they are obscure non-function
    # symbols which for efficiency reasons we don't really want to have to load.
    IntPush2_M32L_offset = 0xb6 - 0x8e
    IntPop2_M32L_offset = 0x116 - 0x8e

    # These values aren't in the ELF
    PushInfoOffset = 0xbe - 0x8e
    PopInfoOffset = 0x104 - 0x8e

    # WARNING: all the above values may go stale if interrupt.asm changes!!

    def __init__(self, fw_env, core, execution_state=None, stack_base=None, **init_regs):

        StackUnwinder.__init__(self, fw_env, core,
                               execution_state=execution_state, stack_base=stack_base,
                               **init_regs)
        self._reset_working_registers()
        self._from_function_prologue = False
        self._modify_interrupt_frame = False
        if self._core.subsystem is self._core.subsystem.chip.apps_subsystem:
            try:
                self._modify_interrupt_frame = (
                    self._core.subsystem.p0.fw.slt.build_id_number == 1200)
            except NotImplementedError:
                # P0's SLT doesn't exist on an event dump of P1 only
                pass

        from csr.dev.fw.sched import SchedFreeRTOS, SchedOxygen
        self._sched = self.create_component_variant(
            (SchedOxygen, SchedFreeRTOS),
            fw_env, core, parent=self)

    @property
    def frame_type(self):
        """
        Return the type of the associated StackFrame to allow InterruptStacks
        to generate them
        """
        return K32StackFrame

    def _reset_working_registers(self):
        self._pc = self._init_regs.get("pc", self._reg_containing_obj.pc)
        self._r = [self._init_regs.get("r%d" % i, self._reg_containing_obj.r[i])
                   for i in range(11)]
        self._rlink = self._init_regs.get("rlink",
                                          self._reg_containing_obj.rlink)
        self._rlink_stale = False
        self._rmac = self._init_regs.get("rmac", self._reg_containing_obj.rmac)
        self._fp = self._init_regs.get("fp", self._reg_containing_obj.fp)
        self._sp = self._init_regs.get("sp", self._reg_containing_obj.sp)

    # The stack is at the end of data RAM on the Apps

    # Potential extension: Not clear that we need all these functions
    @property
    def _user_mem_begin(self):
        return self._fw_env.abs["DATA_RAM_START_ADDRESS"]

    @property
    def _user_mem_end(self):
        return self._fw_env.abs["DATA_RAM_END_ADDRESS"]

    @property
    def _stack_begin(self):
        try:
            return (self._sched.current_stack_start
                    if self._stack_base is None else self._stack_base)
        except AddressSpace.NoAccess:
            # The FreeRTOS structure containing current_stack_start may be
            # unavailable in some cases, e.g. an event dump.
            return None

    @property
    def _stack_end(self):
        return self._user_mem_end

    @property
    def _current_stack_ptr(self):
        return self._fp

    def _get_prefetch_range(self):
        return (self._stack_begin, self._sp)

    @property
    def _instr_size(self):
        """
        How many addressing units in a single instruction?
        """
        return 4

    def get_register(self, reg_num, cfa):
        """Accessor to Kalimba general (enumerable) registers RMAC/R0..R10/RLINK
        as enumerated by DWARF register number reg_num, which has a value
        from K32StackFrame.DWARF_REG_NUMS or instead the value DW_FRAME_CFA_COL3
        when it returns cfa.
        """
        if reg_num == K32StackFrame.DWARF_REG_NUMS["RMAC"]:
            return self._rmac
        if (K32StackFrame.DWARF_REG_NUMS["R0"] <= reg_num <=
                K32StackFrame.DWARF_REG_NUMS["R10"]):
            return self._r[reg_num-K32StackFrame.DWARF_REG_NUMS["R0"]]
        if reg_num == K32StackFrame.DWARF_REG_NUMS["RLINK"]:
            return self._rlink
        if reg_num == DW_FRAME_CFA_COL3:
            return cfa
        raise RuntimeError("Unexpected DWARF register number %d in "
                           "unwind rule" % reg_num)

    def _decode_interrupt_info(self, info):
        """
        Decode the INT_SAVE_INFO register
        """
        int_source = (
            self._core.fields.INT_SAVE_INFO.INT_SAVE_INFO_SOURCE_NEW.info)
        source = (info & int_source.mask) >> int_source.start_bit
        return "source '%s'" % self._int_source_name(source)

    def _int_source_name(self, source):
        try:
            source_name = self._fw_env.enums["int_source"][source]
        except KeyError:
            source_name = "0x%02x (unknown)" % source
        return source_name

    def _unwind(self, frame, backtrace, prologue_exec_halted=False):
        # Potential extension:: refactor code needed, hence:
        # pylint: disable=too-many-branches,too-many-statements,arguments-differ
        # pylint: disable=too-many-locals

        # We are unwinding the working registers based on the unwinding info
        # stored in the DWARF.
        # In order to apply offset rules we need first to know the CFA.
        #  Presumably this is the same as FP.
        if isinstance(frame, NoUnwindInfoStackFrame):
            # If there's no new info to be had from rlink, we'd better call it
            # a day
            if self._verbose:
                iprint("No unwind info for %s" % frame.name)
            if self._rlink_stale or self._pc == self.rlink - 2:
                backtrace[-1].set_interrupt_info("background")
                if self._verbose:
                    iprint("No unwind info for 0x%x and rlink is already stale"
                           % self._pc)
                return False

            self._pc = self.rlink - 2
            # We can't unwind rlink in this case: either we've just unwound to
            # a function that has unwind info (hopefully not "same value") or
            # we'll have to bomb out next time
            self._rlink_stale = True

        else:
            # Ask the DWARF interface for the rules for these registers
            rules = frame.rules

            try:            
                if (self._modify_interrupt_frame
                        and frame.name == "interrupt_handler"):
                    iprint("Overriding broken return address rule "
                           "in interrupt_handler")
                    rules["return_address"] = {"register" : 1436,
                                               "offset" : 0x14}
                    rules[frame.DWARF_REG_NUMS["R3"]] = {
                        "register" : 1436, "offset" : 0x18}
                    rules[frame.DWARF_REG_NUMS["R10"]] = {
                        "register" : 1436, "offset" : 0x1c}
                    # We want RLINK to be correct too in case the interrupted
                    # function hadn't stacked it (this happens with memcpy
                    # for example)
                    rules[frame.DWARF_REG_NUMS["RLINK"]] = {
                        "register" : 1436, "offset" : 0x20}
            except NotImplementedError:
                # There may not be a P0 present (e.g. limited coredump)
                pass

            cfa = self._fp
            unwound_fp = None
            # If we are in the function prologue, the processor *may* have
            # already started to execute the instruction, so the FP *may* have
            # been updated, contrary to what the DWARF will say.  There's no
            # way to determine this, so we allow the unwinder to be run with
            # or without this assumption being made, the idea being that a
            # plausible stack decode will be obtained in no more than one of the
            # cases.
            # pylint: disable=protected-access
            if ((self._pc & ~1 | frame.ELF_PM_BIT) ==
                    frame.info._func_sym.address):
                # Record that we are in a function prologue, so the caller
                # knows that trying with both assumptions about the pushm state
                # is worthwhile if necessary.
                self._from_function_prologue = True
                if prologue_exec_halted:
                    # Assume we have already started executing the pushm and
                    # therefore the old FP has been written onto the stack at
                    # the current CFA
                    unwound_fp = self._core.dmw[cfa]
            if unwound_fp is None:
                unwound_fp = self._evaluate_unwind_rule(
                    rules[frame.DWARF_REG_NUMS["FP"]], cfa, self._fp)
            unwound_sp = self._evaluate_unwind_rule(
                rules[frame.DWARF_REG_NUMS["SP"]], cfa,
                self._sp)
            unwound_rlink = self._evaluate_unwind_rule(
                rules[frame.DWARF_REG_NUMS["RLINK"]],
                cfa, self._rlink)
            self._rlink_stale = False
            unwound_rmac = self._evaluate_unwind_rule(
                rules[frame.DWARF_REG_NUMS["RMAC"]],
                cfa, self._rmac)
            unwound_r = [None]*11
            for i in range(11):
                unwound_r[i] = self._evaluate_unwind_rule(
                    rules[frame.DWARF_REG_NUMS["R%d" % i]],
                    cfa, self._r[i])

            # There's a slight mistake in read_dwarf: it returns
            # the rule for the register marked as the return address register in
            # the function's CIE, but doesn't tell you what that register is.
            # We only need to know for the "same value" rule; but when that rule
            # applies the return value is in a well-known register
            #
            if self._verbose:
                iprint("Return address rule: %s" % rules["return_address"])

            ra = self._evaluate_unwind_rule(rules["return_address"], cfa,
                                            self._rlink)

            if self._rlink_stale and rules[frame.DWARF_RLINK] == "same_value":
                # If we just unwound through a function with no unwind info in
                # the DWARF then we can't correctly apply a same_value rule, so
                # we have to stop unwinding
                backtrace[-1].set_interrupt_info("background")
                if self._verbose:
                    iprint("'Same value' rule for rlink, "
                           "but rlink wasn't unwound")
                return False

            self._fp = unwound_fp
            self._sp = unwound_sp
            self._rlink = unwound_rlink
            self._rmac = unwound_rmac
            for i in range(11):
                self._r[i] = unwound_r[i]

            if frame.is_interrupt:
                # Create new InterruptStack
                backtrace.append(InterruptStack(self._fw_env, self._core, self))

                # Get the interrupt source
                # If we've just unwound to the bottom of the innermost interrupt
                # context, we look at the INT_SOURCE register
                if len(backtrace) == 2:
                    try:
                        int_source = self._int_source_name(
                            self._core.fields["INT_SOURCE"])
                    except AddressSpace.NoAccess:
                        int_source = "Unknown"
                    backtrace[0].set_interrupt_info(int_source)

                # Info for the next Interrupt context out (the one we're about
                # to unwind) is stashed on the stack, or else still in
                # INT_SAVE_INFO if we stopped in pre/post-amble code in the
                # interrupt_handler
                if (frame.offset < self.PushInfoOffset or
                        frame.offset > self.PopInfoOffset):
                    # In this range the interrupt controller state is
                    # in INT_SAVE_INFO
                    info = self._core.fields["INT_SAVE_INFO"]
                else:
                    # Otherwise it's 12 words up the stack
                    info = build_le(self._core.dm[cfa + 48:cfa + 52],
                                    word_width=8)
                int_info_string = self._decode_interrupt_info(info)
                backtrace[-1].set_interrupt_info(int_info_string)

                self._pc = ra - 1
            else:
                # The PC should be one instruction before the return address
                self._pc = ra - 2

        # If we've unwound back to 0 we're probably done
        if self._pc <= 0:
            backtrace[-1].set_interrupt_info("background")
            if self._verbose:
                iprint("PC back to 0 (0x%x)" % self._pc)
            return False

        return True

    @property
    def rlink(self):
        """Accessor for Kalimba32 return address register rLink"""
        return self._rlink

    @property
    def sp(self):
        """Accessor for Kalimba32 stack pointer register SP"""
        return self._sp

    @property
    def fp(self):
        """Accessor for Kalimba32 register FP"""
        return self._fp

    @property
    def rmac(self):
        """Accessor for Kalimba32 register rMAC"""
        return self._rmac

    @property
    def r(self):
        """Accessor for Kalimba32 general (indexable) register R"""
        return self._r


    def bt(self, raise_exc=False, verbose=False):
        '''
        Generate a generic backtrace by progressively finding functions at the
        latest unwound program counter and creating a stack frame for it.  The
        logic to determine return address, frame pointer adjustment or to
        restore the interrupt context (as required) is provided by the unwind
        method.

        This version of bt() is specific to K32StackUnwinder as it has some
        logic to try to handle the possibility of ambiguous frame pointer values
        when the unwind starts from a function prologue.
        '''

        self._verbose = verbose

        # TODO: refactor code needed, hence:
        # pylint: disable=too-many-branches
        self._reset_working_registers()

        backtrace = StackBacktrace()
        backtrace.append(InterruptStack(self._fw_env, self._core, self))

        try:
            try:
                self._core.data.address_range_prefetched
            except AttributeError:
                self._backtrace_loop(backtrace, prologue_exec_halted=False)
            else:
                prefetch_begin, prefetch_end = self._get_prefetch_range()
                if prefetch_begin is not None and prefetch_end is not None:
                    with self._core.data.address_range_prefetched(
                            prefetch_begin, prefetch_end):
                        self._backtrace_loop(
                            backtrace, prologue_exec_halted=False)
                else:
                    self._backtrace_loop(backtrace, prologue_exec_halted=False)
        except Exception as exc: # pylint: disable=broad-except
            bad_backtrace = exc
        else:
            outermost_frame = backtrace[-1][-1]
            try:
                outermost_fp = outermost_frame.base_ptr
            except AttributeError:
                outermost_fp = None
            # When not-equal, no exception, but we didn't make it back to
            # the start of stack memory
            try:
                current_stack_start = self._core.fw.sched.current_stack_start
            except AddressSpace.NoAccess:
                # We can't access the current_stack_start info, probably because
                # we only have an XED file and the FreeRTOS task stack structures
                # haven't been dumped, so we'll just assume the stack starts where
                # we got to.
                current_stack_start = outermost_fp 
            bad_backtrace = bool(outermost_fp !=
                                 (current_stack_start
                                  if self._stack_base is None else self._stack_base))

        if bad_backtrace and self._from_function_prologue:
            # The backtrace looks bad but it involves a pushm instruction, so
            # try again assuming that the pushm was halted mid-execution.
            self._reset_working_registers()
            alt_backtrace = StackBacktrace()
            alt_backtrace.append(InterruptStack(self._fw_env, self._core, self))

            try:
                self._backtrace_loop(alt_backtrace, prologue_exec_halted=True)
            except Exception: # pylint: disable=broad-except
                # We failed again.  Treat the first failure as the "real" one
                pass
            else:
                bad_backtrace = False
                backtrace = alt_backtrace

        if bad_backtrace:
            class StackUnwinderError(object):
                # pylint: disable=too-few-public-methods
                """
                Provides for holding and displaying an error message
                for display during stack backtrace unwinding.
                """
                def __init__(self, msg):
                    self._msg = msg
                def display(self):
                    """Formats the error msg passed to constructor"""
                    return "ERROR unwinding: %s" % self._msg
            # bad_backtrace may be an exception or a boolean; tell pylint
            # pylint: disable=singleton-comparison
            if bad_backtrace == True:
                # No error, but we didn't make it to the start of the stack
                backtrace.append(StackUnwinderError("not fully unwound"))
            else:
                backtrace.append(StackUnwinderError(bad_backtrace))

                if self._verbose or raise_exc:
                    # In the verbose case the frames are printed as we go, so we
                    # don't need to suppress the crash.
                    # (Once we reach here, bad_backtrace is an exception;
                    #  it cannot be a bool of either value, so tell pylint)
                    # pylint: disable=raising-bad-type
                    raise bad_backtrace

        return backtrace


