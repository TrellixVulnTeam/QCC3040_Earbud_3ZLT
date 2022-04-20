############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from ...debug_bus_mux import ARMDAP, GdbserverMux
from csr.wheels import autolazy, NameSpace, iprint
from csr.dev.hw.port_connection import AccessType
from csr.dev.hw.debug.coresight import CoresightDisabledAP
import time

class DebugPortReferenceRequest:
    type = AccessType.MISC

class AccessPortReferenceRequest:
    type = AccessType.MISC
    def __init__(self, index):
        self.index = index

class HasJtagSwdDebugTrans(object):
    """
    Mixin for Chip classes that support JTAG/SWD debugging, via either core-by-core
    gdbserver connections or via whole-chip DP/AP-based access.  
    To support the (possible) auto-instantiation of gdbserver instances for
    different cores, classes inheriting
    this mixin should have a class attribute GDBSERVER_PARAMS which looks like
    
    {<jtag_dongle1_name> : 
          { "core1_name" : <dict of args for starting dongle gdbserver for core1>,
            "core2_name" : <dict of args for starting dongle gdbserver for core2>},
     <jtag_dongle2_name> :...
    }            
    
    E.g. see QuartzChip.GDBSERVER_PARAMS.  Currently only JLinkGdbserver is supported.
    """

    @property
    def dap(self):
        try:
            self._dap
        except AttributeError:
            properties = getattr(self, "DAP_PROPERTIES", {})
            self._dap = ARMDAP(self.data_space_owners,
                               self.AP_MAPPING, properties=properties)
        return self._dap.port

    @property
    def gdbserver_mux(self):
        try:
            self._gdbserver_mux
        except AttributeError:
            self._gdbserver_mux = GdbserverMux(self.cores)
        return self._gdbserver_mux.port
    
    @classmethod
    def get_gdbserver_params(cls, dongle_type="jlink", core_name=False):
        """
        Get details of how a particular core on the chip appears over different 
        debug dongle types (currently just the jlink is supported
        """
        try:
            gdbserver_params = cls.GDBSERVER_PARAMS[dongle_type]
        except KeyError:
            raise ValueError("dongle type '%s' not recognised.  Available "
                             "types are '%s'" % (dongle_type, 
                                                 ", ".join(d for d in cls.GDBSERVER_PARAMS)))
        if core_name is False:
            return gdbserver_params
        else:
            try:
                return gdbserver_params[core_name]
            except KeyError:
                raise ValueError("Core '%s' not recognised. Available cores are "
                                 "'%s'" %  ", ".join(c for c in gdbserver_params["cores"]))

    def get_ap_flags(self):
        return {} # no additional flags by default.

    @property
    @autolazy
    def debug(self):
        return ArmChipDebug(self)

