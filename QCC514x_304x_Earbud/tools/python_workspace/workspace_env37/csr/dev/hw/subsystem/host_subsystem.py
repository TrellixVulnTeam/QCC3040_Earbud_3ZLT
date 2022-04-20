############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .hydra_subsystem import HydraSubsystem
from csr.wheels.global_streams import iprint
from csr.wheels import PureVirtualError, NameSpace, bitsandbobs
from csr.dev.hw.address_space import AddressMap, NullAccessCache, \
                                     AddressSlavePort, AddressMasterPort, \
                                     AddressConnection, ReadRequest, \
                                     WriteRequest, BankedAddressSpace, \
                                     AddressSpace, AddressRange
                                     
from csr.dev.hw.core.meta.i_io_map_info import FieldRefDict, FieldValueDict
from csr.dev.hw.core.meta.i_layout_info import XapDataInfo


class HostSubsystem (HydraSubsystem):
    """\
    Host Subsystem (Proxy)
    
    Contains register space but no processor or firmware
    """
        
    @property
    def name(self):
        return "Host subsystem"

    def blocks_present(self):
        """
        Names and address-space details of HIF hw blocks present.  Overridden
        by chip-specific subclasses
        """
        raise PureVirtualError
    
    def populate(self):
        """
        The HIF subsystem data map is a bit unusual:
         - the MS nibble indicates the subsystem that is looking
         - the next MS nibble indicates the hw block to look at (SDIO, USB etc)
         - the LS octet defines the register's address within the block
         
        Within each block there are two basic sets of registers - the common
        ones and the SS-specific ones.  If there are n blocks and m subsystems,
        then the number of separate addressable units needed is(1 + m)*n.
        
        In addition, some of the blocks have other muxing, for example, the USB
        has separate copies of the EP_CONFIG registers (and one or two others)
        for each endpoint.  Is that worth modelling? 

        """
        
        self._components = NameSpace()
        comps = self._components
        
        #First create the HIF_SPI map.  This maps everything to one of the
        #subblocks.  The mappings are set up below
        comps.spi_map = AddressMap("HIF_SPI", self._access_cache_type)
        #The TRB map is different
        comps.trb_map = AddressMap("HIF_TRB", self._access_cache_type,
                                   word_bits=8, max_access_width=2)
        comps.per_ss_blocks = {}
        comps.common_blocks = {}
        comps.common_usb = [None]* (max(self.chip.subsystems)+1)
        
        for block in self.blocks_present:
            per_ss = [None]* (max(self.chip.subsystems)+1)

            for ssid in self.chip.subsystems:
                if ssid == self.chip.SSID.HOST:
                    continue;
                  
                if block.startswith("USB2"):  
                    comps.common_usb[ssid] = AddressMap("HIF_COMMON_%s_%s" % (
                                    block, self.chip.subsystems[ssid].name),
                                                   self._access_cache_type)
                per_ss[ssid] = AddressMap(\
                    "HIF_%s_%s" % (block, self.chip.subsystems[ssid].name),
                                                        self._access_cache_type)
                    
            comps.per_ss_blocks[block] = per_ss
            comps.common_blocks[block] = AddressMap(\
                  "HIF_COMMON_%s" % (block), self._access_cache_type)
            
        #Then create all the mappings to the different blocks

        #Note: start and end addresses in the "from" spaces share the lower octet 
        #with the corresponding addresses in the "to" spaces (i.e. the subblock 
        #spaces)
        for block, block_params in self.blocks_present.items():
            blockid = block_params["block_id"]
            
            #The common block has a mapping from all subsystems (apart
            #from the host ss, of course). These will reside in the
            #HOST_SS block
            start = self.blocks_present[block]["common_start"]
            end = self.blocks_present[block]["ss_start"]
            
            for ssid in self.chip.subsystems:
                if ssid == self.chip.SSID.HOST:
                    continue
                
                if block.startswith("USB2"):
                    banked = BankedAddressSpace(
                        "USB2_ENDPOINT_SELECT",
                                     self.chip.subsystems[ssid],
                                     "HIF_USB_EP_CONFIG_BANK",
                                     self._access_cache_type,
                                     length = end-start+1)
                    common = comps.common_usb[ssid]
                    common.add_mapping(start, end, banked)
                else:
                    common = comps.common_blocks[block]

                # SPI debug access
                from_start = start + (ssid << 12) + (blockid << 8)
                from_end = end + (ssid << 12) + (blockid << 8)
                comps.spi_map.add_mapping(from_start, from_end, common.port)
                
                # TRB debug access
                from_start = 0x00A00000 + (ssid << 16) + (blockid << 9) + (start << 1)
                from_end =   0x00A00000 + (ssid << 16) + (blockid << 9) + (end << 1)
                comps.trb_map.add_mapping(from_start, from_end, common.port)

            #The subsystem-specific blocks obviously just map from the
            #specific subsystems
            start = self.blocks_present[block]["ss_start"]
            end = self.blocks_present[block]["ss_end"]
            
            for ssid in self.chip.subsystems:
                if ssid == self.chip.SSID.HOST:
                    continue
                
                per_ss = comps.per_ss_blocks[block][ssid]
                
                # SPI debug access
                from_start = start + (ssid << 12) + (blockid << 8)
                from_end = end + (ssid << 12) + (blockid << 8)
                comps.spi_map.add_mapping(from_start, from_end, per_ss.port)
                
                # TRB debug access
                from_start = 0x00A00000 + (ssid << 16) + (blockid << 9) + (start << 1)
                from_end =   0x00A00000 + (ssid << 16) + (blockid << 9) + (end << 1)
                comps.trb_map.add_mapping(from_start, from_end, per_ss.port)

    def _generate_report_body_elements(self):
       return None

    def get_subsystem_view(self, block, ssid):
        """
        Lazily construct subsystem ssid's view of host hw block "block".
        """
        
        #Has anyone asked for any kind of view before?
        try:
            self._components.ss_views
        except AttributeError:
            self._components.ss_views = {}
            
        #Has anyone asked for a view of this block?
        try:
            self._components.ss_views[block]
        except KeyError:
            self._components.ss_views[block] = {}
        
        #Has anyone asked for this particular view of this block?
        try:
            self._components.ss_views[block][ssid]
        except KeyError:
            self._components.ss_views[block][ssid] = \
                                        self._create_subsystem_view(block, ssid)
            
        return self._components.ss_views[block][ssid] 
        
    def _create_subsystem_view(self, block, ssid):
        """
        Return an AddressMap that gives access to exactly the things the
        specified subsystem can see in the specified block
        """
        
        #We can use a NullAccessCache unconditionally because these maps will
        #pass everything through
        view_map = HostHwBlockView(block, self.chip.subsystems[ssid],
                                   NullAccessCache)
        
        if block.startswith("USB2"):
            common = self._components.common_usb[ssid]
        else:
            common = self._components.common_blocks[block]
        per_ss = self._components.per_ss_blocks[block][ssid]
        
        view_map.add_mapping(self.blocks_present[block]["common_start"],
                 self.blocks_present[block]["ss_start"], common.port)

        #Map address in the ss-specific range into the ss-specific subblock
        view_map.add_mapping(self.blocks_present[block]["ss_start"],
                             self.blocks_present[block]["ss_end"], per_ss.port)
        return view_map
    
    
    def _create_spi_data_map(self):
        """
        It's simpler to create this when all the subsystem blocks are created
        in the populate method
        """
        return self._components.spi_map
        
    def _create_trb_map(self):
        return self._components.trb_map

    @property
    def addr_unit_bits(self):
        return XapDataInfo().addr_unit_bits


