############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import logging
from csr.wheels.bitsandbobs import NameSpace
from csr.dev.fw.trap_api.system_message import SystemMessage as SysMsg
from csr.dev.fw.trap_api.stream import Sink, Source
from csr.dev.env.env_helpers import var_address, var_size

class ConnectRsp(object):
    """
    Interesting values for the result field of RFC_AUTO_CONNECT_XXX messages
    """
    INITIATING = 0x817
    PENDING    = 1
    SUCCESS    = 0

class RfcommError(RuntimeError):
    """
    Generic Rfcomm error
    """

class RfcommUnexpectedPrim(RfcommError):
    """
    Indicates an unexpected RFCOMM prim was seen
    """
    
class RfcommConnectionStateError(RfcommError):
    """
    Indicates that something is a bit wrong with the RFCOMM connection sequence
    """

class RfcommRegistrationError(RfcommError):
    """
    Indicates that RFCOMM registration failed
    """

class RfcommConnectionError(RfcommError):
    """
    Indicates that a connection failure status was received in a client 
    connection confirmation
    """

class RfcommRemoteRefusal(RfcommConnectionError):
    """
    The server explicitly refused a connection attempt from the client, perhaps 
    because the requested channel wasn't available
    """

class RFCOMMAppMsgs(object):
    """
    Messages that a controlling App might want to receive
    """
    REGISTRATION_COMPLETE = 0
    INIT_COMPLETE = 1
    CONNECTION_COMPLETE = 2
    DISCONNECTION_COMPLETE = 3
    ERROR_IND = 0xff

