############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import NameSpace
from csr.dev.fw.trap_api.system_message import SystemMessage as SysMsg
from csr.dev.fw.trap_api.trap_utils import UnexpectedMsgType
from csr.dev.env.env_helpers import var_address, var_size
from csr.dwarf.read_dwarf import DwarfNoSymbol
import logging

try:
    long
except NameError:
    long = int

class CSBConst(object):
    CSB_SYNCHRONIZATION_SCAN_TIMEOUT = 0x100
    CSB_SYNCHRONIZATION_SCAN_WINDOW  = 0x100
    CSB_SYNCHRONIZATION_SCAN_INTERVAL= 0x200
    CSB_INTERVAL_MIN = 0x64 # 
    CSB_INTERVAL_MAX = 0x64 #
    CSB_SUPERVISION_TIMEOUT = 0x1000
    CSB_LPO_ALLOWED = 1
    CSB_PACKET_TYPE = 0x08
    CSB_SYNCHRONIZATION_INTERVAL_MIN = 0xA0
    CSB_SYNCHRONIZATION_INTERVAL_MAX = 0xC0
    CSB_SYNCHRONIZATION_TIMEOUT = 0xC80 # 2 sec timeout
    CSB_SYNCHRONIZATION_SERVICE_DATA = 0
        
class HCIConst(CSBConst):
    PAGESCAN_INTERVAL_MIN       = 0x12
    PAGESCAN_INTERVAL_DEFAULT   = 0x800
    PAGESCAN_INTERVAL_MAX       = 0x1000
    PAGESCAN_WINDOW_MIN         = 0x11
    PAGESCAN_WINDOW_DEFAULT     = 0x12
    PAGESCAN_WINDOW_MAX         = 0x1000

class DmUnexpectedPrim(RuntimeError):
    """
    Indicates an unexpected DM prim was seen
    """
    
