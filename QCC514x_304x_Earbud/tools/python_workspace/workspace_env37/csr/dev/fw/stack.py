############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Class to model the call stack
"""
import sys
from collections import OrderedDict
from csr.wheels.bitsandbobs import bytes_to_dwords, PureVirtualError,\
    display_hex
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.hw.address_space import AddressSpace
# pylint: disable=undefined-variable
if sys.version_info > (3,):
    # Python 3
    int_type = int
    long = int
else:
    int_type = (int, long) # pylint: disable=undefined-variable

class Stack(FirmwareComponent):
    """
    An abstract class that models the call stack and is used to account for
    memory usage in memory usage reports.
    The _magic_num is assumed to have been used by firmware to initialise
    the stack memory region. The unused property counts the number of
    occurrences from the end of the stack and is used in calculating
    percentage used in the memory usage report in classes derived from
    FirmwareStack.
    """

    @property
    def unused(self):
        """
        Unused memory in stack
        """
        raise PureVirtualError(self)

    @property
    def size(self):
        """
        Size of stack
        """
        raise PureVirtualError(self)

    @property
    def _magic_num(self):
        """
        Magic number the firmware initialise the stack with
        """
        raise PureVirtualError(self)

class FirmwareStack(Stack):
    """
    Provides a partial implementation, including that for
     _generate_memory_report_component.
     Derived classes need to implement attributes:
     integers _end, _start, _magicnumber, and also a dictionary _dm_stack
     typically set from a firmware symbol via self._core.sym_get_range
    """
    def __init__(self, fw_env, core):
        """
        Stack constructor
        """
        # Caller should do the super call.
        # pylint: disable=super-init-not-called
        raise PureVirtualError(self)

    @property
    def unused(self):
        """
        Unused memory in stack
        """
        raise PureVirtualError(self)

    @property
    @display_hex
    def size(self):
        return self._end - self._start

    @property
    @display_hex
    def start(self):
        """The start, beginning of the stack"""
        return self._start

    @property
    @display_hex
    def end(self):
        """The end limit of the stack"""
        return self._end

    @property
    def _magic_num(self):
        return self._magic_number

    def _generate_memory_report_component(self):
        # By returning this inside a container, we make sure it is a
        # subcomponent of DM RAM when it gets displayed
        ret = self._dm_stack
        used = self.size - self.unused
        ret["used"] = used
        ret["unused"] = self.unused
        ret["size"] = self.size

        #It is possible for some cores to be totally unused
        if self.size == 0:
            ret["percent_used"] = 0
        else:
            ret["percent_used"] = (float(used) / self.size) * 100
        return [ret.copy()]

class AppsStack(FirmwareStack):
    """
    Models the call stack for the Apps subsystem firmware
    """

    def __init__(self, fw_env, core):
        # pylint: disable=super-init-not-called,non-parent-init-called
        Stack.__init__(self, fw_env, core)
        self._magic_number = 0xaaaa
        self._start = self._core.fields["STACK_START_ADDR"]
        self._end = self._core.fields["STACK_END_ADDR"]
        self._dm_stack = self._core.sym_get_range('MEM_MAP_STACK')

    @property
    def unused(self):
        #Grab the stack as an octet array, the turn it into 32bit dwords
        octets = self._core.dm[self._start:self._end]
        octets = bytes_to_dwords(octets)
        octets = reversed(octets)
        ctr = 0
        #As long as we find our magic number, the stack is still unused
        for i in octets:
            if i == self._magic_number:
                ctr = ctr + 1
            else:
                break
        #We looped through in 32 bit dwords so return unused stack in bytes
        return ctr * 4


    def _generate_memory_report_component(self):
        """\
        We override the Apps stack to show the "protect bytes" we leave after
        the normal stack space. This extra space allows us to take hardware
        error interrupts when our stack is full
        """
        stack_overrun_protection_bytes = self._dm_stack["end"] - self._end # pylint: disable=invalid-name
        # First account for the stack_overrun_protection_bytes in the
        # main Apps stack
        output = FirmwareStack._generate_memory_report_component(self)
        output[0]["end"] = output[0]["end"] - stack_overrun_protection_bytes

        stack_overrun = {
            "name":"stack_overrun_protection_bytes",
            "start": output[0]["end"],
            "end": output[0]["end"] + stack_overrun_protection_bytes,
            "size": stack_overrun_protection_bytes}
        return [output[0], stack_overrun]

class AudioStack(FirmwareStack):
    """
    Models the call stack for the Audio subsystem firmware
    """

    def __init__(self, fw_env, core):
        # pylint: disable=super-init-not-called,non-parent-init-called
        Stack.__init__(self, fw_env, core)
        self._magic_number = 0x0000
        # pylint: disable=protected-access
        self._dm_stack = self._core.sym_get_range(self._core.fw._stack_sym)
        self._start = self._core.fields["STACK_START_ADDR"]
        self._end = self._core.fields["STACK_END_ADDR"]
        self._fw_running = True
        if self._start == 0 and self._end == 0:
            # This core isn't running so we can't look at the HW registers
            self._start = self._dm_stack["start"]
            self._end = self._dm_stack["end"]
            self._fw_running = False

    @property
    def unused(self):
        if self._fw_running:
            # If the firmware is running we call the baseclass to get the
            # unused amount
            return AppsStack.unused.fget(self)
        # If the firmware is not running report the full stack as being unused
        return AppsStack.size.fget(self)

