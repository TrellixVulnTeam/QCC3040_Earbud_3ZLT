############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from collections import namedtuple

from .port_connection import SlavePort, MasterPort, PortConnection
from .address_space import AccessPath as AddressAccessPath, AddressRange

class MuxedAccessRequest(object):
    """
    Generic access request wrapper that includes muxing information of some sort
    of other.  Typically used via a subclass that returns self.mux_select under
    a context-relevant name.
    """
    def __init__(self, basic_request, mux_select):
        
        self.basic_request = basic_request
        self.mux_select = mux_select
        
    def __getattr__(self, attr):
        return getattr(self.basic_request, attr)
        
class NoComponentDebugSupport(TypeError):
    """
    A component that we're attempting to wire up some kind of debug support for
    doesn't provide the interface we're looking for.
    """

class DebugBusMux(object):
    """
    Generic logic for routing access requests through a generic debug bus mux,
    for example a Hydra chip's TRB interface, which has a separate debug bus
    for each subsystem.  This base class is parametrised with three quantities:
     DEBUG_MUX_ID_NAME - name of the muxed component attribute that contains 
                         the component's mux ID (e.g. for a HydraSubsystem, 
                         this is "id").  The DebugBusMux may be told the IDs of
                         the components at construction time, in which case this
                         attribute isn't required.
     DEBUG_PORT_NAME - name of the muxed component attribute that contains the
                       component's debug slave port (e.g. for a HydraSubsystem,
                       this is "trb_in")
     ALT_DEBUG_PORT_NAME - alternative name of the muxed component attribute that
                           contains the component's debug slave port, for cases
                           where there are two different kinds of component
                           to be muxed together 
     MUXED_ACCESS_REQUEST - subclass of MuxedAccessRequest to instantiate when
                            pass access requests outward through the mux.
                            
    """
    
    class ChipDebugPort(SlavePort):
        """
        The multiplexed debug bus slave: processes multiplexed access requests
        which combine a normal bus-orientated access request (address range[, data])
        with a mux selection (e.g. subsystem bus ID)
        """
        def __init__(self, mux):
            SlavePort.__init__(self)
            self.mux = mux
    
        def _extend_access_path(self, access_path):
            for m in self.mux.masters.values():
                m.extend_access_path(access_path)
                
        def resolve_access_request(self, muxed_request):
            basic_request = muxed_request.basic_request
            master = self.mux.masters[muxed_request.subsys]
            master.resolve_access_request(basic_request)

    class DebugBusMaster(MasterPort):
        """
        Bus master for a specific mux component's bus: processes de-/pre-multiplexed
        access request destined for/originating from this component's debug bus
        """
        def __init__(self, mux, component, cmpt_id=None):
            
            MasterPort.__init__(self)
            self.mux = mux
            self._id = getattr(component, mux.DEBUG_MUX_ID_NAME) if cmpt_id is None else cmpt_id
            if mux.DEBUG_PORT_NAME is not None:
                try:
                    port = getattr(component, mux.DEBUG_PORT_NAME)
                except AttributeError:
                    raise NoComponentDebugSupport
            else:
                port = component
            self._auto_connection = PortConnection(self, port)
            self._cmpt = component
        
        def execute_outwards(self, access_request):
            """
            Replace the access request with a wrapper that indicates the
            subsystem ID
            """
            rq = self.mux.MUXED_ACCESS_REQUEST(access_request, self._id)
            self.mux.port.execute_outwards(rq)

        def extend_access_path(self, access_path):
            access_path.create_simple_fork(self)
            self._cmpt.has_data_source = True

        
    @property
    def port(self):
        return self._port

    @property 
    def masters(self):
        return self._cmpt_masters

class SimpleDebugBusMux(DebugBusMux):

    def __init__(self, components, ids=None):
        
        self._port = self.ChipDebugPort(self)
        
        self._cmpt_masters = {}
        if ids is None:
            # The ids are obtained from a named attribute of the component
            ids = [getattr(cmpt, self.DEBUG_MUX_ID_NAME) for cmpt in components]

        for cmpt,cmpt_id in zip(components, ids):
            try:
                master = self.DebugBusMaster(self, cmpt, cmpt_id=cmpt_id)
            except NoComponentDebugSupport:
                pass
            else:
                self._cmpt_masters[cmpt_id] = master

        

    
class TrbAccessRequest(MuxedAccessRequest):
    """
    Tweaks the interface of a generic MuxedAccessRequest to use the language of
    TRB
    """
    @property
    def subsys(self):
        return self.mux_select

