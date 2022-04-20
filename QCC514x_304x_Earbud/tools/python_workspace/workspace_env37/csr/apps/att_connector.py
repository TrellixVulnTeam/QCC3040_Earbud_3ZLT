# Copyright (c) 2016 Qualcomm Technologies International, Ltd.
#   %%version
import sys
import time
from csr.wheels.global_streams import iprint
from csr.wheels.polling_loop import poll_loop
from .libs.dm import DMAppMsgs, DMLib, DMException
from .libs.att import ATTAppMsgs, ATTLib, ATTError
from csr.dev.fw.trap_api.bluestack_utils import BluestackUtils

class ATTConnector(object):
    """
    Base class providing access to ATTConn objects once a connection has
    been made plus a factory method for constructing the right type of
    connector
    """
    @staticmethod
    def factory(mst_dev, slv_dev, verbose=False):
        """
        Return ATTLoopingConnector
        """
        ConnType = ATTLoopingConnector
        return ConnType(mst_dev, slv_dev, verbose=verbose)
    
    def __init__(self):
        
        self._conns = {}
        self.num_connection_attempts = 5
    
    def get_conn_cids(self):
        """
        Return a list of master CIDs
        """
        return self._conns.keys()
    
    def get_connected_cid(self, mst_cid):
        """
        Get the slave CID for the given master CID
        """
        return self._conns[mst_cid] if mst_cid in self._conns else None

    def get_conn(self, mst_cid):
        """
        Get the master and slave sides of a connection
        """
        return (self._mst_att.get_conn(mst_cid), 
                self._slv_att.get_conn(self._conns[mst_cid]))

