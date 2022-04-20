############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2022 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import time

from csr.dev.hw.address_space import AddressMap, AddressSpace
from csr.dev.hw.core.apps_p0_core import AppsCore
from csr.wheels import timeout_clock, iprint, wprint
from csr.dev.fw.apps_firmware import AppsP1Firmware, AppsP1DefaultFirmware
from csr.dev.fw.adk.libs.adk_libs import ADKLibs
from csr.dev.fw.adk.caa.caa_app import CAAApp


class AppsP1Core(AppsCore):
    
    def __init__(self, subsystem, access_cache_type, p0_access_flag = None):
        '''
        Create the fundamental memory blocks
        '''
        AppsCore.__init__(self, subsystem)
        self.processor_number = 1
        self._program_memory = AddressMap("P1_PROGRAM_MEMORY", access_cache_type, 
                                    length= 0x00810000, word_bits=8)

        # There may be access restrictions on what p1 can see/modify in p0
        self._p0_access_flag = p0_access_flag

    nicknames = ("apps1",)
    
    @property
    def firmware_type(self):
        return AppsP1Firmware

    @property
    def default_firmware_type(self):
        return AppsP1DefaultFirmware

    def extra_firmware_layers(self, plugins):
        extra_layers_dict = {"libs" : ADKLibs}
        plugins = plugins.get_plugins()
        if plugins:
            extra_layers_dict.update(plugins)
        else:
            # Default to the CAA App if no plug-in provided.  It will self-detect
            # so we won't accidentally get it when running against stub firmware,
            # for instance.
            extra_layers_dict.update({"app" : CAAApp})
        return extra_layers_dict

    @property
    def core_commands(self):
        '''
        Dictionary of commands (or other objects) you want to be registered
        as globals in interactive shells for this core.
        '''
        common_core_cmds, exception_list = self._common_core_commands()

        p1_only_commands = {
            'libs_ver'      : "self.fw.slt.libs_ver",
            # Application message logging
            'trap_log'          : "self.fw.trap_log.generate_decoded_event_report",
            'trap_live_log'     : "self.fw.trap_log.live_log",
            'trap_log_xml'      : "self.fw.trap_log.generate_decoded_event_report_xml",
            'trap_live_log_xml' : "self.fw.trap_log.live_log_xml",
            }

        commands_dict = dict(list(p1_only_commands.items()) +
                             list(common_core_cmds.items()))

        return commands_dict, exception_list


    @property
    def firmware_build_info_type(self):
        from csr.dev.fw.meta.i_firmware_build_info import HydraAppsP1FirmwareBuildInfo
        return HydraAppsP1FirmwareBuildInfo

    def wait_for_memory_access(self, timeout=5):
        """
        Keep trying to read the first byte of P1 RAM until it succeeds, with the
        supplied timeout.
        """
        t0 = timeout_clock()
        while True:
            try:
                self.data[self.P1_DATA_RAM_START]
            except AddressSpace.ReadFailure:
                if timeout_clock() - t0 > timeout:
                    # One more try
                    try:
                        self.data[self.P1_DATA_RAM_START]
                    except AddressSpace.ReadFailure:
                        raise P1ReadinessTimeout("Couldn't read RAM "
                                "successfully after {} secs", timeout)
            else:
                break

    def wait_for_sched_running(self, timeout=5):
        """
        If the sched class is available, run sched.wait_for_runlevel(1) to wait until
        the scheduler is up. (Note: the runlevel doesn't meaning anything on 
        FreeRTOS.
        """
        self.wait_for_memory_access()
        try:
            sched = self.fw.sched
        except AttributeError:
            wprint("No P1 firmware supplied: can't check for sched running state")
            return

        sched.wait_for_runlevel(1, quiet=True)


    def disallow_dormant(self, quiet=False):
        """
        Invoke the firmware function that prevents the application going into Dormant
        mode.  Returns with a warning if this wasn't possible
        """
        # special magic to keep the buds awake
        if not quiet:
            iprint("Disabling dormant mode")
        try:
            self.fw.call
        except AttributeError:
            if not quiet:
                wprint("WARNING: can't disable dormant because no Apps1 ELF supplied")
            return False

        try:
            self.fw.call.appTestPowerAllowDormant
        except AttributeError:
            if not quiet:
                wprint("Can't disable dormant because the supplied firmware doesn't contain the "
                        "expected function 'appTestPowerAllowDormant'")
            return False

        # Now wait until we can see that the processor is running
        self.wait_for_sched_running()
        
        # Wait a bit longer to give the App a chance to initialise the dormant state, so we have 
        # a better chance of not coming in too early and having our settings clobbered by the
        # initialisation sequence.
        time.sleep(0.5)

        old_timeout = self.fw.call.set_timeout(1)
        self.fw.call.appTestPowerAllowDormant(False)
        self.fw.call.set_timeout(old_timeout)
