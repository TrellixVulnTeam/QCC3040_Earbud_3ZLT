############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides classes that report on the execution state of a variety of processor
cores:

  KalimbaExecState --+
                     |
  XapExecState     --+-> BaseExecState
                     |
  ArmExecState     --+
"""

import operator
from collections import OrderedDict
from csr.wheels.bitsandbobs import PureVirtualError
from csr.dev.model.interface import Group, Table, Text, Warning as Warn
from csr.dev.adaptor.text_adaptor import StringTextAdaptor
from csr.dev.model.base_component import BaseComponent
from csr.dev.hw.core.meta.io_struct_io_map_info import RegistersUnavailable
from csr.dev.hw.register_field.register_field import bitz_engine

class BaseExecState(BaseComponent):
    """
    Base class for execution states.
    """

    def __init__(self, core):
        self.core = core

    def _generate_report_body_elements(self):
        raise PureVirtualError

    def __repr__(self):
        raise PureVirtualError


class KalimbaExecState(BaseExecState):
    """
    Execution state of Kalimba cores.
    """

    @property
    def r(self):
        'Accessor to the Kalimba R register bank'
        #pylint: disable=invalid-name
        return self.core.r

    @property
    def i(self):
        'Accessor to the Kalimba I register bank'
        return self.core.i

    @property
    def pc(self):
        'Register accessor'
        return self.core.pc

    @property
    def sp(self):
        'Register accessor'
        return self.core.sp

    @property
    def fp(self):
        'Register accessor'
        return self.core.fp

    @property
    def rmac(self):
        """
        rmac is a 72-bit value formed from two 32-bit registers and an 8-bit
        register
        """
        return self.core.rmac

    @property
    def rlink(self):
        'Accessor to the REGFILE_RLINK register'
        return self.core.rlink

    @property
    def core_regs_dict(self):
        """
        Returns the core registers as a dictionary. Keys are lower case and of
        the basic form. i.e. REGFILE_R0 would have key of "r0"
        """
        return self.core.core_regs_dict

    def _print_registers(self, report=False):
        """
        Pretty print method for displaying the registers.

        p: report If False, then return output as a string. If True,
        then return a Group object (as used in reports)
        """

        columns = ['field', 'bits', 'value']
        table = Table(headings=columns)

        for reg_name, value in self.core_regs_dict.items():
            reg = self.core.field_refs["REGFILE_{}".format(reg_name.upper())]
            bits = "[{}:{}]".format(reg.info.start_bit, reg.info.stop_bit - 1)

            hex_padding = (reg.info.stop_bit - reg.info.start_bit) // 4
            hex_value = "0x{:0{}x}".format(value, hex_padding)

            row = [reg_name.upper(), bits, hex_value]
            table.add_row(row)

        output = Group("Kalimba Core Registers")
        output.append(table)
        if report:
            return output
        return StringTextAdaptor(output)

    def _generate_report_body_elements(self):
        return [self._print_registers(report=True)]

    def __repr__(self):
        return self._print_registers()


class ArmExecState(BaseExecState):
    """
    Execution state of ARM cores.
    """

    @property
    def r(self):
        'General indexable register'
        #pylint: disable=invalid-name
        return self.core.r

    @property
    def sp(self):
        'Accessor to Stack Pointer (SP) register'
        return self.core.sp

    @property
    def lr(self):
        'Accessor to Link Register (LR)'
        return self.core.lr

    @property
    def pc(self):
        'Accessor to Program Counter (PC) register'
        return self.core.pc

    def _populate_registers(self, registers):
        """
        Over-ridable that fills registers Ordered Dictionary with values
        ready for reporting in self._print_registers
        """
        # Note that there are 3 specially named registers: SP, LR and PC
        for index in range(self.core.RegNames.NUM_UNNAMED_REGS):
            registers["r{}".format(index)] = self.r[index]
        registers['sp'] = self.sp
        registers['lr'] = self.lr
        registers['pc'] = self.pc

    def _analyse_registers(self, registers, output):
        """
        Analyse the register info and modify output by appending
        any appropriate anomalies to it.
        """

    def _print_registers(self, report=False):
        """
        Pretty print method for displaying the registers.

        p: report If False, then return output as a string. If True,
        then return a Group object (as used in reports)
        """

        columns = ['field', 'bits', 'value', 'note']
        output = Group("ARM Core Registers")
        table = Table(headings=columns)
        anomalies = []
        # Create dictionary mapping names to registers.
        # Special ones are chip-specific and added here as well.
        registers = OrderedDict()
        try:
            self._populate_registers(registers)
        except RegistersUnavailable:
            anomalies.append(Warn(
                'ARM register definitions unavailable: '
                'you may want to check why. \n'
                'This affects identification of '
                'live_stack (needs PSP) and stack usage (needs PC). \n'
                'PC value of 0 will be falsely reported as possibly asleep.'))
            output.extend(anomalies)
            # abort processing
        else:
            output.extend(anomalies)
            for reg_name, value in registers.items():
                # All registers are 32 bit
                bits = "0: 31"
                if isinstance(value, tuple):
                    value, note = value
                else:
                    note = ''
                hex_value = "0x{:0X}".format(value)
                row = [reg_name.upper(), bits, hex_value, note]
                table.add_row(row)
            self._analyse_registers(registers, output)
            output.append(table)
        if report:
            return output
        return StringTextAdaptor(output)

    def _generate_report_body_elements(self):
        return [self._print_registers(report=True)]

    def __repr__(self):
        return self._print_registers()

class CortexM0ExecState(ArmExecState):
    """
    Execution state of ARM CortexM0 cores.
    """
    # Meanings of exception number in IPSR[0:6],
    # this holds the exception number or the current IRQ
    IPSR_EXCEPTION_IRQ0 = 16 # lowest value of exception number in IPSR
    IPSR_EXCEPTION_IRQ_COUNT = 32 # implementation defined number of IRQs
    # highest IRQ exception number in IPSR:
    IPSR_EXCEPTION_IRQ_MAX = IPSR_EXCEPTION_IRQ0 + IPSR_EXCEPTION_IRQ_COUNT
    IPSR_EXCEPTION_THREADMODE = 0
    IPSR_EXCEPTION_NMI = 2
    IPSR_EXCEPTION_HARDFAULT = 3
    IPSR_EXCEPTION_SVCALL = 11
    IPSR_EXCEPTION_PENDSV = 14
    IPSR_EXCEPTION_SYSTICK = 15
    _ipsr_exception_names = {
        0:"Thread mode",
        1:"Reserved",
        2:"NMI",
        3:"HardFault",
        4:"Reserved",
        5:"Reserved",
        6:"Reserved",
        7:"Reserved",
        8:"Reserved",
        9:"Reserved",
        10:"Reserved",
        11:"SVCall",
        12:"Reserved",
        13:"Reserved",
        14:"PendSV",
        15:"SysTick",
        16:"IRQ0",
        # ... n+15 = IRQ(n-1); n is implementation defined; range 1-32
        # n .. 63, "Reserved"
        }

    def ipsr_exception_name(self, number):
        '''
        Looks up meaning of an IPSR exception number and returns a string
        describing it. This includes the IRQ bits.
        Does not read the value from register: it is supplied in number.
        '''
        try:
            return self._ipsr_exception_names[number]
        except KeyError:
            if self.IPSR_EXCEPTION_IRQ0 < number < self.IPSR_EXCEPTION_IRQ_MAX:
                # one-off population of rest of IRQ descriptors
                for i in range(
                        self.IPSR_EXCEPTION_IRQ0,
                        self.IPSR_EXCEPTION_IRQ0+self.IPSR_EXCEPTION_IRQ_COUNT):

                    self._ipsr_exception_names[i] = ("IRQ" + str(
                        i - self.IPSR_EXCEPTION_IRQ0))
                for i in range(
                        self.IPSR_EXCEPTION_IRQ0+self.IPSR_EXCEPTION_IRQ_COUNT,
                        64):
                    self._ipsr_exception_names[i] = "Reserved"
                return self._ipsr_exception_names[number]
            raise

    @property
    def xpsr(self):
        '''
        Accessor to CortexM0 combined extended program status register (XPSR),
        which combines APSR/EPSR/IPSR information.
        '''
        return self.core.xpsr

    @staticmethod
    def ipsr_exception(ipsr):
        '''
        The part of ipsr containing iPSR exception number is in bottom 6 bits.
        Does not read register; uses value passed in ipsr.
        '''
        # Potential extension:: need a better way of representing the bits in this register
        CORTEXM0_IPSR_EXCEPTION_MASK = 0x3F # pylint: disable=invalid-name
        return ipsr & CORTEXM0_IPSR_EXCEPTION_MASK

    @property
    def msp(self):
        'Accessor to CortexM0 main stack pointer (MSP); banked in SP'
        return self.core.msp

    @property
    def psp(self):
        'Accessor to CortexM0 process stack pointer (PSP); banked in SP'
        return self.core.psp

    @property
    def special(self):
        '''
        Accessor to CortexM0 special registers:
        PRIMASK/FAULTMASK/BASEPRI/CONTROL
        '''
        return self.core.special

    def dhcsr(self):
        'Accessor to the Debug Halt Control Status Register (DHCSR)'
        return self.core.dhcsr

    def _analyse_registers(self, registers, output):
        """
        Analyse the register info and modify output by appending
        any appropriate anomalies to it.
        """
        #Get some register info (without re-reading register)
        dhcsr_reg = self.core.arm_regs.DHCSR
        if registers['dhcsr'] & dhcsr_reg.S_SLEEP.mask:
            output.append(Text(
                'DHCSR.CORTEX_M0_S_SLEEP bit is set, hence many registers '
                'are zero while the chip naps or sleeps: \n'
                'a value read at that instant would '
                'be returned as zero.'))

    def _populate_registers(self, registers):
        """
        Over-ridable that fills registers Ordered Dictionary with values
        ready for reporting in self._print_registers
        """
        ArmExecState._populate_registers(self, registers)
        # There are more specially named registers to add here:
        xpsr = self.xpsr
        registers['xpsr'] = (xpsr, self.ipsr_exception_name(
            self.ipsr_exception(xpsr)))
        registers['msp'] = self.msp
        registers['psp'] = self.psp
        # The one between psp and special is reserved so not displayed
        registers['special'] = self.special
        # rest of banked registers are reserved so not displayed
        registers['dhcsr'] = self.core.dhcsr.read()

    def _generate_report_body_elements(self):
        output = self._print_registers(report=True)
        # Note that __repr__ doesn't give you this more detailed report

        # Here add DHCSR, maybe later add others like CONTROL and SPECIAL
        try:
            output.append(bitz_engine(
                self.core.dhcsr, report=True, value=self.core.dhcsr.read()))
        except RegistersUnavailable:
            output.append(Warning('DHCSR register definition unavailable'))
        else:
            if self.core.subsystem.chip.device.transport is None: # a coredump
                output.append(Text(
                    'Note this shows the saved value of DHCSR before coredump '
                    'halted the chip. \nThe memory mapped register value in '
                    'bt.arm_regs.CORTEXM0_DCB_DHCSR may differ.'))

        return [output]

class XapExecState(BaseExecState):
    """
    Execution state of XAP cores.
    """
    # A lot of property names are named after register names which are usually
    # very short and upset pylint, so shut it up:
    #pylint: disable=invalid-name

    @property
    def pc(self):
        'Accessor to Program Counter (PC) register'
        return self.core.pc

    @property
    def xl(self):
        'Accessor to XL register'
        return self.core.xap_uxl

    @property
    def xh(self):
        'Accessor to XH register'
        return self.core.xap_uxh

    @property
    def y(self):
        'Accessor to Y register'
        return self.core.xap_uy

    @property
    def ah(self):
        'Accessor to AH register'
        return self.core.xap_ah

    @property
    def al(self):
        'Accessor to AL register'
        return self.core.xap_al

    def _print_registers(self, report=False):
        """
        Pretty print method for displaying the registers.

        p: report If False, then return output as a string. If True,
        then return a Group object (as used in reports)
        """

        columns = ['field', 'bits', 'value']
        t = Table(headings=columns)

        reg_sizes = OrderedDict()
        reg_sizes['uxl'] = 16
        reg_sizes['uxh'] = 8
        reg_sizes['uy'] = 16
        reg_sizes['al'] = 16
        reg_sizes['ah'] = 16
        reg_sizes['ixl'] = 16
        reg_sizes['ixh'] = 8
        reg_sizes['iy'] = 16

        for reg_name, length in sorted(reg_sizes.items(),
                                       key=operator.itemgetter(0)):
            reg = self.core.fields["XAP_{}".format(reg_name.upper())]
            # xap registers are 16 bit
            bits = "[0:{}]".format(length - 1)

            hex_value = "0x{:04x}".format(reg)

            row = [reg_name.upper(), bits, hex_value]
            t.add_row(row)

        t.add_row(['PC', "[0:23]", "0x{:08x}".format(self.pc)])

        output = Group("XAP Core Registers")
        output.append(t)
        if report:
            return output
        return StringTextAdaptor(output)

    def _generate_report_body_elements(self):
        return [self._print_registers(report=True)]

    def __repr__(self):
        return self._print_registers()
