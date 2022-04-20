############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import time
import sys
import os
from collections import OrderedDict

from csr.wheels.global_streams import iprint
from csr.dev.fw.firmware import GenericHydraFirmware, DefaultFirmware
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.fw.debug_log import DebugLogReader, HydraLog, PerModuleHydraLog, GlobalHydraLog, ClassicHydraLog, GenericHydraLogDecoder
from csr.dev.hw.address_space import AddressSpace
from csr.dev.fw.slt import AudioBaseSLT, AudioSLTNotImplemented,  AudioFakeSLT
from csr.dev.fw.stack_unwinder import K32StackUnwinder
from csr.dev.fw.stack import AudioStack
from csr.dev.fw.pmalloc import AudioP0Pmalloc, AudioP1Pmalloc
from csr.dev.fw.heap import AudioHeap

# long lines code for pylint
# pylint: disable=C0301

class AudioDefaultFirmware(DefaultFirmware):
    """
    Default firmware object, providing information derived from the SLT.

    Functionality in this class is inherited by the "real" firmware object
    which adds all the ELF/DWARF-based functionality.
    """

    def create_slt(self):
        # Current firmware uses baseline SLT.
        # Change this to specific SLT if/when SLT gets extended.
        try:
            return AudioBaseSLT.generate(self._core)
        except AddressSpace.NoAccess:
            if hasattr (self._core, "dump_build_id") and \
                hasattr(self._core, "dump_build_string"):
                return AudioFakeSLT(self._core.dump_build_id,
                    self._core.dump_build_string)
            else:
                return AudioSLTNotImplemented()

class AudioFirmware(GenericHydraFirmware, AudioDefaultFirmware):
    """
    Audio Firmware Instance
    """

    def __init__(self, fw_env, curator_core, fw_build_id=None,
                 fw_build_str=None, build_info=None):

        GenericHydraFirmware.__init__(self, fw_env, curator_core,
                                      build_info=build_info)

        self.device = None
        self.curator = None
        self.audio = None
        self.apps = None

    @property
    def stack(self):
        """
        Unwind the stack using the Kalimba 32 unwinder
        """
        return self._stack()

    def _stack(self, **kwargs):
        return K32StackUnwinder(self.env, self._core).bt(**kwargs)

    def _create_debug_log(self):
        # Potential extension:: autodetect variants
        try:
            return FirmwareComponent.create_component_variant((GlobalHydraLog,
                                                               PerModuleHydraLog,
                                                               ClassicHydraLog),
                                                              self.env,
                                                              self._core,
                                                              parent = self)
        except FirmwareComponent.NotDetected:
            #If we don't have enough info to look at structures, we can't
            #support log levels
            return HydraLog(self.env, self._core, self)

    def _create_debug_log_decoder(self):
        return GenericHydraLogDecoder(self)

    # create_slt inherited from AudioDefaultFirmware

    def _get_debug_buffer_name(self):
        if self._core.nicknames[0] == 'audio1':
            return "debugBuffer1"
        else:
            return "debugBuffer"

    def test_shallow_sleep(self, iterations=10):
        """
            Setup accmd comms and then monitor the NUM_INSTRS and NUM_RUN_CLKS for a number of seconds.
            You need to manualy send down accds (through omnicli) and watch the registers change values.
        """
        self.test_accmd(1)
        for dummy_i in range(0, iterations):
            num_instrs = self._core.fields["NUM_INSTRS"]
            num_run_clks = self._core.fields["NUM_RUN_CLKS"]
            time.sleep(2)
            num_instrs_new = self._core.fields["NUM_INSTRS"]
            num_run_clks_new = self._core.fields["NUM_RUN_CLKS"]
            iprint("instrs dif = {instr_dif}  run_clks dif = {clk_dif}".format(
                instr_dif = num_instrs_new - num_instrs,
                clk_dif = num_run_clks_new - num_run_clks))

    @property
    def stack_model(self):
        try:
            self._stack_model
        except AttributeError:
            self._stack_model = AudioStack(self.env, self._core)
        return self._stack_model

    @property
    def heap(self):
        try:
            self._heap
        except AttributeError:
            self._heap = AudioHeap(self.env, self._core)
        return self._heap

    def _all_subcomponents(self):
        comps = OrderedDict(GenericHydraFirmware._all_subcomponents(self))
        comps.update([("stack_model", "_stack_model")])
        return comps