class HostHwBlockView(AddressMap):
    """
    Class that adds field information to an AddressMap that spans a particular
    HIF Hw block
    """
    
    def __init__(self, block, parent_subsystem, access_cache_type):
        
        map_name = "HIF_%s_%s_VIEW" % (block, parent_subsystem.name)
        
        AddressMap.__init__(self, map_name, access_cache_type)
        
        self._name = block
        #Note: the core creates one of these, so we don't want to reference the
        #core until later, or we'll get an infinite recursion
        self._parent_subsystem = parent_subsystem
        
    @property
    def fields(self):
        return FieldValueDict(self.field_refs)
    
    @property
    def field_refs(self):
        try:
            self.__field_ref_dict
        except AttributeError:
            self.__field_ref_dict = FieldRefDict(self._io_map_info, self.port)
        return self.__field_ref_dict
    
    @property
    def registers(self):
        return self.port
    
    @property
    def _io_map_info(self):
        try:
            self.__io_map_info
        except AttributeError:
            ss_io_map_info = self._parent_subsystem.core.info.io_map_info
            self.__io_map_info = ss_io_map_info.filter_io_map_info(self._name,
                             addr_xfm = lambda x : x & 0xff,
                             name_xfm = lambda s : s.replace(self._name+"_",""))
        return self.__io_map_info

    