class TrbSubsystemMux(SimpleDebugBusMux):
    """
    Concrete mux for per-subsystem transaction bridge access
    """
    DEBUG_MUX_ID_NAME = "id"
    DEBUG_PORT_NAME = "trb_in"
    MUXED_ACCESS_REQUEST = TrbAccessRequest
    
    class DebugBusMaster(DebugBusMux.DebugBusMaster):
        def extend_access_path(self, access_path):
            fork = AddressAccessPath(self._id,
                                     access_path.rank+1, self,
                                     AddressRange(0,1<<32))
            access_path.add_fork(fork)
            self._cmpt.has_data_source = True

    
class GdbserverAccessRequest(MuxedAccessRequest):
    """
    """
    
class GdbserverMux(SimpleDebugBusMux):
    """
    Passes high-level gdbserver-orientated requests on to the transport with
    an attached ID
    """
    DEBUG_MUX_ID_NAME = "name"
    DEBUG_PORT_NAME = "debug_controller"
    MUXED_ACCESS_REQUEST = GdbserverAccessRequest
    

TapMemoryAccessInfo = namedtuple("TapMemoryAccessInfo", "desc ir_length direct_to_mem")
class TapMemoryAccessType:
    DAP      = TapMemoryAccessInfo(desc="dap", ir_length=4, direct_to_mem=False)  # it exposes a 
                # mux not (directly) an address bus 
    JTAG2AHB = TapMemoryAccessInfo(desc="jtag2ahb", ir_length=11, direct_to_mem=True) # as far as 
                # we're concerned, it exposes an address space directly


class ARMDAPAccessRequest(MuxedAccessRequest):
    """
    Tweaks the interface of a generic MuxedAccessRequest to use the language of
    Jtag
    """    
    @property
    def ap(self):
        return self.mux_select

class ARMDAP(DebugBusMux):
    """
    Concrete mux for chips accessed via an ARM-style Debug Access Port
    """
    DEBUG_MUX_ID_NAME = "name"
    DEBUG_PORT_NAME = "mem_ap"
    MUXED_ACCESS_REQUEST = ARMDAPAccessRequest
    info = TapMemoryAccessType.DAP
    
    APIndex = namedtuple("APIndex", "ap_number ap_type")

    def __init__(self, components, ap_mapping, properties=None):
        """
        :param components: Iterable of objects that this mux muxes together
         (e.g. subsystems/cores)
        :param ap_mapping: Mapping from the name of a component to the details of 
         its associated Access Port, in the form of an ARMDAP.ApIndex
        :param properties: Optional dictionary of properties of the DAP that
         might be useful for the hardware connecting to it.  Supported properties
         are "speed_khz" and "interface", the latter being either "jtag" or "swd".
        """

        self._port = self.ChipDebugPort(self, properties)
        
        self._cmpt_masters = {}
        for cmpt in components:
            mux_id = getattr(cmpt, self.DEBUG_MUX_ID_NAME)
            try:
                master = self.DebugBusMaster(self, cmpt, 
                                             ap_mapping[mux_id])
            except NoComponentDebugSupport:
                pass
            else:
                self._cmpt_masters[mux_id] = master

    def get_conn_type(self, name):
        return self.info.desc
        
    class DebugBusMaster(DebugBusMux.DebugBusMaster):

        def __init__(self, mux, component, target_ap):
            DebugBusMux.DebugBusMaster.__init__(self, mux, component)
            self._target_ap = target_ap

        def extend_access_path(self, access_path):
            fork = AddressAccessPath(self._id,
                                     access_path.rank+1, self,
                                     AddressRange(0,1<<32))
            access_path.add_fork(fork)
            self._cmpt.has_data_source = True

        def execute_outwards(self, access_request):
            """
            Multiplex the given access request into the DAP by adding a wrapper
            specifying the AP index that it should be delivered to, plus the
            type of AP that it is.
            """
            rq = self.mux.MUXED_ACCESS_REQUEST(access_request, self._target_ap)
            self.mux.port.execute_outwards(rq)

    class ChipDebugPort(DebugBusMux.ChipDebugPort):
        
        def __init__(self, mux, properties):
            
            DebugBusMux.ChipDebugPort.__init__(self, mux)
            self._properties = properties or {}
            self.tap_type = mux.info
            
        def get_properties(self):
            return self._properties

        @property
        def targetsel(self):
            """
            Retrieve this DAP's ID within the multidrop set-up.  This needs to
            be set explicitly for the context (typically it is set at the device
            level to distinguish the DAPs of multiple chips on the device)
            """
            try:
                return self._targetsel
            except AttributeError:
                raise TypeError("Attempting to use DAP in multidrop context but "
                                "no targetsel attribute has been assigned")

        def assign_targetsel(self, targetid, instance):
            """
            Set the multidrop ID for this DAP
            """
            self._targetsel = (targetid, instance)
            

