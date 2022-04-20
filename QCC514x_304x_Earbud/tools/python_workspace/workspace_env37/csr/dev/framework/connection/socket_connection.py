############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.hw.port_connection import MasterPort, PortConnection
from csr.dev.hw.address_space import AccessPath, AddressRange, ReadRequest, \
WriteRequest, AddressSpace, AccessView
from csr.transport.socket_transport import SocketTransport, SocketTransportException, \
    SocketTransportDeviceChanged
from csr.wheels import build_le
from csr.wheels.bitsandbobs import iprint

class SocketConnection:
    """
    Class representing connection to a socket-based transport, assumed just to
    enable connecting to Apps P1 via processor-based reads and writes.
    """

    class SocketMasterPort(MasterPort):
        """
        Master for a specific data access channel - either data or program space,
        as appropriate.  This is identified to the underlying transport via the
        "block ID".
        """

        def __init__(self, socket_conn, block_id=False):
            MasterPort.__init__(self)
            
            self._conn = socket_conn
            self._block_id = block_id

        def connect(self, slave_space):
            self._connection = PortConnection(self, slave_space)
            self._access_path = AccessPath("SocketConnection", 0, self,
                                           AddressRange(0,0x100000000))
            self._access_path.extend(trace=True)

        def execute_outwards(self, access_request):
    
            '''Pass this access request to the socket transport'''
    
            if isinstance(access_request, ReadRequest):
                self._conn.read(access_request, self._block_id)
            elif isinstance(access_request, WriteRequest):
                self._conn.write(access_request, self._block_id)


    def __init__(self, device_url):
        
        self._socket = SocketTransport(device_url)
        self._max_payload = 0
        self._verbose = False
        self._apps_id = 4

        self._data_master = self.SocketMasterPort(self, AccessView.PROC_1_DATA)
        self._prog_master = self.SocketMasterPort(self, AccessView.PROC_1_PROG)
        
    def open(self):
        
        self._socket.open()
        self._max_payload = self._socket.get_max_payload()
        
    def close(self):
        
        self._socket.close()

    def toggle_verbose(self):
        self._verbose = not self._verbose
        
    def connect(self, chip):
        
        data_space = chip.apps_subsystem.p1.dm
        program_space = chip.apps_subsystem.p1.program_space
        self._data_master.connect(data_space)
        self._prog_master.connect(program_space)
        chip.apps_subsystem.p1.has_data_source = True

    def get_chip_id(self):
        return build_le(self._socket.read(self._apps_id, AccessView.PROC_1_DATA,
                                          0xffff9100, 4),
                        word_width=8)

    def read(self, access_request, block_id):

        rq = access_request
        if self._verbose:
            range = rq.region
            iprint("TRB read request for [0x%08x,0x%08x)[0x%x]"
                       % (range.start, range.stop, rq.block_id))
        try:
            to_read = rq.length
            read_from = rq.region.start
            data = []
            while to_read:
                chunk_size = min(to_read, self._max_payload)
                data += self._socket.read(self._apps_id, block_id, read_from, chunk_size)
                read_from += chunk_size
                to_read -= chunk_size
        except SocketTransportDeviceChanged as exc:
            raise AddressSpace.ReadFailureDeviceChanged(exc.__class__.__name__ + " " +str(exc))
        except SocketTransportException as exc:
            raise AddressSpace.ReadFailure(exc.__class__.__name__ + " " +str(exc))
        else:
            rq.data = data

    def write(self, access_request, block_id):
        
        rq = access_request
        if self._verbose:
            range = rq.region
            iprint("TRB write request for [0x%08x,0x%08x)[0x%x]"
                       % (range.start, range.stop, rq.block_id))
        try:
            to_write = rq.length
            write_from = rq.region.start
            data = rq.data
            while to_write:
                chunk_size = min(to_write, self._max_payload)
                self._socket.write(self._apps_id, block_id, write_from,  
                                   data[write_from-rq.region.start:
                                        write_from-rq.region.start+chunk_size])
                write_from += chunk_size
                to_write -= chunk_size
        except SocketTransportDeviceChanged as exc:
            raise AddressSpace.WriteFailureDeviceChanged(exc.__class__.__name__ + " " +str(exc))
        except SocketTransportException as exc:
            raise AddressSpace.WriteFailure(exc.__class__.__name__ + " " +str(exc))

    def reset(self):
        self._socket.reset_transport()

    def reset_device(self, curator, reset_type):
        self._socket.reset_chip()