class CoreHostBlockViewManager(AddressMap):
    """
    AddressMap that dynamically modifies the mapping of processor addresses
    to hw blocks based on the value of the specified block select register and 
    thereby models a core's view
    """
    def __init__(self, name, core, block_select_reg,
                 host_subsystem_view, access_cache_type):
        
        from csr.dev.hw.subsystem.host_subsystem import HostSubsystem 
        
        AddressMap.__init__(self, name, access_cache_type)
        
        self._mux_field_ref = core.field_refs[block_select_reg]
        
        # Accessor for the register that controls permission
        # to substitute subsystem id in trb requests to hostif  
        self._ss_in_addr_en_ref = core.field_refs["HOST_SYS_REG_ACCESS_SUBSYS_IN_ADDR_EN"]
        
        # Bitfield stores most recently used register name in
        # core.register_space.cached_register_names
        # we need it to infer hostif block
        self._core_register_space = core.register_space

        # Save trb debugger id, we will need it to give debugger
        # permission to substitute subsystem id in trb requests to hostif
        try:
            self._trb_debugger_mask = (1 << core.subsystem.chip.SSID.DEBUGGER)
        except AttributeError:
            self._trb_debugger_mask = None
            
        # Whether to attach comments about selected hostif block to requests
        # Now that we have hostif block autodetection from a register
        # name this can be disabled by default.
        self._verbose = False
        
        #Grab handles to the chip's view
        self._view_ports = {}
        self._port_conns = {}
        for block, block_params in core.subsystem.chip.host_subsystem.blocks_present.items():
            try:
                block_id = block_params["block_id"]
                self._view_ports[block_id] = self._BlockMaster(block)
                #Note: you have to retain a reference to the AddressConnections
                #because their del operator breaks the connection.
                self._port_conns[block_id] = \
                   AddressConnection(self._view_ports[block_id],
                                     host_subsystem_view.__getattribute__(\
                                                          block.lower()).port)
            except AttributeError:
                #The host doesn't happen to have this type of host transport
                pass
            
        self._proxy_slave = self._DynamicSlave("XAP_CORE_HIF_VIEWER", self)
        
        # We can have more than one bitserial block.
        # On Xap-based cores there will be only one set
        # of registers: BITSERIAL_XXX.
        # In order to be able to access registers in
        # particular bitserial blocks we replace every BITSERIAL_XXX
        # with BITSERIAL0_XXX, BITSERIAL1_XXX, ... BITSERIAL<n-1>_XXX, 
        # depending on the number of bitserial blocks
        bitserial_count = 0
        for block in core.subsystem.chip.host_subsystem.blocks_present:
            if block.startswith("BITSERIAL"):
                bitserial_count += 1
                
        if bitserial_count > 1:
            bitserial_syms = []
            fields = core.info.io_map_info._field_records
            for sym in fields:
                if sym.startswith("BITSERIAL_"):
                    bitserial_syms.append(sym)
                    
            for sym in bitserial_syms:
                value = fields[sym]
                del fields[sym]
                for index in range(0,bitserial_count):
                    new_sym = sym.replace("BITSERIAL_", "BITSERIAL%d_" % index)
                    fields[new_sym] = value
        
    @property
    def port(self):
        return self._proxy_slave
        
    @property
    def _dynamic_port(self):
        port = None
        
        # Check the most recently used register name saved by BitField
        if self._core_register_space.cached_register_names:
            reg_name = self._core_register_space.cached_register_names[-1]
            # Check whether the register name starts with one of block names
            for block_id in self._view_ports:
                block_port = self._view_ports[block_id]
                # If match found use this block to resolve address request
                if reg_name.startswith(block_port.block_name):
                    port = block_port
                    break

        # If no match was found use the mux select register value
        # (e.g. MMU_HOST_SUBSYSTEM_BLOCK_SELECT)               
        if port is None:
            mux_select = self._mux_field_ref.read()
            iprint("Infer hostif block from mux register value: %d" % mux_select)

            try:
                port = self._view_ports[mux_select]
            except KeyError:
                raise RuntimeError("%s points to unknown subsystem 0x%02x" \
                                   % (self._mux_field_ref.info.name, mux_select))            

        # To access per-subsytem hostif register we need to give
        # trb debugger permission to substitute subsystem ID in 
        # a trb transaction address. The register that controls this
        # resides inside the HOST_SYS hostif block (which luckily
        # does not have per-subsystem registers)
        if self._trb_debugger_mask and port.block_name != "HOST_SYS":
            # we don't want any comments about these register accesses
            verbose = self._verbose
            self._verbose = False
            try:
                enabled = self._ss_in_addr_en_ref.read()
                if not (enabled & self._trb_debugger_mask):
                    self._ss_in_addr_en_ref.write(enabled | self._trb_debugger_mask)
            except AddressSpace.NoAccess:
                # Running from coredump? OK, we can live without it...
                pass
            # turn-on comments back
            self._verbose = verbose
                
        return port
        
    class _DynamicSlave(AddressSlavePort):
        """
        AddressSlave that resolves accesses by passing then straight on to 
        whichever hw block the core is currently looking at
        """

        def __init__(self, name, manager):
            
            AddressSlavePort.__init__(self, name)
            self._manager = manager
        
        def resolve_access_request(self, access_request):
            
            port = self._manager._dynamic_port
            if self._manager._verbose:
                access_request.add_comment("(%s space)" % port.block_name)
            port.resolve_access_request(access_request)
            
        def _extend_access_path(self, access_path):
            pass
            
    class _BlockMaster(AddressMasterPort):
        """
        Simple placeholder AddressMasterPort.  Knows its own name to enable
        the manager to report to clients what hw block they're actually looking
        at, which is handy to know.
        """
        
        def __init__(self, name):
            
            AddressMasterPort.__init__(self)
            self._name = name
            
        @property
        def block_name(self):
            return self._name


