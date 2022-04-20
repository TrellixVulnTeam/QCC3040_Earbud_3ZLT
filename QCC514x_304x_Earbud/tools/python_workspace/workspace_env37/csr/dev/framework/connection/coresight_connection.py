############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.hw.port_connection import AccessType
from csr.dev.hw.address_space import AddressSpace, ReadRequest, WriteRequest
from csr.dev.hw.core.debug_controller import DebugAccessRequest, DebugReadRequest
from csr.dev.hw.chip.mixins.has_jtag_swd_debug_trans import DebugPortReferenceRequest,\
    AccessPortReferenceRequest
from csr.dev.hw.debug.utils import TransportAccessError
from csr.dev.hw.port_connection import MasterPort
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import _display_ints_hex, display_hex


class CoresightConnection(MasterPort):
    """
    Base class for connecting the address space modelling logic to the transport logic
    for Coresight-based chips.  Relies on the subclass constructing an attribute
    _conn_manager, from which transport objects appropriate to the specific transport
    mechanism (J-Link or Trace32) can be retrieved.
    """

    def __init__(self):
        MasterPort.__init__(self)
        self._dongle_manager = None
        self._dongle_id = None
        # subclasses must construct a _conn_manager attribute.

    @display_hex
    def probe_jtag(self):
        jtag_util = self._conn_manager.activate_conn("probe").get_transport()
        return jtag_util.get_id_codes()

    def get_tap_controller(self, tap_name):

        return self._conn_manager.activate_conn(tap_name).get_jtag_controller()

    def invalidate_connections(self):
        """
        Force the connection manager to regenerate all transport connections.
        Useful if the debug hardware has been reconfigured.
        """
        self._conn_manager.invalidate_conns()


    def execute_outwards(self, access_request, **kwargs):
        """
        Handle incoming access requests by passing them to the read/write
        functions.
        """
        # Open the connection to the correct transport dongle via
        # the dongle manager
        self._dongle_manager.select(self._dongle_id)

        # Unpeel the outer layer of muxing for multidrop, if present
        try:
            targetsel = access_request.targetsel
        except AttributeError:
            try:
                targetsel = access_request.tap_name
            except AttributeError:
                targetsel = None
            else:
                access_request = access_request.basic_request
        else:
            access_request = access_request.basic_request

        # Switch to the requested connection
        transport = self._conn_manager.activate_conn(targetsel).get_transport()

        if isinstance(access_request, DebugPortReferenceRequest):
            access_request.data = transport.get_dp()
        elif isinstance(access_request, AccessPortReferenceRequest):
            access_request.data = transport.get_connection(access_request.index,
                                                            allow_disabled=True)
        else:
            core_rq = access_request.basic_request

            # Unpeel the wrapper indicating that an SBA access must be
            # performed, if present
            try:
                rq = core_rq.peripheral_req
            except AttributeError:
                peripheral = None
            else:
                peripheral = core_rq.peripheral_type
                core_rq = rq
            if isinstance(core_rq, ReadRequest):
                self._read(transport, access_request.mux_select, core_rq, 
                           targetsel=targetsel, peripheral=peripheral)
            elif isinstance(core_rq, WriteRequest):
                self._write(transport, access_request.mux_select, core_rq, 
                            targetsel=targetsel, peripheral=peripheral)
            elif isinstance(core_rq, DebugAccessRequest):
                self._debug_request(transport, access_request.mux_select, core_rq, 
                                    peripheral=peripheral)

    def _read(self, transport, conn_id, rq, targetsel=None, peripheral=False):
    
        region = rq.region
        if self._verbose:
            iprint("Request to connection {} to read [0x{:x}:0x{:x}){}".format(
                                    conn_id,
                                    region.start, region.stop,
                                    "" if targetsel is None else " (TARGETSEL={})".format(targetsel)))
        try:
            rq.data = transport.memory_read(conn_id, region.start, region.stop, 
                                            peripheral=peripheral)
        except TransportAccessError as exc:
            raise AddressSpace.ReadFailure("Error reading through connection ID {}: {}".
                                            format(conn_id, str(exc)))
        
    def _write(self, transport, conn_id, rq, targetsel=None, peripheral=False):
        
        region = rq.region
        if self._verbose:
            bytes_being_written = str(_display_ints_hex(rq.data))
            if len(rq.data) > 4:
                bytes_being_written = bytes_being_written.replace("]",", ...") 

            iprint("Request to connection {} to write {} at 0x{:x}{}".format(
                                    conn_id,
                                    bytes_being_written,
                                    region.start,
                                    "" if targetsel is None else " (TARGETSEL={})".format(targetsel)))
        try:
            transport.memory_write(conn_id, region.start, rq.data, peripheral=peripheral)
        except TransportAccessError as exc:
            raise AddressSpace.WriteFailure("Error writing through connection ID {}: {}".
                                            format(conn_id, str(exc)))

    def _debug_request(self, transport, conn_id, rq, peripheral=False):
        if isinstance(rq, DebugReadRequest):
            if rq.type == AccessType.REGISTERS:
                rq.data = transport.register_read(conn_id, rq.meta, peripheral=peripheral)
            elif rq.type == AccessType.RUN_CTRL:
                rq.data = transport.run_ctrl_read(conn_id, rq.meta, peripheral=peripheral)
            else:
                raise NotImplementedError("AccessType {} not implemented for coresight connections".format(rq.type))
        else:
            if rq.type == AccessType.REGISTERS:
                transport.register_write(conn_id, rq.meta, rq.data, peripheral=peripheral)
            elif rq.type == AccessType.RUN_CTRL:
                transport.run_ctrl_write(conn_id, rq.meta, peripheral=peripheral)
