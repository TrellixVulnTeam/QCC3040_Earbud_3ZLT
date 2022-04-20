############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from weakref import WeakSet
from ...wheels.bitsandbobs import TypeCheck

class AccessFailure (RuntimeError):
    """\
    Raised when a synchronous access request fails for any reason
    """
    
class NoAccess (AccessFailure):
    """\
    Raised when there's no known way to access the addressed memory 
    (e.g no SPI plugged in)
    """

class AmbiguousAccess(AccessFailure):
    """
    Raised when there's more than one way to send the same type of transaction.
    """


class ReadFailure (AccessFailure):
    """\
    Raised when a synchronous read request fails even though there is a
    path.
    """
    
class WriteFailure (AccessFailure):
    """\
    Raised when a synchronous write request fails even though there is a
    path.
    """

class ReadFailureSubsystemInaccessible(ReadFailure):
    """
    The access reached the chip-level transport but failed at the subsytem level
    """
class WriteFailureSubsystemInaccessible(WriteFailure):
    """
    The access reached the chip-level transport but failed at the subsytem level
    """
class ReadFailureDUTInaccessible(ReadFailure):
    """
    The access reached the transport driver/hardware but failed at the DUT level
    """
class WriteFailureDUTInaccessible(WriteFailure):
    """
    The access reached the transport driver/hardware but failed at the DUT level
    """
class ReadFailureLinkInvalid(ReadFailure):
    """
    The access couldn't be serviced by the transport layer because the logical
    link was unusuable.  Implies a transport reset might be required.
    """
class WriteFailureLinkInvalid(WriteFailure):
    """
    The access couldn't be serviced by the transport layer because the logical
    link was unusuable.  Implies a transport reset might be required.
    """
class ReadFailureDeviceChanged(ReadFailure):
    """
    The response to the read was received from a different device to the one
    at link creation
    """
class WriteFailureDeviceChanged(WriteFailure):
    """
    The response to the write  was received from a different device to the one
    at link creation
    """


class NetPort (object):
    """\
    Connection point in a network model.
    """    
    def __init__(self):
        
        self._connections = set()
    
    @property
    def connections(self):
        """\
        The set of foreign NetPorts connected to this one.
            
        Do not manipulate directly. The population is the responsibility of
        NetConnection objects.
        """
        return self._connections


class NetConnection (object):
    """
    Represents binary contact of a pair of NetPorts in a network model.

    Connections may or may not be asymmetrical. In fact there not fussy about
    the port types at all (but derived types typically are).
    
    Does not extend this to represent a bus - if we need one we will build one
    - e.g. in terms of multiple ports - don't stretch this class please.
    """
    def __init__(self, port_a, port_b):
        
        self._a = port_a
        self._b = port_b
        self._make()
    
    def _make(self):
        
        self._a.connections.add(self._b)
        self._b.connections.add(self._a)

    def _break(self):
        
        self._a.connections.remove(self._b)
        self._b.connections.remove(self._a)

class UnconnectedMasterPort(RuntimeError):
    """
    Raised when a MasterPort is used to resolve an access request without having
    a slave attached at all.
    """

class MasterPort(NetPort):
    """
    Model of a generic master port, memory-related or otherwise
    """

    def __init__(self):

        NetPort.__init__(self) 
    
    @property
    def slave(self):
        """\
        Connected slave port.
        """
        # should be exactly one connection - to slave port
        for slave in self.connections: return slave
    
    def execute_outwards(self, access_request):
        """\
        Execute the memory access request (synchronously).
        
        Not called directly by applications. This is used internally to route
        access requests outwards recursively towards a debug connection.
        
        This must be specialised by implementations.
        
        Specialisation typically involves mapping the request outwards through
        this component (mux or map), ensuring the access path through the
        component is configured correctly (ie. set the mux).
        
        The request will finally hit a host debug port (SPI/TRB) which should
        physically serve the request by calling the respective transport
        driver.
        
        This is only called when there is known to be a direct access path via
        this port (because there is an associated AccessPath).        
        """
        raise PureVirtualError(self)

    def resolve_access_request(self, access_request):
        """\
        Pass the resolution on via the connected AddressSlavePort.
        """
        if self.slave is None:
            raise UnconnectedMasterPort("MasterPort not connected")
        self.slave.execute(access_request)

    def handles_view(self, view):
        """
        By default, an MasterPort ignores the view information that a slave port
        might care about.
        """
        return True

