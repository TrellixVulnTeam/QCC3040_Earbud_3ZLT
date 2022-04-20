# Copyright (c) 2016 Qualcomm Technologies International, Ltd.
#   %%version
import sys
from csr.wheels.global_streams import iprint
from csr.wheels.polling_loop import poll_loop
from .libs.dm import DMAppMsgs, DMLib, DMException
from .libs.l2cap import L2CAPAppMsgs, L2CAPLib
from csr.dev.fw.trap_api.bluestack_utils import BluestackUtils
from csr.wheels.bitsandbobs import words_to_bytes

class DevScoConnDescriptor(object):

    def __init__(self, hci_handle):
        self._data = {'hci' : hci_handle}
       
    def get_hci_handle(self):
        return self._data["hci"]

    @property
    def sink_handle(self):
        if "sink" in self._data:
            return self._data["sink"]
        else:
            return 0

    @sink_handle.setter
    def sink_handle(self, sink):
        self._data["sink"] = sink

    @property
    def source_handle(self):
        if "source" in self._data:
            return self._data["source"]
        else:
            return 0
            
    @source_handle.setter
    def source_handle(self, source):
        self._data["source"] = source

class ScoConnDescriptor(object):
    def __init__(self, master_hci_handle, slave_hci_handle):
        master = DevScoConnDescriptor(master_hci_handle)
        slave  = DevScoConnDescriptor(slave_hci_handle)
        self._data = { 'master' : master, 'slave' : slave }
        
    def master_hci_handle(self):
        return  self._data['master'].get_hci_handle()
               
    def slave_hci_handle(self):
        return  self._data['slave'].get_hci_handle()       

class ScoConnector(object):
    """
    Base class providing access to ScoConn objects once a connection has
    been made plus a factory method for constructing the right type of
    connector
    """
    @staticmethod
    def factory(mst_dev, slv_dev, verbose=False, linear=False, esco=True):
        """
        Return an ScoLoopingConnector
        """
        ConnType = ScoLoopingConnector
        return ConnType(mst_dev, slv_dev, verbose=verbose, esco=esco)
    
    def __init__(self):        
        self._conns = {}
        self._connection_index = 0;
        self.num_connection_attempts = 5
    
    def get_conn_ids(self):
        """
        Return a list of connection ids
        """
        return self._conns.keys()
    
    def get_conn(self, conn_id):
        """
        Get the master and slave sides of a connection
        """
        return (self._conns[conn_id].master_hci_handle(), self._conns[conn_id].slave_hci_handle())
       
