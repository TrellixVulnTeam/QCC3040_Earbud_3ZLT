############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

"""
Support for a jlink client implementing AddressMasterPort
to enable direct use of jlink as a debug transport for Pydbg.
"""
import os
from csr.dev.hw.port_connection import PortConnection, AccessPath
from csr.dev.hw.chip.mixins.has_jtag_swd_debug_trans import JTAGSWDConnectionManager
from csr.dev.hw.debug.swd import SWD
from csr.dev.model.interface import Group, Table
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.wheels import gstrm
from csr.transport.jlink import JLinkTransportAPI, JLinkTrans, JLinkSWDDriver
from .coresight_connection import CoresightConnection

class JLinkConnectionLifetimeError(RuntimeError):
    """
    Mistake in the attach/close/reopen sequence
    """

class JLinkConnection(CoresightConnection):
    """
    Interface to JLink-based memory access execution.  Acts as the end of the line for the bus modelling logic,
    converting access request objects into memory reads and writes on the transport(s).

    Essentially JLinkConnection exists to put all the various pieces together for JLink-specific 
    transport accesses.

    The multi-transport case is managed on this class's behalf by the JTAGSWDConnectionManager, and
    details specific to the bring-up of connections via the J-Link DLL API are managed by
    the JLinkTransportAPI class, which this class instantiates and passes to the Connection Manager. 
    The sequence of events when an access request arrives are:
    
    1. The access_requests are examined for details of what muxing layers were passed through in the bus
       modelling
    2. the correct transport object is requested from the Connection Manager based on this information
    3. the transport object is then invoked to execute the given request

    In addition this class provides an API for querying and managing some aspects of the J-Link DLL session,
    such as the transport speed, what J-Links are connected, etc.
    """
    
    def __init__(self, dongle_mgr=None, **jlink_args):
        
        CoresightConnection.__init__(self)
        self._jlink_args = jlink_args
        self._verbose = False
        self._is_open = False
        self._dongle_manager = dongle_mgr
        
    def connect(self, debug_port, verbose=None, chip=None):
        """
        Propagate the connection of this external-facing interface to a transport
        API through the network of address spaces that can see this interface,
        so that access requests can be routed here.
        """
        self._connection = PortConnection(self, debug_port)
        self._access_path = AccessPath(0, self)
        self._access_path.extend(trace=False)

        self._dap_properties = debug_port.get_properties()
        self._jlink_speed = self._jlink_args.get("speed_khz", 
                                                 self._dap_properties.get("speed_khz", 1000))
        self._jlink_interface = self._jlink_args.get("interface",
                                                     self._dap_properties.get("interface", "swd"))
        if verbose is None:
            verbose = self._jlink_args.get("verbose")
        else:
            verbose = int(verbose)

        if self._jlink_interface == "jtag":
            self._interface = JLinkTrans.JTAG
        elif self._jlink_interface == "swd":
            self._interface = JLinkTrans.SWD
        else:
            raise ValueError("Interface '{}' not recognised - must be one of "
                             "jtag or swd".format(self._jlink_interface))

        self._jlink_api = JLinkTransportAPI(s_dllpath=self._jlink_args.get("path",
                                            os.getenv("PYDBG_JLINK_PATH")))

        self._is_open = True

        if verbose is None:
            # Get it from the environment
            verbose = int(os.getenv("PYDBG_CORESIGHT_VERBOSITY", -1))

        extra_ap_args = chip.get_ap_flags() if chip is not None else {}

        self._conn_manager = JTAGSWDConnectionManager(debug_port.mux, self._jlink_api,
                                                      self._jlink_interface, verbose=verbose,
                                                      raw_driver=False,
                                                      extra_ap_args=extra_ap_args)

        self._dongle_id = self._dongle_manager.register(self._conn_manager,
                                                        serial_no=self._jlink_args.get("sn"),
                                                        ip_addr=self._jlink_args.get("ip"),
                                                        speed=self._jlink_speed,
                                                        target_interface=self._interface,
                                                        verbose=verbose)
        self._dongle_manager.select(self._dongle_id) # select self in order for enable_debug to work
        self._is_open = True

        try:
            chip.enable_debug
        except AttributeError:
            pass # doesn't need to be enabled
        else:
            chip.enable_debug()

    def execute_outwards(self, access_request, **kwargs):
        if not self.is_open:
            raise JLinkConnectionLifetimeError("Attempting to execute memory accesses "
                                               "over an unattached/closed JLink connection")
        CoresightConnection.execute_outwards(self, access_request)


    @property
    def is_open(self):
        """
        Is the connection up and usable?
        """
        return self._is_open

    @property
    def _is_connected(self):
        try:
            self._connection
        except AttributeError:
            return False
        return True

    def close(self):
        """
        Close the transport connection
        """
        if not self._is_open:
            raise JLinkConnectionLifetimeError("Attempting to close a "
                                            "connection that isn't open")
        self._jlink_api.close()
        self._is_open = False

    def reopen(self):
        """
        Open the transport again after a close call, with the same settings
        as before.
        """
        if self._is_open:
            raise JLinkConnectionLifetimeError("Attempting to re-open a "
                                        "connection that is already open")
        if not self._is_connected:
            raise JLinkConnectionLifetimeError("Attempting to reopen a "
                        "connection that wasn't made in the first place")
        self._jlink_api.open(serial_no=self._jlink_args.get("sn"),
                             ip_addr=self._jlink_args.get("ip"),
                            speed=self._jlink_speed,
                            target_interface=self._interface)
        self._is_open = True

    def reset(self):
        """
        Reconnect the transport, re-using the original settings.
        """
        self.close()
        self.reopen()

    def current_speed_khz(self):
        """
        Get the current speed the JLink has been configured to run at,
        in kHz.
        """
        if not self._is_connected:
            raise JLinkConnectionLifetimeError("Can't query speed until "
                            "transport interface is internally connected")
        return self._jlink_speed

    def reset_speed_khz(self, speed_khz):
        """
        Reset the speed the JLink should be configured to run at,
        in kHz.  The previous setting is returned.
        """
        if not self._is_connected:
            raise JLinkConnectionLifetimeError("Can't reset speed until "
                        "transport interface is internally connected")
        old_speed = self._jlink_speed
        self._jlink_speed = speed_khz
        if self.is_open:
            self._jlink_api.set_speed(self._jlink_speed)
        # Otherwise it will be reset next time reopen is called.
        return old_speed

    @property
    def api(self):
        return self._jlink_api

    def get_raw_swd(self, verbose=False):
        swd_driver = JLinkSWDDriver(self.api.dll, verbose=verbose)
        return SWD(swd_driver)

    def list_connected_jlinks(self, verbose=False, report=False):
        output = Group("Connected JLinks")
        if verbose:
            jlink_tbl = Table()
        else:
            table = ["SerialNumber", "Connection", "CurrentlyInUse"]
            jlink_tbl = Table(table)
        emulator_dict = self._jlink_api.collect_jlink_info(verbose)

        # Construct a table displaying each J-Link emulator's details
        for key, val in emulator_dict.items():
            if verbose:
                jlink_tbl.add_row(["Serial Number", key], make_first_header=True)
                jlink_tbl.add_row(["Connection", val["Connection"]], make_first_header=True)
                if val["Connection"] == "USB":
                    jlink_tbl.add_row(["USBAddr", val["USBAddr"]], make_first_header=True)
                jlink_tbl.add_row(["acProduct", val["acProduct"]], make_first_header=True)
                if val["Connection"] == "IP":
                    jlink_tbl.add_row(["aIPAddr", val["aIPAddr"]], make_first_header=True)
                    jlink_tbl.add_row(["Time", val["Time"]], make_first_header=True)
                    jlink_tbl.add_row(["Time_us", val["Time_us"]], make_first_header=True)
                    jlink_tbl.add_row(["HWVersion", val["HWVersion"]], make_first_header=True)
                    jlink_tbl.add_row(["abMACAddr", val["abMACAddr"]], make_first_header=True)
                    jlink_tbl.add_row(["acNickName", val["acNickName"]], make_first_header=True)
                    jlink_tbl.add_row(["acFWString", val["acFWString"]], make_first_header=True)
                    if val["IsDHCPAssignedIPIsValid"]:
                        jlink_tbl.add_row(["IsDHCPAssignedIP", val["IsDHCPAssignedIP"]],
                                          make_first_header=True)
                    if val["NumIPConnectionsIsValid"]:
                        jlink_tbl.add_row(["NumIPConnections", val["NumIPConnections"]],
                                          make_first_header=True)
                jlink_tbl.add_row(["Currently In Use", val["CurrentlyInUse"]],
                                  make_first_header=True)
            else:
                row = [key, val["Connection"], val["CurrentlyInUse"]]
                jlink_tbl.add_row(row)
        output.append(jlink_tbl)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)

    def toggle_verbose(self):
        self._verbose = not self._verbose

    def ap_scan(self, detailed=False, report=False, targetsel=None):
        """
        Scan the APs present on the DP and provide a table of information.

        By default, the table contains the AP index, its type including the 
        type of the attached memory bus for MEM-APs, and whether it is enabled
        or not.

        If detailed==True, the table contains additional columns describing the
        hardware design, the current access size and whether a transfer is currently
        in progress.

        By default the table is printed to the console; if report==True a renderable
        object is returned instead, which may be rendered to different formats via
        an Adaptor class (e.g. TextAdaptor, XHTMLAdaptor).  To obtain the underlying
        dictionary of raw data, call jlink.ap_scan() instead, which takes the same 
        arguments.

        For multidrop devices, the target DP can be selected via targetsel.  This
        should be a pair of values containing the device and instance IDs for the 
        target device.
        """
        transport = self._conn_manager.activate_conn(targetsel).get_transport()
        return transport.ap_scan(detailed=detailed, report=report)


