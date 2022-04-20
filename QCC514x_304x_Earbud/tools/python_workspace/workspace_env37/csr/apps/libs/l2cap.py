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
from csr.dev.fw.trap_api.stream import Source, Sink
from csr.dev.env.env_helpers import var_address, var_size
from .dm import DMLib
import logging

class L2CAPrim(object):
    """
    A selection of the constants defined for prim types in l2cap_prim.h
    """
    REGISTER_REQ = 0x500
    REGISTER_CFM = 0x540
    AUTO_CONNECT_REQ = 0x501
    AUTO_CONNECT_RSP = 0x502
    AUTO_CONNECT_CFM = 0x541
    AUTO_CONNECT_IND = 0x542

class FlowMode(object):
    """
    A copy of the L2CA_FLOW_MODE enum from l2cap_prim.h
    """
    BASIC             = 0x00
    RTM_OBSOLETE      = 0x01
    FC_OBSOLETE       = 0x02
    ENHANCED_RETRANS  = 0x03
    STREAMING         = 0x04

class ConnectRsp(object):
    """
    Interesting values for the result field of L2CA_AUTO_CONNECT_XXX messages
    """
    INITIATING = 0x817
    PENDING    = 1
    SUCCESS    = 0

class L2capError(RuntimeError):
    """
    Generic L2cap error
    """

class L2capUnexpectedPrim(L2capError):
    """
    Indicates an unexpected L2CAP prim was seen
    """
    
class L2capConnectionStateError(L2capError):
    """
    Indicates that something is a bit wrong with the l2cap connection sequence
    """

class L2capRegistrationError(L2capError):
    """
    Indicates that L2CAP registration failed
    """

class L2CAPAppMsgs(object):
    """
    Messages that a controlling App might want to receive
    """
    REGISTRATION_COMPLETE = 0
    CONNECTION_COMPLETE = 1
    DISCONNECTION_COMPLETE = 2