class ScoLoopingConnector(ScoConnector):
    """
    Applicaton that runs the message_loop to do an L2CAP connection.  If
    necessary it will perform initialisation (protocol registration, etc) first.
    """
    
    def __init__(self, mst_dev, slv_dev, verbose=False, esco=True):
        
        ScoConnector.__init__(self)

        self._verbose=verbose
        self.esco = esco

        self._mst_apps = mst_dev.chip.apps_subsystem
        self._slv_apps = slv_dev.chip.apps_subsystem
        
        self._mst_utils = self._mst_apps.p1.fw.trap_utils
        self._slv_utils = self._slv_apps.p1.fw.trap_utils

        # Create the Bluestack library instances
        self._mst_dm = DMLib(self._mst_utils, verbose=self._verbose, tag="dm", esco=self.esco)
        self._slv_dm = DMLib(self._slv_utils, verbose=self._verbose, tag="dm", esco=self.esco)

        # Set state variables for keeping track of master and slave progress
        self._slv_ready = False
        self._slv_listening = False
        self._mst_ready = False
        
    def toggle_verbose(self):
        self._verbose = not self._verbose

    def prepare_loop(self):
        """
        Prepare the loop if it is currently not prepared
        """
        if not hasattr(self, "_mst_dm_task"):
            # Create "app tasks" for the bluestack library handlers to report back
            # to
            self._mst_dm_task = self._mst_utils.create_task(self._mst_handler)
            self._slv_dm_task = self._slv_utils.create_task(self._slv_handler)
            
            self._mst_dm.reset_app_task(self._mst_dm_task)
            self._slv_dm.reset_app_task(self._slv_dm_task)
            
            # Set the per-protocol bluestack handlers for the main polling loop
            # handler
            BluestackUtils(self._mst_utils).set_bluestack_handler(dm=self._mst_dm.handler)
            BluestackUtils(self._slv_utils).set_bluestack_handler(dm=self._slv_dm.handler)
        
    def loop(self):
        poll_loop()

    def end_loop(self, mst_or_slv):
        """
        Free the tasks associated with a connection or disconnection loop
        """
        utils = getattr(self, "_%s_utils"%mst_or_slv)

        utils.remove_msg_type_task("BlueStack")

        attr = "_%s_dm_task" % mst_or_slv
            
        task = getattr(self, attr)
        
        utils.delete_task(task)
        
        delattr(self, attr)
            

    def _sco_connect_send_req(self):
        """
        Master device sends a SCO connection request to the slave device
        """    
        self._mst_dm.sync_connect_req(bd_addr=self._slv_dm.local_bd_addr)
        
    def _sco_connect_loop(self):
        """
        Enter the connection loop and wait for the master and slave responses.
        In case of failure, up to self.num_connection_attempts are re-tried.
        On entering, the first sync connection request should be triggered externally 
        """         
        num_tries = 0     
        success = False
             
        while (num_tries < self.num_connection_attempts) and (success == False):
            try:
                self.loop()
                success = True
            except DMException as error:
                num_tries += 1
                iprint("Caught SCO connection exception (%s). Connection retry number %d" % (error, num_tries))
                #Have another go
                self._sco_connect_send_req()
                success = False

        if success:
            self._connection_index += 1   
            self._conns[self._connection_index] = ScoConnDescriptor(self._mst_dm.hci_handle, self._slv_dm.hci_handle)

    def connect(self):
        """
        Trigger the sequence to establish a SCO connection between master and slave devices 
        """
        
        self.prepare_loop()
        self._connect_ongoing = True

        self._slv_ready = False
        self._slv_listening = False
        self._mst_ready = False
                
        iprint("[mst] Start: registering with sync DM")
        self._mst_dm.register()

        iprint("[slv] Start: registering with sync DM")
        self._slv_dm.register()

        self._sco_connect_loop()        
    
    def disconnect(self, connection_id):
        """
        Trigger the sequence to disconnect a SCO connection 
        """    
        if not connection_id in self._conns:
            iprint("Unrecognised connection id")
            return False
        
        self._connect_ongoing = False
        self.prepare_loop()
            
        self._disconnect_hci_handle = self._conns[connection_id].master_hci_handle()
               
        iprint("[mst] Disconnect start")
        self._mst_dm.sync_disconnect_req(self._disconnect_hci_handle) 
                      
        self.loop()
        
        del self._conns[connection_id]

        
    def _mst_handler(self, msg):
        """
        Sequence on master for connection establishment is:
            - Registration to DM synch events 
            - Registration to DM events
            - Getting local BT address
            - Start connection
        """
        if msg["t"] == self._mst_dm_task:

            if msg["id"] == DMAppMsgs.SYNC_REGISTRATION_COMPLETE:
                iprint("[mst] Sync DM registration complete: registering with DM")
                if self._connect_ongoing:
                    self._mst_dm.request_local_bd_addr()
                else:
                    self._mst_dm.sync_disconnect_req(self._disconnect_hci_handle)               
            
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[mst] DM registration complete: requesting Bluetooth address")
                self._mst_dm.sync_register()
                
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[mst] Bluetooth address obtained: starting connection")
                self._mst_ready = True
                if self._slv_listening and self._mst_ready:
                    self._sco_connect_send_req()

            elif msg["id"] == DMAppMsgs.SYNC_CONNECTION_SUCCESS:
                iprint("[mst] Sco connection completed")
                self.end_loop("mst")

            elif msg["id"] == DMAppMsgs.SYNC_CONNECTION_TERMINATED:
                iprint("[mst] Sco connection terminated")

                iprint("[mst] Start: unregistering DM sync")
                self._mst_dm.sync_unregister()

            elif msg["id"] == DMAppMsgs.SYNC_UNREGISTRATION_COMPLETE:
                iprint("[mst] DM sync unregistration completed")
                self.end_loop("mst")
               
    def _slv_handler(self, msg):
        """
        Sequence on slave is:
           - Registration to DM sync events
           - Registration to DM events
           - Get BT address
           - Enter listening mode
           - Restond to incoming connection indication
        """
        if msg["t"] == self._slv_dm_task:
        
            if msg["id"] == DMAppMsgs.SYNC_REGISTRATION_COMPLETE:
                iprint("[slv] Sync DM registration complete: registering with DM")
                if self._connect_ongoing:
                    self._slv_dm.request_local_bd_addr()
                else:
                    # Wait for connection termination
                    pass
                        
            elif msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[slv] DM registeration complete: requesting Bluetooth address")
                self._slv_dm.sync_register()
                
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[slv] Bluetooth address obtained: entering listening mode")
                self._slv_ready = True
                self._slv_dm.listen()
            
            elif msg["id"] == DMAppMsgs.LISTENING:
                iprint("[slv] Listening: ready to connect"            )
                self._slv_listening = True
                if self._slv_listening and self._mst_ready:
                    self._sco_connect_send_req()

            elif msg["id"] == DMAppMsgs.SYNC_CONNECT_IND:
                iprint("[slv] Connect indication: accept connection and wait for confirmation")
                self._slv_dm.sync_accept_incoming (bd_addr = self._mst_dm.local_bd_addr)

            elif msg["id"] == DMAppMsgs.SYNC_CONNECTION_SUCCESS:
                iprint("[slv] Sco connection completed")
                self.end_loop("slv")

            elif msg["id"] == DMAppMsgs.SYNC_CONNECTION_TERMINATED:
                iprint("[slv] Sco connection terminated")

                iprint("[slv] Start: unregistering DM sync")
                self._slv_dm.sync_unregister()

            elif msg["id"] == DMAppMsgs.SYNC_UNREGISTRATION_COMPLETE:
                iprint("[slv] DM sync unregistration completed")
                self.end_loop("slv")