def kalimba_load_available_check():
    """
        Checks if kalimba_load is available or not
    """
    try:
        # Dont freak out if kalimba lab is not there. Just report it.
        scriptpath = r"c:\KalimbaLab21c\pythontools"
        sys.path.append(os.path.abspath(scriptpath))
        import kalimba_load
        return True
    except ImportError:
        iprint("Warning, kalimba python tools not available")
        return False

class AudioP0Firmware (AudioFirmware):

    def __init__(self, fw_env, curator_core, fw_build_id=None,
                 fw_build_str=None, build_info=None):

        AudioFirmware.__init__(self, fw_env, curator_core,
                               build_info=build_info)

        self._stack_sym = 'MEM_MAP_P0_STACK'

    def _all_subcomponents(self):
        # Potential extension:: Currently we claim that only AudioP0 has a heap.
        # This makes our life easier for reporting because P0 always runs
        # and so we can read its symbols
        cmps = AudioFirmware._all_subcomponents(self)
        cmps.update({"heap" : "_heap"})
        return cmps

    @property
    def pmalloc(self):
        # Construct lazily...
        try:
            self._pmalloc
        except AttributeError:
            self._pmalloc = AudioP0Pmalloc(self.env, self._core)

        return self._pmalloc

    @property
    def _p1_is_running(self):
        # This is nasty because we are reaching from P0 into P1
        # If the stack hasn't been set up, we know we aren't running P1 code
        return self._core._subsystem.cores[1].fw.stack_model._fw_running

    def _generate_memory_report_component(self):
        """
        Returns the report for fundamental AudioFirmware memory areas like initc and bss.
        """

        total_audio_ram = {"name":"DATA_RAM",
        "start":0x0,
        "end":self._core.subsystem.dm_total_size,
        "size":self._core.subsystem.dm_total_size}

        dm1_initc = self._core.sym_get_range("MEM_MAP_DM1_INITC")
        dm1_bss = self._core.sym_get_range("MEM_MAP_DM1_BSS")
        dm_guard = self._core.sym_get_range("MEM_MAP_DM_GUARD")

        dm1_p0_initc = self._core.sym_get_range("MEM_MAP_DM1_P0_INITC")
        dm1_p0_bss = self._core.sym_get_range("MEM_MAP_DM1_P0_BSS")

        p0_preserve = self._core.sym_get_range("MEM_P0_PRESERVE")

        dm2_initc = self._core.sym_get_range("MEM_MAP_DM2_INITC")
        dm2_bss = self._core.sym_get_range("MEM_MAP_DM2_BSS")

        return [total_audio_ram,
                dm1_initc, dm1_bss, dm_guard,
                dm1_p0_initc, dm1_p0_bss, p0_preserve,
                dm2_initc, dm2_bss]

class AudioP1Firmware (AudioFirmware):

    def __init__(self, fw_env, curator_core, fw_build_id=None,
                 fw_build_str=None, build_info=None):

        AudioFirmware.__init__(self, fw_env, curator_core,
                               build_info=build_info)

        self._stack_sym = 'MEM_MAP_P1_STACK'

    @property
    def pmalloc(self):
        # Construct lazily...
        try:
            self._pmalloc
        except AttributeError:
            self._pmalloc = AudioP1Pmalloc(self.env, self._core)

        return self._pmalloc

    def _generate_memory_report_component(self):

        dm1_p1_private_start = self._core.sym_get_value("DM1_P0_PRIVATE_MEMORY_END_ADDRESS")
        dm1_p1_private_end = self._core.sym_get_value("DM1_P1_PRIVATE_MEMORY_END_ADDRESS")

        dm1_p1_private = {"name":"DM1_P1_PRIVATE_MEMORY",
                          "start":dm1_p1_private_start,
                          "end":dm1_p1_private_end,
                          "size":dm1_p1_private_end - dm1_p1_private_start}

        p1_preserve = self._core.sym_get_range("MEM_P1_PRESERVE")
        return [dm1_p1_private, p1_preserve]