class MultidropAccessRequest(MuxedAccessRequest):
    """
    Request for a multidrop access: needs to provide the Multidrop ID
    """
    @property
    def targetsel(self):
        return self.mux_select
    
    
class SWDMultidrop(SimpleDebugBusMux):
    
    DEBUG_MUX_ID_NAME = "targetsel"
    DEBUG_PORT_NAME = None
    MUXED_ACCESS_REQUEST = MultidropAccessRequest

    class ChipDebugPort(DebugBusMux.ChipDebugPort):
        
        def get_properties(self):
            # We expect the properties to be identical in all the underlying
            # logical SWD ports so just return one of them
            try:
                return self.mux.masters[0].slave.get_properties()
            except KeyError:
                return {}

    def get_conn_type(self, name):
        # we know that all the targets in an SWD multidrop are DAPs.
        return "dap"


class MultitapAccessRequest(MuxedAccessRequest):
    """
    Request for a multidrop access: needs to provide the Multidrop ID
    """
    @property
    def tap_name(self):
        return self.mux_select


class DisabledTAP(ValueError):
    """
    The TAP required to access the requested area of the chip is currently disabled
    """

class ConfigurableMultitap(DebugBusMux):
    """
    Represents a configurable JTAG scan chain where any of several TAPs can
    be present in the chain.

    Which TAPs are currently present in the chain affects which access requests are
    allowed to pass through towards the debug port.  It also affects the scan
    chain settings that need to be applied by the transport driver.  These are
    exposed by some extra API calls.
   """

    DEBUG_MUX_ID_NAME = "tap_name"
    DEBUG_PORT_NAME = None
    MUXED_ACCESS_REQUEST = MultitapAccessRequest

    class DebugBusMaster(DebugBusMux.DebugBusMaster):

        def __init__(self, mux, component, tap_name):
            # Label the muxed component (should this attribute be intrinsic to the object instead?)
            component.tap_name = tap_name
            DebugBusMux.DebugBusMaster.__init__(self, mux, component)
            self._tap_name = tap_name
            self._addressable_slave = component.mux.info.direct_to_mem

        def execute_outwards(self, access_request):
            """
            Multiplex the given access request into the DAP by adding a wrapper
            specifying the AP index that it should be delivered to, plus the
            type of AP that it is.
            """
            # Can only forward the request if the request TAP is enabled
            if not self.mux.is_enabled(self._tap_name):
                raise DisabledTAP("'{}' is not enabled".format(self._tap_name))
            rq = self.mux.MUXED_ACCESS_REQUEST(access_request, self._tap_name)
            self.mux.port.execute_outwards(rq)

        def extend_access_path(self, access_path):
            if self._addressable_slave:
                fork = AddressAccessPath(self._id,
                                        access_path.rank+1, self,
                                        AddressRange(0,1<<32))
                access_path.add_fork(fork)
                self._cmpt.has_data_source = True
            else:
                DebugBusMux.DebugBusMaster.extend_access_path(self,access_path)

    class ChipDebugPort(DebugBusMux.ChipDebugPort):

        def __init__(self, mux, properties):
            
            DebugBusMux.ChipDebugPort.__init__(self, mux)
            self._properties = properties or {}
        def get_properties(self):
            return self._properties
        def reset_enabled(self, tap_names):
            self.mux.reset_enabled(tap_names)

    def __init__(self, taps, enabled=None, properties=None):
        """
        Based around a dictionary of named TAPs with associated IR lengths
        """

        self._port = self.ChipDebugPort(self, properties=properties)
        
        self._tap_info = {tap_name : tap.mux.info for (tap_name, tap) in taps.items()}
        self._cmpt_masters =   {tap_name : self.DebugBusMaster(self, tap, tap_name)
                                                              for (tap_name, tap) in taps.items()}
        self._enabled = enabled if enabled is not None else []

    def is_enabled(self, tap_name):
        return tap_name in self._enabled

    def reset_enabled(self, tap_names):
        """
        Pass in a list of TAP names to indicate which are enabled and which
        order they are in in the scan chain.
        """
        self._enabled = tap_names

    def get_scan_chain_settings_of(self, tap_name, all=False):
        """
        Computes the scan chain settings of the named tap based on which TAPs are
        enabled and what their IR lengths are.
        """
        if not all and tap_name not in self._enabled:
            return None

        tap_index = self._enabled.index(tap_name)

        settings = {}
        settings["DRPost"] = tap_index
        settings["DRPre"] = len(self._enabled) - tap_index - 1
        settings["IRPost"] = sum(self._tap_info[tap_name].ir_length for tap_name in self._enabled[:tap_index])
        settings["IRPre"] = sum(self._tap_info[tap_name].ir_length for tap_name in self._enabled[tap_index+1:])
        settings["IRLength"] = self._tap_info[tap_name].ir_length

        return settings

    def get_tap_type(self, tap_name):
        return self._tap_info[tap_name].desc

    def get_conn_type(self, name):
        return self.get_tap_type(name)