class L2CAPLib(object):
    """
    Library of functionality supporting registering for L2CAP messages, creating
    raw connections and accessing them via Sources and Sinks
    """
    
    
    def __init__(self, trap_utils, linear=False, app_task=None, verbose=False,
                 tag=""):
        
        self._log = logging.getLogger(tag if tag else "l2cap")
        self._log.setLevel(logging.INFO if verbose else logging.WARNING)
        hdlr = logging.StreamHandler()
        hdlr.setFormatter(logging.Formatter('%(name)s: %(message)s'))
        self._log.addHandler(hdlr)

        self._utils = trap_utils
        self._apps1 = trap_utils.apps1
        self._apps0 = trap_utils.apps0
        self._app_task = app_task
        self._linear = linear

        self._prims = NameSpace()
        self._types = NameSpace()

        self._enum_prim_names = self._apps1.fw.env.enums["L2CAP_PRIM_T"]
        self._l2cap_prims = []
        for prim in self._enum_prim_names.keys():
            # Do not add the prims to prims list that are not in the elf file
            if (prim != 'ENUM_L2CA_CREATE_CHANNEL_IND' and
                prim != 'ENUM_L2CA_MOVE_CHANNEL_REQ' and
                prim != 'ENUM_L2CA_MOVE_CHANNEL_CMP_IND' and
                prim != 'ENUM_L2CA_MOVE_CHANNEL_RSP' and
                prim != 'ENUM_L2CA_CREATE_CHANNEL_CFM' and
                prim != 'ENUM_L2CA_MOVE_CHANNEL_CFM' and
                prim != 'ENUM_L2CA_MOVE_CHANNEL_IND' and
                prim != 'ENUM_L2CA_DEBUG_DROP_REQ' and
                prim != 'ENUM_L2CA_AMP_LINK_LOSS_IND' and
                prim != 'ENUM_L2CA_CREATE_CHANNEL_RSP' and
                prim != 'ENUM_L2CA_CREATE_CHANNEL_REQ' and
                prim != 'ENUM_L2CA_GET_CHANNEL_INFO_REQ'):
                self._l2cap_prims.append("%s_T" % prim[len('ENUM_'):])
        self._l2cap_prims.append("L2CA_UPRIM_T")
        try:
            # Note: we attempt to look these up in P*0*'s type dictionary 
            # because they aren't directly referenced in a bare-bones build 
            # of P1 firmware, so won't appear in its DWARF.
            env_types = self._apps0.fw.env.types
        except AttributeError:
            # However, if no P0 env is available, we're probably in customer
            # context, in which case the prims *will* be available in P1.
            env_types = self._apps1.fw.env.types
        for prim in self._l2cap_prims:
            setattr(self._prims, prim, env_types[prim])
            if prim != "L2CA_UPRIM_T":
                import re
                enumtor = re.sub("_T$", "", prim)
                setattr(self._types, enumtor, self._enum_prim_names["ENUM_%s"%enumtor])

        self._auto_conn_cfm_result_enum = self._apps0.fw.env.enums["l2ca_conn_result_t"]

        # State variable for current in-progress connection sequence
        self._conn_state = None
        # List of current connections, by CID
        self._conns = {}
        self._latest_conn_or_disconn = None
        
    def reset_app_task(self, new_task):
        """
        Register a different task for communicating with the App.  The caller 
        owns the task; this object simply stores a reference to it.
        """
        self._app_task = new_task
        
    def send(self, prim):
        """
        Push a DM prim through the right trap call
        """
        self._utils.call1.VmSendL2capPrim(prim)
 
    def rsp(self, raw=None):
        """
        Either get an L2CAP response messages from Bluestack if any is waiting,
        or process the given raw msg.  If a message is available on way or the 
        other, it is returned as a L2CA_UPRIM_T, which the caller can use 
        to determine the actual message, based on the "type" field.
        """
        if raw is None:
            raw = self._utils.get_core_msg(SysMsg.BLUESTACK_L2CAP_PRIM,
                                           timeout=2 if self._linear else None)
        if raw is False:
            return raw
        return self._utils.build_var(self._prims.L2CA_UPRIM_T, raw)

    def rsp_check(self, name, get_fields=None, raw=None):
        """
        Get the next L2CAP response message, if any, or process the provided
        raw message, check that it has the expected type, and optionally return 
        the values of any number of (integer) fields
        """
        
        # We free the message iff we retrieved it: if it is passed in, it is 
        # the caller's responsibility
        free_msg = (raw is None)

        prim_type = getattr(self._types, name)
        prim_dict = getattr(self._prims, name+"_T")
        
        rsp = self.rsp(raw=raw)
        if rsp == False:
            raise L2capUnexpectedPrim("No sign of %s" % name)
        
        type = rsp.type.value
        
        if type != prim_type:
            self._utils.free_var_mem(rsp)
            raise L2capUnexpectedPrim("Saw %s but expected %s " % 
                                      (self._enum_prim_names[type].replace("ENUM_",""), 
                                      name))
            
        if get_fields is not None:
            fields = []
            for field_name in get_fields:
                # Cast the prim to its real type and grab the value of the field.
                # Note that this only works for integer fields
                prim = self._utils.build_var(prim_dict, var_address(rsp))
                
                fields.append(getattr(prim, field_name).value)
            
        if free_msg:
            self._log.info("Freeing 0x%x after l2cap.rsp_check" % var_address(rsp))
            self._utils.free_var_mem(rsp)
        
        if get_fields is not None:
            return fields

    def register(self, psm=5, ret=False):
        """
        Send a registration request
        """
        reg_req = self._utils.create_prim(self._prims.L2CA_REGISTER_REQ_T)
        
        reg_req.type.value = self._types.L2CA_REGISTER_REQ
        reg_req.psm_local.value = psm
        reg_req.mode_mask.value = 1 << FlowMode.ENHANCED_RETRANS
        
        # Send the prim down
        self.send(reg_req)
        
        # If we're calling this in looping mode, the loop will execute register
        if self._linear:
            retd, = self.rsp_check("L2CA_REGISTER_CFM", get_fields=(ret,) if ret else [])
            return retd

    # --------------------------------------------------------------------
    # Synchronous connection setup
    # --------------------------------------------------------------------
            
            
    def start(self, dm):
        """
        Start up the L2CAP functionality in Bluestack along with the underlying
        dm functionality, caching the local bluetooth address
        """
        #self._utils.start_bluestack()

        # Set the firmware up to save BlueStack messages for us
        self._utils.handle_msg_type("BlueStack")
                
        dm.register()
        
        result = self.register(ret="result")
        if result != 0:
            raise L2capRegistrationError("Master L2CAP registration failed: saw '%s'" % result)
    
    def clear_conn_state(self):
        self._conn_state = None
        
    def auto_connect_init(self, **cfg_dict):
        """
        Initiate an auto_connect sequence
        """
        
        if self._conn_state is not None:
            raise L2capConnectionStateError("Can't start a connection: current "
                                            "connection in progress (%s)" % 
                                                            self._conn_state)
        
        conn_req = self._utils.create_prim(
                                       self._prims.L2CA_AUTO_CONNECT_REQ_T)
        conn_req.type.value = self._types.L2CA_AUTO_CONNECT_REQ
        conn_req.psm_local.value = cfg_dict["psm"]
        conn_req.bd_addr.uap.value = cfg_dict["bd_addr"].uap
        conn_req.bd_addr.nap.value = cfg_dict["bd_addr"].nap
        conn_req.bd_addr.lap.value = cfg_dict["bd_addr"].lap
        conn_req.psm_remote.value = cfg_dict["psm"]
        
        self.send(conn_req)
        self._conn_state = "AUTO_CONNECT_REQ_SENT"
        
        # Check for a response if we are not running in async mode
        if self._linear:
            return self.auto_connect_init_check()

    def auto_connect_init_check(self, raw=None):
        """
        Check that a received L2CAP prim is AUTO_CONNECT_CFM(initiating)
        """
        if self._conn_state != "AUTO_CONNECT_REQ_SENT":
            raise L2capConnectionStateError("Saw L2CA_AUTO_CONNECT_CFM(init) "
                                            "while in state %s" % 
                                                            self._conn_state)
        
        
        result,cid = self.rsp_check("L2CA_AUTO_CONNECT_CFM",
                                 get_fields=("result","cid"),
                                 raw=raw)
        if result != ConnectRsp.INITIATING:
            raise L2capUnexpectedPrim("CONNECT_CFM had result 0x%x, but "
                                      "expected L2CA_CONNECT_INITIATING (0x%x)",
                                      result,
                                      ConnectRsp.INITIATING)
        
        self._conn_state = "AUTO_CONNECT_CFM_INITIATING"
            
        # All good: we'll create a connection object
        self._conns[cid] = L2CAPConn(cid, self, is_master=True)
        self._latest_conn_or_disconn = cid
        
        return cid

    def auto_connect_ind_rsp(self, raw=None):
        """
        On a slave device, see an AUTO_CONNECT_IND and return an 
        AUTO_CONNECT_RSP
        """
        
        if self._conn_state is not None:
            raise L2capConnectionStateError("Received an L2CA_AUTO_CONNECT_IND"
                                            " while in connection state %s" % 
                                                            self._conn_state)
        
        cid, id = self.rsp_check("L2CA_AUTO_CONNECT_IND",
                                 get_fields=("cid",
                                             "identifier"),
                                 raw=raw)

        self._conns[cid] = L2CAPConn(cid, self, is_master=False)
        self._latest_conn_or_disconn = cid
        
        conn_rsp = self._utils.create_prim(self._prims.L2CA_AUTO_CONNECT_RSP_T)
        
        conn_rsp.type.value = self._types.L2CA_AUTO_CONNECT_RSP 
        conn_rsp.identifier.value = id
        conn_rsp.cid.value = cid
        
        self.send(conn_rsp)
        self._conn_state = "AUTO_CONNECT_RSP_SENT"
        return cid
        
    def auto_connect_complete(self, master=True,raw=None):
        """
        Perform the final stage of an L2CAP connection sequence on either master
        or slave (used by the synchronous interface only)
        """
        # The master sees L2CA_AUTO_CONNECT_CFM(pending)
        if master:
            
            self.auto_connect_pending_check(raw=raw)
        
        # They both see L2CA_AUTO_CONNECT_CFM(success)
        self.auto_connect_complete_check(is_master=master, raw=raw)
        

    def auto_connect_pending_check(self,raw=None):
        """
        Check that the next/received prim is L2CA_AUTO_CONNECT_CFM(pending)
        """
        if self._conn_state != "AUTO_CONNECT_CFM_INITIATING":
            raise L2capConnectionStateError("Saw L2CA_AUTO_CONNECT_CFM(pending) "
                                            "while in state %s" % 
                                                            self._conn_state)
        result, = self.rsp_check("L2CA_AUTO_CONNECT_CFM",
                                 get_fields=("result",),
                                 raw=raw)
        if result != ConnectRsp.PENDING:
            raise L2capUnexpectedPrim("Saw AUTO_CONNECT_CFM with status "
                                      "%s when expecting PENDING (0x%x)" %
                                      (self._auto_conn_cfm_result_enum[result], 
                                       ConnectRsp.PENDING))
            
        self._conn_state = "AUTO_CONNECT_CFM_PENDING"


    def auto_connect_complete_check(self,is_master=True,raw=None):
        """
        Check that the next/recevied prim is L2CA_AUTO_CONNECT_CFM(success)
        """
        if ((is_master and self._conn_state != "AUTO_CONNECT_CFM_PENDING")
            or (not is_master and 
                            self._conn_state != "AUTO_CONNECT_RSP_SENT")):
            raise L2capConnectionStateError("Saw L2CA_AUTO_CONNECT_CFM(success) "
                                            "while in state %s" % 
                                                            self._conn_state)
        result, = self.rsp_check("L2CA_AUTO_CONNECT_CFM",
                                get_fields=("result",),
                                raw=raw)
        if result != ConnectRsp.SUCCESS:
            raise L2capUnexpectedPrim("Saw AUTO_CONNECT_CFM with status "
                                      "0x%x when expected SUCCESS (0x%x)" %
                                      (result, ConnectRsp.SUCCESS))
        # Finished connecting
        self._conn_state = None


    def connect(self, slv_l2cap, **cfg_dict):
        
        """
        Drive the L2CAP connection sequence.  This goes as follows:
        
         1. Slave issues l2cap_slv_connect.  No response expected yet.
         2. Master issues l2cap_mst_connect. 
         3. Master expects the following:
             L2CA_AUTO_CONNECT_REQ 
             L2CA_AUTO_CONNECT_CFM (result = 0x0817 (L2CA_CONNECT_INITIATING)
             (DM_ACL_OPENED_IND)
             (DM_SM_ACCESS_IND)
             L2CA_AUTO_CONNECT_CFM (result = 0x0001 (L2CA_CONNECT_PENDING)
             L2CA_AUTO_CONNECT_CFM (result = 0x0000 (L2CA_CONNECT_SUCCESS)
         4. Slave expects the following:
             (DM_ACL_OPENED_IND)
             (DM_SM_ACCESS_IND)
             L2CA_AUTO_CONNECT_IND
             L2CA_AUTO_CONNECT_RSP (response = 0x0000 (L2CA_CONNECT_SUCCESS)
             L2CA_AUTO_CONNECT_CFM (response = 0x0000 (L2CA_CONNECT_SUCCESS)
             
        Returns objects representing the master and slave sides of the 
        connection
        """

        # Master issues L2CA_AUTO_CONNECT_REQ and expects
        #  L2CA_AUTO_CONNECT_CFM(initiating)
        self._log.info(" Initiating the connection")
        mst_cid = self.auto_connect_init(**cfg_dict)
            
        # Now we expect to see DM_ACL_OPENED_IND, DM_SM_ACCESS_IND on each
        #iprint(" Checking DM-level INDs on master")
        #mst_dm.auto_connect_ind()
        #iprint(" Checking DM-level INDs on slave")
        #slv_dm.auto_connect_ind()
        
        # ... followed on the slave by L2CA_AUTO_CONNECT_IND, which we have to 
        # respond to
        self._log.info(" Slave sees and responds to AUTO_CONNECT_IND")
        slv_cid = slv_l2cap.auto_connect_ind_rsp()
        
        # Then master should get L2CA_AUTO_CONNECT_CFM(pending) and both should
        # get L2CA_AUTO_CONNECT_CFM(success)
        self._log.info(" Waiting for master completion sequence")
        self.auto_connect_complete(master=True)
        self._log.info(" Waiting for slave completion sequence")
        slv_l2cap.auto_connect_complete(master=False)
        
        self._conns[mst_cid] = L2CAPConn(mst_cid, self)
        slv_l2cap._conns[slv_cid] = L2CAPConn(slv_cid, slv_l2cap,is_master=False)
        

    def disconnect(self, slv_l2cap, cid):
        """
        Do a linear disconnection sequence
        """
        self.disconnect_req(cid)
        slv_l2cap.disconnect_ind_rsp()
        self.disconnect_cfm_check()
        

    def disconnect_req(self, cid):
        """
        Request disconnection of the given channel
        """
        prim = self._utils.create_prim(self._prims.L2CA_DISCONNECT_REQ_T)
        prim.type.value = self._types.L2CA_DISCONNECT_REQ
        prim.cid.value = cid
        
        self.send(prim)
            
    def disconnect_ind_rsp(self, raw=None):
        """
        On a slave device, see a DISCONNECT_IND and return an 
        DISCONNECT_RSP
        """
        
        if self._conn_state is not None:
            raise L2capConnectionStateError("Received an L2CA_DISCONNECT_IND"
                                            " while in connection state %s" % 
                                                            self._conn_state)
        
        cid, id = self.rsp_check("L2CA_DISCONNECT_IND",
                                 get_fields=("cid",
                                             "identifier"),
                                 raw=raw)

        disconn_rsp = self._utils.create_prim(self._prims.L2CA_DISCONNECT_RSP_T)       
        disconn_rsp.type.value = self._types.L2CA_DISCONNECT_RSP 
        disconn_rsp.identifier.value = id
        disconn_rsp.cid.value = cid
        
        self.send(disconn_rsp)
        del self._conns[cid]
        self._latest_conn_or_disconn = cid

    def disconnect_cfm_check(self, raw=None):
        cid, = self.rsp_check("L2CA_DISCONNECT_CFM", raw=raw, 
                               get_fields=("cid",))
        del self._conns[cid]
        self._latest_conn_or_disconn = cid


    # ----------------------------------------------------------------------
    # Asynchronous interface
    # ----------------------------------------------------------------------

    def handler(self, msg):
        """
        General handler for L2CAP messages. Sends a message to the app task 
        after
         - registration completes
         - connection completes
        """
        if msg["id"] != SysMsg.BLUESTACK_L2CAP_PRIM:
            raise UnexpectedMsgType("Expected L2CAP prim but received msg_id "
                                     "0x%x" % msg["id"])
            
        uprim = self.rsp(msg["m"])
        
        self._log.info("Received %s" % self._utils.apps1.fw.env.enums["L2CAP_PRIM_T"][uprim.type.value])
        
        if uprim.type.value == self._types.L2CA_REGISTER_CFM:
            result, = self.rsp_check("L2CA_REGISTER_CFM", get_fields=("result",), 
                                             raw=var_address(uprim))
            if result != 0:
                raise L2capRegistrationError("L2CA_REGISTER_CFM result field "
                                             "was 0x%x" % result)
            self._utils.send_msg(self._app_task, 
                                 L2CAPAppMsgs.REGISTRATION_COMPLETE)


        elif uprim.type.value == self._types.L2CA_AUTO_CONNECT_IND:
            
            self.auto_connect_ind_rsp(raw=var_address(uprim))

        elif uprim.type.value == self._types.L2CA_AUTO_CONNECT_CFM:
            if self._conn_state is None:
                raise L2capUnexpectedPrim("Saw AUTO_CONNECT_CFM but not in "
                                          "connection sequence!")
            elif self._conn_state == "AUTO_CONNECT_REQ_SENT":
                # We've sent an auto connect request, so this should CFM should
                # have result "initiating"
                self.auto_connect_init_check(raw=var_address(uprim))
                
            elif self._conn_state == "AUTO_CONNECT_RSP_SENT":
                # Slave sees final connection
                self.auto_connect_complete_check(is_master=False, 
                                                 raw=var_address(uprim))
                self._utils.send_msg(self._app_task, 
                                     L2CAPAppMsgs.CONNECTION_COMPLETE)
                
            elif self._conn_state == "AUTO_CONNECT_CFM_INITIATING":
                self.auto_connect_pending_check(raw=var_address(uprim))
                
            elif self._conn_state == "AUTO_CONNECT_CFM_PENDING":
                self.auto_connect_complete_check(is_master=True, 
                                                 raw=var_address(uprim))
                self._utils.send_msg(self._app_task, 
                                     L2CAPAppMsgs.CONNECTION_COMPLETE)

        elif uprim.type.value == self._types.L2CA_DISCONNECT_IND:

            cid, = self.rsp_check("L2CA_DISCONNECT_IND", raw=var_address(uprim), 
                                   get_fields=("cid",))
            self.disconnect_ind_rsp(raw=var_address(uprim))
            self._utils.send_msg(self._app_task,
                                 L2CAPAppMsgs.DISCONNECTION_COMPLETE)
        
        elif uprim.type.value == self._types.L2CA_DISCONNECT_CFM:

            self.disconnect_cfm_check(raw=var_address(uprim))
            self._utils.send_msg(self._app_task,
                                 L2CAPAppMsgs.DISCONNECTION_COMPLETE)

        else:
            self._log.warn("Saw %s" % self._utils.apps1.fw.env.enums["L2CAP_PRIM_T"][uprim.type.value])

        self._log.info("New connection state: %s" % self._conn_state)

    def get_conn_cids(self):
        """
        """
        return self._conns.keys()

    def get_conn(self, cid):
        """
        """
        return self._conns[cid]

    def get_latest_conn_or_disconn(self):
        return self._latest_conn_or_disconn


class L2CAPConn(object):
    """
    Simple class representing one side of an L2CAP connection 
    """
    def __init__(self, cid, l2cap, is_master=True):
        
        self._cid = cid
        self._l2cap = l2cap
        self._utils = l2cap._utils
        self._is_mst = is_master
        
    @property
    def sink(self):
        """
        Return the Sink object associated with this connection, creating it
        if necessary
        """
        try:
            self._sink
        except AttributeError:
            sink_id = self._utils.call1.StreamL2capSink(self._cid)
            self._sink = Sink(sink_id, self._utils, drop_msgs=True)
        return self._sink
        
    @property
    def source(self):
        """
        Return the Source object associated with this connection, creating it
        if necessary
        """
        try:
            self._source
        except AttributeError:
            source_id = self._utils.call1.StreamSourceFromSink(
                                    self._utils.call1.StreamL2capSink(self._cid))
            self._source = Source(source_id, self._utils, drop_msgs=True)
        return self._source
    
    def disconnect(self):
        """
        Disconnect the connection
        """
        return NotImplemented


