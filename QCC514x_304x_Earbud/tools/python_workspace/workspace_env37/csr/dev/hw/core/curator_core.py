############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from contextlib import contextmanager
from csr.wheels.global_streams import iprint
from csr.dev.hw.core.mixin.is_xap import IsXAP
from csr.dev.hw.core.mixin.is_in_hydra import IsInHydra
from csr.dev.hw.core.base_core import BaseCore
from csr.dev.hw.core.xap_core import XAPCore
from csr.dev.hw.register_field.register_field import AdHocBitField
from csr.dev.hw.generic_window import CuratorGenericWindow
from csr.dev.model import interface
import time
import sys

# Py2/3 compatibility as raw_input is replaced with input in Py3
input_func = raw_input if sys.version_info < (3,) else input


class CuratorCore (IsXAP, IsInHydra, XAPCore):
    """
    Common Base for Curator cores.
    """
    def __init__(self, subsystem):

        IsXAP.__init__(self)
        IsInHydra.__init__(self, subsystem)
        BaseCore.__init__(self)

    def populate(self, access_cache_type):

        IsXAP.populate(self, access_cache_type)

    @contextmanager
    def ensure_apps_powered(self):
        '''
        @deprecated - name changed to ensure_apps_clocked because it ensures
        it is both powered and clocked.
        '''
        import warnings
        warnings.warn(
            "cur.ensure_apps_powered() is deprecated",
            DeprecationWarning, stacklevel=2)
        with self.ensure_apps_clocked():
            yield

    @contextmanager
    def ensure_apps_clocked(self):
        """
        As a context manager, can be used 'with' a block of code that
        needs to ensure subsystem is powered and clocked for an operation
        and wants to restore it to original state afterwards.
        """
        with self.subsystem.chip.apps_subsystem.ensure_clocked():
            yield

    # BaseCore compliance

    nicknames = ("cur", "curator")

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
            #SLT
            'fw_ver'    : "self.fw.fw_ver",
            'patch_ver'    : "self.fw.patch_ver",
            #Other
            'display_brk' : "self.brk_display",
            'show_reset': "self.show_reset_report",
            'show_pmu'  : "self.show_pmu_report"
            }
        # Commands that might not be supported because they rely on the
        # ELF/DWARF.
        core_fw_cmds = {
        #Logging
        'log'       : "self.fw.debug_log.generate_decoded_event_report",
        'live_log'  : "self.fw.debug_log.live_log",
        'trb_live_log' : "self.fw.debug_log.trb_live_log",
        'clear_log' : "self.fw.debug_log.clear",
        'reread_log': "self.fw.debug_log.reread",
        'log_level' : "self.fw.debug_log.log_level",
        #Other
        'stack'     : "self.fw.stack_report"
        }
        core_cmds.update(core_fw_cmds)

        core_xide_cmds = {
            #Symbol lookup
            'disp'      : "self.disp_report",
            'psym'      : "self.sym_.psym",
            'dsym'      : "self.sym_.dsym",
            'dispsym'   : "self.sym_.dispsym",
            'sym'       : "self.sym_.sym",
            'struct'    : "self.fw.env.struct",
        }
        core_cmds.update(core_xide_cmds)

        core_cucmd_cmds = {
            #Other
            'cucmd_start_service'     : "self.fw.cucmd.start_service_",
            'cucmd_start_bt_service'  : "self.fw.cucmd.start_bt_service",
            'cucmd_stop_service'      : "self.fw.cucmd.stop_service",
            'cucmd_set_mibs_from_file': "self.fw.cucmd.set_mibs_from_file"
        }
        core_cmds.update(core_cucmd_cmds)

        #MIB access
        core_fw_cmds['mib_dump'] = "self.fw.mib.dump"
        core_cmds.update(core_fw_cmds)

        return core_cmds, [AttributeError]

    # Alias that used to be installed via core_commands
    @property
    def mem(self):
        return self.mem_report

    # Extensions

    @property
    def num_efuse_banks(self):
        """
        Number of efuse banks defined in curator core.
        """
        raise PureVirtualError

    def halt_chip(self):
        """\
        Bring the whole chip to a halt.

        Normally called indirectly via HydraChip.halt()
        """
        
        #Do a debugger access to bring the chip out of deep sleep.
        chip_id = self.regs.SUB_SYS_CHIP_VERSION
        
        #Don't halt the Curator until it has had chance to exit SQIFs from DPD
        time.sleep(0.02)
        
        # Halt the curator first so it can't interfere with attempt
        # to halt the other subsystems!
        #
        self.pause()

        # Now halt the rest of the subsystems.
        #
        # Is this actually enough to stop all io hardware in its tracks?
        #
        self.regs.CURATOR_SUBSYSTEMS_RUN_EN = 0

    def reset_chip(self):
        """\
        Reset the whole chip.

        N.B. The core/janitor may take a while (~5mS) to recover enough for
        SPI (or other) access after reset.

        Normally called indirectly via HydraChip.reset()
        """
        # The docs say write 1 to reset, in practice it seems any value
        # will do.
        #
        # Don't be tempted to to write 0 afterwards or you'll reset it
        # again - and in any case the core may take a while to recover enough
        # for SPI access.
        #
        self.fields["DBG_RESET"] = 1

    def show_reset_report(self):

        sched_flags = self.fw.env.cus["sched.c"].localvars["sched_flags"]
        rl = sched_flags["current_runlevel"].value
        grp = interface.Group("Reset")
        grp.append(interface.Code("Current runlevel is %d\n" % rl))
        try:
            reset_data = self.fw.env.globalvars["reset_data"]
            rs = reset_data["reset_status"].value
            grp.append(interface.Code("Reset state recorded in software (inferred during boot):"))
            grp.append(self.bitz("MILDRED_PBR_RESET_STATUS", value=rs, report=True))
        except KeyError:
            # Reset data not present
            # Check is for backwards compatibility with older Curator firmware
            pass

        grp.append(interface.Code("Hardware reset state (does not show "
                                  "post-boot-reset bits):"))
        grp.append(interface.Code(self.fields.MILDRED_PBR_RESET_STATUS.__repr__()))
        return grp

    def show_pmu_report(self):
        """Override this function for each chip variant with a PMU.
        There is no error if it is not overridden as Curator variants
        may not have a PMU."""
        return interface.Code("This chip variant has no PMU.")

    def is_subsystem_up(self, ss):
        """\
        Is the specified subsystem "up".

        Normally called indirectly via HydraSubsystem.is_up()
        """
        ss_up_field = self._infer_ss_field("CURATOR_SUBSYSTEMS_UP", ss)
        return ss_up_field.read() == 1

    def set_subsystem_power(self, ss, on_not_off = True):
        """\
        Set power to specified subsystem.
        Can take a while to stabilise. See is_power_stable()

        Normally called indirectly via HydraSubsystem.set_power()
        """
        ss_pwr_field = self._infer_ss_field("CURATOR_SUBSYSTEMS_POWERED", ss)
        ss_pwr_field.write(1 if on_not_off else 0)

    # Protected / IsXap compliance

    def _create_generic_window(self, name, access_cache_type):
        # So far one size does for all derived curator cores.
        gw = CuratorGenericWindow(self, name)
        gw.populate(access_cache_type)
        return gw

