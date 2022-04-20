############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""\
Device Factory and friends.

As well as the "pure" device model factory this contains some
application helpers that attach to devices based on certain
url conventions and includes device auto-detection logic
implied by some of the conventions.

Future:-
- This helper stuff probably wants a home of its own when
have time to think about it more deeply.
"""
import os
import re
from csr.wheels.global_streams import iprint
from csr.wheels import TypeCheck
from csr.dev.hw.chip_version import ChipVersion, JTAGVersion
from csr.dev.hw.address_space import NullAccessCache, PassiveAccessCache, ExtremeAccessCache
from csr.dev.hw.chip.chip_factory import ChipFactory
from .dynamic_factory.get_plugins import get_factory_plugins

class DeviceFactory(object):
    
    @staticmethod
    def create(chip_version, transport, access_cache_type, hint=None,
               emulator_build=None):
        
        # Find modules in this package that export a CHIP_MAJOR_VERSION attribute
        chip = ChipFactory.fry(chip_version, access_cache_type)
        factory_plugins = get_factory_plugins()
        try:
            factory = factory_plugins[chip_version.major]
        except KeyError:
            raise NotImplementedError("Unsupported chip version %s" % chip_version)
        else:
            device = factory(chip_version, chip, transport, access_cache_type)
        
        # Give each chip a reference to its device
        for c in device.chips:
            c.set_device(device)

        return device




def _attach_via_trbtrans(path, hint=None, recovery=False,
                         emulator_build=None, target=None):

    from csr.dev.framework.connection.trb import TrbTransConnection
    from csr.transport.trbtrans import TrbError, TrbErrorCouldNotEnumerateDevice
    try:
        debug_connection = TrbTransConnection.create_connection_from_url(
            path)
    except TrbErrorCouldNotEnumerateDevice as exc: #No debug dongles found
        iprint(str(exc) +
              ": the debugger has no attached device,"
              " consider using the command line option -d sim:chipid.")
        raise exc
    try:
        if target:
            try:
                target = int(target,16)
            except ValueError:
                raise ValueError("If using target option with TRB, you must "
                                 "specify a hex number representing the target "
                                 "chip version ID")
        chip_version = ChipVersion(target or debug_connection.get_chip_id())

        if chip_version.raw != 0 or not recovery:
            device = DeviceFactory.create(chip_version, debug_connection, 
                                          NullAccessCache, hint=hint,
                               emulator_build=emulator_build)
    
            trb_space = device.trb
            debug_connection.connect(trb_space)
        else:
            iprint("Couldn't get chip ID: dongle register read returned 0")
            device = None
    except TrbError as e:
        if not recovery:
            raise TrbError(e)
        else:
            iprint("Couldn't get chip ID: %s" % e)
            device = None

    if device is None:
        iprint("No device model created. Call reattach() after manual recovery.")

    return device, debug_connection


def _attach_via_xcd3(xcd3_path, hint=None, emulator_build=None):
    """\
    Construct a Device model with no physical connections and load the
    specified xcd3 file into its caches for post-mortem analysis.
    """
    from os import path
    from csr.dev.hw.device.xcd3_importer import XCD3Importer

    if xcd3_path != '-':
        xcd3_path  = path.expanduser(xcd3_path)
        xcd3_path  = path.abspath(xcd3_path)

    return XCD3Importer().import_device(xcd3_path, 
                                        emulator_build=emulator_build)


def _attach_via_xed(path, hint=None, emulator_build=None):

    from csr.dev.hw.device.xed_importer import XEDImporter

    return XEDImporter().import_device(path)


def _attach_for_simulation(sim_path, emulator_build=None, load_defaults=True):
    """
    Create a device model for the specified chip, loading the address map
    caches with register reset values.
    """
    chip_version = ChipVersion(int(sim_path,16))
    device = DeviceFactory.create(chip_version, None, PassiveAccessCache,
                                  emulator_build=emulator_build)
    if load_defaults:
        device.load_register_defaults()
    return device


def _attach_via_low_cost_debug(device_id,
                               emulator_build=None, transport="usbdbg"):
    from csr.dev.framework.connection.low_cost_debug import LowCostDebugConnection
    
    debug_connection = LowCostDebugConnection(dongle_id=device_id, transport=transport)
    chip_version = ChipVersion(debug_connection.get_chip_id())
    device = DeviceFactory.create(chip_version, debug_connection, 
                                  NullAccessCache,
                                  emulator_build=emulator_build)
    debug_connection.connect(device.chip)
    return device, debug_connection


def _attach_via_socket(path, target=None):
    from csr.dev.framework.connection.socket_connection import SocketConnection
    debug_connection = SocketConnection(path)
    debug_connection.open()
    chip_version = ChipVersion(debug_connection.get_chip_id() or target)
    device = DeviceFactory.create(chip_version, debug_connection,
                                  NullAccessCache, emulator_build=None)
    debug_connection.connect(device.chip)
    return device, debug_connection


def _attach_via_jlink(jlink_params, io_struct=None, dongle_manager=None):
    
    from csr.dev.framework.connection.jlink_connection import JLinkConnection
    # Capture any options that have been specified on the command line.
    jlink_params_dict = {}
    if jlink_params is not None:
        for jlink_param in jlink_params.split(";"):
            try:
                key, value = jlink_param.split(":", 1)
            except ValueError:
                raise ValueError("Couldn't parse '{}'".format(jlink_param))
            jlink_params_dict[key] = value

    from csr.dev.hw.device.generic_device import GenericDevice
    jtag_version = get_chip_version_via_jlink(jlink_params_dict)
    
    chip = ChipFactory.bake(jtag_version.partno, NullAccessCache,
                            io_struct=io_struct)

    debug_connection = JLinkConnection(dongle_mgr=dongle_manager, **jlink_params_dict)
    
    device = GenericDevice(chip, debug_connection)
    chip.set_device(device)

    onboard_debug = device.debug_access
    debug_connection.connect(onboard_debug, chip=chip)
    
    return device, debug_connection



def get_chip_version_via_jlink(jlink_params):

    from csr.transport.jlink import JLinkTransportAPI
    from csr.dev.hw.chip.mixins.has_jtag_swd_debug_trans import JTAGSWDConnectionManager
    
    if jlink_params is None:
        jlink_params = {}
    
    jlink_api = JLinkTransportAPI(s_dllpath=jlink_params.get("path",
                                            os.getenv("PYDBG_JLINK_PATH")))
    # The JTAGSWDConnectionManager wants a debug_port object to be able to query about the type
    # of connection being asked for.  We just provide a dummy one here to ensure we get a DAP
    # connection. 
    class DummyDebugPort:
        def get_conn_type(self, target_sel):
            return "dap"
    jlink_api.open(serial_no=jlink_params.get("sn"),
                   ip_addr=jlink_params.get("ip"))
    conn_manager = JTAGSWDConnectionManager(DummyDebugPort(), jlink_api, "jtag", verbose=-1)
    dap_connection = conn_manager.activate_conn(None)
    jlink = dap_connection.get_transport()
    vers = jlink.get_chip_version()
    jlink_api.close()
    if vers is None:
        return None
    return JTAGVersion(*vers)


class DeviceAttacher (object):
    """
    Attaches to a device based on various url schemes.

    Future:- - Explicit Configurations and or Smarter autodetect Configurations
    can get much more complicated. E.g. Consider 2 identical devices attached
    via 2xspi, 2xsdio, 2xuart + 2xtrb. That will require smarter autodetection
    or explicit/manual configuration descriptors. Tricks like writing a magic#
    via 1 connection and reading via others comes to mind - maybe we can even
    find unique chip id in efuse.
    """

    @staticmethod
    def attach(device_url, xide_mem = None, hint=None, recovery=False,
               target=None, emulator_build=None, dongle_manager=None):
        """\
        Attach to a Device according to url scheme.

        Returns the Device model and transport as a tuple.

        Params:

        - device_url: Specifies the device. Schemes supported:-
            xcd3: Autodetect and Load state from xcd3
            trb: Use the transaction bridge as the transport to a live chip
        - dongle manager: The DongleManager class instance that manages
            the connections for some transport schemes.

        """
        if device_url.endswith(".xcd") and os.path.isfile(device_url):
            # Quick workaround to let you specify coredumps with no explicit
            # scheme specifier
            scheme, path = "xcd3", device_url
        elif device_url.endswith(".xed") and os.path.isfile(device_url):
            # Accept XED files. These are debug partitions specifying debug events.
            scheme, path = "xed", device_url
        else:
            try:
                # Only split on the first colon so that Windows drives don't break
                # everything
                scheme, path = device_url.split(':',1)
            except ValueError:
                # If there are no meaningful arguments to the scheme, we don't
                # want to require a trailing colon.
                scheme = device_url
                path = None
        transport = None
        if scheme == "trb" or scheme == "tbr":
            device,transport = _attach_via_trbtrans(path,
                                                        hint=hint,
                                                        recovery=recovery,
                                                        emulator_build=emulator_build,
                                                        target=target)
        elif scheme in ("lcd","usb","usb2tc", "tc", "usbcc", "btoip"):
            if scheme in ("tc",):
                # deal with the tc:usb2tc:<dongle_id> form passed down by QMDE
                scheme, path = path.split(":",1)
                if scheme != "usb2tc":
                    raise ValueError("Unknown subscheme '%s' of tc" % scheme)

            transport = scheme if scheme in ("usbcc", "btoip") else "usb2tc"
            device,transport = _attach_via_low_cost_debug(path,
                                   emulator_build=emulator_build,
                                   transport=transport)
        elif scheme == "xcd3":
            device = _attach_via_xcd3(path, hint=hint, emulator_build=emulator_build)
        elif scheme in ("sim", "fastsim"):
            device = _attach_for_simulation(path,
                                   emulator_build=emulator_build, load_defaults=(scheme!="fastsim"))
        elif scheme == "xed":  # Debug Partition
            # If we are parsing an xed file we are looking at a debug partition.
            # This contains a number of debug events which we want to represent as
            # separate devices.
            device = _attach_via_xed(path)
        elif scheme in ("socket", "skt", "ebd", "earbud"):
            device, transport = _attach_via_socket(path)
        elif scheme == "jlink":
            device, transport = _attach_via_jlink(path, io_struct=emulator_build, 
                                                  dongle_manager=dongle_manager)
        else:
            raise ValueError("Unknown URL scheme: '%s'" % scheme)

        return device, transport


    @staticmethod
    def _attach_via_socket(path, target=None):
        from csr.dev.framework.connection.socket_connection import SocketConnection
        debug_connection = SocketConnection(path)
        debug_connection.open()
        chip_version = ChipVersion(debug_connection.get_chip_id() or target)
        device = DeviceFactory.create(chip_version, debug_connection,
                                      NullAccessCache, emulator_build=None)
        debug_connection.connect(device.chip)
        return device, debug_connection

    @classmethod
    def checked_attach(cls, firmware_builds, device_url, allow_recovery, 
                       target, emulator_build=None, dongle_manager=None):
        '''
        Create device and transport objects from supplied firmware builds
        for the supplied device_url.
        Checks device is created and raises IOError if not.
        '''
        hint = None
        if firmware_builds is not None:
            # We search for the following substrings in the build path.  Note
            # that the order is important, as "dev" is likely to appear for
            # other reasons, whereas d00 and d01 aren't.  We may get false
            # positives for "dev" therefore, but only if there is no firmware
            # specified that indicates digital revisions

            for version in ("d00", "d01", "dev"):
                if version in firmware_builds:
                    hint = version
                    break

        # Could log device_url but log object not supplied as parameter.
        _device, _trans = cls.attach(device_url, hint=hint,
                                     recovery=allow_recovery,
                                     target=target,
                                     emulator_build=emulator_build,
                                     dongle_manager=dongle_manager)
        if _device is None:
            iprint("Device attach failed.")
        else:
            try:
                _trans.get_freq_mhz
            except AttributeError:
                iprint("Device attach succeeded.")
            else:
                frequency = _trans.get_freq_mhz()
                if frequency != NotImplemented:
                    iprint("Device attach succeeded. "
                          "Using TRB clock speed: {}MHz.".format(frequency))
                else:
                    iprint("Device attach succeeded")
        return _device, _trans

class DeviceAttacherContextManager(DeviceAttacher):
    '''
    Returns a DeviceAttacher object that
    ensures the device attach is always followed by detach/close.
    This is for use with a python 'with' block.

    Usage:
        with DeviceAttacherContextManager(firmware_builds, url) as attacher:
            # use attacher.device
    Upon exit from the with block the debugger transport is closed.

    c.f. with open(path, mode) as f
    which returns an auto-close file object f.
    '''

    def __init__(self, firmware_builds, device_url, recovery=False,
                 target=None, emulator_build=None):
        self._firmware_builds = firmware_builds
        self._device_url = device_url
        self._recovery = recovery
        self._target = target
        self._device = None
        self._trans = None
        self._custom_digits_root=emulator_build

    def _raw_detach(self):
        '''
        Detach the debugger by closing the transport connection.
        '''
        # self._trans.disconnect() # - should it do this?
        # This is based on what we see in the _trans.reset() method
        if self._trans:
            self._trans.close()
            self._trans = None
        self._device = None

    def __enter__(self):
        '''
        Performs device attachment storing results in this object itself.
        '''
        self._device, self._trans = self.checked_attach(
            self._firmware_builds, self._device_url, self._recovery,
            self._target, self._custom_digits_root)
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        '''
        Performs detach from device.
        '''
        self._raw_detach()

    @property
    def device(self):
        'Provide access to the device'
        return self._device

    @property
    def trans(self):
        'Provide access to the transport'
        return self._trans

    @property
    def recovery(self):
        'Provide access to recovery mode switch'
        return self._recovery

    @property
    def emulator_build(self):
        return self._custom_digits_root
