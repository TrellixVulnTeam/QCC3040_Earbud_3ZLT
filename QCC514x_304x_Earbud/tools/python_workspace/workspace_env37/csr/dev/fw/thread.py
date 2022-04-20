############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
""" This module provides API for the Oxygen and FreeRTOS Scheduler
"""
# pylint: disable=no-self-use, too-many-branches

import itertools
from abc import ABCMeta, abstractmethod
from csr.dev.fw.stack_unwinder import K32StackUnwinder
from csr.dev.model.interface import Group, Table, Text
from csr.dev.adaptor.text_adaptor import StringTextAdaptor


# python 2/3 compatibility
ABC = ABCMeta('ABC', (object,), {'__slots__': ()})


class AbstractThread(ABC):
    """
    Abstract base class for Operating system thread data
    """
    @property
    @abstractmethod
    def address(self):
        raise NotImplementedError

    @property
    @abstractmethod
    def name(self):
        raise NotImplementedError

    @property
    @abstractmethod
    def priority(self):
        raise NotImplementedError

    @property
    @abstractmethod
    def task_state(self):
        raise NotImplementedError

    @property
    @abstractmethod
    def stack_address(self):
        raise NotImplementedError

    @property
    @abstractmethod
    def stack_size_bytes(self):
        raise NotImplementedError

    @property
    @abstractmethod
    def description(self):
        raise NotImplementedError

    @property
    @abstractmethod
    def high_water_mark(self):
        raise NotImplementedError


class FreeRTOSTask(AbstractThread):
    """
    Class providing an API for data relating to the provided FreeRTOS task.
    The state is passed in to avoid the more complicated process of
    deriving it from the TCB.
    """

    def __init__(self, core, env, tcb, state, desc=None):
        self._core = core
        self._env = env
        # Task control block
        self.tcb = tcb
        self._address = self.tcb.address
        self._name = "".join(chr(c.value) for c in itertools.takewhile(
            lambda x: x.value, self.tcb.pcTaskName))
        self._priority = self.tcb.uxPriority.value
        self._state = state
        self._stack_addr = self.tcb.pxStack.value
        self._stack_size_bytes = core.dmw[self.stack_address] - self.stack_address
        self._description = desc
        self.thread_obj = ThreadExecState(self._core.dataw, self.tcb)

    @property
    def address(self):
        return self._address

    @property
    def name(self):
        return self._name

    @property
    def priority(self):
        return self._priority

    @property
    def task_state(self):
        return self._state

    @property
    def stack(self):
        """
        Unwind the stack using the Kalimba32 unwinder
        """
        return self._stack()

    def _stack(self, **kwargs):
        if self.task_state != "Running":
            execution_state = self.thread_obj
        else:
            execution_state = self._core
        return K32StackUnwinder(self._env, self._core,
                                execution_state=execution_state,
                                # The stack base is the second entry
                                # on the stack, the first entry is
                                # the pointer to the end of the stack
                                stack_base=self.stack_address+4).bt(**kwargs)

    @property
    def stack_address(self):
        """
        The first entry on the stack
        """
        return self._stack_addr

    @property
    def stack_size_bytes(self):
        return self._stack_size_bytes

    @property
    def description(self):
        return self._description

    @property
    def high_water_mark(self):
        """
        The minimum amount of remaining stack space that was available to the
        task since the task started executing. The stack high water mark is
        calculated by checking where the FILL_BYTES are up to in the stack.
        If the high water mark is 0 then the stack has overflowed.
        """
        unused_stack = 0
        fill_bytes = 0xa5
        # portSTACK_GROWTH is set to 1 in the firmware
        port_stack_growth = 1
        try:
            # Check if the uxTaskGetStackHighWaterMark macro has been set to 1
            # This needs to be set to 1 for high water mark to be displayed.
            self._core.fw.env.functions['uxTaskGetStackHighWaterMark']
        except KeyError:
            pass
        else:
            stack_val = self.tcb.pxEndOfStack.value
            stack_arr = self._core.data[stack_val - self.stack_size_bytes + 1:stack_val + 1]
            for val in stack_arr:
                if val == fill_bytes:
                    unused_stack = unused_stack + port_stack_growth
            return (self.stack_size_bytes - unused_stack)/4
        return unused_stack


class ThreadExecState(object):
    """
    Execution state of a suspended thread in FreeRTOS
    """
    def __init__(self, dataw, tcb):
        self.tcb = tcb
        self.dataw = dataw

    @property
    def r(self):
        """
        Accessor to the Kalimba R register bank
        0x04 - 0x0c, 0x1c - 0x20, 0x7c- 0x90
        See https://confluence.qualcomm.com/confluence/display/AppsFW/Developer+Notes#DeveloperNotes-TaskContextLayout
        """
        # pylint: disable=invalid-name
        reg_list = []
        r_offset = [180, 176, 172, 156, 152, 60,
                    56, 52, 48, 44, 40]
        tos = self.tcb.pxTopOfStack.value
        for val in r_offset:
            reg_list.append(self.dataw[tos - val])
        return reg_list

    @property
    def pc(self):
        """
        Program counter (stored in rLink register)
        """
        return self.dataw[self.tcb.pxTopOfStack.value - 148]

    @property
    def sp(self):
        """
        Stack pointer
        """
        return self.dataw[self.tcb.pxTopOfStack.value]

    @property
    def fp(self):
        """
        FP register
        """
        return self.dataw[self.tcb.pxTopOfStack.value - 184]

    @property
    def rmac(self):
        """
        rmac is a 72-bit value formed from two 32-bit registers and an 8-bit
        register
        """
        reg_list = []
        r_offset = [104, 100, 96]
        tos = self.tcb.pxTopOfStack.value
        for val in r_offset:
            reg_list.append(self.dataw[tos - val])
        rmac = reg_list[0] << 40 | reg_list[1] << 8 | reg_list[2]
        return rmac

    @property
    def rlink(self):
        """
        Accessor to the REGFILE_RLINK register
        """
        return self.dataw[self.tcb.pxTopOfStack.value - 148]

    @property
    def core_regs_dict(self):
        """
        Returns the core registers as a dictionary. Keys are lower case and of
        the basic form. i.e. REGFILE_R0 would have key of "r0"
        """
        r_dict = {}
        i = 0
        while i < len(self.r):
            r_dict["r{}".format(i)] = self.r[i]
        return r_dict

    def _print_registers(self, report=False):
        """
        Pretty print method for displaying the registers.

        p: report If False, then return output as a string. If True,
        then return a Group object (as used in reports)
        """

        columns = ['value']
        table = Table(headings=columns)

        for reg_name, value in self.core_regs_dict.items():
            row = [reg_name.upper(), value]
            table.add_row(row)

        output = Group("FreeRTOS Registers")
        output.append(table)
        if report:
            return output
        return StringTextAdaptor(output)

    def _generate_report_body_elements(self):
        return [self._print_registers(report=True)]

    def __repr__(self):
        return self._print_registers()
