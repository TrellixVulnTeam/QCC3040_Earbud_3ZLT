############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
QCC514X_QCC304X device
"""
import time
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import NameSpace, construct_lazy_proxy, timeout_clock
from csr.dev.hw.address_space import NullAccessCache
from csr.transport.trb_raw import TrbRaw
from csr.dev.hw.device.base_device import BaseDevice
from .mixins.implements_bt_protocol_stack import ImplementsBTProtocolStack

# The Apps service should start pretty quickly
APPS_SERVICE_START_TIMEOUT = 2.0

CURATOR_BOOT_TIMEOUT = 4.0 # If it takes longer than 4s something is wrong.

class CuratorBootError(RuntimeError):
    """
    Curator took too long to boot
    """

class QCC514X_QCC304XDevice(BaseDevice, ImplementsBTProtocolStack):
    """
    Functionality common to all QCC514X_QCC304X devices
    """
    def __init__(self, chip, transport, access_cache_type=NullAccessCache):
        #pylint: disable=unused-argument
        # Configure significant PCB-level Components
        #
        self.components = NameSpace()
        comps = self.components # shorthand

        # Solder down the oven-ready chip
        #
        comps.chip = chip

        # ...and the debug transport
        comps.transport = transport


    @property
    def name(self):
        return "QCC514X_QCC304X"

    @property
    def chip(self):
        "The sole chip on this device"
        return self.components.chip

    @property
    def chips(self):
        "Iterable of chips on this device, being just the one self.chip"
        return (self.chip, )

    @property
    def transport(self):
        return self.components.transport

    @property
    def trb_raw(self):
        "accessor to the low level (raw) transaction bus object"
        try:
            self._trb_raw
        except AttributeError:
            try:
                self._trb_raw = TrbRaw(self.transport)
            except TypeError:
                #We're evidently not on TRB
                self._trb_raw = None
        return self._trb_raw

    @property
    def lpc_sockets(self):
        raise NotImplementedError()

    HW_RESET_DELAY = 1.00 # Delay to let hardware come out of reset.

    def _wait_for_curator(self, curator_core, report=False,
                          until_powered=False):
        """
        Wait for the Curator to come up.

        Can either wait for the Curator to become fully operational, of only
        until GLOBMAN reports POWERED_ON e.g booted by config file not applied.
        """
        start_time = timeout_clock()

        cur = curator_core

        time.sleep(self.HW_RESET_DELAY) #Delay before polling the Curator

        operational_state = \
            (cur.fw.env.enums["GLOBMAN_STATES"]["GLOBMAN_STATE_OPERATIONAL"])

        powered_on_state = \
            (cur.fw.env.enums["GLOBMAN_STATES"]["GLOBMAN_STATE_POWERED_ON"])

        state_var = cur.fw.env.cus["globman.c"].localvars["globman_state"]
        while (state_var.value != operational_state or
               (time.sleep(0.01) is not None) or
               state_var.value != operational_state):
            if timeout_clock() - start_time > CURATOR_BOOT_TIMEOUT:
                break

        #Unpick the possible results.
        if state_var.value != operational_state:
            if until_powered and state_var.value == powered_on_state:
                if report:
                    iprint ("Curator only powered on after %1.2fs" %
                           (timeout_clock() - start_time))
            else:
                raise CuratorBootError(
                    "Curator boot took longer than %d secs!" %
                    CURATOR_BOOT_TIMEOUT)
        else:
            if report:
                iprint ("Curator became operational in %1.2fs " %
                       (timeout_clock() - start_time))

    @staticmethod
    def _wait_for_apps(curator_core, apps, report=False,
                       check_scheduler=True):
        """
        Wait for the Apps to come up.
        """
        # A bit of belt and braces because occasionally I've seen the Apps fw
        # load fail apparently because the Apps still isn't quite powered at
        # this point
        cur = curator_core
        for i in range(10):
            if cur.fields["CURATOR_SUBSYSTEMS_UP"] & (1<<4):
                if report:
                    iprint("Curator finished powering apps in %1.2fs" % (i / 100.0))
                break
            time.sleep(0.01)

        # Wait whilst the Curator finishes AutoStarting the Apps otherwise
        # the Curator may 'restart' the application after we've stopped it.
        time.sleep(APPS_SERVICE_START_TIMEOUT)

        if check_scheduler:
            # Wait for the kalimba scheduler to reach RUNLEVEL_FINAL
            apps.fw.sched.wait_for_runlevel(2, APPS_SERVICE_START_TIMEOUT)

    def apps_go(self, p0_prog=None, p1_prog=None,
                trace_boot=False, verbose=False, curator_from_rom=False):
                #pylint: disable=unused-argument,too-many-arguments
        '''
        Perform the actions needed to run the code on the apps subsystem
        firmware on the lab board or the HAPS7 emulator
        '''
        # Use default memories for this platform
        if p0_prog is None:
            p0_prog = "sqif0"
        if p1_prog is None:
            p1_prog = "sqif0"

        if p0_prog == "rom" or p1_prog == "rom":
            raise ValueError("%s has no ROM emulation!" % self.title)

        cur = self.chip.curator_subsystem.core
        p0 = self.chip.apps_subsystem.cores[0]
        p1 = self.chip.apps_subsystem.cores[1]

        self.reset()

        # We wait for the Curator here because once it is operational we can be
        # sure it has read its MIB and therefore has the Apps powered, which is
        # obviously a prerequisite for what follows
        self._wait_for_curator(cur, True)
        self._wait_for_apps(cur, p0, True, False)

        # Test if we have an elf file for P1
        have_p1 = (construct_lazy_proxy(p1.fw.env) is not None)

        # If we are connected remotely do not load the fw, as it takes too long.
        if not self.transport.is_remote:
            self.try_command("apps0.fw.load('%s')" % p0_prog,
                             p0.fw.load, p0_prog)
            if have_p1:
                self.try_command("apps1.fw.load('%s')" % p1_prog,
                                 p1.fw.load, p1_prog)

        # Reset again to get back into an operational state.
        self.reset()

        # We wait for the Curator here because once it is operational we can be
        # sure it has read its MIB and therefore has the Apps powered, which is
        # obviously a prerequisite for what follows
        self._wait_for_curator(cur, True)
        self._wait_for_apps(cur, p0, True, True)

        cur.fw_ver()
        p0.fw_ver()

        if have_p1:
            #The application should already have done this..
            p1.run()
            self._wait_for_apps(cur, p1, True, False)
            p1.fw_ver()

    def go(self):
        """
        Start the Apps with default parameters
        """
        self.apps_go()

    @property
    def dap(self):
        "accessor to the ARM DAP debug connector object"
        return self.chip.dap

    @property
    def gdbserver_mux(self):
        return self.chip.gdbserver_mux

    def try_command(self, message, command, *args):
        """
        Execute command using try/except up to 3 times if it is failing and
        keep list of errors to report
        """
        i = 0
        exception = True
        self.total_errors = []
        while i < 3 and exception:
            try:
                if i > 0:
                    iprint("Attempt %i: %s)" % (i + 1, message))
                    self.reset()
                command(*args)
                exception = False
            except Exception as error: # pylint:disable=broad-except
                iprint("FAILED TO EXECUTE '%s'" % message)
                iprint(error)
                exception = True
                self.total_errors.append("Error: %s" % message)
            i = i + 1