class DMException(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class DMAppMsgs(object):
    # Messages that a controlling App might need to receive
    REGISTRATION_COMPLETE      = 0
    LOCAL_BD_ADDR_OBTAINED     = 1
    LISTENING                  = 2
    LISTENING_FAILED           = 3
    SYNC_REGISTRATION_COMPLETE = 4
    SYNC_CONNECT_IND           = 5
    SYNC_CONNECTION_SUCCESS    = 6    
    SYNC_CONNECTION_TERMINATED = 7
    SYNC_UNREGISTRATION_COMPLETE = 8
    BT_VER_SET                 = 9
    SET_RESERVED_LT_ADDR_COMPLETE=10
    WRITE_SYNCHRONIZATION_TRAIN_PARAM_COMPLETE=11
    START_SYNCHRONIZATION_TRAIN_COMPLETE=12
    SET_CSB_COMPLETE           = 13
    RECEIVE_SYNCHRONIZATION_TRAIN_COMPLETE = 14
    RECEIVE_SYNCHRONIZATION_TRAIN_FAILED = 15
    SET_CSB_RECEIVE_COMPLETE = 16
    CSB_AFH_MAP_AVAILABLE_IND = 17
    CSB_AFH_MAP_CHANGE_IND = 18
    DM_ULP_SET_DEFAULT_PHY_CFM = 19
    DM_ULP_SET_DEFAULT_PHY_CFM_FAILED = 20
    DM_ULP_SET_PHY_CFM = 21
    DM_ULP_SET_PHY_CFM_FAILED = 22
    DM_HCI_WRITE_SCAN_ENABLE_CFM = 23
    DM_HCI_WRITE_SCAN_ENABLE_CFM_FAILED = 24
    DM_HCI_SWITCH_ROLE_CFM = 25
    DM_HCI_SWITCH_ROLE_CFM_FAILED = 26
    DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM = 27
    DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM_FAILED = 28
    SM_INIT_CFM = 29
    SM_INIT_CFM_FAILED = 30
    SM_IO_CAPABILITY_REQUEST_IND = 31
    SM_BONDING_CFM = 32
    SM_BONDING_CFM_FAILED = 33

class DMLib(object):
    """
    Library supporting access to the DM-related trap API functionality in the
    firmware, such as
     - creating and sending DM primitives via VmSendDmPrim
     - getting and interpreting DM_PRIM messages
    """
    
    
    
    def __init__(self, trap_utils, linear=False, app_task=None, verbose=True, 
                 tag="", esco=True, logger=None):
        
        if logger == None:
            self._log = logging.getLogger(tag if tag else "dm")
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
        self.esco = esco

        # Callback arg passed in the DM_SYNC register and connect primitives
        self._pv_cbarg_in_req = 0

        # Callback arg returned in the DM_SYNC_CONNECT cfm primitive
        self._pv_cbarg_in_connect_cfm = 0

        self._type_ids = self._apps1.fw.env.enums["DM_PRIM_T"]
        self._dm_prims = []
        for prim in self._type_ids.keys():
            # Whitelist of 'reserved' primitives for which there are
            # no structures generated for Hydra chips.
            if (prim != 'ENUM_DM_SM_LOCAL_KEY_DELETED_IND' and
                prim != 'ENUM_DM_SM_GENERATE_LOCAL_KEY_IND' and
                prim != 'ENUM_DM_ULP_ENABLE_ZERO_SLAVE_LATENCY_REQ' and
                prim != 'ENUM_DM_ULP_ENABLE_ZERO_SLAVE_LATENCY_CFM' and
                prim != 'ENUM_DM_CRYPTO_GENERATE_SHARED_SECRET_KEY_REQ' and
                prim != 'ENUM_DM_CRYPTO_GENERATE_SHARED_SECRET_KEY_CFM' and
                prim != 'ENUM_DM_CRYPTO_GENERATE_PUBLIC_PRIVATE_KEY_REQ' and
                prim != 'ENUM_DM_CRYPTO_GENERATE_PUBLIC_PRIVATE_KEY_CFM' and
                prim != 'ENUM_DM_CRYPTO_ENCRYPT_REQ' and
                prim != 'ENUM_DM_CRYPTO_ENCRYPT_CFM' and
                prim != 'ENUM_DM_CRYPTO_HASH_REQ' and
                prim != 'ENUM_DM_CRYPTO_HASH_CFM' and
                prim != 'ENUM_DM_SM_GENERATE_LOCAL_KEY_CFM'):
                self._dm_prims.append("%s_T" % prim[len('ENUM_'):])
        self._dm_prims.append("DM_UPRIM_T")
        try:
            # Note: we attempt to look these up in P*0*'s type dictionary 
            # because they aren't directly referenced in a bare-bones build 
            # of P1 firmware, so won't appear in its DWARF.
            env_types = self._apps0.fw.env.types
        except AttributeError:
            # However, if no P0 env is available, we're probably in customer
            # context, in which case the prims *will* be available in P1.
            env_types = self._apps1.fw.env.types
        for prim in self._dm_prims:
            try:
                setattr(self._prims, prim, env_types[prim])
            except DwarfNoSymbol:
                pass
            else:
                if prim != "DM_UPRIM_T":
                    import re
                    enumtor = re.sub("_T$", "", prim)
                    setattr(self._types, enumtor, self._type_ids["ENUM_%s"%enumtor])

    def reset_app_task(self, new_task):
        """
        Register a different task for communicating with the App.  The caller 
        owns the task; this object simply stores a reference to it.
        """
        self._app_task = new_task

    def set_cbarg_in_dm_sync_req(self,value=0):
        """
        Register the callback argument for
        DM_SYNC_REGISTER/CONNECT REQ primitives.
        """
        self._pv_cbarg_in_req = value

    def get_cbarg_in_dm_sync_req(self):
        """
        Return the callback argument sent in
        the DM_SYNC_REGISTER/CONNECT REQ primitives.
        """
        return self._pv_cbarg_in_req

    def get_cbarg_in_dm_sync_cfm(self):
        """
        Return the callback argument received in
        the DM_SYNC_CONNECT_CFM primitive.
        """
        return self._pv_cbarg_in_connect_cfm

    def get_tp_addrt_in_sm_io_req_ind(self):
        """
        Return tp_addrt arguments received in the
        DM_SM_IO_CAPABILITY_REQUEST_IND primitive.
        """
        return self._io_cap_req_ind

    def send(self, prim):
        """
        Push a DM prim through the right trap call
        """
        self._utils.call1.VmSendDmPrim(prim)
 
    def send_prim(self, name, **fields):
        """
        Send an arbitrary prim, setting the supplied fields to the supplied
        values.  All other fields will be set to 0, apart from the type field.
        """
        prim_name = name + "_T"
        prim_dict = self._apps0.fw.env.types[prim_name]
        prim = self._utils.create_prim(prim_dict)
        if prim.member_list[0][0] == "type":
            # A bona fide DM prim
            prim.type.value = self._type_ids["ENUM_%s" % name]
        elif prim.member_list[0][0] == "common":
            # An HCI prim masquerading as a DM one
            prim.common.op_code.value = self._type_ids["ENUM_%s" % name]
            # Subtract offset of  start of the params from the total prim size
            if len(prim_dict["members"]) == 1:
                prim.common.length.value = 0
            else:
                prim.common.length.value = prim_dict["byte_size"] - prim_dict["members"][1][1]
             
        for field, value in fields.items():
            if isinstance(value, (int, long)):
                prim[field].value = value
            elif isinstance(value, dict):
                for subfield, subvalue in value.items():
                    prim[field][subfield].value = subvalue
            else:
                raise TypeError("DMLib.send_prim: don't know how to assign "
                                "prim field using a %s" % type(value))
        
        self.send(prim)

    def rsp(self, raw=None, timeout=2):
        """
        Get a DM response messages from Bluestack if any is waiting, or take the
        given one.  If there is one available on way or the other, it is 
        returned as a DM_UPRIM_T, which the caller can use to determine the 
        actual message, based on the "type" field.
        """
        if raw is None:
            raw = self._utils.get_core_msg(SysMsg.BLUESTACK_DM_PRIM,
                                           timeout=timeout if self._linear else None)
        if raw is False:
            return raw
        return self._utils.build_var(self._prims.DM_UPRIM_T, raw)
    
    def rsp_check(self, prim_name, raw=None):
        """
        Get the next DM response prim, or use the given one, and check that its 
        type is as expected
        """
        while True:
            # If we see "IND"s we weren't looking for we'll just throw them away
            rsp = self.rsp(raw=raw)
            if rsp == False:
                raise DmUnexpectedPrim("No sign of %s" % prim_name)
            type = rsp.type.value
            self._utils.free_var_mem(rsp)
            
            prim_type = getattr(self._types, prim_name) 
            if type != prim_type:
                name = self._type_ids[type].replace("ENUM_","")
                if "_IND" not in name:
                    raise DmUnexpectedPrim("Saw %s but expected "
                                           "%s (0x%x)" % (name, 
                                                          prim_name, prim_type))
                else:
                    iprint("Ignoring %s" % name)
                    
            else:
                break

    def _populate_sync_config(self, config):
        """
        Populate a configuration variable of type DM_SYNC_CONFIG_T with 
        values accoding to http://wiki/BtcliConnectionsAndPackets#SCO_Connections
        for SCO or eSCO connection.
        """     
        config.max_latency.value    = 13         # 13ms
        config.retx_effort.value    = 2          # Quality
        if self.esco:
            config.packet_type.value = 0x0380    # 2EV3
        else:
            config.packet_type.value = 0x03C4    # HV3
        config.voice_settings.value = 0x63       # Transparent Data Air Coding Format
        config.tx_bdw.value         = 8000
        config.rx_bdw.value         = 8000
            
    def _get_dm_sync_connect(self):
        """
        Return a new DM_SYNC_CONFIG_T structure allocated and populated
        """     
        config = self._apps1.fw.call.pnew("DM_SYNC_CONFIG_T")

        self._populate_sync_config(config)

        return config
       
    
    def sync_connect_req(self, **cfg_dict):
        """
        Construct a DM_SYNC_CONNECT_REQ_T and send it to the master device.
        """       
        conn_req = self._utils.create_prim(self._prims.DM_SYNC_CONNECT_REQ_T)
        # 7.1.26 Setup Synchronous Connection Command
        conn_req.type.value    = self._types.DM_SYNC_CONNECT_REQ
               
        conn_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        conn_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        conn_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap
        
        conn_req.length.value = 0
        
        config = self._get_dm_sync_connect()

        # Copy config values for logging
        self.config_max_latency = config.max_latency.value
        self.config_retx_effort = config.retx_effort.value
        self.config_packet_type = config.packet_type.value
        self.config_voice_settings = config.voice_settings.value
        self.config_tx_bdw = config.tx_bdw.value
        self.config_rx_bdw = config.rx_bdw.value

        conn_req.u.config.value = var_address(config)
        conn_req.pv_cbarg.value = self._pv_cbarg_in_req

        # Send the prim down
        self.send(conn_req)

    def read_rssi(self, raw=None, timeout=5):
        """
        Read the rssi request response - DM_HCI_READ_RSSI_CFM
        and return success/failure with rssi as a tuple
        rssi will be zero if it is within Golden Receive Power Range
        7.5.4 Read RSSI Command
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw, timeout=timeout)

        value = rsp.type.value
        if value != self._types.DM_HCI_READ_RSSI_CFM:
            self._utils.free_var_mem(rsp)
            raise DmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.DM_HCI_READ_RSSI_CFM))

        rssi_cfm = self._utils.build_var(
                                      self._prims.DM_HCI_READ_RSSI_CFM_T,
                                      rsp.address)

        result = rssi_cfm.status.value

        if result == 0: # '0' is for success
            success = True
            rssi  = rssi_cfm.rssi.value
        else:
            success = False
            # rssi value will be zero for success
            # if it is within Golden Receive Power Range
            # in case of failure return invalid rssi
            # valid rssi -128<= rssi <= 127
            rssi = -255

        if free_mem:
            self._utils.free_var_mem(rsp)
        return (success, rssi)

    def rssi_read_request(self, hci_handle, **cfg_dict):
        """
        Construct a DM_HCI_READ_RSSI_REQ_T and send it to the master device
        7.5.4 Read RSSI Command
        """
        rssi_req = self._utils.create_prim(self._prims.DM_HCI_READ_RSSI_REQ_T)

        rssi_req.common.op_code.value = self._types.DM_HCI_READ_RSSI_REQ

        rssi_req.tp_addrt.addrt.addr.uap.value = cfg_dict["bd_addr"].uap
        rssi_req.tp_addrt.addrt.addr.nap.value = cfg_dict["bd_addr"].nap
        rssi_req.tp_addrt.addrt.addr.lap.value = cfg_dict["bd_addr"].lap

        rssi_req.handle.value = hci_handle

        # Send the prim down
        self.send(rssi_req)

    def acl_open_req(self, **cfg_dict):
        """
        Construct a DM_ACL_OPEN_REQ_T and send it to the master device.
        """
        conn_req = self._utils.create_prim(self._prims.DM_ACL_OPEN_REQ_T)
        conn_req.type.value    = self._types.DM_ACL_OPEN_REQ

        conn_req.addrt.addr.uap.value = cfg_dict["bd_addr"].uap
        conn_req.addrt.addr.nap.value = cfg_dict["bd_addr"].nap
        conn_req.addrt.addr.lap.value = cfg_dict["bd_addr"].lap

        # Send the prim down
        self.send(conn_req)

    def acl_open_ind_check(self, raw=None, timeout=5):
        """
        Check the acl_open ind and return success/failure with hci_handle as a tuple
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw, timeout=timeout)

        value = rsp.type.value
        if value != self._types.DM_ACL_OPENED_IND:
            self._utils.free_var_mem(rsp)
            raise DmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.DM_ACL_OPENED_IND))

        acl_open_ind = self._utils.build_var(
                                      self._prims.DM_ACL_OPENED_IND_T,
                                      rsp.address)

        result = acl_open_ind.status.value

        if result == 0: # '0' is for success
            success = True
            handle = acl_open_ind.phandle.value
        else:
            success = False
            handle = 0

        if free_mem:
            self._utils.free_var_mem(rsp)
        return (success, handle, acl_open_ind.addrt.addr)

    def acl_open_req_cfm_check(self, raw=None, timeout=5):
        """
        Check the acl_open cfm and return success/failure with hci_handle as a tuple
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw, timeout=timeout)

        value = rsp.type.value
        if value != self._types.DM_ACL_OPEN_CFM:
            self._utils.free_var_mem(rsp)
            raise DmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.DM_ACL_OPEN_CFM))

        acl_open_cfm = self._utils.build_var(
                                      self._prims.DM_ACL_OPEN_CFM_T,
                                      rsp.address)
        result = acl_open_cfm.success.value

        if result: # '1' is for success
            success = True
            handle = acl_open_cfm.phandle.value
        else:
            success = False
            handle = 0

        if free_mem:
            self._utils.free_var_mem(rsp)
        return (success, handle)

    def sync_accept_incoming(self, **cfg_dict):
        """
        Accept an incoming SCO connection by sending a DM_SYNC_CONNECT_RSP_T.
        The input cfg_dict dictionary should include the "bd_addr" entry with the master BT address
        """    
        conn_req = self._utils.create_prim(self._prims.DM_SYNC_CONNECT_RSP_T)

        conn_req.type.value    = self._types.DM_SYNC_CONNECT_RSP
                       
        conn_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        conn_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        conn_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap
        conn_req.response.value = 0 # HCI_SUCCESS
        self._populate_sync_config(conn_req.config)

        # Send the prim down        
        self.send(conn_req)        

    def sync_register_check(self, ret=False, raw=None):
        """
        Accept an incoming SCO connection by sending a DM_SYNC_CONNECT_RSP_T.
        The input cfg_dict dictionary should include the "bd_addr" entry with the master BT address
        """            
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value

        if value != self._types.DM_SYNC_REGISTER_CFM:
            self._utils.free_var_mem(rsp)
            raise DmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value, self._types.DM_SYNC_REGISTER_CFM))

        if ret:
            return self._utils.build_var(self._prims.DM_SYNC_REGISTER_CFM_T, var_address(rsp))
            
        if free_mem:
            self._utils.free_var_mem(rsp)
                    
    def sync_register(self, ret=False):
        """
        Construct a DM_SYNC_REGISTER_REQ_T and send it to the appropriate device
        """
        reg_req = self._utils.create_prim(self._prims.DM_SYNC_REGISTER_REQ_T)

        reg_req.type.value  = self._types.DM_SYNC_REGISTER_REQ
               
        reg_req.flags.value = 0
        reg_req.pv_cbarg.value = self._pv_cbarg_in_req

        # Send the prim down
        self.send(reg_req)

    def sync_unregister(self, ret=False):
        """
        Construct a DM_SYNC_UNREGISTER_REQ_T and send it to the appropriate device
        """
        unreg_req = self._utils.create_prim(self._prims.DM_SYNC_UNREGISTER_REQ_T)

        unreg_req.type.value  = self._types.DM_SYNC_UNREGISTER_REQ
        unreg_req.pv_cbarg.value = self._pv_cbarg_in_req

        # Send the prim down
        self.send(unreg_req)
 
    def register(self, ret=False):
        """
        Construct a DM_AM_REGISTER_REQ_T with the appropriate type and flags
        fields, push it through the API and then either
         - read back the response message, returning a DM_AM_REGISTER_CFM_T 
         _Structure whose underlying memory we own. If ret=True, this object 
         will be returned to the caller who *must ensure utils.free_var_mem is 
         ultimately called on it**; or
         - exit and let the message loop receive and handle the response
        """
        
        reg_req = self._utils.create_prim(self._prims.DM_AM_REGISTER_REQ_T)
        
        reg_req.type.value = self._types.DM_AM_REGISTER_REQ
        reg_req.flags.value = 0
        
        # Send the prim down
        self.send(reg_req)
        
        if self._linear:
            # We expect a response, but is it the right thing?
            return self.register_check(ret=ret)
        
        
    def register_check(self, ret=False, raw=None):
        
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        value = rsp.type.value
        if value != self._types.DM_AM_REGISTER_CFM:
            self._utils.free_var_mem(rsp)
            raise DmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.DM_AM_REGISTER_CFM))

        if ret:
            return self._utils.build_var(self._prims.DM_AM_REGISTER_CFM_T,
                                                  var_address(rsp))
        if free_mem:
            self._utils.free_var_mem(rsp)


    def request_local_bd_addr(self):
        """
        Send a request for the local bluetooth address
        """
        bdaddr_req = self._utils.create_prim(
                                         self._prims.DM_HCI_READ_BD_ADDR_REQ_T)
        bdaddr_req.common.op_code.value = self._types.DM_HCI_READ_BD_ADDR_REQ
        
        self.send(bdaddr_req)

    def _set_local_bd_addr(self, raw_prim):
        """
        Having received a DM_HCI_READ_BD_ADDR_CFM, store the address
        """
        # Cast to the expected prim type
        read_bd_addr = self._utils.build_var(
                                      self._prims.DM_HCI_READ_BD_ADDR_CFM_T,
                                      var_address(raw_prim))
        # Copy the address out
        self._addr = NameSpace()
        self._addr.lap = read_bd_addr.bd_addr.lap.value
        self._addr.uap = read_bd_addr.bd_addr.uap.value
        self._addr.nap = read_bd_addr.bd_addr.nap.value
        
    def change_local_name(self, name):
        """
        Issue a DM_HCI_CHANGE_LOCAL_NAME_REQ with the given name, which can be
        at most 248 bytes.  In linear mode waits for the CFM.
        """
        assert len(name) <= 248, "Supplied name is of a standard-conforming length"
        
        name_chg_req = self._utils.create_prim(
                                   self._prims.DM_HCI_CHANGE_LOCAL_NAME_REQ_T)
        name_chg_req.common.op_code.value = 0xc13
        name = name + chr(0)
        name_chg_req.common.length.value = 3 + len(name)

        i = 0
        while i < len(name):
            part = self._apps1.fw.call.pnew("unsigned char", 32)
            start = i
            name_chg_req.name_part[i//32].value = var_address(part)
            part_end = min(i + 32, len(name))
            while i < part_end:
                part[i-start].value = ord(name[i])
                i += 1
            
        self.send(name_chg_req)
        
        if self._linear:
            self.rsp_check("DM_HCI_CHANGE_LOCAL_NAME_CFM")
            
        
        
    def read_local_name(self, raw=None):
        """
        Issue a DM_HCI_READ_LOCAL_NAME_REQ and return the string that is
        received.  If a memory pointer is supplied via the raw argument, the 
        command is not issued and the function simply decodes it as a pointer to
        a DM_HCI_READ_LOCAL_NAME_CFM (this is for looping mode handlers to call).
        """
        free_mem = (raw is None)

        if raw is None:
            
            name_read_req = self._utils.create_prim(
                                       self._prims.DM_HCI_READ_LOCAL_NAME_REQ_T)
            name_read_req.common.op_code.value = 0xc14
            self.send(name_read_req)

        old_size = self._apps1.fw.call.trap_api_test_reset_max_message_body_bytes(
                                    self._prims.DM_HCI_READ_LOCAL_NAME_CFM_T["byte_size"])

        rsp = self.rsp(raw=raw)
        
        self._apps1.fw.call.trap_api_test_reset_max_message_body_bytes(old_size)
        
        value = rsp.type.value
        if value != self._types.DM_HCI_READ_LOCAL_NAME_CFM:
            self._utils.free_var_mem(rsp)
            raise DmUnexpectedPrim("Saw 0x%x but expected 0x%x" % 
                                   (value,
                                    self._types.DM_HCI_READ_LOCAL_NAME_CFM))
        
        prim = self._utils.build_var(self._prims.DM_HCI_READ_LOCAL_NAME_CFM_T,
                                              var_address(rsp))
        # Copy the name out of the prim
        chars = []
        c = None
        i = 0
        while True:
            c = prim.name_part[i//32][i%32].value
            i += 1
            if c == 0 or i >= 248:
                break
            chars.append(chr(c))
        name = "".join(chars) 

        for i in range((i//32) + 1):
            self._apps1.fw.call.pfree(prim.name_part[i])
            
        if free_mem:
            self._utils.free_var_mem(rsp)

        return name


    @property
    def local_bd_addr(self):
        """
        Get the device's Bluetooth address via an HCI READ_BD_ADDR request
        """
        try:
            self._addr
        except AttributeError:
            if self._linear:
                # We can get the bd addr synchronously
                
                self.request_local_bd_addr()
                
                rsp = self.rsp()
                if rsp == False:
                    raise DmUnexpectedPrim("Didn't get a response to "
                                           "DM_HCI_READ_BD_ADDR_REQ!")
        
                type = rsp.type.value
                if type != self._types.DM_HCI_READ_BD_ADDR_CFM:
                    
                    self._utils.free_var_mem(rsp)
                    raise DmUnexpectedPrim("Saw %s but expected "
                                           "DM_HCI_READ_BD_ADDR_CFM (0x%x)" %
                   (self._apps1.fw.env.enums["DM_PRIM_T"][type].replace("ENUM_",""), 
                                            self._types.DM_HCI_READ_BD_ADDR_CFM))
                
                self._set_local_bd_addr(rsp)
                self._utils.free_var_mem(rsp)
            else:
                # We can't get the bd addr synchronously: this function should
                # not have been called yet.
                raise RuntimeError("Bd addr not available: no bd addr request fulfilled yet")
                        
        # Return the address
        return self._addr
            
    def scan_enable(self, page_scan=True, inq_scan=True):
        """
        Configure scan settings of the device
        """       
        # Create a write scan enable request
        writescan_req = self._utils.create_prim(
                               self._prims.DM_HCI_WRITE_SCAN_ENABLE_REQ_T)
        writescan_req.common.op_code.value = (
                                      self._types.DM_HCI_WRITE_SCAN_ENABLE_REQ)
        writescan_req.scan_enable.value = 1 if inq_scan is True else 0
        writescan_req.scan_enable.value |= 1 << 1 if page_scan is True else 0 
        
        # Send them
        self.send(writescan_req)
        
        if self._linear:
            # Get and check the responses
            self.rsp_check("DM_HCI_WRITE_SCAN_ENABLE_CFM")
        else:
            self._listening_cfms = 0
        
    def listen(self):
        """
        Put the device into listening mode
        """
        # Create a write pagescan activity request
        pagescan_req = self._utils.create_prim(
                           self._prims.DM_HCI_WRITE_PAGESCAN_ACTIVITY_REQ_T)
        pagescan_req.common.op_code.value = (
                                 self._types.DM_HCI_WRITE_PAGESCAN_ACTIVITY_REQ) 
        pagescan_req.pagescan_interval.value = (
                                            HCIConst.PAGESCAN_INTERVAL_DEFAULT)
        pagescan_req.pagescan_window.value = HCIConst.PAGESCAN_WINDOW_DEFAULT
        
        # Create a write scan enable request
        writescan_req = self._utils.create_prim(
                               self._prims.DM_HCI_WRITE_SCAN_ENABLE_REQ_T)
        writescan_req.common.op_code.value = (
                                      self._types.DM_HCI_WRITE_SCAN_ENABLE_REQ)
        writescan_req.scan_enable.value = 3
        
        # Send them
        self.send(pagescan_req)
        self.send(writescan_req)
        
        if self._linear:
            # Get and check the responses
            self.rsp_check("DM_HCI_WRITE_PAGESCAN_ACTIVITY_CFM")
            self.rsp_check("DM_HCI_WRITE_SCAN_ENABLE_CFM")
        else:
            self._listening_cfms = 0
        
        
    def auto_connect_ind(self):
        """
        Expect DM_ACL_OPENED_IND and DM_SM_ACCESS_IND as a result of a
        L2CA_AUTO_CONNECT_REQ_T being issued on the other side
        """
        
        self.rsp_check("DM_ACL_OPENED_IND")
        self.rsp_check("DM_SM_ACCESS_IND")
        

    def set_app_task(self, task):
        self._app_task = task


    def sync_connect_cnf_check(self, raw=None, ret=False):
        """
        Check if the sync connect confirm is a success or a failure.
        """    
        free_mem = (raw is None)        
        rsp = self.rsp(raw=raw)

        sync_connect_complete = self._utils.build_var(
                                      self._prims.DM_SYNC_CONNECT_CFM_T,
                                      var_address(rsp))
                              
        result = sync_connect_complete.status.value
        
        if result == 0: # '0' is for success
            success = True
            handle  = sync_connect_complete.handle.value
        else:
            success = False
            handle = 0

        if self._pv_cbarg_in_req:
            connect_cfm = self._utils.build_var(self._prims.DM_SYNC_CONNECT_CFM_T,var_address(rsp))
            self._pv_cbarg_in_connect_cfm = connect_cfm.pv_cbarg.value

        if ret:
            return (success, handle)
            
        if free_mem:
            self._utils.free_var_mem(rsp)

    def sync_disconnect_req(self, hci_handle):
        """
        Create a DM_SYNC_DISCONNECT_REQ for the passed HCI handle and send it.
        """           
        disconnect_req = self._utils.create_prim(self._prims.DM_SYNC_DISCONNECT_REQ_T)
        
        # 7.1.26 Setup Synchronous Connection Command
        disconnect_req.type.value    = self._types.DM_SYNC_DISCONNECT_REQ

        disconnect_req.handle.value = hci_handle
        disconnect_req.reason.value = 0

        # Send the prim down
        self.send(disconnect_req)

    def set_bt_version(self):
        """
        Create a DM_SET_BT_VERSION_REQ for LE link and send it.
        """           
        version_req = self._utils.create_prim(self._prims.DM_SET_BT_VERSION_REQ_T)
        
        version_req.type.value = self._types.DM_SET_BT_VERSION_REQ
        version_req.version.value = 0x7

        # Send the prim down
        self.send(version_req)

    def check_bt_version(self, raw=None):
        rsp = self.rsp(raw=raw)
        bt_ver_cfm = self._utils.build_var(
                                      self._prims.DM_SET_BT_VERSION_CFM_T,
                                      var_address(rsp))
                              
        status = bt_ver_cfm.status.value
        version = bt_ver_cfm.version.value
        
        #raise exception if not set as then for sure LE connection will not work
        if ((status != 0) or (version != 7)):
            self._utils.free_var_mem(rsp)
            raise DmUnexpectedPrim("Setting BT version failed with status %s " 
                                    "and version %s" % (status, version))
        
    def set_reserved_lt_addr(self, lt_addr):
        """
        Create a DM_HCI_SET_RESERVED_LT_ADDR_REQ on CSB transmitter and send it.
        """
        prim_dict = self._prims.DM_HCI_SET_RESERVED_LT_ADDR_REQ_T
        send_req = self._utils.create_prim(prim_dict)
        send_req.common.op_code.value = self._types.DM_HCI_SET_RESERVED_LT_ADDR_REQ
        send_req.common.length.value = prim_dict["byte_size"]
        send_req.lt_addr.value=lt_addr
        # Send the prim down
        self.send(send_req)

    def set_reserved_lt_addr_check(self, raw=None, ret=False):
        """
        Check if the set reserved lt_addr confirm is a success or a failure.
        """    
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        set_lt_addr_complete = self._utils.build_var(
                                      self._prims.DM_HCI_SET_RESERVED_LT_ADDR_CFM_T,
                                      var_address(rsp))
        result = set_lt_addr_complete.status.value
        if result == 0: # '0' is for success
            success = True
        else:
            success = False

        if ret:
            return (success)
        if free_mem:
            self._utils.free_var_mem(rsp)
        
    def set_csb(self, enable=False, lt_addr=None):
        """
        Create a DM_HCI_SET_CSB_REQ on CSB transmitter and send it.
        """
        prim_dict = self._prims.DM_HCI_SET_CSB_REQ_T
        send_req = self._utils.create_prim(prim_dict)
        send_req.common.op_code.value=self._types.DM_HCI_SET_CSB_REQ
        send_req.common.length.value = prim_dict["byte_size"]
        send_req.lt_addr.value=lt_addr
        send_req.enable.value=enable
        send_req.lpo_allowed.value=HCIConst.CSB_LPO_ALLOWED
        send_req.packet_type.value=HCIConst.CSB_PACKET_TYPE
        send_req.interval_min.value=HCIConst.CSB_INTERVAL_MIN
        send_req.interval_max.value=HCIConst.CSB_INTERVAL_MAX
        send_req.supervision_timeout.value = HCIConst.CSB_SUPERVISION_TIMEOUT
        # Send the prim down
        self.send(send_req)

    def write_sync_train_param_req(self):
        """
        Create a DM_HCI_WRITE_SYNCHRONIZATION_TRAIN_PARAMS_REQ on CSB transmitter and send it.
        """
        prim_dict = self._prims.DM_HCI_WRITE_SYNCHRONIZATION_TRAIN_PARAMS_REQ_T
        send_req = self._utils.create_prim(prim_dict)
        send_req.common.op_code.value=self._types.DM_HCI_WRITE_SYNCHRONIZATION_TRAIN_PARAMS_REQ
        send_req.common.length.value = prim_dict["byte_size"]
        send_req.interval_min.value=HCIConst.CSB_SYNCHRONIZATION_INTERVAL_MIN
        send_req.interval_max.value=HCIConst.CSB_SYNCHRONIZATION_INTERVAL_MAX
        send_req.sync_train_timeout.value = HCIConst.CSB_SYNCHRONIZATION_TIMEOUT
        send_req.service_data.value = HCIConst.CSB_SYNCHRONIZATION_SERVICE_DATA 
        # Send the prim down
        self.send(send_req)

    def start_sync_train_req(self):
        """
        Create a DM_HCI_START_SYNCHRONIZATION_TRAIN_REQ on CSB transmitter and send it.
        """
        prim_dict = self._prims.DM_HCI_START_SYNCHRONIZATION_TRAIN_REQ_T
        send_req = self._utils.create_prim(prim_dict)
        send_req.common.op_code.value=self._types.DM_HCI_START_SYNCHRONIZATION_TRAIN_REQ
        send_req.common.length.value = prim_dict["byte_size"] 
        # Send the prim down
        self.send(send_req)

    def set_ulp_default_phy_req(self, **cfg_dict):
        """
        Create a DM_ULP_SET_PHY_REQ for 2 LE.
        """
        prim_dict = self._prims.DM_ULP_SET_DEFAULT_PHY_REQ_T
        send_req = self._utils.create_prim(prim_dict)
        send_req.type.value = cfg_dict["type"]
        send_req.phandle.value = cfg_dict["phandle"]
        send_req.min_tx.value = cfg_dict["min_tx"]
        send_req.max_tx.value = cfg_dict["max_tx"]
        send_req.min_rx.value = cfg_dict["min_rx"]
        send_req.max_rx.value = cfg_dict["max_rx"]
        send_req.flags.value = cfg_dict["flags"]
        #Send the prim down
        self.send(send_req)
        
    def set_ulp_phy_req(self, **cfg_dict):
        """
        Create a DM_ULP_SET_PHY_REQ for 2 LE.
        """
      
        prim_dict = self._prims.DM_ULP_SET_PHY_REQ_T
        send_req = self._utils.create_prim(prim_dict)
        send_req.type.value = cfg_dict["type"]
        send_req.phandle.value = cfg_dict["phandle"]
        send_req.tp_addrt.addrt.type.value = 0  #Public 
        send_req.tp_addrt.addrt.addr.lap.value = cfg_dict["bdaddress"].lap
        send_req.tp_addrt.addrt.addr.uap.value = cfg_dict["bdaddress"].uap
        send_req.tp_addrt.addrt.addr.nap.value = cfg_dict["bdaddress"].nap
        send_req.tp_addrt.tp_type.value = 1 #LE ACL
        send_req.min_tx.value = cfg_dict["min_tx"]
        send_req.max_tx.value = cfg_dict["max_tx"]
        send_req.min_rx.value = cfg_dict["min_rx"]
        send_req.max_rx.value = cfg_dict["max_rx"]
        send_req.flags.value = cfg_dict["flags"]
        #Send the prim down
        self.send(send_req)
        
    def init_dm_sm(self):
        """
        Initialise DM Security Manager
        """       
        req = self._utils.create_prim(
                               self._prims.DM_SM_INIT_REQ_T)
        req.type.value = self._types.DM_SM_INIT_REQ
        req.options.value = 0x9FF # DM_SM_INIT_ALL_SC_BREDR
        req.security_mode.value = 4 # SEC_MODE3_LINK
        req.security_level_default.value = 0xC000 # SECL4_IN_P256_ONLY | SECL4_OUT_P256_ONLY
        req.config.value = 0x00 
        req.write_auth_enable.value = 0
        req.mode3_enc.value = 0
        req.max_enc_key_size_thres.value = 0x10 # 16
        # Send them
        self.send(req)
        
        if self._linear:
            # Get and check the responses
            self.rsp_check("DM_SM_INIT_CFM")
        
    def init_dm_sm_check(self, ret=False, raw=None):
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        init_dm_sm_complete = self._utils.build_var(
                                self._prims.DM_SM_INIT_CFM_T,
                                var_address(rsp))
        if init_dm_sm_complete.status.value == 0:
            success = True
        else:
            success = False
            
        if ret:
            return (success)
        
        if free_mem:
            self._utils.free_var_mem(rsp)

    def sm_bond_request(self, **cfg_dict):
        """
        Security Bonding request
        """
        req = self._utils.create_prim(
                               self._prims.DM_SM_BONDING_REQ_T)
        req.type.value = self._types.DM_SM_BONDING_REQ
        req.addrt.type.value = 0
        req.addrt.addr.lap.value = cfg_dict["bd_addr"].lap
        req.addrt.addr.uap.value = cfg_dict["bd_addr"].uap
        req.addrt.addr.nap.value = cfg_dict["bd_addr"].nap
        req.flags.value = 0
        # Send the prim down
        self.send(req)

    def sm_bond_check(self, ret=False, raw=None):
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        sm_bond_cfm = self._utils.build_var(
                            self._prims.DM_SM_BONDING_CFM_T,
                            var_address(rsp))
        if sm_bond_cfm.status.value == 0:
            success = True
        else:
            success = False
            
        if ret:
            return (success)
        
        if free_mem:
            self._utils.free_var_mem(rsp)
        
    def sm_io_cap_req_rsp(self, **cfg_dict):
        rsp = self._utils.create_prim(
                               self._prims.DM_SM_IO_CAPABILITY_REQUEST_RSP_T)
        rsp.type.value = self._types.DM_SM_IO_CAPABILITY_REQUEST_RSP
        rsp.tp_addrt.addrt.type.value = cfg_dict["tp_addrt"].tp_addrt_addrt_type
        rsp.tp_addrt.addrt.addr.lap.value = cfg_dict["tp_addrt"].tp_addrt_addrt_lap
        rsp.tp_addrt.addrt.addr.uap.value = cfg_dict["tp_addrt"].tp_addrt_addrt_uap
        rsp.tp_addrt.addrt.addr.nap.value = cfg_dict["tp_addrt"].tp_addrt_addrt_nap
        rsp.tp_addrt.tp_type.value = cfg_dict["tp_addrt"].tp_addrt_tptype
        rsp.io_capability.value =  0
        rsp.oob_data_present.value = 0
        rsp.key_distribution.value = 0
        # Send the prim down
        self.send(rsp)

    def sm_io_cap_req_ind_check(self, raw=None):
        
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        # Save response params temporarily. These will be used in sm_io_cap_req_rsp
        self.io_cap_req_ind = NameSpace()
        io_cap_ind = self._utils.build_var(
                            self._prims.DM_SM_IO_CAPABILITY_REQUEST_IND_T,
                            var_address(rsp))
        # Save response params temporarily. These will be used in set_csb_receive
        self._io_cap_req_ind = NameSpace()
        self._io_cap_req_ind.tp_addrt_addrt_nap = io_cap_ind.tp_addrt.addrt.addr.nap.value
        self._io_cap_req_ind.tp_addrt_addrt_uap = io_cap_ind.tp_addrt.addrt.addr.uap.value
        self._io_cap_req_ind.tp_addrt_addrt_lap = io_cap_ind.tp_addrt.addrt.addr.lap.value
        self._io_cap_req_ind.tp_addrt_addrt_type = io_cap_ind.tp_addrt.addrt.type.value
        self._io_cap_req_ind.tp_addrt_tptype = io_cap_ind.tp_addrt.tp_type.value
        
        if free_mem:
            self._utils.free_var_mem(rsp)
        
    def set_ulp_default_phy_check(self, ret=False, raw=None):
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        set_ulp_default_phy_complete = self._utils.build_var(
                                self._prims.DM_ULP_SET_DEFAULT_PHY_CFM_T,
                                var_address(rsp))
        if set_ulp_default_phy_complete.status.value == 0:
            success = True
        else:
            success = False

        if ret:
            return (success)           
    
    def set_ulp_phy_check(self, ret=False, raw=None):
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        set_ulp_phy_complete = self._utils.build_var(
                                self._prims.DM_ULP_SET_PHY_CFM_T,
                                var_address(rsp))
        if set_ulp_phy_complete.status.value == 0:
            success = True
        else:
            success = False

        if ret:
            return (success)

    def hci_write_scan_enable_check(self, ret=False, raw=None):
        """
        Reconstruct a DM_HCI_WRITE_SCAN_ENABLE_CFM from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        hci_write_scan_enable = self._utils.build_var(
                                self._prims.DM_HCI_WRITE_SCAN_ENABLE_CFM_T,
                                var_address(rsp))

        if hci_write_scan_enable.status.value == 0:
            success = True
        else:
            success = False
            
        if ret:
            return (success)
        
        if free_mem:
            self._utils.free_var_mem(rsp)

    def hci_write_pagescan_activity_check(self, ret=False, raw=None):
        """
        Reconstruct a DM_HCI_WRITE_PAGESCAN_ACTIVITY_CFM from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        hci_write_pagescan_activity = self._utils.build_var(
                                      self._prims.DM_HCI_WRITE_PAGESCAN_ACTIVITY_CFM_T,
                                      var_address(rsp))

        if hci_write_pagescan_activity.status.value == 0:
            success = True
        else:
            success = False
            
        if ret:
            return (success)

        if free_mem:
            self._utils.free_var_mem(rsp)
            
    def receive_sync_train_req(self, **cfg_dict):
        """
        Create a DM_HCI_RECEIVE_SYNCHRONIZATION_TRAIN_REQ for CSB link and send it.
        """
        send_req = self._utils.create_prim(self._prims.DM_HCI_RECEIVE_SYNCHRONIZATION_TRAIN_REQ_T)
        send_req.common.op_code.value=self._types.DM_HCI_RECEIVE_SYNCHRONIZATION_TRAIN_REQ
        send_req.common.length.value = self._prims.DM_HCI_RECEIVE_SYNCHRONIZATION_TRAIN_REQ_T["byte_size"]
        send_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        send_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        send_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap
        send_req.sync_scan_timeout.value = HCIConst.CSB_SYNCHRONIZATION_SCAN_TIMEOUT
        send_req.sync_scan_window.value = HCIConst.CSB_SYNCHRONIZATION_SCAN_WINDOW
        send_req.sync_scan_interval.value = HCIConst.CSB_SYNCHRONIZATION_SCAN_INTERVAL
        # save current (or default) max message bytes size and set it to the size of response
        # message expected for this request.  
        self._max_message_byte_old_size = self._apps1.fw.call.trap_api_test_reset_max_message_body_bytes(
                                          self._prims.DM_HCI_RECEIVE_SYNCHRONIZATION_TRAIN_CFM_T["byte_size"])
        # Send the prim down
        self.send(send_req)

    def receive_sync_train_check(self, ret=False, raw=None):
        """
        Accept receive synchronization train confirm.
        """            
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        sync_train_complete = self._utils.build_var(
                                self._prims.DM_HCI_RECEIVE_SYNCHRONIZATION_TRAIN_CFM_T,
                                var_address(rsp))

        result = sync_train_complete.status.value    
        if result == 0: # '0' is for success
            success = True
            # Save response params temporarily. These will be used in set_csb_receive
            self._sync_train_info = NameSpace()                       
            self._sync_train_info.lt_addr = sync_train_complete.lt_addr.value
            self._sync_train_info.bd_addr_nap = sync_train_complete.bd_addr.nap.value
            self._sync_train_info.bd_addr_uap = sync_train_complete.bd_addr.uap.value
            self._sync_train_info.bd_addr_lap = sync_train_complete.bd_addr.lap.value
            self._sync_train_info.csb_interval = sync_train_complete.csb_interval.value
            self._sync_train_info.clock_offset = sync_train_complete.clock_offset.value
            self._sync_train_info.next_broadcast_instant = sync_train_complete.next_broadcast_instant.value
            self._sync_train_info.map = []
            for i in range(sync_train_complete.map.num_elements):
                self._sync_train_info.map.append(sync_train_complete.map[i].value)
        else:
            success = False
        # restore old (or default) max message body bytes value
        # overridden in receive_sync_train_req earlier
        self._apps1.fw.call.trap_api_test_reset_max_message_body_bytes(
                                        self._max_message_byte_old_size)
        if ret:
            return (success)           
        if free_mem:
            self._utils.free_var_mem(rsp)

    def set_csb_receive(self, enable=False, **cfg_dict):
        """
        Create a DM_HCI_SET_CSB_RECEIVE_REQ for CSB link and send it.
        """
        prim_dict = self._prims.DM_HCI_SET_CSB_RECEIVE_REQ_T
        send_req = self._utils.create_prim(prim_dict)
        send_req.common.op_code.value=self._types.DM_HCI_SET_CSB_RECEIVE_REQ
        send_req.common.length.value = prim_dict["byte_size"]
        send_req.enable.value=enable

        send_req.lt_addr.value = self._sync_train_info.lt_addr
        send_req.bd_addr.nap.value = self._sync_train_info.bd_addr_nap
        send_req.bd_addr.uap.value = self._sync_train_info.bd_addr_uap
        send_req.bd_addr.lap.value = self._sync_train_info.bd_addr_lap
        send_req.interval.value =  self._sync_train_info.csb_interval
        send_req.clock_offset.value =  self._sync_train_info.clock_offset
        send_req.next_csb_clock.value =  self._sync_train_info.next_broadcast_instant
        send_req.supervision_timeout.value = 0x200
        send_req.remote_timing_accuracy.value = 200
        send_req.skip.value = 0
        send_req.packet_type.value = 8
        # Copy afh map array 
        for i in range(len(self._sync_train_info.map)):
            send_req.afh_channel_map[i].value = self._sync_train_info.map[i]
        # Send the prim down
        self.send(send_req)

    def set_csb_receive_check(self, ret=False, raw=None):
        
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        if ret:
            return self._utils.build_var(self._prims.DM_HCI_SET_CSB_RECEIVE_CFM_T,
                                                  var_address(rsp))
        if free_mem:
            self._utils.free_var_mem(rsp)

    def csb_afh_map_available_ind(self, raw=None):
        """
        Process afh map available indication and store afh map info
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)

        afh_map_avail_ind = self._utils.build_var(
                                self._prims.DM_HCI_CSB_AFH_MAP_AVAILABLE_IND_T,
                                var_address(rsp))
        self._afh_map_info = NameSpace()
        self._afh_map_info.clock = afh_map_avail_ind.clock.value
        self._afh_map_info.map = []
        for i in range(afh_map_avail_ind.map.num_elements):
            self._afh_map_info.map.append(afh_map_avail_ind.map[i].value)

        if free_mem:
            self._utils.free_var_mem(rsp)

    def enter_sniff_mode_req(self, **cfg_dict):
        """
        Send enter sniff mode request
        """
        send_req = self._utils.create_prim(self._prims.DM_HCI_SNIFF_MODE_REQ_T)

        send_req.common.op_code.value = self._types.DM_HCI_SNIFF_MODE_REQ
        send_req.max_interval.value = 0x100
        send_req.min_interval.value = 0x100
        send_req.attempt.value = 0x1
        send_req.timeout.value = 0x1
        send_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        send_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        send_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap

        self.send(send_req)
        
    def exit_sniff_mode_req(self, **cfg_dict):
        """
        Send exit sniff mode request
        """
        send_req = self._utils.create_prim(self._prims.DM_HCI_EXIT_SNIFF_MODE_REQ_T)
        send_req.common.op_code.value = self._types.DM_HCI_EXIT_SNIFF_MODE_REQ
        send_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        send_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        send_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap

        self.send(send_req)

    def write_link_policy_settings(self, **cfg_dict):
        """
        Write link poilcy settings.
        """
        send_req = self._utils.create_prim(self._prims.DM_HCI_WRITE_LINK_POLICY_SETTINGS_REQ_T)

        send_req.common.op_code.value = self._types.DM_HCI_WRITE_LINK_POLICY_SETTINGS_REQ
        send_req.link_policy_settings.value = cfg_dict["link_policy_settings"]
        send_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        send_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        send_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap

        self.send(send_req)
        
    def hci_write_link_policy_settings_cfm_check(self, ret=False, raw=None):
        """
        Reconstruct a DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        hci_write_link_policy_settings_cfm = self._utils.build_var(
                                             self._prims.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM_T,
                                             var_address(rsp))

        if hci_write_link_policy_settings_cfm.status.value == 0:
            success = True
        else:
            success = False
        if ret:
            return (success)
        if free_mem:
            self._utils.free_var_mem(rsp)

    def switch_role(self, **cfg_dict):
        """
        Switch role
        """
        send_req = self._utils.create_prim(self._prims.DM_HCI_SWITCH_ROLE_REQ_T)

        send_req.common.op_code.value = self._types.DM_HCI_SWITCH_ROLE_REQ
        send_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        send_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        send_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap

        if cfg_dict['role'] == 'master':
            send_req.role.value = 0
        else:
            send_req.role.value = 1

        # Send the prim down
        self.send(send_req)

    def hci_switch_role_cfm_check(self, ret=False, raw=None):
        """
        Reconstruct a DM_HCI_SWITCH_ROLE_CFM_T from response prim
        """
        free_mem = (raw is None)
        rsp = self.rsp(raw=raw)
        hci_switch_role_cfm = self._utils.build_var(
                              self._prims.DM_HCI_SWITCH_ROLE_CFM_T,
                              var_address(rsp))

        # Used for debugging
        self.cfm = hci_switch_role_cfm
        self.cfm_status = hci_switch_role_cfm.status.value

        if hci_switch_role_cfm.status.value == 0:
            success = True
        else:
            success = False
            
        if ret:
            return (success)
        if free_mem:
            self._utils.free_var_mem(rsp)

    def handler(self, msg):
        """
        Simple handler for DM.  Sends a message to the app task after
         - registration completes
         - listening mode is confirmed
        """
               
        if msg["id"] != SysMsg.BLUESTACK_DM_PRIM:
            raise UnexpectedMsgType("Expected DM prim but received msg_id "
                                     "0x%x" % msg["id"])

        uprim = self.rsp(raw=msg["m"])
        
        if uprim.type.value == self._types.DM_AM_REGISTER_CFM:
            self.register_check(raw=var_address(uprim))
            self._utils.send_msg(self._app_task, DMAppMsgs.REGISTRATION_COMPLETE)

        elif uprim.type.value == self._types.DM_SYNC_REGISTER_CFM:
            self.sync_register_check(raw=var_address(uprim))
            self._utils.send_msg(self._app_task, DMAppMsgs.SYNC_REGISTRATION_COMPLETE)

        elif uprim.type.value == self._types.DM_SYNC_UNREGISTER_CFM:
            self._utils.send_msg(self._app_task, DMAppMsgs.SYNC_UNREGISTRATION_COMPLETE)

        elif uprim.type.value == self._types.DM_SYNC_CONNECT_IND:       
            self._utils.send_msg(self._app_task, DMAppMsgs.SYNC_CONNECT_IND)
            

        elif uprim.type.value == self._types.DM_SYNC_CONNECT_CFM:
            success, self.hci_handle = self.sync_connect_cnf_check(raw=var_address(uprim), ret=True)

            if success:
                self._utils.send_msg(self._app_task, DMAppMsgs.SYNC_CONNECTION_SUCCESS)
            else:           
                raise DMException ("Connection failure in master")

        elif uprim.type.value == self._types.DM_SYNC_CONNECT_COMPLETE_IND:
            success, self.hci_handle = self.sync_connect_cnf_check(raw=var_address(uprim), ret=True)
            if success:
                self._utils.send_msg(self._app_task, DMAppMsgs.SYNC_CONNECTION_SUCCESS)              
            else:           
                raise DMException ("Connection failure in slave")
 
        elif uprim.type.value in (self._types.DM_SYNC_DISCONNECT_CFM, self._types.DM_SYNC_DISCONNECT_IND):
            self._utils.send_msg(self._app_task, DMAppMsgs.SYNC_CONNECTION_TERMINATED)
  
        elif uprim.type.value in (self._types.DM_HCI_WRITE_PAGESCAN_ACTIVITY_CFM,
                                  self._types.DM_HCI_WRITE_SCAN_ENABLE_CFM):

            self._listening_cfms += 1

            if self._listening_cfms == 1:
                success = self.hci_write_scan_enable_check(raw=var_address(uprim), ret=True)
                if success:
                    self._log.info("DM_HCI_WRITE_SCAN_ENABLE confirmed")
                    self._utils.send_msg(self._app_task,
                                        DMAppMsgs.DM_HCI_WRITE_SCAN_ENABLE_CFM)
                else:
                    self._log.info("DM_HCI_WRITE_SCAN_ENABLE confirm failed")
                    self._utils.send_msg(self._app_task,
                                        DMAppMsgs.DM_HCI_WRITE_SCAN_ENABLE_CFM_FAILED)

            if self._listening_cfms == 2:
                success = self.hci_write_pagescan_activity_check(raw=var_address(uprim), ret=True)
                if success:
                    self._log.info("DM_HCI_WRITE_PAGESCAN_ACTIVITY confirmed")
                    self._utils.send_msg(self._app_task, DMAppMsgs.LISTENING)
                else:
                    self._log.info("DM_HCI_WRITE_PAGESCAN_ACTIVITY confirm failed")
                    self._utils.send_msg(self._app_task, DMAppMsgs.LISTENING_FAILED)
                                  
        elif uprim.type.value == self._types.DM_HCI_READ_BD_ADDR_CFM:
            self._set_local_bd_addr(uprim)
            self._utils.send_msg(self._app_task, 
                                 DMAppMsgs.LOCAL_BD_ADDR_OBTAINED)
            
        elif uprim.type.value in (self._types.DM_ACL_OPENED_IND,
                                  self._types.DM_ACL_CLOSED_IND,
                                  self._types.DM_SM_ACCESS_IND):
            # These are prims we expect to see so we won't make a fuss
            self._log.debug("Saw %s" % self._type_ids[uprim.type.value].
                                                            replace("ENUM_",""))
            
        elif uprim.type.value in (self._types.DM_HCI_CHANGE_LOCAL_NAME_REQ,):
            self._log.info("Local name change successful")
            
        elif uprim.type.value in (self._types.DM_HCI_READ_LOCAL_NAME_REQ,):
            name = self.read_local_name(raw=var_address(uprim))
            self._log.info("Read local name as '%s'" % name)

        elif uprim.type.value == self._types.DM_SET_BT_VERSION_CFM:
            self._log.info("BT Version set confirm received")
            self.check_bt_version(raw=var_address(uprim))
            self._utils.send_msg(self._app_task, 
                                 DMAppMsgs.BT_VER_SET)

        elif uprim.type.value == self._types.DM_HCI_SET_RESERVED_LT_ADDR_CFM:
            success = self.set_reserved_lt_addr_check(var_address(uprim), ret=True)
            if success:
                self._log.info("Set reserved lt_addr confirm received")
                self._utils.send_msg(self._app_task,
                                    DMAppMsgs.SET_RESERVED_LT_ADDR_COMPLETE)
            else:
                raise DMException ("Set reserved lt_addr failure")

        elif uprim.type.value == self._types.DM_HCI_SET_CSB_CFM:
            self._log.info("Set csb confirm received")
            self._utils.send_msg(self._app_task,
                                 DMAppMsgs.SET_CSB_COMPLETE)

        elif uprim.type.value == self._types.DM_HCI_WRITE_SYNCHRONIZATION_TRAIN_PARAMS_CFM:
            self._log.info("Write sychronization train param received")
            self._utils.send_msg(self._app_task,
                                 DMAppMsgs.WRITE_SYNCHRONIZATION_TRAIN_PARAM_COMPLETE)

        elif uprim.type.value == self._types.DM_HCI_RECEIVE_SYNCHRONIZATION_TRAIN_CFM:
            success = self.receive_sync_train_check(raw=var_address(uprim), ret=True)
            if success:
                self._log.info("Receive sychronization train confirm received")
                self._utils.send_msg(self._app_task,
                                    DMAppMsgs.RECEIVE_SYNCHRONIZATION_TRAIN_COMPLETE)
            else:
                self._log.info("Receive sychronization train confirm failed")
                self._utils.send_msg(self._app_task,
                                    DMAppMsgs.RECEIVE_SYNCHRONIZATION_TRAIN_FAILED)
            
        elif uprim.type.value == self._types.DM_HCI_SET_CSB_RECEIVE_CFM:
            self._log.info("Set csb receive confirm received")
            self._utils.send_msg(self._app_task,
                                 DMAppMsgs.SET_CSB_RECEIVE_COMPLETE)

        elif uprim.type.value == self._types.DM_HCI_START_SYNCHRONIZATION_TRAIN_CFM:
            self._log.info("Start synchronization train confirm received")
            self._utils.send_msg(self._app_task,
                         DMAppMsgs.START_SYNCHRONIZATION_TRAIN_COMPLETE)


        elif uprim.type.value == self._types.DM_HCI_CSB_AFH_MAP_AVAILABLE_IND:
            self._log.info("Csb afh map available indication received")
            self.csb_afh_map_available_ind(var_address(uprim))
            self._utils.send_msg(self._app_task,
                                 DMAppMsgs.CSB_AFH_MAP_AVAILABLE_IND)
            
        elif uprim.type.value == self._types.DM_HCI_CSB_CHANNEL_MAP_CHANGE_IND:
            self._log.info("Csb afh map change indication received")
            self._utils.send_msg(self._app_task,
                                 DMAppMsgs.CSB_AFH_MAP_CHANGE_IND)
            
        elif uprim.type.value == self._types.DM_ULP_SET_DEFAULT_PHY_CFM:
            success = self.set_ulp_default_phy_check(raw=var_address(uprim), ret=True)
            if success:
                self._log.info("2LE ULP set default PHY REQ confirmed")
                self._utils.send_msg(self._app_task,
                                 DMAppMsgs.DM_ULP_SET_DEFAULT_PHY_CFM)
            else:
                self._log.info("2LE ULP set default PHY REQ confirm failed")
                self._utils.send_msg(self._app_task,
                                    DMAppMsgs.DM_ULP_SET_DEFAULT_PHY_CFM_FAILED)
            
        elif uprim.type.value == self._types.DM_ULP_SET_PHY_CFM:
            success = self.set_ulp_phy_check(raw=var_address(uprim), ret=True)
            if success:
                self._log.info("2LE ULP set PHY REQ confirmed")
                self._utils.send_msg(self._app_task,
                                 DMAppMsgs.DM_ULP_SET_PHY_CFM)
            else:
                self._log.info("2LE ULP set PHY REQ confirm failed")
                self._utils.send_msg(self._app_task,
                                    DMAppMsgs.DM_ULP_SET_PHY_CFM_FAILED)

        elif uprim.type.value == self._types.DM_HCI_SWITCH_ROLE_CFM:
            success = self.hci_switch_role_cfm_check(raw=var_address(uprim), ret=True)
            if success:
                self._log.info("DM_HCI_SWITCH_ROLE_CFM confirmed")
                self._utils.send_msg(self._app_task,
                                 DMAppMsgs.DM_HCI_SWITCH_ROLE_CFM)
            else:
                self._log.info("DM_HCI_SWITCH_ROLE_CFM confirm failed")
                self._utils.send_msg(self._app_task,
                                    DMAppMsgs.DM_HCI_SWITCH_ROLE_CFM_FAILED)

        elif uprim.type.value == self._types.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM:
            success = self.hci_write_link_policy_settings_cfm_check(raw=var_address(uprim), ret=True)
            if success:
                self._utils.send_msg(self._app_task,
                                     DMAppMsgs.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM)
            else:
                self._utils.send_msg(self._app_task,
                                     DMAppMsgs.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM_FAILED)
                
        elif uprim.type.value == self._types.DM_SM_INIT_CFM:
            success = self.init_dm_sm_check(raw=var_address(uprim), ret=True)
            if success:
                self._utils.send_msg(self._app_task,
                                DMAppMsgs.SM_INIT_CFM)
            else:
                self._utils.send_msg(self._app_task,
                                DMAppMsgs.SM_INIT_CFM_FAILED)
                
        elif uprim.type.value == self._types.DM_SM_BONDING_CFM:
            success = self.sm_bond_check(raw=var_address(uprim), ret=True)
            if success:
                self._utils.send_msg(self._app_task,
                                DMAppMsgs.SM_BONDING_CFM)
            else:
                self._utils.send_msg(self._app_task,
                                DMAppMsgs.SM_BONDING_CFM_FAILED)
                
        elif uprim.type.value == self._types.DM_SM_IO_CAPABILITY_REQUEST_IND:
            self.sm_io_cap_req_ind_check(raw=var_address(uprim))
            self._utils.send_msg(self._app_task,
                                DMAppMsgs.SM_IO_CAPABILITY_REQUEST_IND)
        else:
            # Anything else might be problem
            self._log.warn("Saw %s" % self._type_ids[uprim.type.value].replace("ENUM_",""))

