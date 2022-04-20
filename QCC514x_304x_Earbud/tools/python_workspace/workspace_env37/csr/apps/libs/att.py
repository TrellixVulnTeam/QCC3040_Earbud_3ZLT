############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.bitsandbobs import NameSpace
from csr.dev.fw.trap_api.system_message import SystemMessage as SysMsg
from csr.dev.fw.trap_api.trap_utils import UnexpectedMsgType
from csr.dev.fw.trap_api.stream import Source, Sink
from csr.dev.env.env_helpers import var_address, var_size
from .dm import DMLib
import logging

class ConnectRsp(object):
    """
    Interesting values for the result field of ATT_CONNECT_XXX messages
    """
    INITIATING = 0x817
    PENDING    = 1
    SUCCESS    = 0
    INDICATION_SENT = 0xff7a

class ATTError(RuntimeError):
    """
    Generic ATT error
    """

class ATTUnexpectedPrim(ATTError):
    """
    Indicates an unexpected ATT prim was seen
    """
    
class ATTConnectionStateError(ATTError):
    """
    Indicates that something is a bit wrong with the ATT connection sequence
    """

class ATTRegistrationError(ATTError):
    """
    Indicates that ATT registration failed
    """

class ATTHandleValueError(ATTError):
    """
    Indicates that ATT handle value sequence has failed
    """

class ATTAppMsgs(object):
    """
    Messages that a controlling App might want to receive
    """
    REGISTRATION_COMPLETE = 0
    CONNECTION_COMPLETE = 1
    DISCONNECTION_COMPLETE = 2
    UNREGISTRATION_COMPLETE = 3
    ADD_DB_CFM = 4
    HANDLE_VALUE_COMPLETE = 5

