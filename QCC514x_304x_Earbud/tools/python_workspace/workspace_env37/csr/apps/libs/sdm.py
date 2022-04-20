############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels.bitsandbobs import NameSpace
from csr.dev.fw.trap_api.system_message import SystemMessage as SysMsg
from csr.dev.fw.trap_api.trap_utils import UnexpectedMsgType
from csr.dev.env.env_helpers import var_address, var_size
from csr.dwarf.read_dwarf import DwarfNoSymbol
import logging
import sys

# Py2/3 compatibility
if sys.version_info > (3,):
    int_type = int
else:
    int_type = (int, long)


class SdmUnexpectedPrim(RuntimeError):
    """
    Indicates an unexpected SDM prim was seen
    """
    
class SDMConst(object):
    LINK_TYPE_ACL       = 0x1
    LINK_TYPE_SCO       = 0x2
    REASON_CONN_TERM_LOCAL_HOST = 0x16
    PHANDLE = 0x00
    
class SDMException(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)
    
class SDMAppMsgs(object):
    # Messages that a controlling App might need to receive
    REGISTRATION_COMPLETE = 0
    SDM_SHADOW_ACL_LINK_CREATE_CFM = 1
    SDM_SHADOW_ACL_LINK_CREATE_CFM_FAILED = 2
    SDM_SHADOW_ACL_LINK_CREATE_IND = 3
    SDM_SHADOW_ACL_LINK_CREATE_IND_FAILED = 4
    SDM_SHADOW_LINK_DISCONNECT_CFM = 5
    SDM_SHADOW_ACL_LINK_DISCONNECT_IND = 6
    SDM_SHADOW_ESCO_LINK_CREATE_CFM = 7
    SDM_SHADOW_ESCO_LINK_CREATE_CFM_FAILED = 8
    SDM_SHADOW_ESCO_LINK_CREATE_IND = 9
    SDM_SHADOW_ESCO_LINK_CREATE_IND_FAILED = 10
    SDM_SET_BREDR_SLAVE_ADDRESS_IND = 11
    SDM_SET_BREDR_SLAVE_ADDRESS_IND_FAILED = 12
    SDM_SET_BREDR_SLAVE_ADDRESS_CFM = 13
    SDM_SET_BREDR_SLAVE_ADDRESS_CFM_FAILED = 14

