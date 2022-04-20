############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"Provides a class to represent a processor on the BT subsystem"

import re
from csr.wheels.global_streams import iprint
from csr.dev.hw.core.mixin.is_xap import IsXAP
from csr.dev.hw.core.mixin.is_in_hydra import IsInHydra
from csr.dev.hw.core.xap_core import XAPCore
from csr.wheels.bitsandbobs import NameSpace
# Misnomer... but Curator and BT's memory maps may be similar enough
from csr.dev.hw.generic_window import CuratorGenericWindow
from csr.dev.hw.address_space import AddressSpace
from csr.dev.hw.core.arm_core import CortexM0Core
from csr.dev.hw.address_space import NullAccessCache, AddressMap,\
    AddressSlavePort
from csr.dev.fw.bt_firmware import BTZeagleFirmware, BTZeagleDefaultFirmware
from .meta.io_struct_io_map_info import IoStructIOMapInfo

class BTCore(IsXAP, IsInHydra, XAPCore):
    #pylint: disable=too-many-ancestors,abstract-method
    """
    Common abstract base class for BT XAP-based cores.
    """
    def __init__(self, subsystem):

        IsXAP.__init__(self)
        IsInHydra.__init__(self, subsystem)
        XAPCore.__init__(self)

    def populate(self, access_cache_type):

        IsXAP.populate(self, access_cache_type)

    @property
    def patch_type(self):
        from csr.dev.fw.patch import HydraPatch
        return HydraPatch

    @property
    def core_commands(self):

        # Dictionary of commands (or other objects) you want to be registered
        # in this core's namespace in addition to its native methods.
        #
        core_cmds = {
            'report'    : "self.subsystem.generate_report",
            #Buf/MMU
            'buf_list'  : "self.subsystem.mmu.buf_list",
            'buf_read'  : "self.subsystem.mmu.buf_read",
            'fw_ver'    : "self.fw.fw_ver",
            'patch_ver' : "self.fw.patch_ver",
        }

        # Commands that might not be supported because they rely on the
        # ELF/DWARF.
        core_fw_cmds = {
            #Logging
            'log'       : "self.fw.debug_log.generate_decoded_event_report",
            'live_log'  : "self.fw.debug_log.live_log",
            'clear_log' : "self.fw.debug_log.clear",
            'reread_log': "self.fw.debug_log.reread",
            'log_level' : "self.fw.debug_log.log_level",
            #Symbol lookup
            'disp'      : "self.disp_report",
            'psym'      : "self.sym_.psym",
            'dsym'      : "self.sym_.dsym",
            'dispsym'   : "self.sym_.dispsym",
            'sym'       : "self.sym_.sym",
            'struct'    : "self.fw.env.struct",
        }
        core_cmds.update(core_fw_cmds)
        exception_list = [AttributeError, AddressSpace.ReadFailure]

        return core_cmds, exception_list

    @property
    def _hif_subsystem_view(self):

        return self.subsystem.hif

    @property
    def nicknames(self):
        return ["bt", "bluetooth"]

    def _create_generic_window(self, name, access_cache_type):
        # So far one size does for all derived curator cores.
        gwin = CuratorGenericWindow(self, name)
        gwin.populate(access_cache_type)
        return gwin

    def _is_running_from_rom(self):
        """
        Is the core configured to fetch code from ROM or SQIF/LPC?
        """
        # This is a default implementation that works on all chips at the
        # moment. It can be overriden in subclasses.
        # pylint: disable=no-member
        try:
            half = self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_SQIF_LOW_ROM
        except AttributeError:
            half = self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_LPC_LOW_ROM

        return (self.bitfields.NV_MEM_ADDR_MAP_CFG_STATUS_ORDER.read()
                in (half, self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_ROM_LOW_ROM))


