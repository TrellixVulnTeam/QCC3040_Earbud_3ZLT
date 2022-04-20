############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .system_message import SystemMessage as SysMsg
from csr.wheels.bitsandbobs import NameSpace

try:
    long
except NameError:
    long = int

class BluestackUtils(object):
    """
    General bluestack-related support for the trap API
    """
    def __init__(self, trap_utils):
        
        self._utils = trap_utils
        self._bluestack_hdlrs = {"att" : None,
                                 "dm" : None,
                                 "l2cap" : None,
                                 "rfcomm" : None,
                                 "sdp" : None}
        
    def set_bluestack_handler(self, att=None, dm=None, l2cap=None, rfcomm=None,
                              sdp=None, mdm=None):
        """
        Set up the internal list of handlers for individual bluestack protocols
        and create a Task to receive them all
        """
        if att is not None:
            self._bluestack_hdlrs["att"] = att
        if dm is not None:
            self._bluestack_hdlrs["dm"] = dm
        if l2cap is not None:
            self._bluestack_hdlrs["l2cap"] = l2cap
        if rfcomm is not None:
            self._bluestack_hdlrs["rfcomm"] = rfcomm
        if sdp is not None:
            self._bluestack_hdlrs["sdp"] = sdp
        if mdm is not None:
            self._bluestack_hdlrs["mdm"] = mdm
        
        # Create a new task for handling bluestack messages
        task = self._utils.handle_msg_type("BlueStack", 
                                           handler=self.bluestack_handler)
        if att is not None:
            # Extend the task to handle ATT messages
            self._utils.handle_msg_type("Att", task=task)
        if mdm is not None:
            self._utils.handle_msg_type("Mdm", task=task)
        
        return task

    def bluestack_handler(self, msg):
        """
        Standard forwarding handler for Bluestack primitives
        """
        if msg["id"] == SysMsg.BLUESTACK_ATT_PRIM:
            if self._bluestack_hdlrs["att"]:
                self._bluestack_hdlrs["att"](msg)
        elif msg["id"] == SysMsg.BLUESTACK_DM_PRIM:
            if self._bluestack_hdlrs["dm"]:
                self._bluestack_hdlrs["dm"](msg)
        elif msg["id"] == SysMsg.BLUESTACK_L2CAP_PRIM:
            if self._bluestack_hdlrs["l2cap"]:
                self._bluestack_hdlrs["l2cap"](msg)
        elif msg["id"] == SysMsg.BLUESTACK_RFCOMM_PRIM:
            if self._bluestack_hdlrs["rfcomm"]:
                self._bluestack_hdlrs["rfcomm"](msg)
        elif msg["id"] == SysMsg.BLUESTACK_SDP_PRIM:
            if self._bluestack_hdlrs["sdp"]:
                self._bluestack_hdlrs["sdp"](msg)
        elif msg["id"] == SysMsg.BLUESTACK_MDM_PRIM:
            if self._bluestack_hdlrs["mdm"]:
                self._bluestack_hdlrs["mdm"](msg)

    def create_tp_bdaddr(self, bdaddr=None, ble=False, type=0):
        """
        Construct a tp_bdaddr object containing the given bd_addr passing 
        memory ownership to the caller
        
        If bdaddr == False, the fields are not filled in
        If bdaddr is None, the fields are set by calling self.get_local_bdaddr()
        Otherwise, bdaddr can either be an integer or an object with integral
        attributes "lap", "uap" and "nap"
        """
        if bdaddr == False:
            return self._utils.apps1.fw.call.pnew("tp_bdaddr")
        
        if bdaddr is None:
            bdaddr = self.get_local_bdaddr()
        
        if isinstance(bdaddr, (int, long)):
            lap = bdaddr & 0xffffff
            uap = bdaddr>>24 & 0xff
            nap = bdaddr>>32 & 0xffff
        else:
            lap = bdaddr.lap
            uap = bdaddr.uap
            nap = bdaddr.nap
            
        bd_addr_obj = self._utils.apps1.fw.call.pnew("tp_bdaddr")
        bd_addr_obj.taddr.addr.lap.value = lap
        bd_addr_obj.taddr.addr.uap.value = uap
        bd_addr_obj.taddr.addr.nap.value = nap
        if ble:
            bd_addr_obj.transport.value = 1
        bd_addr_obj.taddr.type.value = type
        return bd_addr_obj 

    def create_bdaddr(self, bdaddr=None):
        """
        Construct a bdaddr object containing the given bd_addr passing 
        memory ownership to the caller

        If bdaddr == False, the fields are not filled in
        If bdaddr is None, the fields are set by calling self.get_local_bdaddr()
        Otherwise, bdaddr can either be an integer or an object with integral
        attributes "lap", "uap" and "nap"
        """
        if bdaddr == False:
            return self._utils.apps1.fw.call.pnew("bdaddr")

        if bdaddr is None:
            bdaddr = self.get_local_bdaddr()

        if isinstance(bdaddr, (int, long)):
            lap = bdaddr & 0xffffff
            uap = bdaddr>>24 & 0xff
            nap = bdaddr>>32 & 0xffff
        else:
            lap = bdaddr.lap
            uap = bdaddr.uap
            nap = bdaddr.nap

        bd_addr_obj = self._utils.apps1.fw.call.pnew("bdaddr")
        bd_addr_obj.lap.value = lap
        bd_addr_obj.uap.value = uap
        bd_addr_obj.nap.value = nap

        return bd_addr_obj 

    def get_local_bdaddr(self, as_int=False):
        """
        Go and request the local bluetooth address.  We probably need to be
        already registered with DM for this to work
        """
        from csr.apps.libs.dm import DMLib
        dmlib = DMLib(self._utils, linear=True)
        new_task = self._utils.handle_msg_type("BlueStack")
        bd_addr = dmlib.local_bd_addr
        if new_task != False:
            self._utils.remove_msg_type_task("BlueStack")
        if as_int:
            return bdaddr_to_int(bd_addr)
        return bd_addr

    def override_bdaddr(self, bd_addr):
        """
        Override the local bluetooth address, bd_addr can either be an integer
        or an object with integral attributes "lap", "uap" and "nap". 
        If bdaddr is an integer then convert to an object before overriding.
        """
        if isinstance(bd_addr, (int, long)):
            bd_addr_obj = self.create_bdaddr(bd_addr)
        else:
            bd_addr_obj = bd_addr

        return self._utils.apps1.fw.call.VmOverrideBdaddr(bd_addr_obj)

def bdaddr_to_int(bdaddr):
    """
    Convert a bdaddr to an integer - can either be an integer to start with,
    in which case it is returned unchanged, or an arbitrary object with 
    lap, uap and nap attributes which are themselves integers.
    """
    if isinstance(bdaddr, (int, long)):
        return bdaddr
    else:
        return (bdaddr.nap << 32 | 
                bdaddr.uap << 24 | 
                bdaddr.lap)