class ArmChipDebug:
    """
    Simple class providing basic functionality for a Coresight debug block
    """
    def __init__(self, the_chip):
        self._chip = the_chip

        rq = DebugPortReferenceRequest()
        self._chip.dap.execute_outwards(rq)
        self.dp = rq.data

    def _retrieve_aps(self):
        aps = NameSpace()
        ap_dict = {}
        for ap_name, ap_index in self._chip.AP_MAPPING.items():
            rq = AccessPortReferenceRequest(ap_index)
            self._chip.dap.execute_outwards(rq)
            setattr(aps, ap_name, rq.data)
            ap_dict[ap_index.ap_number] = rq.data
        return aps, ap_dict

    @property
    def aps(self):
        try:
            self._aps
        except AttributeError:
            self._aps, self._aps_dict = self._retrieve_aps()
        return self._aps

    @property
    def aps_dict(self):
        try:
            self._aps_dict
        except AttributeError:
            self._aps, self._aps_dict = self._retrieve_aps()
        return self._aps_dict

    def get_ap_of_subsystem(self, subsys_or_core):
        try:
            subsys = subsys_or_core.subsystem
        except AttributeError:
            # assume it's a subsystem
            subsys = subsys_or_core
        try:
            ap_number = subsys.ap_number
        except AttributeError:
            raise ValueError("Given subsystem has no record of an associated AP")
        else:
            return self.aps_dict[ap_number]

    def clear_power_request(self):
        """
        Clear CDbgPwrUpReq and CSysPwrUpReq in DP.CTRL_STAT
        """
        self.dp.fields.CTRL_STAT.writebits(CDBGPWRUPREQ=0,
                                           CSYSPWRUPREQ=0)

    def assert_power_request(self):
        """
        Set CDbgPwrUpReq and CSysPwrUpReq in DP.CTRL_STAT
        """
        self.dp.fields.CTRL_STAT.writebits(CDBGPWRUPREQ=1,
                                           CSYSPWRUPREQ=1)

    def power_is_requested(self):
        """
        True if either CDbgPwrUpReq or CSysPwrUpReq is set in DP.CTRL_STAT
        """
        ctrl_stat = self.dp.fields.CTRL_STAT.capture()
        return ctrl_stat.CDBGPWRUPREQ == 1 or ctrl_stat.CSYSPWRUPREQ == 1

    def reset_request(self):
        """
        Issue a reset on the DAP via CDbgRstReq.
        If wait, don't exit until CDbgRstAck has gone high.
        """
        ctrl_stat = self.dp.fields.CTRL_STAT.capture()
        ctrl_stat.CDBGRSTREQ = 0
        ctrl_stat.flush()
        ctrl_stat.CDBGRSTREQ = 1
        ctrl_stat.flush()
        # Wait for CDBGRSTACK to go high
        t0 = time.time()
        while self.dp.fields.CTRL_STAT.CDBGRSTACK == 0:
            if time.time() - t0 > 0.5:
                break
        # Now clear CDBGRSTREQ
        self.dp.fields.CTRL_STAT.CDBGRSTREQ = 0

    def clear_sticky_error(self):
        """
        Write ABORT.STKERRCLR to clear a sticky error reported in
        CTRL_STAT.STICKYERR.
        """
        self.dp.fields.ABORT.writebits(0, STKERRCLR=1, ORUNERRCLR=1, WDERRCLR=1, STKCMPCLR=1, DAPABORT=1)
        self.reset_request() # to clear any problems in the AP state


class HasMEMAP:
    ap_number = None



class DebugConnectionError(RuntimeError):
    pass

