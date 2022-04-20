############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
class IDeviceConnection:
    """\
    CSR Device Debug Connection (Interface)    
    """
    class AccessFailure (RuntimeError):
        """Raised when an access request fails"""
        
    class ReadFailure (AccessFailure):
        """Raised when a synchronous read request fails"""
        
    class WriteFailure (AccessFailure):
        """Raised when a synchronous write request fails"""
        
    def detect_chip_version(self):
        """\
        Detect chip version or None.
        """
        raise NotImplementedError()
    
    def attach(self, device):
        """
        This connection should construct an AccessGraph wrt the 
        specified Device's memory model.
        
        This is called once the type of attached device 
        has been inferred by some omniscient power (or just based on
        calling .detect_chip_version in simple cases).
        
        This involves walking inwards from the respective port (default if none
        specified) constructing a directed access graph with an AccessNode 
        registered with each reachable AddressSpace.
        
        The path should be extended through all exclusively owned address
        muxes and all fixed mappings.

        AddressSpaces use the set of registered nodes to determine and
        invoke the best access path to server application requests.
        """
        raise NotImplementedError()