class RFCOMMLib(object):
    """
    Library of functionality supporting registering for RFCOMM messages, creating
    raw connections and accessing them via Sources and Sinks
    """
    
    
    def __init__(self, trap_utils, linear=False, app_task=None, verbose=False,
                 tag=""):
        
        self._log = logging.getLogger(tag if tag else "rfcomm")
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
        self._enum_prim_names = self._apps1.fw.env.enums["RFC_PRIM_T"]
        self._error_codes = self._apps1.fw.env.enums["RFC_RESULT_T"]
        try:
            # Note: we attempt to look these up in P*0*'s type dictionary 
            # because they aren't directly referenced in a bare-bones build 
            # of P1 firmware, so won't appear in its DWARF.
            env_types = self._apps0.fw.env.types
        except AttributeError:
            # However, if no P0 env is available, we're probably in customer
            # context, in which case the prims *will* be available in P1.
            env_types = self._apps1.fw.env.types
        for prim in ("RFC_PRIM_T",
                     "RFC_INIT_REQ_T",
                     "RFC_INIT_CFM_T",
                     "RFC_REGISTER_REQ_T",
                     "RFC_REGISTER_CFM_T",
                     "RFC_CLIENT_CONNECT_REQ_T",
                     "RFC_SERVER_CONNECT_IND_T",
                     "RFC_SERVER_CONNECT_RSP_T",
                     "RFC_CLIENT_CONNECT_CFM_T",
                     "RFC_SERVER_CONNECT_CFM_T",
                     "RFC_DISCONNECT_REQ_T",
                     "RFC_DISCONNECT_IND_T",
                     "RFC_DISCONNECT_RSP_T",
                     "RFC_DISCONNECT_CFM_T",
                     "RFC_ERROR_IND_T"):
            setattr(self._prims, prim, env_types[prim])
            if prim != "RFC_PRIM_T":
                enumtor = prim.replace("_T","")
                setattr(self._types, enumtor, 
                                    self._enum_prim_names[enumtor])
        #self._auto_conn_cfm_result_enum = self._apps0.fw.env.enums["l2ca_conn_result_t"]


        # State variable for current in-progress connection sequence
        self._conn_state = None
        # Counter for connection IDs
        self._next_conn_id = 1
        # List of current connections, by channel number
        self._conns = {}
        self._latest_conn_or_disconn = None
        
    def clear_conn_state(self):
        self._conn_state = None
        
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
        self._utils.call1.VmSendRfcommPrim(prim)
 
    def rsp(self, raw=None):
        """
        Either get an RFCOMM response messages from Bluestack if any is waiting,
        or process the given raw msg.  If a message is available on way or the 
        other, it is returned as a RFC_PRIM_T, which the caller can use 
        to determine the actual message, based on the "type" field.
        """
        if raw is None:
            raw = self._utils.get_core_msg(SysMsg.BLUESTACK_RFCOMM_PRIM,
                                           timeout=2 if self._linear else None)
        if raw is False:
            return raw
        return self._utils.build_var(self._apps1.fw.env.types["RFC_PRIM_T"], raw)

    def rsp_check(self, name, get_fields=None, raw=None):
        """
        Get the next RFCOMM response message, if any, or process the provided
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
            raise RfcommUnexpectedPrim("No sign of %s" % name)
        
        type = rsp.value
        type_name = rsp.symbolic_value
        
        if type != prim_type:
            self._utils.free_var_mem(rsp)
            raise RfcommUnexpectedPrim("Saw %s but expected %s " % (type_name, 
                                                                    name))
            
        if get_fields is not None:
            fields = []
            for field_name in get_fields:
                # Cast the prim to its real type and grab the value of the field.
                # Note that this only works for integer fields
                prim = self._utils.build_var(prim_dict, var_address(rsp))
                
                fields.append(getattr(prim, field_name).value)
            
        if free_msg:
            self._utils.free_var_mem(rsp)
        
        if get_fields is not None:
            return fields

    def register(self, channel, ret=False):
        """
        Send a registration request for a server channel
        """
        reg_req = self._utils.create_prim(self._prims.RFC_REGISTER_REQ_T)
        
        reg_req.type.value = self._types.RFC_REGISTER_REQ
        reg_req.loc_serv_chan_req.value = channel
        
        # Send the prim down
        self.send(reg_req)
        
        # If we're calling this in looping mode, the loop will execute register
        if self._linear:
            retd, = self.rsp_check("RFC_REGISTER_CFM", 
                                   get_fields=("accept",) if ret else None)
            return retd


    def unregister(self, channel, ret=False):
        """
        Send an unregistration request for a server channel
        """
        unreg_req = self._utils.create_prim(self._prims.RFC_UNREGISTER_REQ_T)
        
        unreg_req.type.value = self._types.RFC_UNREGISTER_REQ
        unreg_req.loc_serv_chan.value = channel
        
        # Send the prim down
        self.send(unreg_req)
        
        # If we're calling this in looping mode, the loop will execute register
        if self._linear:
            retd, = self.rsp_check("RFC_UNREGISTER_CFM", 
                                   get_fields=("status",) if ret else [])
            return retd

    def init(self):
        """
        Send an init request
        """
        init_req = self._utils.create_prim(self._prims.RFC_INIT_REQ_T)
        init_req.type.value = self._types.RFC_INIT_REQ
        self.send(init_req)
        
        if self._linear:
            self.rsp_check("RFC_INIT_CFM")
    
    
    def client_connect(self, bd_addr, server_channel):
        """
        Request connection to a given channel on a device at the given address
        """
        connect_req = self._utils.create_prim(self._prims.RFC_CLIENT_CONNECT_REQ_T)
        connect_req.type.value = self._types.RFC_CLIENT_CONNECT_REQ
        connect_req.bd_addr.lap.value = bd_addr.lap
        connect_req.bd_addr.uap.value = bd_addr.uap
        connect_req.bd_addr.nap.value = bd_addr.nap
        connect_req.rem_serv_chan.value = server_channel
        connect_req.client_security_chan.value = 1
        
        connect_req.max_payload_size.value = 0x79
        connect_req.total_credits.value = 3
        connect_req.msc_timeout.value = 500
        
        self.send(connect_req)
        
        self._conn_state = "CLIENT_CONNECT_REQ_SENT"
        
        if self._linear:
            self.client_connect_cfm(raw=None)
        
    def disconnect(self, conn_id):

        disconnect_req = self._utils.create_prim(self._prims.RFC_DISCONNECT_REQ_T)
        disconnect_req.type.value = self._types.RFC_DISCONNECT_REQ
        disconnect_req.conn_id.value = conn_id
        self.send(disconnect_req)
        
        self._conn_state = "DISCONNECT_REQ_SENT"
        
        if self._linear:
            self.disconnect_cfm(raw=None)
            
    def server_connect_ind_rsp(self, raw=None):
        """
        On a slave device, see an AUTO_CONNECT_IND and return an 
        AUTO_CONNECT_RSP
        """
        
        if self._conn_state is not None:
            raise RfcommConnectionStateError("Received an RFC_SERVER_CONNECT_IND"
                                            " while in connection state %s" % 
                                                            self._conn_state)
        
        chan, conn_id = self.rsp_check("RFC_SERVER_CONNECT_IND",
                                 get_fields=("loc_serv_chan","conn_id"),
                                 raw=raw)
        self._log.info("Connection requested to channel %d" % chan)
        conn_rsp = self._utils.create_prim(self._prims.RFC_SERVER_CONNECT_RSP_T)
        
        conn_rsp.type.value = self._types.RFC_SERVER_CONNECT_RSP 
        conn_rsp.conn_id.value = conn_id
        conn_rsp.response.value = 0x1000 # ACCEPT_SERVER_CONNECTION
        conn_rsp.max_payload_size.value = 0x79
        conn_rsp.total_credits.value = 3
        
        self.send(conn_rsp)
        self._conn_state = "SERVER_CONNECT_RSP_SENT"

    def client_connect_cfm(self, raw=None):
        
        if self._conn_state not in ("CLIENT_CONNECT_REQ_SENT",
                                    "CLIENT_CONNECT_PENDING"):
            raise RfcommConnectionStateError("Saw CLIENT_CONNECT_CFM when "
                                             "connection status was %s" % 
                                             self._conn_state)

        status, conn_id = self.rsp_check("RFC_CLIENT_CONNECT_CFM",
                                         get_fields=("status",
                                                     "conn_id"),
                                         raw=raw)
        status = self._error_codes[status]
        
        if self._conn_state == "CLIENT_CONNECT_REQ_SENT":
            if status != "RFC_CONNECTION_PENDING":
                raise RfcommConnectionError(
                         "Saw status %s in RFC_CLIENT_CONNECT_CFM when "
                         "connection status was %s" % (status, self._conn_state))
            self._conn_state = "CLIENT_CONNECT_PENDING"
        elif self._conn_state == "CLIENT_CONNECT_PENDING":
            if status == "RFC_CONNECTION_PENDING":
                # Not sure why we should see this status when we've already
                # seen it, but perhaps it's harmless
                pass
            elif status == "RFC_REMOTE_REFUSAL":
                raise RfcommRemoteRefusal("Server rejected connection attempt")
            elif status != "RFC_SUCCESS":
                raise RfcommConnectionError(
                         "Saw status %s in RFC_CLIENT_CONNECT_CFM when "
                         "connection status was %s" % (status, self._conn_state))
            self._conns[conn_id] = RFCOMMConn(conn_id, self, is_server=False)
            self._latest_conn_or_disconn = conn_id
            self._conn_state = None
            self._utils.send_msg(self._app_task,
                                 RFCOMMAppMsgs.CONNECTION_COMPLETE)
        
    def server_connect_cfm(self, raw=None):
        
        status, conn_id = self.rsp_check("RFC_SERVER_CONNECT_CFM",
                                         get_fields=("status",
                                                     "conn_id"),
                                         raw=raw)

        status = self._error_codes[status]
        if self._conn_state == "SERVER_CONNECT_RSP_SENT": 
            if status != "RFC_SUCCESS":
                raise RfcommConnectionError(
                     "Saw status %s in RFC_SERVER_CONNECT_CFM when " 
                     "connection state was %s" % (status, self._conn_state))
        
            self._conns[conn_id] = RFCOMMConn(conn_id, self, is_server=True)
            self._latest_conn_or_disconn = conn_id
            self._conn_state = None
            self._utils.send_msg(self._app_task,
                                 RFCOMMAppMsgs.CONNECTION_COMPLETE)
        elif self._conn_state is None:
            self._log.warn("Saw SERVER_CONNECT_CFM with status %s" % status)
        
    def disconnect_cfm(self, raw=None):
        if self._conn_state not in ("DISCONNECT_REQ_SENT",):
            raise RfcommConnectionStateError("Saw DISCONNECT_CFM when "
                                             "connection status was %s" % 
                                             self._conn_state)

        status, conn_id = self.rsp_check("RFC_DISCONNECT_CFM",
                                         get_fields=("status",
                                                     "conn_id"),
                                         raw=raw)
        status = self._error_codes[status]
        
        if status != "RFC_SUCCESS":
            raise RfcommConnectionStateError(
                         "Saw status %s in RFC_DISCONNECT_CFM when "
                         "connection status was %s" % (status, self._conn_state))
        if not conn_id in self._conns:
            raise RfcommConnectionStateError("Saw DISCONNECT_CFM for unknown "
                                             "conn_id 0x%x" % conn_id)
        del self._conns[conn_id]        
        self._latest_conn_or_disconn = conn_id
        self._conn_state = None
        self._utils.send_msg(self._app_task,
                             RFCOMMAppMsgs.DISCONNECTION_COMPLETE)
        
    def disconnect_ind_rsp(self, raw=None):
        
        if self._conn_state is not None:
            raise RfcommConnectionStateError("Received an RFC_DISCONNECT_IND"
                                            " while in connection state %s" % 
                                                            self._conn_state)
        
        reason, conn_id = self.rsp_check("RFC_DISCONNECT_IND",
                                         get_fields=("reason","conn_id"),
                                         raw=raw)
        reason = self._error_codes[reason]
        
        self._log.info("Disconnection requested of conn_id 0x%x for reason '%s'" 
                       % (conn_id, reason))
        disconn_rsp = self._utils.create_prim(self._prims.RFC_DISCONNECT_RSP_T)
        disconn_rsp.type.value = self._types.RFC_DISCONNECT_RSP 
        disconn_rsp.conn_id.value = conn_id
        
        self.send(disconn_rsp)
        self._utils.send_msg(self._app_task,
                             RFCOMMAppMsgs.DISCONNECTION_COMPLETE)
        
        if not conn_id in self._conns:
            raise RfcommConnectionStateError("Saw DISCONNECT_IND for unknown "
                                             "conn_id 0x%x" % conn_id)
        del self._conns[conn_id]
        
        
    def handler(self, msg):
        
        if msg["id"] != SysMsg.BLUESTACK_RFCOMM_PRIM:
            raise UnexpectedMsgType("Expected RFCOMM prim but received msg_id "
                                     "0x%x" % msg["id"])
            
        type = self.rsp(msg["m"])
        
        self._log.info("Received %s" % self._enum_prim_names[type.value])
        
        if type.value == self._types.RFC_INIT_CFM:
            self._utils.send_msg(self._app_task, 
                                 RFCOMMAppMsgs.INIT_COMPLETE)

            
        elif type.value == self._types.RFC_REGISTER_CFM:
            retd, = self.rsp_check("RFC_REGISTER_CFM", 
                                   get_fields=("accept",),
                                   raw=msg["m"])
            self._utils.send_msg(self._app_task, 
                                 RFCOMMAppMsgs.REGISTRATION_COMPLETE)
        
        elif type.value == self._types.RFC_ERROR_IND:
            type, status = self.rsp_check("RFC_ERROR_IND",
                                          get_fields=("err_prim_type", "status"),
                                          raw=msg["m"])
            try:
                self._log.warn("ERROR_IND received with status %s concerning %s" % 
                               (self._error_codes[status], 
                                self._enum_prim_names[type]))
            except KeyError:
                self._log.warn("ERROR_IND received with status %s" % 
                               self._error_codes[status])
            self._utils.send_msg(self._app_task,
                                 RFCOMMAppMsgs.ERROR_IND)

        elif type.value == self._types.RFC_SERVER_CONNECT_IND:
            self.server_connect_ind_rsp(raw=msg["m"])

        elif type.value == self._types.RFC_CLIENT_CONNECT_CFM:
            self.client_connect_cfm(raw=msg["m"])

        elif type.value == self._types.RFC_SERVER_CONNECT_CFM:
            self.server_connect_cfm(raw=msg["m"])
            
        elif type.value == self._types.RFC_DISCONNECT_IND:
            self.disconnect_ind_rsp(raw=msg["m"])
        
        elif type.value == self._types.RFC_DISCONNECT_CFM:
            self.disconnect_cfm(raw=msg["m"])
                

    def get_conn_ids(self):
        """
        """
        return self._conns.keys()

    def get_conn(self, conn_id):
        """
        """
        return self._conns[conn_id]

    def get_latest_conn_or_disconn(self):
        return self._latest_conn_or_disconn
    


class RFCOMMConn(object):
    """
    Simple class representing one side of an RFCOMM connection 
    """
    def __init__(self, conn_id, rfcomm, is_server=True):
        
        self._conn_id = conn_id
        self._rfcomm = rfcomm
        self._utils = rfcomm._utils
        self._is_srv = is_server
        
    @property
    def sink(self):
        """
        Return the Sink object associated with this connection, creating it
        if necessary
        """
        try:
            self._sink
        except AttributeError:
            sink_id = self._utils.call1.StreamRfcommSink(self._conn_id)
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
                                    self._utils.call1.StreamRfcommSink(self._conn_id))
            self._source = Source(source_id, self._utils, drop_msgs=True)
        return self._source
    
    def disconnect(self):
        """
        Disconnect the connection
        """
        return NotImplemented