class JTAGSWDConnectionManager(object):
    """
    Manager for the various transport objects that may be created across a JTAG/SWD connection.
    This implementation is agnostic to the details of how the connection is physically made to
    the chip (e.g. via Trace32 or J-Link).  It relies on an abstraction of those interfaces to
    generate the various connections involved.  However, it does know whether the connection
    has been made via JTAG or SWD.

    It is assumed that the underlying physical link driver supports two basic types
    of connection - a generic JTAG TAP orientated connection, and an ARM DAP orientated connection.
    The latter has two flavours - JTAG and SWD-based.   The connection objects act as interfaces
    providing access to transport objects, and they are capable of connecting and disconnecting 
    themselves on demand.  
    
    The idea is that when the bus access logic passes through an access request
    corresponding to a particular named connection (e.g. the ARM DAP, or the JTAG2AHB), this class
    will ensure all the other connections are disconnected and the required one is connected,
    before returning a transport object for the connection. Most frequently this will be the
    same connection that was used last so nothing much will actually happen, but with this structure
    it becomes possible not only for the user to cleanly switch from one transport to another, but
    for the way those transports actually work to change dynamically, e.g. if the JTAG daisy chain
    is reconfigured.

    This class has a reference to the debug port object, which enables it to request details of how
    each named connection is currently configured, whenever it is necessary to reconnect that
    connection.  This approach allows this class to implement an API by which higher-level code can 
    indicate a reconfiguration  of the debug transport hardware (e.g. a change in the daisy chain),
    by simply switching all connections to the disconnected state.
    """

    def __init__(self, debug_port, remote_api, interface, **transport_kwargs):

        # Book keeping for the instances
        self._conns = {}
        self._current_client = None
        self._debug_port = debug_port
        self.api = remote_api
        self._interface = interface
        self._transport_kwargs = transport_kwargs

    def create_probe_connection(self):
        """
        Create a "probe" connection, which is a special type of transport in which the JTAG scan
        chain is viewed as a whole.  This is typically used to scan out the IDCODE values for 
        all the TAPs in the chain so that it is possible to determine what the current daisy chain 
        configuration is.
        """
        assert(self._interface == "jtag")

        def get_scan_chain():
            return dict(IRPre=0,IRPost=0,DRPre=0,DRPost=0,IRLength=4)

        return self.api.TAPConnectionType(self.api, get_scan_chain, "probe")

    def create_tap_connection(self, name, conn_type):
        """
        Create a named TAP connection of the given type.  If name is None it is assumed
        that there is a single TAP in the scan chain and that it is an ARM DAP (or at least,
        a TAP with an IR length of 4).  If the name is given, the scan chain details are
        looked up on the debug_port.
        """

        assert(self._interface == "jtag")

        def get_scan_chain():
            if name is None:
                # assume scan_chain is trivial and it's an ARM DAP
                scan_chain = dict(IRPre=0,IRPost=0,DRPre=0,DRPost=0,IRLength=4)
            else:
                scan_chain = self._debug_port.get_scan_chain_settings_of(name)

            if scan_chain is None:
                raise DebugConnectionError("TAP '{}' unknown or not enabled!".format(name))
            return scan_chain

        return self.api.TAPConnectionType(self.api, get_scan_chain, conn_type, 
                                          **self._transport_kwargs)

    def create_dap_connection(self, targetsel=None):
        """
        Create an object representing an available connection to the DAP indicated
        by targetsel.  The exact Connection object type used may depend on whether
        the underlying connection is over JTAG or SWD (although on J-Link this
        doesn't make any difference in practice so the same type is used).
        """
        if self._interface in ("jtag",):
            
            name = targetsel

            def get_scan_chain():
                if name is None:
                    # assume scan_chain is trivial and it's an ARM DAP
                    scan_chain = dict(IRPre=0,IRPost=0,DRPre=0,DRPost=0,IRLength=4)
                else:
                    scan_chain = self._debug_port.get_scan_chain_settings_of(name)
                if scan_chain is None:
                    raise DebugConnectionError("TAP '{}' unknown or not enabled!".format(name))
                return scan_chain

            return self.api.JTAGDAPConnectionType(self.api, get_scan_chain, "dap",
                                                  **self._transport_kwargs)

        return self.api.SWDDAPConnectionType(self.api, targetsel, "dap",
                                             **self._transport_kwargs)


    def activate_conn(self, name_or_targetsel):
        """
        Create a TAP/DAP connection or set a previously-created TAP/DAP connection as the 
        (unique) active instance, meaning all other connections are disconnected.
        """
        try:
            self._conns[name_or_targetsel]
        except KeyError:
            if name_or_targetsel == "probe":
                self._conns[name_or_targetsel] = self.create_probe_connection()
            else:
                conn_type = self._debug_port.get_conn_type(name_or_targetsel)
                if conn_type == "dap":
                    self._conns[name_or_targetsel] = self.create_dap_connection(name_or_targetsel)
                elif conn_type == "jtag2ahb":
                    self._conns[name_or_targetsel] = self.create_tap_connection(name_or_targetsel,
                                                                                conn_type)
                else:
                    raise ValueError("Connection type '{}' for connection '{}' "
                                     "not recognised".format(conn_type, name_or_targetsel))

        # We must make sure all the other connections are disconnected before we connect the
        # requested one, because they share resources.
        for conn_name, conn in self._conns.items():
            if conn_name != name_or_targetsel:
                conn.disconnect()

        for conn_name, conn in self._conns.items():
            if conn_name == name_or_targetsel:
                conn.connect()
                break

        return conn

    def invalidate_conns(self):
        """
        Force all connection wrappers to disconnected state so that
        they have to be reconnected afresh on next use.  Useful if the debug
        hardware has been reconfigured.
        """
        for conn in self._conns.values():
            conn.disconnect()