class JTAGTAPAccessRequest(MuxedAccessRequest):
    pass

class JTAGTAP(SimpleDebugBusMux):
    """
    Debug mux representing a JTAG TAP.  Contains additional information giving the
    IR length, the type of hardware the TAP is connected to, and
    a flag saying whether that hardware is a memory space or not (typically if not it is
    a mux of memory spaces, like a DAP, but there are other possibilities.)
    """
    DEBUG_MUX_ID_NAME = "tap_type"
    DEBUG_PORT_NAME = "data"
    MUXED_ACCESS_REQUEST = JTAGTAPAccessRequest

    def __init__(self, components, info):
        SimpleDebugBusMux.__init__(self, components)
        self.info = info

    def get_conn_type(self, name):
        return self.info.desc

class ProcessMux(SimpleDebugBusMux):
    
    DEBUG_MUX_ID_NAME = "pid"
    DEBUG_PORT_NAME = "debug_controller"
    MUXED_ACCESS_REQUEST = ARMDAPAccessRequest # because that's what gdbserver needs at the moment


class TcMemWindowedRequest(MuxedAccessRequest):
    """
    Request for a windowed memory access.  Just needs to indicate the subsystem.
    """
    @property
    def subsys(self):
        return self.mux_select


class TcMemWindowed(SimpleDebugBusMux):
    """
    Requests for windowed toolcmd memory access go through this mux and are
    presented to the toolcmd-based transport (e.g. low-cost debug). 
    """
    DEBUG_MUX_ID_NAME = "id"
    DEBUG_PORT_NAME = "tc_in"
    MUXED_ACCESS_REQUEST = TcMemWindowedRequest
    
    class DebugBusMaster(DebugBusMux.DebugBusMaster):
        def extend_access_path(self, access_path):
            for name, m in self.mux.masters.items():
                fork = AddressAccessPath(name,
                                         access_path.rank+1, self,
                                         AddressRange(0,1<<24))
                access_path.add_fork(fork)
                self._cmpt.has_data_source = True

    
class TcMemRegBasedRequest(MuxedAccessRequest):
    """
    Request for a reg-based memory access.  Just needs to indicate the subsystem.
    """
    @property
    def subsys(self):
        return self.mux_select
    
class TcMemRegBased(SimpleDebugBusMux):
    """
    Requests for register-based toolcmd memory access go through this mux and 
    are presetnted to the toolcmd-based transport (i.e. low-cost debug)
    """
    DEBUG_MUX_ID_NAME = "id"
    DEBUG_PORT_NAME = "tc_in" # 
    MUXED_ACCESS_REQUEST = TcMemRegBasedRequest

    class DebugBusMaster(DebugBusMux.DebugBusMaster):
        def extend_access_path(self, access_path):
            for name, m in self.mux.masters.items():
                fork = AddressAccessPath(name,
                                         access_path.rank+1, self,
                                         AddressRange(0,1<<32))
                access_path.add_fork(fork)
                self._cmpt.has_data_source = True

class AudioAccessRequest(MuxedAccessRequest):
    """
    Requests for AudioAccess accesses must specify the core number 
    """
    @property
    def core_num(self):
        return self.mux_select

class AudioAccessCoreMux(SimpleDebugBusMux):
    """
    Muxes the debug_controllers from multiple (Audio) cores
    with the AudioAccess core number as selector.
    Caller must pass the core numbers of the muxed cores into
    the constructor.
    """
    DEBUG_PORT_NAME = "debug_controller"
    MUXED_ACCESS_REQUEST = AudioAccessRequest