############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from ...hw.port_connection import MasterPort, PortConnection, AccessPath
from ...hw.address_space import ReadRequest, WriteRequest, AddressSpace
from ...hw.debug_bus_mux import TcMemWindowedRequest, TcMemRegBasedRequest
from ....wheels.bitsandbobs import words_to_bytes, bytes_to_words, TypeCheck,\
dwords_to_bytes, bytes_to_dwords
from ....transport.tctrans import TcTrans, TcError
from .trb import trb_aligned_read_helper, trb_aligned_write_helper
import platform
import sys
import os
from .connection_utils import SupportsGAIAApp

system = platform.system()
is_32bit = sys.maxsize == (1 << 31) - 1 

def print_data_array(data):
    if len(data) < 4:
        iprint(" -> [%s]" % (",".join((hex(v) for v in data))))
    else:
        iprint(" -> [%s, ..." % (",".join((hex(v) for v in data))))


class LowCostDebugConnection(SupportsGAIAApp):
    """
    Interface to the two available low-cost debug transports - windowed and
    register-based access over toolcmd.  These are not distinguished to the user:
    Pydbg selects the best transport for each subsystem within TcMemMux
    """
    # From the specification - the IP port to use for the link
    DEFAULT_PORT_NUMBER = 13570

    def __init__(self, transport="usb2tc", dongle_id=None):
        
        # Dongle ID must be an ascii string
        if dongle_id:
            try:
                dongle_id = str(dongle_id)
            except UnicodeEncodeError:
                raise ValueError("'dongle_id' must be an ascii string")
        elif transport.startswith("btoip"):
            full_transport = transport
            try:
                transport, suffix = full_transport.split(":")
            except ValueError:
                transport, suffix = transport, None
            if suffix in ("l", "L", None):
                dongle_id = "501"
            elif suffix in ("r", "R"):
                dongle_id = "502"
            else:
                raise RuntimeError("Expected 'btoip', 'btoip:L' or "
                                    "'btoip:R', not '{}'".format(full_transport))

        if transport == "btoip":
            self._ip_port = self.DEFAULT_PORT_NUMBER
            self._start_adb_forwarding()
            self._start_gaia_debug_client_app()

        if system == "Windows":
            lib_path = "win32" if is_32bit else "win64"
            self._tctrans = TcTrans(override_lib_path=os.path.join(lib_path,"tctrans.dll"))
            if dongle_id is not None:
                try:
                    self._tctrans.open(dongle_id = dongle_id)
                except TcError as e:
                    raise 
            else:
                devices = [dev for dev in self._tctrans.enumerate_devices() 
                                                if dev.transport == transport]

                if len(devices) == 1:
                    self._tctrans.open(devices[0].id)
                elif len(devices) == 0:
                    raise RuntimeError("No {} devices detected".format(transport))
                else:
                    raise RuntimeError("Detected multiple {} devices with IDs {}"
                        "specify device ID on command line".format(transport,
                                            ", ".join(dev.id for dev in devices)))

        else:
            raise NotImplementedError("Low-cost debug support isn't available on "
                                      "%s" % system)

        self.transport_type = transport
        

        self._win_master = self.WindowedMaster(self._tctrans)
        self._reg_master = self.RegBasedMaster(self._tctrans)
        
        self._verbose = False
        self.is_remote = False

    def reset_device(self, curator_unused, reset_type=None):
        """
        Reset the DUT. 
        :param curator_unused: Unused
        :param reset_type: Valid values are prefer_pbr, require_pbr, prefer_por 
            and require_por. However at this time USB DEBug only has one method 
            of provoking a reset which is via toolcmd, generating a post boot 
            reset. As such a NotImplementedError is raised if require_por passed.
        """

        if reset_type is None:
            reset_type = "require_pbr"
        
        reset_map = {"prefer_pbr": 'tctrans_reset',
                     "require_pbr": 'tctrans_reset',
                     "prefer_por": 'tctrans_reset',
                     "require_por": "notimplemented"}
            
        if reset_map[reset_type] == "tctrans_reset":
            self._tctrans.reboot_curator_cmd()
        else:
            raise NotImplementedError("{} is not a valid \
reset type for this configuration".format(reset_type))
        
    def reset(self, **kwargs):
        pass

    def toggle_verbose(self):
        self._win_master.toggle_verbose()
        self._reg_master.toggle_verbose()

    @property
    def windowed_master(self):
        return self._win_master

    @property
    def regbased_master(self):
        return self._reg_master
        
    def connect(self, chip):
        
        self._win_master.connect(chip.tc_mem_win_port)
        self._reg_master.connect(chip.tc_mem_reg_port)
    
    def get_chip_id(self):
        
        chip_version_reg = self._tctrans.read_windowed(0, 0xfe81, 1)
        return chip_version_reg[0]

    def block_read32(self, subsys, address, locations, block_id=0):
        return self._reg_master.block_read32(subsys, address, locations,
                                                            block_id=block_id)
        
    def block_read16(self, subsys, address, locations, block_id=0):
        return self._reg_master.block_read16(subsys, address, locations,
                                                            block_id=block_id)
        
    def xap_block_read(self, subsys, word_address, len_words):
        return self._win_master.xap_block_read(subsys, word_address, len_words)

    @property
    def tctrans(self):
        return self._tctrans
    
    class LowCostDebugMaster(MasterPort):
    
        def __init__(self, tctrans):
            
            MasterPort.__init__(self)
            self._tctrans = tctrans
            self._verbose = False
            self._dongle_id = self._tctrans.get_device_id()
            
        def toggle_verbose(self):
            old_value = self._verbose
            self._verbose = not self._verbose
            return old_value
    
        def execute_outwards(self, access_request):
    
            TypeCheck(access_request, self.REQUEST_TYPE)
    
            basic_request = access_request.basic_request
    
            if isinstance(basic_request, ReadRequest):
                self._read(access_request.subsys,
                           basic_request)
            elif isinstance(basic_request, WriteRequest):
                self._write(access_request.subsys,
                            basic_request)
        
        def connect(self, tc_chip_port):
            """\
            Logically connect self to the specified spi_space for access routing
            purposes.
    
            Also builds an extended AccessPath so that reachable AddressSpaces know
            "at a glance" what connections can reach them.
            """
            self._connection = PortConnection(self, tc_chip_port)
            self._access_path = AccessPath(0, self)
            self._access_path.extend()


    class WindowedMaster(LowCostDebugMaster):

        REQUEST_TYPE = TcMemWindowedRequest
    
        def _read(self, subsys_id, access_request):
            rq = access_request
            if self._verbose:
                iprint("Windowed read request for SS %d: %d words at 0x%x" %
                       (subsys_id, len(rq.region), rq.region.start))
            try: 
                rq.data = self._tctrans.read_windowed(subsys_id, rq.region.start,
                                                        len(rq.region))
            except TcError as e:
                raise AddressSpace.ReadFailure(": ".join((type(e).__name__,str(e))))
            if self._verbose:
                print_data_array(rq.data)
    
        def _write(self, subsys_id, access_request):
            rq = access_request
            if self._verbose:
                iprint("Windowed write request for SS %d: %d words at 0x%x" %
                       (subsys_id, len(rq.region), rq.region.start))
                print_data_array(rq.data)
            try:
                self._tctrans.write_windowed(subsys_id, rq.region.start,
                                             rq.data)
            except TcError as e:
                raise AddressSpace.WriteFailure(": ".join((type(e).__name__,str(e))))
            
        def xap_block_read(self, subsys_id, word_addr, word_len, block_id=0):
            
            self._tctrans.read_windowed(subsys_id, word_addr, word_len)


    class RegBasedMaster(LowCostDebugMaster):
    
        REQUEST_TYPE = TcMemRegBasedRequest
    
        def reader(self, subsys, block, addr, width, length):
            """
            Simple converter function that makes tctrans.read_regbased look the
            same as trbtrans.read, so the same helper function can be used to
            drive them both.
            """
            return self._tctrans.read_regbased(subsys, block, width, True, addr, length)
        def writer(self, subsys, block, addr, width, data):
            """
            Simple converter function that makes tctrans.write_regbased look the
            same as trbtrans.write, so the same helper function can be used to
            drive them both.
            """
            return self._tctrans.write_regbased(subsys, block, width, True, addr, data)
    
        def _read(self, subsys_id, access_request):
            
            try:
                trb_aligned_read_helper(subsys_id, access_request, 
                                        self.reader,
                                        verbose=self._verbose,
                                        dongle_id=self._dongle_id)
            except TcError as e:
                raise AddressSpace.ReadFailure(": ".join((type(e).__name__,str(e))))
            
        def _write(self, subsys_id, access_request):
            
            try:
                trb_aligned_write_helper(subsys_id, access_request, self.reader,
                                         self.writer,
                                         verbose=self._verbose, dongle_id=self._dongle_id)
            except TcError as e:
                raise AddressSpace.WriteFailure(": ".join((type(e).__name__,str(e))))

    
        def block_read32(self, subsys, address, locations, block_id=0):
            """
            Reads a block of dwords using a sequence of Debug Read Request 
            transactions. Similar to trbtrans.read().
            """
            return self._tctrans.read_regbased_32(subsys, block_id, True, 
                                                  address, locations)
        
        def block_read16(self, subsys, address, locations, block_id=0):
            """
            Reads a block of 16-bit words using a sequence of Debug Read Request 
            transactions. Similar to trbtrans.read().
            """
            return self._tctrans.read_regbased_16(subsys, block_id, True,
                                                  address, locations)

        