class AppsHifTransform(AddressSlavePort):
    """
    AddressSlave that resolves accesses to hostif registers
    from the Application subsystem. This is needed because the 
    Apps view of the registers space has to be scaled down by 4 to match
    the hostif one. The class also writes HOST_SYS_REG_ACCESS_SUBSYS_IN_ADDR_EN
    to give debugger access to per-subsystem registers.
    """

    def __init__(self, target_port, core):
        
        name = "%s_apps_view" % (target_port.name)
        
        AddressSlavePort.__init__(self, name,
                                  layout_info=core.info.layout_info)
        self._target_port = target_port
        
        # Target port's map has to be an instance of HostHwBlockView,
        # we rely on the port name below
        if not isinstance(target_port.map, HostHwBlockView):
            raise TypeError()
        
        self._trb_debugger_mask = None
        
        # Find if the target's block has per-subsystem registers,
        # we need to configure trb debugger access if this is the case.
        host_ss = core.subsystem.chip.host_subsystem
        for block, block_params in host_ss.blocks_present.items():
            if block in target_port.name:
                if block_params["ss_start"] == block_params["ss_end"]:
                    # the block does not have per-ss registers
                    break;
                # ok, there are some per-ss registers                
                self._ss_in_addr_en_ref = \
                    core.field_refs["HOST_SYS_REG_ACCESS_SUBSYS_IN_ADDR_EN"]

                # Save trb debugger id
                self._trb_debugger_mask = \
                                        (1 << core.subsystem.chip.SSID.DEBUGGER)
        
    def _build_address_lookup(self):
        # Builds a dictionary with address as the key and
        # register reference object as the key value.
        addr_dict = {}
        field_refs = self._target_port.map.field_refs
        
        for reg_name in field_refs.keys():
            reg_ref = field_refs[reg_name] 
            addr_dict[reg_ref.start_addr] = reg_ref
                
        return addr_dict
        
    def _find_register_by_addr(self, addr):
        # Lookup a register by its start_addr
        try:
            return self._address_lookup[addr]
        except (AttributeError, KeyError) as e:
            if isinstance(e, AttributeError):
                self._address_lookup = self._build_address_lookup()
                return self._find_register_by_addr(addr)
        # not found
        return None
        
    def resolve_access_request(self, access_request):
        # To access per-subsytem hostif register we need to give
        # trb debugger permission to substitute subsystem ID in 
        # a trb transaction address.
        if self._trb_debugger_mask is not None:
            try:
                enabled = self._ss_in_addr_en_ref.read()
                if not (enabled & self._trb_debugger_mask):
                    self._ss_in_addr_en_ref.write(enabled | self._trb_debugger_mask)
            except AddressSpace.NoAccess:
                # Running from coredump? OK, we can live without it...
                pass

        rq = access_request

        # To make things simpler we don't support reading of more than one
        # register in a single request and unaligned requests.
        if rq.region.start & 0x3:
            raise ValueError("Apps hostif access requests have to be "
                             "4-bytes aligned, range %s" %   str(rq.region))
        if rq.region.length != 4:
            raise ValueError("Apps hostif access requests can't access "
                             "more than one register at a time, "
                             "range %s" % str(rq.region))

        # Access to a single hostif register from Apps may trigger read/write
        # of one or two words in hostif, depending on the width of the register.
        # For example USB2_EP_CONFIG_RX_FREE_SPACE_IN_BUFFER is 24-bit wide,
        # so we have to read 2 words from the hostif. The majority are 16 bits
        # wide or less, meaning only one word needs to be accessed.
        reg_ref = self._find_register_by_addr(rq.region.start)
        if reg_ref:
            new_length = (reg_ref.num_bits + 15) // 16
        else:
            new_length = 1
                
        # build new request with the register address divided by 4
        # and length depending on how wide is the register,
        # pack/unpack the request data accordingly
        new_start = rq.region.start // 4
        new_range = AddressRange(new_start, new_start + new_length)

        if isinstance(rq, WriteRequest):
            data = bitsandbobs.build_le(rq.data, 8)
                
            new_rq = WriteRequest(new_range, [data])
        elif isinstance(rq, ReadRequest):
            new_rq = ReadRequest(new_range)
        
        self._target_port.execute(new_rq)
        
        if isinstance(rq, ReadRequest):
            data = bitsandbobs.build_be(new_rq.data, 16)
            rq.data = bitsandbobs.flatten_le(data, 4, 8)