class ATTLib(object):
    """
    Library of functionality supporting registering for ATT messages, creating
    raw connections and accessing them via Sources and Sinks
    """
    
    
    def __init__(self, trap_utils, app_task=None, verbose=False,
                 tag=""):
        
        self._log = logging.getLogger(tag if tag else "att")
        self._log.setLevel(logging.INFO if verbose else logging.WARNING)
        hdlr = logging.StreamHandler()
        hdlr.setFormatter(logging.Formatter('%(name)s: %(message)s'))
        self._log.addHandler(hdlr)

        self._utils = trap_utils
        self._apps1 = trap_utils.apps1
        self._apps0 = trap_utils.apps0
        self._app_task = app_task

        self._prims = NameSpace()
        self._types = NameSpace()
        self._enum_prim_names = self._apps1.fw.env.enums["ATT_PRIM_T"]
        try:
            # Note: we attempt to look these up in P*0*'s type dictionary 
            # because they aren't directly referenced in a bare-bones build 
            # of P1 firmware, so won't appear in its DWARF.
            env_types = self._apps0.fw.env.types
        except AttributeError:
            # However, if no P0 env is available, we're probably in customer
            # context, in which case the prims *will* be available in P1.
            env_types = self._apps1.fw.env.types
        for prim in ("ATT_UPRIM_T",
                     "ATT_REGISTER_REQ_T",
                     "ATT_REGISTER_CFM_T",
                     "ATT_UNREGISTER_REQ_T",
                     "ATT_UNREGISTER_CFM_T",
                     "ATT_CONNECT_REQ_T",
                     "ATT_CONNECT_RSP_T",
                     "ATT_CONNECT_CFM_T",
                     "ATT_CONNECT_IND_T",
                     "ATT_DISCONNECT_REQ_T",
                     "ATT_DISCONNECT_IND_T",
                     "ATT_DISCONNECT_CFM_T",                     
                     "ATT_HANDLE_VALUE_REQ_T",                     
                     "ATT_HANDLE_VALUE_CFM_T",
                     "ATT_ADD_DB_REQ_T",                     
                     "ATT_ADD_DB_CFM_T"):
            setattr(self._prims, prim, env_types[prim])
            if prim != "ATT_UPRIM_T":
                enumtor = prim.replace("_T","")
                setattr(self._types, enumtor, 
                                    self._enum_prim_names["ENUM_%s" % enumtor])
        #self._conn_cfm_result_enum = self._apps0.fw.env.enums["l2ca_conn_result_t"]


        # State variable for current in-progress connection sequence
        self._conn_state = None
        # List of current connections, by CID
        self._conns = {}
        self._latest_conn_or_disconn = None
        self._handle_ind = None
        self._att_src_id = None
        self._le_conn_on = None        
        
    def reset_app_task(self, new_task):
        """
        Register a different task for communicating with the App.  The caller 
        owns the task; this object simply stores a reference to it.
        """
        self._app_task = new_task
        
    def send(self, prim):
        """
        Push a prim through the right trap call
        """
        self._utils.call1.VmSendAttPrim(prim)
 
    def rsp(self, raw=None):
        """
        Either get an ATT response messages from Bluestack if any is waiting,
        or process the given raw msg.  If a message is available on way or the 
        other, it is returned as a ATT_UPRIM_T, which the caller can use 
        to determine the actual message, based on the "type" field.
        """
        if raw is None:
            raw = self._utils.get_core_msg(SysMsg.BLUESTACK_ATT_PRIM,
                                           None)
        if raw is False:
            return raw
        return self._utils.build_var(self._prims.ATT_UPRIM_T, raw)

    def rsp_check(self, name, get_fields=None, raw=None):
        """
        Get the next ATT response message, if any, or process the provided
        raw message, check that it has the expected type, and optionally return 
        the values of any number of (integer) fields
        """
        
        # We free the message if we retrieved it: if it is passed in, it is 
        # the caller's responsibility
        free_msg = (raw is None)

        prim_type = getattr(self._types, name)
        prim_dict = getattr(self._prims, name+"_T")
        
        rsp = self.rsp(raw=raw)
        if rsp == False:
            raise ATTUnexpectedPrim("No sign of %s" % name)
        
        type = rsp.type.value
        
        if type != prim_type:
            self._utils.free_var_mem(rsp)
            raise ATTUnexpectedPrim("Saw %s but expected %s " % 
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
            self._log.info("Freeing 0x%x after att.rsp_check" % var_address(rsp))
            self._utils.free_var_mem(rsp)
        
        if get_fields is not None:
            return fields

    def register(self, ret=False):
        """
        Send a registration request
        """
        reg_req = self._utils.create_prim(self._prims.ATT_REGISTER_REQ_T)
        
        reg_req.type.value = self._types.ATT_REGISTER_REQ
                
        # Send the prim down
        self.send(reg_req)

    def unregister(self, ret=False):
        """
        Send a registration request
        """
        reg_req = self._utils.create_prim(self._prims.ATT_UNREGISTER_REQ_T)
        
        reg_req.type.value = self._types.ATT_UNREGISTER_REQ
                
        # Send the prim down
        self.send(reg_req)
        
            
    # --------------------------------------------------------------------
    # Synchronous connection setup
    # --------------------------------------------------------------------
            
            
    def start(self, dm):
        """
        Start up the ATT functionality in Bluestack along with the underlying
        dm functionality, caching the local bluetooth address
        """
        #self._utils.start_bluestack()

        # Set the firmware up to save BlueStack messages for us
        self._utils.handle_msg_type("BlueStack")
                
        dm.register()
        
        result = self.register(ret="result")
        if result != 0:
            raise ATTRegistrationError("Master ATT registration failed: saw '%s'" % result)
    
    def clear_conn_state(self):
        self._conn_state = None
        
    def connect_init(self, le_conn=0, **cfg_dict):
        """
        Initiate an connect sequence
        """
        
        if self._conn_state is not None:
            raise ATTConnectionStateError("Can't start a connection: current "
                                            "connection in progress (%s)" % 
                                                            self._conn_state)
        
        conn_req = self._utils.create_prim(
                                       self._prims.ATT_CONNECT_REQ_T)
        conn_req.type.value = self._types.ATT_CONNECT_REQ
        conn_req.addrt.addr.uap.value = cfg_dict["bd_addr"].uap
        conn_req.addrt.addr.nap.value = cfg_dict["bd_addr"].nap
        conn_req.addrt.addr.lap.value = cfg_dict["bd_addr"].lap
        if not le_conn:
            # setting for BR/EDR connection
            conn_req.flags.value = le_conn
        else:
            # setting for LE connection
            conn_req.flags.value = le_conn
            self._le_conn_on = True
        
        self.send(conn_req)
        self._conn_state = "CONNECT_REQ_SENT"        

    def connect_init_check(self, raw=None):
        """
        Check that a received ATT prim is CONNECT_CFM(initiating)
        """
        if self._conn_state != "CONNECT_REQ_SENT":
            raise ATTConnectionStateError("Saw ATT_CONNECT_CFM(init) "
                                            "while in state %s" % 
                                                      self._conn_state)
        
        
        result,cid = self.rsp_check("ATT_CONNECT_CFM",
                                 get_fields=("result","cid"),
                                 raw=raw)
        if result != ConnectRsp.INITIATING:
            raise ATTUnexpectedPrim("CONNECT_CFM had result 0x%x, but "
                                      "expected ATT_CONNECT_INITIATING (0x%x)",
                                      result,
                                      ConnectRsp.INITIATING)
        
        self._conn_state = "CONNECT_CFM_INITIATING"
            
        # create a connection object
        self._conns[cid] = ATTConn(cid, self, is_master=True)
        self._latest_conn_or_disconn = cid
        
        return cid

    def connect_ind_rsp(self, raw=None):
        """
        On a slave device, see an CONNECT_IND and return an 
        CONNECT_RSP
        """
        
        if self._conn_state is not None:
            raise ATTConnectionStateError("Received an ATT_CONNECT_IND"
                                            " while in connection state %s" % 
                                                            self._conn_state)
        
        cid, = self.rsp_check("ATT_CONNECT_IND",
                                 get_fields=("cid",),
                                 raw=raw)

        self._conns[cid] = ATTConn(cid, self, is_master=False)
        self._latest_conn_or_disconn = cid
        
        conn_rsp = self._utils.create_prim(self._prims.ATT_CONNECT_RSP_T)
        
        conn_rsp.type.value = self._types.ATT_CONNECT_RSP         
        conn_rsp.cid.value = cid
        
        self.send(conn_rsp)
        self._conn_state = "CONNECT_RSP_SENT"
        return cid
        
    def connect_complete(self, master=True,raw=None):
        """
        Perform the final stage of an ATT connection sequence on either master
        or slave
        """
        # The master sees ATT_CONNECT_CFM(pending)
        if master:            
            self.connect_pending_check(raw=raw)
        
        # They both see ATT_CONNECT_CFM(success)
        self.connect_complete_check(is_master=master, raw=raw)
        

    def connect_pending_check(self,raw=None):
        """
        Check that the next/received prim is ATT_CONNECT_CFM(pending)
        """
        if self._conn_state != "CONNECT_CFM_INITIATING":
            raise ATTConnectionStateError("Saw ATT_CONNECT_CFM(pending) "
                                            "while in state %s" % 
                                                            self._conn_state)
        result, = self.rsp_check("ATT_CONNECT_CFM",
                                 get_fields=("result",),
                                 raw=raw)
        if result != ConnectRsp.PENDING:
            raise ATTUnexpectedPrim("Saw CONNECT_CFM with status "
                                      "%s when expecting PENDING (0x%x)" %
                                      (result, ConnectRsp.PENDING))
            
        self._conn_state = "CONNECT_CFM_PENDING"


    def connect_complete_check(self,is_master=True,raw=None):
        """
        Check that the next/recevied prim is ATT_CONNECT_CFM(success)
        """
        if ((is_master and self._conn_state != "CONNECT_CFM_PENDING")
            or (not is_master and 
                            self._conn_state != "CONNECT_RSP_SENT")):
            raise ATTConnectionStateError("Saw ATT_CONNECT_CFM(success) "
                                            "while in state %s" % 
                                                            self._conn_state)
        result,cid = self.rsp_check("ATT_CONNECT_CFM",
                                get_fields=("result","cid"),
                                raw=raw)
        if result != ConnectRsp.SUCCESS:
            raise ATTUnexpectedPrim("Saw CONNECT_CFM with status "
                                      "0x%x when expected SUCCESS (0x%x)" %
                                      (result, ConnectRsp.SUCCESS))
        # Finished connecting
        self._conn_state = None

    def connect_le_complete_check(self,raw=None):
        """
        Check that the next/recevied prim is ATT_CONNECT_CFM(success)
        """
        if (self._conn_state != "CONNECT_CFM_INITIATING"):
            raise ATTConnectionStateError("Saw ATT_CONNECT_CFM(success) "
                                            "while in state %s" % 
                                            self._conn_state)
        result,cid = self.rsp_check("ATT_CONNECT_CFM",
                                get_fields=("result","cid"),
                                raw=raw)
        if result != ConnectRsp.SUCCESS:
            raise ATTUnexpectedPrim("Saw CONNECT_CFM with status "
                                      "0x%x when expected SUCCESS (0x%x)" %
                                      (result, ConnectRsp.SUCCESS))
        # Finished connecting
        self._conn_state = None
        
    def disconnect_req(self, cid):
        """
        Request disconnection
        """
        prim = self._utils.create_prim(self._prims.ATT_DISCONNECT_REQ_T)
        prim.type.value = self._types.ATT_DISCONNECT_REQ
        prim.cid.value = cid
        
        self.send(prim)
            
    def disconnect_ind_check(self, raw=None):
        """
        On a master or slave device, see a DISCONNECT_IND
        """
        cid, = self.rsp_check("ATT_DISCONNECT_IND",
                                 get_fields=("cid",),
                                 raw=raw)
        
        del self._conns[cid]
        self._latest_conn_or_disconn = cid
        if self._le_conn_on:
            self._le_conn_on = False

    def att_handle_value_req(self, cid, handle, flag):        
        """        
        ATT handle value request. 
        Flag received is 1 for indication and 0 for notification
        """        
        prim = self._utils.create_prim(self._prims.ATT_HANDLE_VALUE_REQ_T)        
        prim.type.value = self._types.ATT_HANDLE_VALUE_REQ        
        prim.cid.value = cid
        prim.handle.value = handle
        prim.flags.value = flag
        # to keep track whether sending HANDLE VALUE INDICATION OR NOTIFICATION
        self._handle_ind = flag
        prim.size_value.value = 1
        handle_value = self._apps1.fw.call.pnew("uint16")
        # trying random value
        handle_value.value = 0xb
        # put address of handle value list to prim
        prim.value.value = var_address(handle_value)        
        self.send(prim)

    def att_add_db(self):        
        """        
        Add ATT database        
        """        
        prim = self._utils.create_prim(self._prims.ATT_ADD_DB_REQ_T)        
        prim.type.value = self._types.ATT_ADD_DB_REQ        
        prim.flags.value = 0x1
        prim.size_db.value = 4
        db_value = [0x3005, 0x0a01, 0x0002, 0xb000]
        # put address of databse list to prim
        prim.db.value = id(db_value)
        
        self.send(prim)

    def att_handle_value_check(self, raw=None):        
        """        
        On a master seen a ATT_HANDLE_VALUE_CFM.
        For Indication expecting 2 of these and for notification
        expecting only 1.
        """
        complete = False
        if self._conn_state == None and self._handle_ind == 1:
            # This must be first confirmation since it is Indication
            self._conn_state = "ATT_HANDLE_VALUE_IND_SENT"
        elif ((self._conn_state == "ATT_HANDLE_VALUE_IND_SENT") or
              (self._conn_state == None and self._handle_ind == 0)):
            #This is second confirmation or only one confirm for notification
            self._conn_state = None            
            complete = True
        else:
            raise ATTHandleValueError("Saw ATT_HANDLE_VALUE_CFM"
                                            "while in state %s" % 
                                             self._conn_state)     
            
        cid,result = self.rsp_check("ATT_HANDLE_VALUE_CFM",                                 
                                     get_fields=("cid","result"),                                 
                                     raw=raw)
                                     
        if ((result != ConnectRsp.INDICATION_SENT) and 
            (result != ConnectRsp.SUCCESS)):
            self._conn_state = None
            complete = False
            raise ATTHandleValueError("Saw ATT_HANDLE_VALUE_CFM with status "                                      
                "0x%x when expected INDICATION_SENT (0x%x) or SUCCESS (0x%x)" %                                      
                (result, ConnectRsp.INDICATION_SENT, ConnectRsp.SUCCESS))

        return complete

    def add_db_cfm_check(self, raw=None):        
        """        
        On a master or slave device, see a ATT_ADD_DB_CFM        
        """        
        result, = self.rsp_check("ATT_ADD_DB_CFM",                                 
                                 get_fields=("result",),                                 
                                 raw=raw)
        if result != ConnectRsp.SUCCESS:            
            raise ATTHandleValueError("Saw ADD_DB_CFM with status "                                      
                                    "0x%x when expected SUCCESS (0x%x)" %                                      
                                    (result, ConnectRsp.SUCCESS))

    def source_add_att_handle(self, cid, att_handle):        
        """        
        Return the handle object added with this ATT connection        
        """
        att_utils = self._utils.call1
        self._att_src_id = att_utils.StreamAttSourceAddHandle(cid, att_handle)
        if not self._att_src_id:
            raise ATTHandleValueError("Addition of attribute handle failed "
                                      "for ATT connection with cid %s" % cid)
        return self._att_src_id


    # ----------------------------------------------------------------------
    # Asynchronous interface
    # ----------------------------------------------------------------------

    def handler(self, msg):
        """
        General handler for ATT messages. Sends a message to the app task 
        after
         - registration completes
         - connection completes
        """
        if msg["id"] != SysMsg.BLUESTACK_ATT_PRIM:
            raise UnexpectedMsgType("Expected ATT prim but received msg_id "
                                     "0x%x" % msg["id"])
            
        uprim = self.rsp(msg["m"])
        
        self._log.info("Received %s" % self._utils.apps1.fw.env.enums["ATT_PRIM_T"][uprim.type.value])
        
        if uprim.type.value == self._types.ATT_REGISTER_CFM:
            result, = self.rsp_check("ATT_REGISTER_CFM", get_fields=("result",), 
                                             raw=var_address(uprim))
            if result != 0:
                raise ATTRegistrationError("ATT_REGISTER_CFM result field "
                                             "was 0x%x" % result)
            self._utils.send_msg(self._app_task, 
                                 ATTAppMsgs.REGISTRATION_COMPLETE)


        elif uprim.type.value == self._types.ATT_CONNECT_IND:
            
            self.connect_ind_rsp(raw=var_address(uprim))

        elif uprim.type.value == self._types.ATT_CONNECT_CFM:
            if self._conn_state is None:
                raise ATTUnexpectedPrim("Saw CONNECT_CFM but not in "
                                          "connection sequence!")
            elif self._conn_state == "CONNECT_REQ_SENT":
                # We've sent an connect request, so this should CFM should
                # have result "initiating"
                self.connect_init_check(raw=var_address(uprim))
                
            elif self._conn_state == "CONNECT_RSP_SENT":
                # Slave sees final connection
                self.connect_complete_check(is_master=False, 
                                            raw=var_address(uprim))
                self._utils.send_msg(self._app_task, 
                                     ATTAppMsgs.CONNECTION_COMPLETE)
                
            elif self._conn_state == "CONNECT_CFM_INITIATING":
                if self._le_conn_on:
                    self.connect_le_complete_check(raw=var_address(uprim))
                    self._utils.send_msg(self._app_task, 
                                     ATTAppMsgs.CONNECTION_COMPLETE)
                else:
                    self.connect_pending_check(raw=var_address(uprim))
                
            elif self._conn_state == "CONNECT_CFM_PENDING":
                self.connect_complete_check(is_master=True, 
                                            raw=var_address(uprim))
                self._utils.send_msg(self._app_task, 
                                     ATTAppMsgs.CONNECTION_COMPLETE)

        elif uprim.type.value == self._types.ATT_DISCONNECT_IND:
            
            self.disconnect_ind_check(raw=var_address(uprim))
            self._utils.send_msg(self._app_task,
                                 ATTAppMsgs.DISCONNECTION_COMPLETE)

        elif uprim.type.value == self._types.ATT_UNREGISTER_CFM:            
            self._utils.send_msg(self._app_task, 
                                 ATTAppMsgs.UNREGISTRATION_COMPLETE)

        elif uprim.type.value == self._types.ATT_ADD_DB_CFM:            
            self.add_db_cfm_check(raw=var_address(uprim))            
            self._utils.send_msg(self._app_task,                                  
                                 ATTAppMsgs.ADD_DB_CFM)

        elif uprim.type.value == self._types.ATT_HANDLE_VALUE_CFM:
            complete = self.att_handle_value_check(raw=var_address(uprim))
            if complete:
                self._utils.send_msg(self._app_task, 
                                     ATTAppMsgs.HANDLE_VALUE_COMPLETE)

        else:
            self._log.warn("Saw %s" % self._utils.apps1.fw.env.enums["ATT_PRIM_T"][uprim.type.value])

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


class ATTConn(object):
    """
    Simple class representing one side of an ATT connection 
    """
    def __init__(self, cid, att, is_master=True):
        
        self._cid = cid
        self._att = att
        self._utils = att._utils
        self._is_mst = is_master
        
    @property    
    def source(self):        
        """        
        Return the Source object associated with this connection, if not already stored 
        then get it 
        """        
        try:            
            self._source        
        except AttributeError:            
            source_id = self._utils.call1.StreamAttSource(self._cid,self._att_handle)                    
        return self._source       
    
    def disconnect(self):
        """
        Disconnect the connection
        """
        return NotImplemented