#    def _create_extra_gw_connections(self, gw):
#        return (
#            AddressConnection(gw.trb_port, trb_map),
#        )


    @property
    def _hif_subsystem_view(self):

        try:
            return self.subsystem.hif
        except AttributeError:
            return None


    def _generate_report_body_elements(self):
        pc = interface.Group("Program counter")
        pc.append(self.pc_report())
        reset = interface.Group("Reset state")
        reset.append(self.show_reset_report())
        pmu = interface.Group("PMU state")
        pmu.append(self.show_pmu_report())

        return [pc, reset, pmu]

    # Private

    def _infer_ss_field(self, reg_name, ss):
        """\
        Infer reference to bit Field in specified curator control register
        applicable to the specified subsystem.
        """
        # There are no symbols for these fields (or are there now?)
        # so hand-roll... The bit posn == the subsystem id.
        #
        ss_reg = self.field_refs[reg_name]
        ss_bit = ss.id
        bit_field = AdHocBitField(self.data,
                                  self._info.layout_info,
                                  ss_reg.start_addr,
                                  ss_bit,
                                  1,
                                  ss_reg.is_writeable)

        return bit_field

    def is_subsystem_running(self, subsys_id):
        """
        By checking subserv_data to decide if the queried subsystem is fully
        booted up and running.
        """
        running_subsys = self.list_running_subsystems()
        return (subsys_id in running_subsys)

    @property
    def _is_running_from_rom(self):
        """
        Is the core configured to fetch code from ROM or SQIF/LPC?
        """
        # This is a default implementation that works on all chips at the
        # moment. It can be overriden in subclasses.

        try:
            half = self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_SQIF_LOW_ROM
        except AttributeError:
            half = self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_LPC_LOW_ROM

        return (self.bitfields.NV_MEM_ADDR_MAP_CFG_STATUS_ORDER.read()
                in (half, self.iodefs.NV_MEM_ADDR_MAP_CFG_HIGH_ROM_LOW_ROM))

    def get_patch_id(self, env=None):
        """
        Patch ID retrieval method.  If env is supplied, use that; otherwise
        look for the env attached to this core
        """
        if env is None:
            env = self.fw.env

        # B-256662: Test if we can read the patch fw version from the FS.
        # If false we are likely running a HTOL build and we wish
        # to continue as normal.
        try:
            env.globalvars["patched_fw_version"]
        except KeyError:
            return None

        return env.globalvars["patched_fw_version"].value

    def suppress_tc_logging(self):
        """
        While using tctrans, set the log suppression flag so the
        Curator log remains sane.

        During each log fetch a ToolCmd is generated which adds a log entry
        which is then fetched and this circular process continues until the
        entire log buffer is full of garbage.

        See B-271477.
        """
        try:
            self.fw.env.var.hydra_log_suppressed.set_value(1)
        except AttributeError:
            return

    def poll_rx(self):
        '''
        To be called after sending a toolcmd. Will loop until toolcmd.poll_for_rx()
        returns a response PDU with an ID of "efuse_write_rsp" and then return True
        if the result field equals 0 (success).
        '''
        rsp = False
        while not rsp:
            rx = self.subsystem.chip.toolcmd.poll_for_rx()
            if rx is not None:
                rsp = rx['id'] == "efuse_write_rsp"
        return rx['payload'][0] & 0xff == 0
