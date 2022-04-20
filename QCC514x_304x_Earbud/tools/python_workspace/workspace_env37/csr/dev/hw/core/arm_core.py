############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides following classes to represent ARM processor core:
  ISPRInfo, ISPRBitFieldInfoDIct
  ArmSoftwareIntRegisterInfo
  RegisterCachePort
  ArmCore, CortexMCore, CortexM0Core, CortexM4Core
"""
import time
import sys
from csr.wheels.global_streams import iprint
from csr.dev.hw.core.meta.i_core_info import ArmCortexMCoreInfo
from csr.dev.hw.core.mixin.supports_custom_digits import SupportsCustomDigits
from .base_core import BaseCore
from .execution_state import ArmExecState, CortexM0ExecState
from .debug_controller import DebugController, CortexMDebugController
from ....wheels.bitsandbobs import display_hex, NameSpace, bytes_to_dwords,\
    PureVirtualError
from ..address_space import ExtremeAccessCache, AddressSlavePort, AddressMap,\
NullAccessCache
from .meta.i_io_map_info import IIOMapInfo, FieldValueDict, FieldRefDict, \
FieldArrayRefDict
from ..register_field.meta.i_register_field_info import IRegisterFieldInfo, \
SimpleRegisterFieldInfo
from .meta.i_layout_info import ArmCortexMDataInfo
from .meta.io_struct_io_map_info import IoStructIOMapInfo
from ..io.arch import armv6_m_io_struct as m0_regs_io_struct
# pylint: disable=undefined-variable
if sys.version_info > (3,):
    # Python 3
    int_type = int
else:
    int_type = (int, long) # pylint: disable=undefined-variable


class ISPRInfo(IRegisterFieldInfo):
    """
    Metadata for the ISPR, the Interrupt Set-pending Register,
    generated automatically from the overall ISPR base
    address and the particular register number this info represents.
    """
    def __init__(self, base_addr, ispr_num):

        self._base_addr = base_addr
        self._num = ispr_num

    @property
    def num(self):
        'particular register number of the ISPR'
        return self._num

    @property
    def name(self):
        'Name of ISPR'
        return "INTERRUPT_SET_PENDING%d" % self._num

    @property
    def description(self):
        'Returns description of the ISPR'
        return ("Interrupt set-pending register for software interrupts "
                "%d to %d") % (32*self._num, 32*self._num+31)

    @property
    def parent(self):
        return None

    @property
    def children(self):
        try:
            self._children
        except AttributeError:
            self._children = ISPRBitfieldInfoDict(self)
        return self._children

    @property
    def start_addr(self):
        return self._base_addr + 4*self._num

    @property
    def stop_addr(self):
        return self._base_addr + 4*self._num + 4

    @property
    def start_bit(self):
        return 0

    @property
    def stop_bit(self):
        return self.num_bits

    @property
    def num_bits(self):
        return 32

    @property
    def mask(self):
        return 0xffffffff

    @property
    def does_span_words(self):
        return False

    @property
    def is_writeable(self):
        return True

    @property
    def reset_value(self):
        return 0

    @property
    def layout_info(self):
        return ArmCortexMDataInfo()

    @property
    def enum_infos(self):
        raise PureVirtualError(self, "enum_infos")

    def enum_value_by_name(self, enum_name):
        raise PureVirtualError(self, "enum_value_by_name")

class ISPRBitfieldInfoDict(object):
    """
    Information about a given ISPR register's fields, generated on the fly.
    """
    def __init__(self, parent):
        self._parent = parent

    def __getitem__(self, field_name):

        field_name_cmpts = field_name.split("_")
        if (len(field_name_cmpts) != 3 or field_name_cmpts[0] != "ISPR" or
                int(field_name_cmpts[1]) != self._parent.num or
                int(field_name_cmpts[2]) > 31):
            raise KeyError("No bit field '%s' in %s" % (field_name,
                                                        self._parent.name))
        bit_pos = int(field_name_cmpts[2])
        return SimpleRegisterFieldInfo(
            field_name, "", self._parent,
            False, self._parent.start_addr,
            self._parent.stop_addr, bit_pos, bit_pos+1,
            1<<bit_pos, False, True, 0,
            ArmCortexMDataInfo())

    def keys(self):
        'implementation of dict.keys for self'
        return ["ISPR_%d_%d" % (self._parent.num, i) for i in range(32)]

    def items(self):
        'implementation of dict.items interface for self'
        return [(k, self[k]) for k in self.keys()]


class ArmSoftwareIntRegisterInfo(IIOMapInfo):
    """
    Container for information about the ARM Interrupt set-pending registers,
    suitable for use with FieldValueDict for convenient access
    """
    def __init__(self, base_addr, num_regs, stir=False):

        self._reg_info_dict = {
            ("INTERRUPT_SET_PENDING%d" % i):ISPRInfo(base_addr, i)
            for i in range(num_regs)}
        if stir:
            self._reg_info_dict["STIR"] = SimpleRegisterFieldInfo(
                "STIR", "Software-triggered interrupt register",
                None, None, stir, stir+4,
                0, 32, 0xffffffff, False, True, 0,
                ArmCortexMDataInfo())

    def lookup_field_info(self, field_sym):
        return self._reg_info_dict[field_sym]

    @property
    def field_records(self):
        return self._reg_info_dict

class RegisterCachePort(object):
    'Provides debugger access to the Arm Core Register Cache'
    def __init__(self, num_regs):

        self._cache = [0]*num_regs

    def read(self, regids):
        'Reads the named registers in regids from the Arm Core Register Cache'
        from ..port_connection import NoAccess
        values = [self._cache[ireg] for ireg in regids]
        if None in values:
            raise NoAccess("Access requested to unavailable register(s)")
        return values

    def write(self, regids, values):
        '''
        Writes the values for the named registers in regids
        to the Arm Core Register Cache
        '''
        for i, ireg in enumerate(regids):
            self._cache[ireg] = values[i]

class ArmCore(BaseCore): #pylint: disable=abstract-method
    '''
    Abstract base class for processors based on ARM.
    Debugger representation of the state of the ARM Core processor
    '''
    def __init__(self, access_cache_type):
        BaseCore.__init__(self)
        self.access_cache_type = access_cache_type
        self._arm_regs_io_map_info = None

    class RegisterWriteError(RuntimeError):
        'Base class for exceptions when setting ARM register'

    class NotHaltedError(RegisterWriteError):
        'Processor is not in debug mode halted state'

    class NotUnHaltedError(RegisterWriteError):
        'Processor is not in debug mode halted state'

    class RegNames(object):
        #Possible not the best way of adding these names into the namespace:
        #pylint: disable=too-few-public-methods
        'Common ARM register names'
        # unnamed general ones are just accessed as r[0..n-1];
        # whereas the ones named below have mnemonic r[name]
        SP = 13
        LR = 14
        PC = 15
        NUM_REGS = 16
        NUM_UNNAMED_REGS = NUM_REGS - 3

    class GeneralReg(object): #pylint: disable=too-few-public-methods
        'General ARM registers'

        def __init__(self, debug_reg_port, core):
            self._reg_port = debug_reg_port
            self._core = core

        @display_hex
        def __getitem__(self, regid):
            if isinstance(regid, int_type):
                regids = [regid]
            elif isinstance(regid, slice):
                start = regid.start if regid.start else 0
                stop = (regid.stop if regid.stop else
                        self._core.RegNames.NUM_REGS)
                step = regid.step if regid.step else 1
                regids = range(start, stop, step)
            else:
                regids = list(regid)
            values = self._reg_port.read(regids)
            if isinstance(values, int_type):
                return values[0]
            result = bytes_to_dwords(values)
            if len(result) == 1:
                return result[0]
            return result

        def __setitem__(self, regid, value):
            # Tip: raise an exception if the processor is not already halted
            if (not self._core.subsystem.chip.device.is_coredump()
                    and not self._core.is_halted()):
                raise self._core.NotHaltedError
            if isinstance(regid, int_type):
                regids = [regid]
                value = [value]
            elif isinstance(regid, slice):
                start = regid.start if regid.start else 0
                stop = (regid.stop if regid.stop else
                        self._core.RegNames.NUM_REGS)
                step = regid.step if regid.step else 1
                regids = range(start, stop, step)
            else:
                regids = list(regid)
            return self._reg_port.write(regids, value)

    @property
    def debug_controller(self):
        'Accessor to the ARM DebugController object'
        try:
            self._debug_controller
        except AttributeError:
            self._debug_controller = DebugController(self.data, {})
        return self._debug_controller.slave

    @property
    def execution_state(self):
        """
        Returns an object representing the execution state of the core.
        """
        try:
            return self._execution_state
        except AttributeError:
            self._execution_state = ArmExecState(core=self)
        return self._execution_state

    @property
    def _general_reg(self):
        try:
            self.__general_reg
        except AttributeError:
            if self.access_cache_type is ExtremeAccessCache:
                regs_port = RegisterCachePort(self.RegNames.NUM_REGS)
            else:
                regs_port = self.debug_controller.regs
            self.__general_reg = self.GeneralReg(regs_port, self)
        return self.__general_reg

    @property
    def r(self): #pylint: disable=invalid-name
        'General ARM register'
        return self._general_reg

    @property
    def sp(self):
        'ARM Stack pointer'
        return self._general_reg[self.RegNames.SP]

    @sp.setter
    def sp(self, value):
        self._general_reg[self.RegNames.SP] = value

    @property
    def lr(self):
        'ARM Link register'
        return self._general_reg[self.RegNames.LR]

    @lr.setter
    def lr(self, value):
        self._general_reg[self.RegNames.LR] = value

    @property
    def pc(self):
        'ARM Program Counter'
        return self._general_reg[self.RegNames.PC]

    @pc.setter
    def pc(self, value):
        self._general_reg[self.RegNames.PC] = value

    def run(self):
        'Starts the ARM Core execution'
        self.debug_controller.run_ctrl.run()

    def pause(self):
        'Pauses the ARM Core execution'
        self.debug_controller.run_ctrl.pause()

    def step(self):
        'Single-steps the ARM Core execution'
        self.debug_controller.run_ctrl.step()

    def brk_display(self, report=False):
        'displays the breakpoint database'
        return self.debug_controller.run_ctrl.brk_display(report)

    def reset(self, level=1):
        """Resets processor
        reset 2 - Resets core & peripherals using RESET pin.
        reset 1 - Resets the core only, not peripherals.
        reset 0 - Resets core & peripherals via SYSRESETREQ & VECTRESET bit."""
        self.debug_controller.run_ctrl.reset(level)

    def brk_set(self, address):
        'Sets a debug breakpoint at address'
        return self.debug_controller.run_ctrl.brk_set(address)

    def brk_delete(self, brk_id):
        'Deletes debug breakpoint identified by brk_id'
        self.debug_controller.run_ctrl.brk_delete(brk_id)

    def brk_enable(self, brk_id):
        'Enables a debug breakpoint identified by brk_id'
        self.debug_controller.run_ctrl.brk_enable(brk_id)

    def brk_disable(self, brk_id):
        'Disables a debug breakpoint identified by brk_id'
        self.debug_controller.run_ctrl.brk_disable(brk_id)

    def watch_set(self, wp_id, address, mask, function):
        #pylint: disable=line-too-long
        """
        Sets a debug watchpoint. See below link for example usage with CortexM0
        https://confluence.qualcomm.com/confluence/display/AUPBTSW/Pydbg+Watchpoint+Notes
        """
        self.debug_controller.run_ctrl.watch_set(
            wp_id, address, mask, function)

    def watch_delete(self, wp_id):
        """
        Deletes/disables a debug watchpoint.
        """
        self.debug_controller.run_ctrl.watch_delete(wp_id)

    def watch_display(self, report=False):
        """
        Displays the watchpoints
        """
        return self.debug_controller.run_ctrl.watch_display(report)

    def _all_subcomponents(self):
        sub_dict = BaseCore._all_subcomponents(self)
        sub_dict.update({"execution_state": "_execution_state"})
        return sub_dict


class CortexMCore(ArmCore): #pylint: disable=abstract-method
    'Common base class for specific Arm Cortex classes'

    # Useful magic numbers that get written into the main ARM Cortex stack when
    # an interrupt occurs which indicate what state the processor returns to
    RETURN_TO_HANDLER = 0xfffffff1
    RETURN_TO_THREAD_USING_MSP = 0xfffffff9
    RETURN_TO_THREAD_USING_PSP = 0xfffffffd

    @property
    def arm_regs(self): #pylint: disable=no-self-use
        'accessor to general purpose arm registers'
        raise PureVirtualError(self, 'arm_regs')

    @property
    def DHCSR_DBGKEY(self): #pylint: disable=invalid-name
        'Provides a constant debug key for use when writing to DHCSR register'
        try:
            return self._dhcsr_dbgkey
        except AttributeError:
            # pylint: disable=no-member
            dhcsr_reg = self.arm_regs.CORTEXM0_DCB_DHCSR
            lower_key_mask = (
                dhcsr_reg.S_LOCKUP.mask |
                dhcsr_reg.S_SLEEP.mask |
                dhcsr_reg.S_HALT.mask |
                dhcsr_reg.S_REGRDY.mask)
            upper_key_mask = (
                dhcsr_reg.DBGKEY31.mask |
                dhcsr_reg.DBGKEY29.mask |
                dhcsr_reg.DBGKEY22.mask |
                dhcsr_reg.DBGKEY20.mask)
            # This is the magic key 0xa05f0000L
            self._dhcsr_dbgkey = upper_key_mask | lower_key_mask
        return self._dhcsr_dbgkey

    @property
    def mem_ap(self):
        return self.data

class CortexM0Core(CortexMCore):
    #pylint: disable=abstract-method,too-many-instance-attributes
    '''
    Abstract class providing debugger representation of the ARM CortexM0
    core processor
    '''

    # useful BP_COMPx constants not available in io_structs
    BP_COMP_ADDR_MASK = 0x1FFFFFFC   # Masks BP_COMPx.COMP field [28:2]
    BP_COMP_MATCH_LOWER = 0x40000000 # BP_COMPx.BP_MATCH = 01 lower halfword
    BP_COMP_MATCH_UPPER = 0x80000000 # BP_COMPx.BP_MATCH = 10 upper halfword
    BP_COMP_ENABLE = 1               # BP_COMPx.ENABLE
    # Watchpoint constants
    DWT_FUNCTION_MASK = 0x0000000F   # FUNCTION field is DWT_FUNCTION Bits 3:0
    DWT_MATCHED_MASK = 0x01000000    # MATCHED field is DWT_FUNCTION Bit 24
    DWT_NUMCOMP_BIT_POS = 28         # NUM_COMP bit position
    DWT_FUNCTIONS = {
        0:"Disabled", 1:"Reserved", 2:"Reserved", 3:"Reserved",
        4:"Iaddr", 5:"Daddr:RO", 6:"Daddr:WO", 7:"Daddr:RW",
        8:"Reserved", 9:"Reserved", 10:"Reserved", 11:"Reserved",
        12:"Reserved", 13:"Reserved", 14:"Reserved", 15:"Reserved"}

    class RegNames(CortexMCore.RegNames):
        #Possibly not the best way of adding these names into the namespace:
        #pylint: disable=too-few-public-methods
        'CORTEXM0 ARM register names'
        # unnamed general ones are just accessed as r[0..n-1];
        # whereas the ones named below have mnemonic r[name]
        XPSR = 0x10
        MSP = 0x11
        PSP = 0x12
        #
        SPECIAL = 0x14
        NUM_REGS = 0x15

    @property
    def execution_state(self):
        """
        Returns an object representing the execution state of the core.
        """
        try:
            return self._execution_state
        except AttributeError:
            self._execution_state = CortexM0ExecState(core=self)
        return self._execution_state

    def populate(self, access_cache_type):
        """
        Create the standard M0 memory map containers.  Particular M0 instances
        can add mappings underneath code, sram, external, etc
        """
        def address_map(name, size):
            'Accessor to the ARM CortexM0 address map'
            return AddressMap(name, length=size, cache_type=access_cache_type,
                              layout_info=self.info.layout_info)

        def address_space(name, size, access_cache_type):
            'Accessor to the ARM CortexM0 address space'
            return AddressSlavePort(
                name, length=size, cache_type=access_cache_type,
                layout_info=self.info.layout_info)

        self._mem_map_cmpts = NameSpace()
        self._mem_map_cmpts.code = address_map("ARM_M0_CODE", 0x20000000)
        self._mem_map_cmpts.sram = address_map("ARM_M0_SRAM", 0x20000000)
        self._mem_map_cmpts.peripheral = address_map(
            "ARM_M0_PERIPHERAL", 0x20000000)
        self._mem_map_cmpts.external = address_map(
            "ARM_M0_EXTERNAL", 0x40000000)
        self._mem_map_cmpts.external_device = address_map(
            "ARM_M0_EXTERNAL_DEVICE", 0x40000000)
        self._mem_map_cmpts.dwt = address_space(
            "ARM_M0_DWT", 0x3c, access_cache_type)
        self._mem_map_cmpts.bpu = address_space(
            "ARM_M0_BPU", 0x18, access_cache_type)
        self._mem_map_cmpts.nvic = address_space(
            "ARM_M0_NVIC", 0xD00, access_cache_type)
        self._mem_map_cmpts.debug_control = address_space(
            "ARM_M0_DEBUG_CONTROL", 0x300, access_cache_type)
        self._mem_map_cmpts.rom_table = address_space(
            "ARM_M0_ROM_TABLE", 0x1000, access_cache_type)

        self._data = address_map("ARM_M0_MEMORY", 0x100000000)
        self._data.add_mappings(
            (0x00000000, 0x20000000, self._mem_map_cmpts.code.port),
            (0x20000000, 0x40000000, self._mem_map_cmpts.sram.port),
            (0x40000000, 0x60000000, self._mem_map_cmpts.peripheral.port),
            (0x60000000, 0xA0000000, self._mem_map_cmpts.external.port),
            (0xA0000000, 0xE0000000, self._mem_map_cmpts.external_device.port),
            (0xE0001000, 0xE000103c, self._mem_map_cmpts.dwt),
            (0xE0002000, 0xE0002018, self._mem_map_cmpts.bpu),
            (0xE000E000, 0xE000ED00, self._mem_map_cmpts.nvic),
            (0xE000ED00, 0xE000F000, self._mem_map_cmpts.debug_control),
            (0xE000FF00, 0xE0100000, self._mem_map_cmpts.rom_table))


        self._arm_regs_io_map_info = IoStructIOMapInfo(m0_regs_io_struct, None,
                                                       self.info.layout_info)

    @property
    def data(self):
        return self._data.port

    @property
    def debug_controller(self):
        try:
            self._debug_controller
        except AttributeError:
            def core_debug_regs(suffix):
                'convert a register name into a full CORTEXM0 register name'
                return getattr(m0_regs_io_struct, suffix).addr

            self._debug_controller = CortexMDebugController(
                self.data, {}, self, core_debug_regs)

        return self._debug_controller.slave

    @property
    def arm_regs(self):
        'Accessor for the Arm CortexM0 registers'
        try:
            self._arm_regs
        except AttributeError:
            arm_regs_field_refs = FieldRefDict(self._arm_regs_io_map_info,
                                               self, None)
            arm_regs_field_array_refs = \
            FieldArrayRefDict(self._arm_regs_io_map_info,
                              self, None)
            self.__arm_regs = FieldValueDict(arm_regs_field_refs,
                                             arm_regs_field_array_refs)
        return self.__arm_regs

    @property
    def xpsr(self):
        'Accessor of ARM Program Status Register(s) APSR/IPSR/EPSR'
        # TBD which you get
        return self.r[self.RegNames.XPSR]

    @xpsr.setter
    def xpsr(self, value):
        'Modifier of ARM Program Status Register(s) APSR/IPSR/EPSR'
        # TBD which you set
        self.r[self.RegNames.XPSR] = value

    @property
    def msp(self):
        'ARM Main Stack pointer'
        return self.r[self.RegNames.MSP]

    @msp.setter
    def msp(self, value):
        'Modifier of ARM Main Stack pointer'
        self.r[self.RegNames.MSP] = value

    @property
    def psp(self):
        'ARM Process Stack pointer'
        return self.r[self.RegNames.PSP]

    @psp.setter
    def psp(self, value):
        'Modifier of Process Stack pointer'
        self.r[self.RegNames.PSP] = value

    @property
    def special(self):
        '''
        Accessor to ARM special registers:
        PRIMASK/FAULTMASK/BASEPRI/CONTROL
        '''
        return self.r[self.RegNames.SPECIAL]

    @special.setter
    def special(self, value):
        'Modifier of Special PRIMASK/FAULTMASK/BASEPRI/CONTROL registers'
        self.r[self.RegNames.SPECIAL] = value

    @property
    def dhcsr(self):
        'Accessor to ARM Debug Halt & Control Status Register (DHCSR) register'
        # This one is memory mapped so direct access possible.
        return self.arm_regs.CORTEX_M0_DCB_DHCSR

    def cpu_shallow_sleep_time(self, sample_ms=1000):
        """
        Returns the % of time the CPU spends in shallow sleep
        """
        asleep = 0
        awake = 0
        initial_time = time.time()
        final_time = initial_time + (sample_ms/1000.0)
        while time.time() < final_time:
            if self.arm_regs.DHCSR.S_SLEEP != 0:
                asleep += 1
            else:
                awake += 1
        total_samples = asleep + awake
        if total_samples < 10:
            self.logger.warning(
                'total samples is less than 10: increase sample_ms.')
        return (asleep * 100.0) / total_samples

    def is_in_reset(self):
        """
        Check if the CPU is in reset state by reading
        the curator register CURATOR_BT_SYS_M0_RESET_N == 0;
        the DHCSR.S_RESET_ST does not seem to be always up to date.
        """
        cur = self.subsystem.chip.curator_subsystem.core # pylint: disable=no-member
        return cur.fields.CURATOR_BT_SYS_M0_RESET_N == 0

    def is_asleep(self):
        """
        Check if the CPU is in sleep state (WFI) by reading
        the Debug Halting Control and Status Register (DHCSR)
        """
        return self.arm_regs.DHCSR.S_SLEEP == 1

    def is_debug_enabled(self):
        'Returns whether ARM core debug mode is enabled'
        return self.arm_regs.DHCSR.C_DEBUGEN == 1

    def enable_debug_mode(self):
        'Enable debug mode without halting processor'
        # If not in debug mode then no harm clearing all other lower word bits
        # however if in debug mode don't want to do so by writing zeros to them,
        # but no need to do anything at all.
        if not self.is_debug_enabled():
            dhcsr_reg = self.arm_regs.DHCSR
            self.arm_regs.DHCSR = \
                self.DHCSR_DBGKEY | dhcsr_reg.C_DEBUGEN.mask
        else:
            iprint("BT Debug mode already enabled")

    def disable_debug_mode(self):
        'Disable debug mode (unhalts processor, unmasks interrupts)'
        if self.is_debug_enabled():
            # It is recommended in ARM documentation when clearing C_DEBUGEN,
            # that we also clear C_MASKINTS, C_STEP and C_HALT
            # in the same access, as here
            self.arm_regs.DHCSR = self.DHCSR_DBGKEY
        else:
            iprint("BT Debug mode already disabled")

    def halt(self):
        """
        The BT ARM chip needs explicitly halting because the
        curator register CURATOR_SUBSYSTEMS_RUN_EN doesn't halt it.

        Halt the CPU by writing to the Debug Halting Control
        and Status Register (DHCSR). Processor remains in debug
        mode.
        """
        # BT could be unpowered, or powered but not clocked (deep sleep),
        # powered and clocked but not running (held in reset or napping),
        # powered/clocked/running.
        bt_subsystem = self.subsystem # pylint: disable=no-member
        if bt_subsystem.is_powered():
            # keep it clocked afterwards for debug purposes
            # older curator don't appear to do this coming out of deep sleep
            # but recent ones are
            if not bt_subsystem.is_clocked():
                bt_subsystem.clocked = 1
                iprint('Info: BT was not clocked (is now)- '
                       'are you using old curator build/patch?')
            if not bt_subsystem.core.is_in_reset():
                arm_reg = self.arm_regs.CORTEXM0_DCB_DHCSR
                self.enable_debug_mode()
                # request a halt
                self.regs.CORTEX_M0_DHCSR.write(
                    self.DHCSR_DBGKEY |
                    arm_reg.C_HALT.mask | arm_reg.C_DEBUGEN.mask)
                if arm_reg.C_HALT == 0:
                    raise self.NotHaltedError(
                        "Unable to halt CPU core: see dhcsr.CORTEX_M0_C_HALT")
                # Have to run-enable BT so that the ARM Cortex can go
                # into the Halted state and update the status bits.
                if not bt_subsystem.is_run_enabled():
                    bt_subsystem.run_enable = 1
                # Test the S_HALT and S_SLEEP bits?
                if not self.is_halted():
                    raise self.NotHaltedError(
                        "Unable to halt CPU core: see dhcsr.CORTEX_M0_S_HALT")
                if self.is_asleep():
                    raise self.NotHaltedError(
                        "Unable to halt CPU core: see dhcsr.CORTEX_M0_S_SLEEP")

    def is_halted(self):
        'Check if CPU is held in debug halted state'
        return self.arm_regs.DHCSR.S_HALT == 1

    def run(self):
        """
        Un-halt the CPU by writing to the Debug Halting Control
        and Status Register (DHCSR). Processor remains in debug
        mode after execution.
        """
        arm_reg = self.arm_regs.CORTEXM0_DCB_DHCSR
        # clear all bits except the DEBUGEN.
        self.regs.CORTEX_M0_DHCSR.write(
            self.DHCSR_DBGKEY | arm_reg.C_DEBUGEN.mask)
        if arm_reg.C_HALT != 0:
            raise self.NotUnHaltedError("Unable to unhalt CPU core")

class CortexM4Core(CortexMCore): #pylint: disable=abstract-method
    '''
    Abstract class providing debugger representation of the ARM CortexM4
    core processor
    '''
    def __init__(self, access_cache_type):

        CortexMCore.__init__(self, access_cache_type)
        self._cortex_m4_regs = AddressSlavePort(
            "CORTEX_M4_REGS",
            length=0x4000,
            cache_type=self.access_cache_type,
            layout_info=self.info.layout_info)

    def add_data_space_mappings(self, data_map):
        'Establish memory map for data space'
        data_map.add_mapping(0xe000000, 0xe0004000, self._cortex_m4_regs)


    @property
    def arm_regs(self):
        'Accessor for the Arm CortexM4 registers'
        try:
            self._arm_regs
        except AttributeError:
            arm_regs_ref = FieldRefDict(ArmSoftwareIntRegisterInfo(
                0xe000e200, 16, stir=0xe000ef00), self)
            arm_regs_field_array_refs = \
                FieldArrayRefDict(self._arm_regs_io_map_info,
                                  self, None)
            self._arm_regs = FieldValueDict(
                arm_regs_ref, arm_regs_field_array_refs)
        return self._arm_regs


class GenericArmCortexMCoreInfo(ArmCortexMCoreInfo, SupportsCustomDigits):
    """Information about the Zeagle chip Core"""
    # ICoreInfo compliance
    def __init__(self, custom_digits=None, custom_digits_module=None):
        ArmCortexMCoreInfo.__init__(self)
        SupportsCustomDigits.__init__(self, custom_digits=custom_digits,
                                      custom_digits_module=custom_digits_module)

    @property
    def io_map_info(self):

        try:
            self._io_map_info
        except AttributeError:
            self._io_map_info = IoStructIOMapInfo(self.custom_io_struct, None,
                                                  self.layout_info)
        return self._io_map_info