class ATTLoopingConnector(ATTConnector):
    """
    Applicaton that runs the message_loop to do an ATT connection.  If
    necessary it will perform initialisation (protocol registration, etc) first.
    """
    
    def __init__(self, mst_dev, slv_dev, verbose=False):
        
        ATTConnector.__init__(self)

        self._verbose=verbose
        
        self._mst_apps = mst_dev.chip.apps_subsystem
        self._slv_apps = slv_dev.chip.apps_subsystem
        
        self._mst_utils = self._mst_apps.p1.fw.trap_utils
        self._slv_utils = self._slv_apps.p1.fw.trap_utils

        # Create the Bluestack library instances
        self._mst_dm = DMLib(self._mst_utils,
                             verbose=self._verbose, tag="master dm")
        self._slv_dm = DMLib(self._slv_utils,
                             verbose=self._verbose, tag="slave dm")
        self._mst_att = ATTLib(self._mst_utils, 
                                   verbose=self._verbose, tag="master att")
        self._slv_att = ATTLib(self._slv_utils, 
                                   verbose=self._verbose, tag="slave att")
        
        # Set state variables for keeping track of master and slave progress
        self._slv_ready = False
        self._slv_listening = False
        self._mst_ready = False
        
        self._next_cid = 0x80
        self._att_handle = 0

    def toggle_verbose(self):
        self._verbose = not self._verbose

    def prepare_loop(self, conn_type, *args):
        """
        Prepare the loop for ATT in BR/EDR if it is currently not prepared.   
        The caller must specify whether to prepare for the master (client)  
        looping, the slave(server) looping, or both.  The prepare_loop call 
        must be matched by end_loop call(s); however, note that prepare_loop 
        should only be called once per loop, i.e. you can't prepare master 
        and slave separately.
        """
        loop_over = args if args else ("mst", "slv")

        for mst_or_slv in loop_over:
            if not hasattr(self, "_%s_dm_task" % mst_or_slv):
                # Create "app tasks" for the bluestack library handlers to report back
                # to
                utils = getattr(self, "_%s_utils" % mst_or_slv)
                bluestack_hdlrs = {}
                for prot in ("dm", "att"):
                    protlib = getattr(self, "_%s_%s" % (mst_or_slv, prot))
                    task = utils.create_task(getattr(self, "_%s_%s_handler" % (mst_or_slv, conn_type))) 
                    setattr(self, "_%s_%s_task" % (mst_or_slv, prot), task)
                    protlib.reset_app_task(task)
                    bluestack_hdlrs[prot] = protlib.handler
                # Set the per-protocol bluestack handlers for the main polling 
                #loop handler
                utils.bluestack.set_bluestack_handler(**bluestack_hdlrs)   
            
    def loop(self):
        poll_loop()

    def end_loop(self, mst_or_slv):
        """
        Free the tasks associated with a connection or disconnection loop
        """
        utils = getattr(self, "_%s_utils"%mst_or_slv)
        utils.remove_msg_type_task("BlueStack")
        for prot in ("dm", "att"):
            attr = "_%s_%s_task"%(mst_or_slv, prot)
            task = getattr(self, attr)
            utils.delete_task(task)
            delattr(self, attr)
                                        
    def connect_br(self):
        """
        Trigger the connection sequence, assuming
         - Registration is complete
         - Slave's local bd_addr has been obtained
         - Slave is in listening mode

        """
        
        self.prepare_loop(conn_type="br")
        
        if not self._slv_ready or not self._mst_ready:
            iprint("[mst] Start: registering with DM")
            self._mst_dm.register()
            iprint("[slv] Start: registering with DM")
            self._slv_dm.register()
        
        else:
            iprint("[mst] Initiating connection")
            self._mst_att.connect_init(cid=self._next_cid,
                                       bd_addr=self._slv_dm.local_bd_addr)            
       
        num_tries = 0     
        success = False     
        while (num_tries < self.num_connection_attempts) and (success == False):
            try:    
                self.loop()
                success = True
            except ATTError as e:
                num_tries += 1
                iprint("Caught ATT exception '%s'. Connection retry number %d" % (e, num_tries))
                #Have another go
                self._mst_att.clear_conn_state()
                self._mst_att.connect_init(cid=self._next_cid,
                                           bd_addr=self._slv_dm.local_bd_addr)
                success = False
         
        self._next_cid += 1
        self._conns[self._mst_att.get_latest_conn_or_disconn()] = \
                                self._slv_att.get_latest_conn_or_disconn()

    def connect_le(self):
        """
        Trigger the connection sequence, assuming
         - Registration is complete
         - Slave's local bd_addr has been obtained
         - BT version has been set

        """
        
        self.prepare_loop(conn_type="le")
        
        if not self._slv_ready or not self._mst_ready:
            iprint("[mst] Start: registering with DM")
            self._mst_dm.register()
            iprint("[slv] Start: registering with DM")
            self._slv_dm.register()
        
        else:
            iprint("[slv] Initiating Advertising")
            self._slv_att.connect_init(24, cid=self._next_cid,
                                       bd_addr=self._mst_dm.local_bd_addr)
            iprint("[mst] Initiating connection")
            self._mst_att.connect_init(9, cid=self._next_cid,
                                       bd_addr=self._slv_dm.local_bd_addr)
       
        num_tries = 0     
        success = False     
        while (num_tries < self.num_connection_attempts) and (success == False):
            try:    
                self.loop()
                success = True
            except ATTError as e:
                num_tries += 1
                iprint("Caught ATT exception '%s'. Connection retry number %d" % (e, num_tries))
                #Have another go
                self._mst_att.clear_conn_state()
                self._mst_att.clear_conn_state()
                self._slv_att.connect_init(24, cid=self._next_cid,
                                       bd_addr=self._mst_dm.local_bd_addr)
                self._mst_att.connect_init(9,cid=self._next_cid,
                                           bd_addr=self._slv_dm.local_bd_addr)
                success = False
         
        self._next_cid += 1
        self._conns[self._mst_att.get_latest_conn_or_disconn()] = \
                                self._slv_att.get_latest_conn_or_disconn()
    
    def disconnect_br(self, master_cid):

        if not master_cid in self._conns:
            iprint("Unrecognised master CID.  Known values are: ")
            iprint(" ".join([str(cid) for cid in mst_cids]))
            return False

        self.prepare_loop(conn_type="br")
        self._mst_att.disconnect_req(master_cid)        
        self.loop()
        del self._conns[self._mst_att.get_latest_conn_or_disconn()]

    def disconnect_le(self, master_cid):

        if not master_cid in self._conns:
            iprint("Unrecognised master CID.  Known values are: ")
            iprint(" ".join([str(cid) for cid in mst_cids]))
            return False

        self.prepare_loop(conn_type="le")
        self._mst_att.disconnect_req(master_cid)        
        self.loop()
        del self._conns[self._mst_att.get_latest_conn_or_disconn()]        

    def send_handle_attribute(self, conn_type, master_cid):
        """
        Scenario to test ATT handle add to ATT connection stream.
        """
        if not master_cid in self._conns:            
            iprint("Unrecognised master CID.  Known values are: "            )
            iprint(" ".join([str(cid) for cid in mst_cids])            )
            return False

        #few defines for clarity
        ATT_HANDLE_VALUE_NOTIFICATION = 0
        ATT_HANDLE_VALUE_INDICATION = 1

        #Prepare loop on ATT server
        self.prepare_loop(conn_type, "mst")
        self._att_handle += 1
        slv_cid = self.get_connected_cid(master_cid)
        # Add ATT handle for ATT connection on ATT client
        source_id = self._slv_att.source_add_att_handle(slv_cid, self._att_handle)
        self._mst_att.att_handle_value_req(master_cid, 
                                           self._att_handle, 
                                           ATT_HANDLE_VALUE_NOTIFICATION)
        self.loop()
        
        return source_id,self._att_handle

    def unregister(self):
        self._mst_att.unregister()
        self._slv_att.unregister()
        self._mst_ready = False
        self._slv_ready = False
        
    def _mst_br_handler(self, msg):
        """
        Sequence on master is:
            - DM register
            - ATT register
            - ATT connect
            - ATT handle value
            - ATT disconnect
        """
        if msg["t"] == self._mst_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[mst] DM registration complete: registering with ATT")
                self._mst_att.register()
        
        elif msg["t"] == self._mst_att_task: 
            if msg["id"] == ATTAppMsgs.REGISTRATION_COMPLETE:
                iprint("[mst] ATT registration complete: ready to connect")
                self._mst_ready = True
                if self._slv_listening and self._mst_ready:
                    self.connect_br()

            elif msg["id"] == ATTAppMsgs.CONNECTION_COMPLETE:
                # Master is now connected: stop looping
                iprint("[mst] ATT connection complete")
                self.end_loop("mst")

            elif msg["id"] == ATTAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[mst] ATT disconnection complete")
                self.end_loop("mst")

            elif msg["id"] == ATTAppMsgs.HANDLE_VALUE_COMPLETE:                
                iprint("[mst] ATT handle value complete"                )
                self.end_loop("mst")


    def _mst_le_handler(self, msg):
        """
        Sequence on master is:
            - DM register
            - DM request local bd addr
            - DM set BT version
            - ATT register
            - ATT connect
            - ATT handle value
            - ATT disconnect
        """
        if msg["t"] == self._mst_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[mst] DM registeration complete: requesting Bluetooth address")
                self._mst_dm.request_local_bd_addr()
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[mst] Bluetooth address obtained: setting BT version")
                self._mst_dm.set_bt_version()
            elif msg["id"] == DMAppMsgs.BT_VER_SET:
                iprint("[mst] BT version set: registering with ATT")
                self._mst_att.register()
        
        elif msg["t"] == self._mst_att_task: 
            if msg["id"] == ATTAppMsgs.REGISTRATION_COMPLETE:
                iprint("[mst] ATT registration complete: ready to connect")
                self._mst_ready = True
                if self._slv_listening and self._mst_ready:                    
                    self.connect_le()
                    
            elif msg["id"] == ATTAppMsgs.CONNECTION_COMPLETE:
                # Master is now connected: stop looping
                iprint("[mst] ATT connection complete")
                self.end_loop("mst")

            elif msg["id"] == ATTAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[mst] ATT disconnection complete")
                self.end_loop("mst")

            elif msg["id"] == ATTAppMsgs.HANDLE_VALUE_COMPLETE:                
                iprint("[mst] ATT handle value complete"                )
                self.end_loop("mst")
        

    def _slv_br_handler(self, msg):
        """
        Sequence on slave is:
           - DM register
           - DM request local bd addr
           - ATT register
           - DM enter listening mode
           - ATT connect
           - ATT disconnect
        """
        if msg["t"] == self._slv_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[slv] DM registeration complete: requesting Bluetooth address")
                self._slv_dm.request_local_bd_addr()
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[slv] Bluetooth address obtained: registering with ATT")
                self._slv_att.register()
            elif msg["id"] == DMAppMsgs.LISTENING:
                iprint("[slv] Listening: ready to connect")
                self._slv_listening = True
                if self._slv_listening and self._mst_ready:
                    self.connect_br()
        
        elif msg["t"] == self._slv_att_task: 
            if msg["id"] == ATTAppMsgs.REGISTRATION_COMPLETE:
                iprint("[slv] ATT registered: entering listening mode")
                self._slv_ready = True
                self._slv_dm.listen()

            elif msg["id"] == ATTAppMsgs.CONNECTION_COMPLETE:
                # Slave is now connected: stop looping
                iprint("[slv] ATT connection complete")
                self._slv_listening = False # assume it has stopped now
                self.end_loop("slv")
                
            elif msg["id"] == ATTAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[slv] ATT disconnection complete")
                self.end_loop("slv")


    def _slv_le_handler(self, msg):
        """
        Sequence on slave is:
           - DM register
           - DM request local bd addr
           - DM set BT version
           - ATT register
           - ATT connect
           - ATT disconnect
        """
        if msg["t"] == self._slv_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[slv] DM registeration complete: requesting Bluetooth address")
                self._slv_dm.request_local_bd_addr()
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[slv] Bluetooth address obtained: setting BT version")
                self._slv_dm.set_bt_version()
            elif msg["id"] == DMAppMsgs.BT_VER_SET:
                iprint("[slv] BT version set: registering with ATT")
                self._slv_att.register()
        
        elif msg["t"] == self._slv_att_task: 
            if msg["id"] == ATTAppMsgs.REGISTRATION_COMPLETE:
                iprint("[slv] ATT registration complete: ready to connect")
                self._slv_ready = True
                self._slv_listening = True
                if self._slv_listening and self._mst_ready:                    
                    self.connect_le()
                    
            elif msg["id"] == ATTAppMsgs.CONNECTION_COMPLETE:
                # Slave is now connected: stop looping
                iprint("[slv] ATT connection complete")
                self._slv_listening = False # assume it has stopped now
                self.end_loop("slv")
                
            elif msg["id"] == ATTAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[slv] ATT disconnection complete")
                self.end_loop("slv")