class BTZeagleCore(CortexM0Core, IsInHydra): #pylint: disable=abstract-method
    """
    Common abstract base class for BT Zeagle-based cores, such as QCC514X_QCC304X.
    """
    def __init__(self, subsystem, access_cache_type):

        CortexM0Core.__init__(self, access_cache_type)
        IsInHydra.__init__(self, subsystem)

    @property
    def nicknames(self):
        return ["bt", "bluetooth"]

    @property
    def data(self):
        return self._data.port

    def populate(self, access_cache_type):
        """
        Create Zeagle memory map, this is different from a standard
        Cortex M0 memory map due to memory re-mapping. Also Zeagle register
        address space needs to accurately reflect actual hardware beacause
        reading a non-existent address causes a hard fault.
        """

        def address_map(name, size, access_cache_type):
            """
            Helper function to create address_map based
            on memory component name and size.
            """
            return AddressMap(name, length=size, cache_type=access_cache_type,
                              layout_info=self.info.layout_info)

        def address_space(name, size, access_cache_type):
            """
            Helper function to create address_space based
            on memory component name and size.
            """
            return AddressSlavePort(name, length=size,
                                    cache_type=access_cache_type,
                                    layout_info=self.info.layout_info)

        self._mem_map_cmpts = NameSpace()
        self._mem_map_cmpts.code = address_map("ZEAGLE_M0_CODE",
                                               0x100000,
                                               access_cache_type)
        self._mem_map_cmpts.sram = address_map("ZEAGLE_M0_SRAM",
                                               0x100000,
                                               access_cache_type)
        self._mem_map_cmpts.registers1 = address_space("ZEAGLE_M0_REGISTERS1",
                                                       0x100000,
                                                       access_cache_type)
        self._mem_map_cmpts.registers2 = address_space("ZEAGLE_M0_REGISTERS2",
                                                       0x50000,
                                                       access_cache_type)
        self._mem_map_cmpts.registers3 = address_space("ZEAGLE_M0_REGISTERS3",
                                                       0x3000,
                                                       access_cache_type)
        self._mem_map_cmpts.registers4 = address_space("ZEAGLE_M0_REGISTERS4",
                                                       0x100,
                                                       access_cache_type)

        # The following a listed in the latest Zealis memory map (Aug 19) but
        # we'll assume they don't  appear in coredumps, so we won't give them
        # populate-able caches.
        self._mem_map_cmpts.fm_core_dtop = address_space(
            "ZEALIS_MAP_FM_CORE_AND_DTOP",
            0x400000, NullAccessCache)
        self._mem_map_cmpts.usb1p1 = address_space(
            "ZEALIS_MAP_USB_1.1", 0x40000,
            NullAccessCache)
        self._mem_map_cmpts.emulation_fpga = address_space(
            "ZEALIS_MAP_EMULATION__FPGA", 0x2000,
            NullAccessCache)
        self._mem_map_cmpts.apb_fpga = address_space(
            "ZEALIS_MAP_APB_FAKE_WSIM_COEX_LTE",
            0x4000, NullAccessCache)


        self._mem_map_cmpts.dwt = address_space("ZEAGLE_M0_DWT",
                                                0x3c,
                                                access_cache_type)
        self._mem_map_cmpts.bpu = address_space("ZEAGLE_M0_BPU",
                                                0x18,
                                                access_cache_type)
        self._mem_map_cmpts.nvic = address_space("ZEAGLE_M0_NVIC",
                                                 0xD00,
                                                 access_cache_type)
        self._mem_map_cmpts.debug_control = address_space(
            "ZEAGLE_M0_DEBUG_CONTROL",
            0x300,
            access_cache_type)
        self._mem_map_cmpts.rom_table = address_space("ZEAGLE_M0_ROM_TABLE",
                                                      0x1000, access_cache_type)

        self._data = address_map("ZEAGLE_M0_MEMORY",
                                 1 << 32, access_cache_type)
        self._data.add_mappings(*self._common_address_mappings)
        # add 1MB of RAM remapped at 0x00
        self._data.add_mappings((0x00000000,
                                 0x00100000,
                                 self._mem_map_cmpts.sram.port))

        from ..io.arch import cortex_m0_io_struct as arm_regs_io_struct
        self._arm_regs_io_map_info = IoStructIOMapInfo(arm_regs_io_struct, None,
                                                       self.info.layout_info)
    @property
    def _common_address_mappings(self):
        return [
            # 1MB of ROM
            (0x18000000, 0x18100000, self._mem_map_cmpts.code.port),
            # 1MB of RAM at actual address
            (0x20000000, 0x20100000, self._mem_map_cmpts.sram.port),
            # Zeagle regs - APB_DUALTIMERS, UART, COEX, HCI_IOP, BT_ANA,
            # plus additional space for new Zealis features in Mora
            (0x30000000, 0x30100000, self._mem_map_cmpts.registers1),
            # Zeagle regs - CONFIG, MPUSS, PP, CE_TMR, CRYPTO, COEX, RCU, MDM
            (0x40000000, 0x40050000, self._mem_map_cmpts.registers2),
            # BT_SYS regs - TAB, PIO, SQIF, SLEEP
            (0x50000000, 0x50400000, self._mem_map_cmpts.fm_core_dtop),
            (0x54000000, 0x54040000, self._mem_map_cmpts.usb1p1),
            (0x58000000, 0x58002000, self._mem_map_cmpts.emulation_fpga),
            (0x5c000000, 0x5c004000, self._mem_map_cmpts.apb_fpga),
            (0x60000000, 0x60003000, self._mem_map_cmpts.registers3),
            # Zeagle registers - GL_TMR
            (0x60100000, 0x60100100, self._mem_map_cmpts.registers4),
            # ARM registers
            (0xE0001000, 0xE000103c, self._mem_map_cmpts.dwt),
            (0xE0002000, 0xE0002018, self._mem_map_cmpts.bpu),
            (0xE000E000, 0xE000ED00, self._mem_map_cmpts.nvic),
            (0xE000ED00, 0xE000F000, self._mem_map_cmpts.debug_control),
            (0xE000FF00, 0xE0100000, self._mem_map_cmpts.rom_table)
        ]

    def _create_trb_map(self):
        trb_map = AddressMap("ZEAGLE_TRB", length=1<<32,
                             cache_type=NullAccessCache,
                             layout_info=self.info.layout_info)

        trb_map.add_mappings(*self._common_address_mappings)

        return trb_map

    @property
    def program_space(self):
        return self.data

    @property
    def name(self):
        """
        name attribute used for jtag muxing
        """
        return "bt"

    @property
    def default_firmware_type(self):
        """
        The type of object to create when ELF/DWARF info unavailable, i.e
        just from SLT
        """
        return BTDefaultZeagleFirmware

    @property
    def firmware_type(self):
        """returns the class for the firmrware"""
        return BTZeagleFirmware

    @property
    def patch_type(self):
        from csr.dev.fw.bt.zeagle_patch import ZeaglePatch
        return ZeaglePatch

    def disable_nap(self):
        """
        Disable nap for the BT M0 processor. To kick the processor out of the
        nap (WFI state) a debug event is raised by momentarly halting and
        un-halting the processor.
        """
        cfg_flags = self.fw.env.globalvars["CFG_Parameters_SYS"]\
                    .Cfg_Sleep_Parameters.Cfg_Flags
        if cfg_flags.value & 0x04:
            iprint("nap is already disabled")
        else:
            cfg_flags.set_value(cfg_flags.value|0x04)
            # raise a debug event by halting and unhalting the processor
            # Ref: B1.5.19 Wait For Interrupt, ARMv6 reference manual
            self.halt()
            self.run()
            if self.is_asleep():
                iprint("Unable to kick the CPU out of sleep")
            else:
                iprint("CPU kicked out of sleep by halting/unhalting")

    def enable_nap(self):
        """
        Enable nap for the BT M0 processor.
        """
        cfg_flags = self.fw.env.globalvars["CFG_Parameters_SYS"]\
                    .Cfg_Sleep_Parameters.Cfg_Flags
        if not cfg_flags.value & 0x04:
            iprint("nap is already enabled")
        else:
            cfg_flags.set_value(cfg_flags.value & 0xFB)

    @property
    def core_commands(self):
        return {"buf_list"  : "self.subsystem.mmu.buf_list",
                "buf_read"  : "self.subsystem.mmu.buf_read",
                "log" : "self.fw.debug_log.generate_decoded_event_report",
                "live_log" : "self.fw.debug_log.live_log",
                "reread_log": "self.fw.debug_log.reread",
                "log_level" : "self.fw.debug_log.log_level",
                "call" : "self.fw.call",
                "fw_ver": "self.fw.fw_ver",
                "patch_ver": "self.fw.patch_ver",
                "stack" : "self.fw.live_stack"}, [AttributeError]