class SDMLib(object):
    """
    Library supporting access to the SDM-related trap API functionality in the
    firmware, such as
     - creating and sending SDM primitives via VmSendSdmPrim
     - getting and interpreting SDM_PRIM messages
    """

    def __init__(self, trap_utils, linear=False, app_task=None, verbose=True, 
                 tag="", esco=True, logger=None):
        
        if logger == None:
            self._log = logging.getLogger(tag if tag else "sdm")
            self._log.setLevel(logging.INFO if verbose else logging.WARNING)
        else:
            self._log = logger

        hdlr = logging.StreamHandler()
        hdlr.setFormatter(logging.Formatter('%(name)s: %(message)s'))
        hdlr.setLevel(logging.INFO if verbose else logging.WARNING)
        self._log.addHandler(hdlr)
               
        self._utils = trap_utils
        self._apps1 = trap_utils.apps1
        self._apps0 = trap_utils.apps0
        self._app_task = app_task
        self._linear = linear
        
        self._prims = NameSpace()
        self._types = NameSpace()
        
        self._type_ids = self._apps1.fw.env.enums["SDM_PRIM_T"]
        self._sdm_prims = []
        
        for prim in self._type_ids.keys():
            self._sdm_prims.append("%s_T" % prim[len('ENUM_'):])
        self._sdm_prims.append("SDM_UPRIM_T")
        
        try:
            # Note: we attempt to look these up in P*0*'s type dictionary 
            # because they aren't directly referenced in a bare-bones build 
            # of P1 firmware, so won't appear in its DWARF.
            env_types = self._apps0.fw.env.types
        except AttributeError:
            # However, if no P0 env is available, we're probably in customer
            # context, in which case the prims *will* be available in P1.
            env_types = self._apps1.fw.env.types
        for prim in self._sdm_prims:
            try:
                setattr(self._prims, prim, env_types[prim])
            except DwarfNoSymbol:
                pass
            else:
                if prim != "SDM_UPRIM_T":
                    import re
                    enumtor = re.sub("_T$", "", prim)
                    setattr(self._types, enumtor, self._type_ids["ENUM_%s"%enumtor])

    def reset_app_task(self, new_task):
        """
        Register a different task for communicating with the App.  The caller 
        owns the task; this object simply stores a reference to it.
        """
        self._app_task = new_task

    def send(self, prim):
        """
        Push a SDM prim through the right trap call
        """
        self._utils.call1.VmSendSdmPrim(prim)

    def send_prim(self, name, **fields):
        """
        Send an arbitrary prim, setting the supplied fields to the supplied
        values.  All other fields will be set to 0, apart from the type field.
        """
        prim_name = name + "_T"
        prim_dict = self._apps0.fw.env.types[prim_name]
        prim = self._utils.create_prim(prim_dict)
        if prim.member_list[0][0] == "type":
            # A bona fide SDM prim
            prim.type.value = self._type_ids["ENUM_%s" % name]
        elif prim.member_list[0][0] == "common":
            # An HCI prim masquerading as a SDM one
            prim.common.op_code.value = self._type_ids["ENUM_%s" % name]
            # Subtract offset of  start of the params from the total prim size
            if len(prim_dict["members"]) == 1:
                prim.common.length.value = 0
            else:
                prim.common.length.value = prim_dict["byte_size"] - prim_dict["members"][1][1]
             
        for field, value in fields.items():
            if isinstance(value, int_type):
                prim[field].value = value
            elif isinstance(value, dict):
                for subfield, subvalue in value.items():
                    prim[field][subfield].value = subvalue
            else:
                raise TypeError("SDMLib.send_prim: don't know how to assign "
                                "prim field using a %s" % type(value))
        
        self.send(prim)
        
 
    def rsp(self, raw=None):
        """
        Get a SDM response messages from Bluestack if any is waiting, or take the
        given one.  If there is one available on way or the other, it is 
        returned as a SDM_UPRIM_T, which the caller can use to determine the 
        actual message, based on the "type" field.
        """
        if raw is None:
            raw = self._utils.get_core_msg(SysMsg.BLUESTACK_SDM_PRIM,
                                           timeout=2 if self._linear else None)
        if raw is False:
            return raw
        return self._utils.build_var(self._prims.SDM_UPRIM_T, raw)
    
    def rsp_check(self, prim_name, raw=None):
        """
        Get the next SDM response prim, or use the given one, and check that its 
        type is as expected
        """
        while True:
            # If we see "IND"s we weren't looking for we'll just throw them away
            rsp = self.rsp(raw=raw)
            if rsp == False:
                raise SdmUnexpectedPrim("No sign of %s" % prim_name)
            type = rsp.type.value
            self._utils.free_var_mem(rsp)
            
            prim_type = getattr(self._types, prim_name) 
            if type != prim_type:
                name = self._type_ids[type].replace("ENUM_","")
                if "_IND" not in name:
                    raise SdmUnexpectedPrim("Saw %s but expected "
                                           "%s (0x%x)" % (name, 
                                                          prim_name, prim_type))
                else:
                    print("Ignoring %s" % name)
                    
            else:
                break

    def register(self, ret=False):
        """
        Construct a SDM_REGISTER_REQ_T with the appropriate type and phandle
        fields, push it through the API and then either
         - read back the response message, returning a SDM_REGISTER_CFM_T 
         _Structure whose underlying memory we own. If ret=True, this object 
         will be returned to the caller who *must ensure utils.free_var_mem is 
         ultimately called on it**; or
         - exit and let the message loop receive and handle the response
        """
        
        reg_req = self._utils.create_prim(self._prims.SDM_REGISTER_REQ_T)
        
        reg_req.type.value = self._types.SDM_REGISTER_REQ
        reg_req.phandle.value = 0
        
        # Send the prim down
        self.send(reg_req)
        
        if self._linear:
            # We expect a response, but is it the right thing?
            return self.register_check(ret=ret)

    def register_check(self, ret=False, raw=None):
        """
        Reconstruct a SDM_REGISTER_CFM_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value
        if value != self._types.SDM_REGISTER_CFM:
            self._utils.free_var_mem(rsp)
            raise SdmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.SDM_REGISTER_CFM))

        if ret:
            return self._utils.build_var(self._prims.SDM_REGISTER_CFM_T,
                                                  var_address(rsp))
        if free_mem:
            self._utils.free_var_mem(rsp)
            
    def _create_shadow_link(self, link_type, **cfg_dict):
        """
        Construct a SDM_SHADOW_LINK_CREATE_REQ_T and send it to appropriate device.
        """
        req = self._utils.create_prim(self._prims.SDM_SHADOW_LINK_CREATE_REQ_T)
        req.type.value = self._types.SDM_SHADOW_LINK_CREATE_REQ
        req.phandle.value = 0
        req.link_type.value = link_type
        req.shadow_bd_addr.tp_type.value = self._apps0.fw.env.enums["PHYSICAL_TRANSPORT_T"]["BREDR_ACL"]
        req.shadow_bd_addr.addrt.type.value = 0x00 # TBDADDR_PUBLIC
        req.shadow_bd_addr.addrt.addr.uap.value = cfg_dict["shadowed_bd_addr"].uap 
        req.shadow_bd_addr.addrt.addr.nap.value = cfg_dict["shadowed_bd_addr"].nap
        req.shadow_bd_addr.addrt.addr.lap.value = cfg_dict["shadowed_bd_addr"].lap
        
        req.secondary_bd_addr.tp_type.value = self._apps0.fw.env.enums["PHYSICAL_TRANSPORT_T"]["BREDR_ACL"]
        req.secondary_bd_addr.addrt.type.value = 0x00 # TBDADDR_PUBLIC
        req.secondary_bd_addr.addrt.addr.uap.value = cfg_dict["secondary_bd_addr"].uap 
        req.secondary_bd_addr.addrt.addr.nap.value = cfg_dict["secondary_bd_addr"].nap
        req.secondary_bd_addr.addrt.addr.lap.value = cfg_dict["secondary_bd_addr"].lap

        # Send the prim down
        self.send(req)
        
    def create_shadow_acl_link(self, **cfg_dict):
        """
        Wrapper function to create shadow ACL link
        """
        self._create_shadow_link(link_type = SDMConst.LINK_TYPE_ACL, **cfg_dict)

    def create_shadow_sco_link(self, **cfg_dict):
        """
        Wrapper function to create shadow SCO link
        """
        self._create_shadow_link(link_type = SDMConst.LINK_TYPE_SCO, **cfg_dict)

    def set_bredr_slave_address(self, **cfg_dict):
        """
        Construct a SDM_SET_BREDR_SLAVE_ADDRESS_REQ_T and send it to appropriate device.
        """
        # SDM_SET_BREDR_SLAVE_ADDRESS
 
        req = self._utils.create_prim(self._prims.SDM_SET_BREDR_SLAVE_ADDRESS_REQ_T)
        req.type.value = self._types.SDM_SET_BREDR_SLAVE_ADDRESS_REQ
        req.phandle.value = 0
        req.remote_bd_addr.uap.value = cfg_dict["remote_bd_addr"].uap 
        req.remote_bd_addr.nap.value = cfg_dict["remote_bd_addr"].nap
        req.remote_bd_addr.lap.value = cfg_dict["remote_bd_addr"].lap

        req.new_bd_addr.uap.value = cfg_dict["new_bd_addr"].uap 
        req.new_bd_addr.nap.value = cfg_dict["new_bd_addr"].nap
        req.new_bd_addr.lap.value = cfg_dict["new_bd_addr"].lap

        self.req = req

        # Send the prim down
        self.send(req)

    def create_shadow_link_acl_cfm_check(self, raw=None, ret=False):
        """
        Reconstruct a SDM_SHADOW_ACL_LINK_CREATE_CFM_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value
        if value != self._types.SDM_SHADOW_ACL_LINK_CREATE_CFM:
            self._utils.free_var_mem(rsp)
            raise SdmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.SDM_SHADOW_ACL_LINK_CREATE_CFM))
        shadow_link_create_cfm = self._utils.build_var(
                                    self._prims.SDM_SHADOW_ACL_LINK_CREATE_CFM_T,
                                    var_address(rsp))
        
        if shadow_link_create_cfm.status.value == 0:
            success = True
            handle = shadow_link_create_cfm.connection_handle.value
            role =  shadow_link_create_cfm.role.value
        else:
            success = False
            handle = 0
            role = 0

        if ret:
            return (success, handle, role)
        
        if free_mem:
            self._utils.free_var_mem(rsp)

    def create_shadow_link_acl_ind_check(self, raw=None, ret=False):
        """
        Reconstruct a SDM_SHADOW_ACL_LINK_CREATE_IND_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value
        if value != self._types.SDM_SHADOW_ACL_LINK_CREATE_IND:
            self._utils.free_var_mem(rsp)
            raise SdmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.SDM_SHADOW_ACL_LINK_CREATE_IND))
        shadow_link_create_ind = self._utils.build_var(
                                    self._prims.SDM_SHADOW_ACL_LINK_CREATE_IND_T,
                                    var_address(rsp))
        
        if shadow_link_create_ind.status.value == 0:
            success = True
            handle = shadow_link_create_ind.connection_handle.value
            role =  shadow_link_create_ind.role.value
        else:
            success = False
            handle = 0
            role = 0

        if ret:
            return (success, handle, role)
        
        if free_mem:
            self._utils.free_var_mem(rsp)

    def create_shadow_link_esco_cfm_check(self, raw=None, ret=False):
        """
        Reconstruct a SDM_SHADOW_ESCO_LINK_CREATE_CFM_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value
        if value != self._types.SDM_SHADOW_ESCO_LINK_CREATE_CFM:
            self._utils.free_var_mem(rsp)
            raise SdmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.SDM_SHADOW_ESCO_LINK_CREATE_CFM))
        shadow_link_create_cfm = self._utils.build_var(
                                    self._prims.SDM_SHADOW_ESCO_LINK_CREATE_CFM_T,
                                    var_address(rsp))
        
        if shadow_link_create_cfm.status.value == 0:
            success = True
            handle = shadow_link_create_cfm.connection_handle.value
            role =  shadow_link_create_cfm.role.value
        else:
            success = False
            handle = 0
            role = 0

        if ret:
            return (success, handle, role)
        
        if free_mem:
            self._utils.free_var_mem(rsp)

    def create_shadow_link_esco_ind_check(self, raw=None, ret=False):
        """
        Reconstruct a SDM_SHADOW_SCO_LINK_CREATE_IND_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value
        if value != self._types.SDM_SHADOW_ESCO_LINK_CREATE_IND:
            self._utils.free_var_mem(rsp)
            raise SdmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.SDM_SHADOW_ESCO_LINK_CREATE_IND))
        shadow_link_create_ind = self._utils.build_var(
                                    self._prims.SDM_SHADOW_ESCO_LINK_CREATE_IND_T,
                                    var_address(rsp))
        
        if shadow_link_create_ind.status.value == 0:
            success = True
            handle = shadow_link_create_ind.connection_handle.value
            role =  shadow_link_create_ind.role.value
        else:
            success = False
            handle = 0
            role = 0

        if ret:
            return (success, handle, role)
        
        if free_mem:
            self._utils.free_var_mem(rsp)

    def disconnect_shadow_link(self, handle):
        """
        Construct a SDM_SHADOW_LINK_DISCONNECT_REQ_T and send it to appropriate device.
        """
        req = self._utils.create_prim(self._prims.SDM_SHADOW_LINK_DISCONNECT_REQ_T)
        req.type.value = self._types.SDM_SHADOW_LINK_DISCONNECT_REQ
        req.phandle.value = 0
        req.conn_handle.value = handle
        req.reason.value = SDMConst.REASON_CONN_TERM_LOCAL_HOST

        # Send the prim down
        self.send(req)

    def disconnect_shadow_link_cfm_check(self, raw=None, ret=False):
        """
        Reconstruct a SDM_SHADOW_LINK_DISCONNECT_CFM_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value
        if value != self._types.SDM_SHADOW_LINK_DISCONNECT_CFM:
            self._utils.free_var_mem(rsp)
            raise SdmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.SDM_SHADOW_LINK_DISCONNECT_CFM))
        shadow_link_disconnect_cfm = self._utils.build_var(
                                    self._prims.SDM_SHADOW_LINK_DISCONNECT_CFM_T,
                                    var_address(rsp))
        
        handle = shadow_link_disconnect_cfm.conn_handle.value
        role =  shadow_link_disconnect_cfm.role.value
        link_type = shadow_link_disconnect_cfm.link_type.value
        reason = shadow_link_disconnect_cfm.reason.value

        if ret:
            return (link_type, handle, role)
        
        if free_mem:
            self._utils.free_var_mem(rsp)

    def set_bredr_slave_address_cfm_check(self, raw=None, ret=False):
        """
        Reconstruct a SDM_SET_BREDR_SLAVE_ADDRESS_CFM_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        set_bredr_slave_address_cfm = self._utils.build_var(
                                      self._prims.SDM_SET_BREDR_SLAVE_ADDRESS_CFM_T,
                                      var_address(rsp))

        if set_bredr_slave_address_cfm.status.value == 0:
            success = True
        else:
            success = False

        if ret:
            return (success)           


    def handler(self, msg):
        """
        Simple handler for SDM.  Sends a message to the app task after:
         - registration completes
         - shadow ACL link is created
         - shadow SCO link is created
         - shadow link is disconnected
         - slave bredr address has been set
        In all cases call the _check method that returns success based on the status flag
        in the cfm primative. If success is False, then return the _FAILED prim to the app
        task. In the case of ACl and SCO link failure, hardcode the return handle to 0xFFFF
        to indicate failure.
        """
        if msg["id"] != SysMsg.BLUESTACK_SDM_PRIM:
            raise UnexpectedMsgType("Expected SDM prim but received msg_id "
                                     "0x%x" % msg["id"])

        uprim = self.rsp(raw=msg["m"])

        if uprim.type.value == self._types.SDM_REGISTER_CFM:
            self.register_check(raw=var_address(uprim))
            self._utils.send_msg(self._app_task, SDMAppMsgs.REGISTRATION_COMPLETE)

        elif uprim.type.value == self._types.SDM_SHADOW_ACL_LINK_CREATE_CFM:
            (success, handle, role) = self.create_shadow_link_acl_cfm_check(
                                                raw=var_address(uprim), ret=True)
            if success:
                self.handle = handle
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_CFM)
            else:
                self.handle = 0xFFFF
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_CFM_FAILED)

        elif uprim.type.value == self._types.SDM_SHADOW_ACL_LINK_CREATE_IND:
            (success, handle, role) = self.create_shadow_link_acl_ind_check(
                                                raw=var_address(uprim), ret=True)
            if success:
                self.handle = handle
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_IND)
            else:
                self.handle = 0xFFFF
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_IND_FAILED)

        elif uprim.type.value == self._types.SDM_SHADOW_ESCO_LINK_CREATE_CFM:
            (success, handle, role) = self.create_shadow_link_esco_cfm_check(
                                                raw=var_address(uprim), ret=True)
            if success:
                self.handle = handle
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_CFM)
            else:
                self.handle = 0xFFFF
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_CFM_FAILED)

        elif uprim.type.value == self._types.SDM_SHADOW_ESCO_LINK_CREATE_IND:
            (success, handle, role) = self.create_shadow_link_esco_ind_check(
                                                raw=var_address(uprim), ret=True)
            if success:
                self.handle = handle
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_IND)
            else:
                self.handle = 0xFFFF
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_IND_FAILED)

        elif uprim.type.value == self._types.SDM_SHADOW_LINK_DISCONNECT_CFM:
            (link_type, handle, role) = self.disconnect_shadow_link_cfm_check(
                                                raw=var_address(uprim), ret=True)
            self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SHADOW_LINK_DISCONNECT_CFM)

        elif uprim.type.value == self._types.SDM_SET_BREDR_SLAVE_ADDRESS_CFM:
            success = self.set_bredr_slave_address_cfm_check(
                                                raw=var_address(uprim), ret=True)
            if success:
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SET_BREDR_SLAVE_ADDRESS_CFM)
            else:
                self._utils.send_msg(self._app_task, SDMAppMsgs.SDM_SET_BREDR_SLAVE_ADDRESS_CFM_FAILED)

        else:
            # Anything else might be problem
            self._log.warn("Saw %s" % self._type_ids[uprim.type.value].replace("ENUM_",""))