class SlavePort(NetPort):
    """
    Abstract slave port: defines the resolve_access_request/execute_outwards
    interface
    Also provides a concrete facility to let a MasterPort register itself as 
    such for this slave by registering an AccessPath object, and provides a 
    default implementation of execute_outwards that passes the access request 
    out along the access paths it has.  If that doesn't make sense, this
    method should be overridden.  
    """
    def __init__(self):
        NetPort.__init__(self)
        self._access_paths = {}

    def _store_path_as(self, access_path, atype):
        self._access_paths.setdefault(atype, WeakSet()).add(access_path)
        
    def register_access_path(self, access_path):
        """\
        Registers an active debug AccessPath with this port/space.
        
        If the associated component is a map or mux then the path will be
        extended, recursively, all AddressSpaces reachable via the component.        
        """
        self._store_path_as(access_path, access_path.type)
        if access_path.type == AccessType.ANY:
            # Also store as each individual type for faster look-up
            for atype in (AccessType.MEMORY, AccessType.REGISTERS, AccessType.RUN_CTRL, AccessType.MISC):
                self._store_path_as(access_path, atype)
            
        self._extend_access_path(access_path)
            
    def resolve_access_request(self, access_request):
        raise PureVirtualError()

    def _get_supporting_access_path(self, rq):
        
        paths = self._access_paths.get(rq.type)
        if paths is None:
            return None
        paths = list(paths) 
        if len(paths) > 1:
            raise AmbiguousAccess
        return paths[0]
        

    def execute_outwards(self, access_request):
        """
        Default implementation that assumes there is just one connection to 
        which the access request can be passed directly
        """
        if not self._access_paths.get(access_request.type):
            # Potential extension: A bit ugly using AddressSpace here
            raise NoAccess("No (suitable) masters are connected to this slave")
        for path in self._access_paths[access_request.type]:
            path.execute_outwards(access_request)
            break # If there are multiple paths, just use one of them


class AccessType(object):
    REGISTERS = 0
    MEMORY = 1
    RUN_CTRL = 2
    MISC = 3
    ANY=99
    


class AccessPath(object):
    
    trace = False
    
    
    
    """
    
    """
    def __init__(self, rank, master_port, type=AccessType.ANY):
        self.master = master_port
        self.rank = rank
        self.forks = set()
        self.type = type

    def extend(self, trace=False):
        """\
        Extend this AccessPath to all directly and indirectly reachable 
        AddressSlavePorts. (fluent)
        """
        if trace:
            self.trace = trace

        slave = self.master.slave
        if slave is not None:
            # Register with the connected address slave. The slave will
            # propagate the path inwards if appropriate and call add_fork() to
            # register any new forks it might make.
            if self.trace:
                self.debug_trace(slave)
            slave.register_access_path(self)

        if trace:
            self.trace = False
            
        return self # fluent

    def debug_trace(self, slave):
        pass

    def add_fork(self, new_fork):
        """\
        Adds a new fork to this AccessPath, and extends it.
        
        Slave ports that propagate AccessPaths inwards should call this back to
        register and extend any new forks they create.
        
        This double dispatch protocol keeps the details of components that
        implement forks (e.g. Maps & MUXes) encapsulated.
        
        See also: create_simple_fork() for helper.
        """
        new_fork.extend()
        self.forks.add(new_fork)
    
    def create_simple_fork(self, master_port):
        """\
        Forks and extends this AccessPath via the specified AddressMasterPort
        without any adjustments (e.g. to the AddressRange).
        
        This can be used by AddressSlavePorts to create new forks in the common
        case where no path splitting or adjustment is needed. AddressMaps are
        an exception as the path must split and narrow through each mapped
        region.
        """
        new_fork = self.clone(master_port, self.rank + 1)
        self.add_fork(new_fork)

    def clone(self, master_port, new_rank):
        return self.__class__(new_rank, master_port)
        
    def execute_outwards(self, access_request):
        """\
        Execute specified AccessRequest via this path.        
        """
        # Delegate to the corresponding AddressMasterPort.
        self.master.execute_outwards(access_request)


class PortConnection(NetConnection):
    def __init__(self, addresser_port, addressee_port):

        # Reuse generic NetConnection but restrict the port types        
        NetConnection.__init__(self, addresser_port, addressee_port)
