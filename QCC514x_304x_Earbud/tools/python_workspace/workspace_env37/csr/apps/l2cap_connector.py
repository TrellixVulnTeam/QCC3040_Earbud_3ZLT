############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.wheels.polling_loop import poll_loop
from .libs.dm import DMAppMsgs, DMLib
from .libs.l2cap import L2CAPAppMsgs, L2CAPLib, L2capError
from csr.dev.fw.trap_api.bluestack_utils import BluestackUtils

class L2CAPConnector(object):
    """
    Base class providing access to L2CAPConn objects once a connection has
    been made plus a factory method for constructing the right type of
    connector
    """
    @staticmethod
    def factory(mst_dev, slv_dev, verbose=False, linear=False):
        """
        Return an L2CAPLinearConnector if linear==True or else an 
        L2CAPLoopingConnector
        """
        ConnType = L2CAPLinearConnector if linear else L2CAPLoopingConnector
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
        return (self._mst_l2cap.get_conn(mst_cid), 
                self._slv_l2cap.get_conn(self._conns[mst_cid]))
        
                


class L2CAPLinearConnector(L2CAPConnector):
    """
    Class implementing a basic L2CAP application which creates a single 
    connection between two devices and presents read and write methods to 
    enable data to be sent interactively between master and slave
    """
    
    def __init__(self, mst_dev, slv_dev, verbose=False):
        """
        Initialise this object, and set up basic things in the firmware too,
        such as the Bluetooth upper layers task
        """
        
        L2CAPConnector.__init__(self)
        
        mst_apps1 = mst_dev.chip.apps_subsystem.p1
        slv_apps1 = slv_dev.chip.apps_subsystem.p1
        
        self._mst_utils = mst_apps1.fw.trap_utils
        self._mst_dm = DMLib(self._mst_utils, linear=True, verbose=verbose)
        self._mst_l2cap = L2CAPLib(self._mst_utils, linear=True, verbose=verbose)

        self._slv_utils = slv_apps1.fw.trap_utils
        self._slv_dm = DMLib(self._slv_utils, linear=True, verbose=verbose)
        self._slv_l2cap = L2CAPLib(self._slv_utils, linear=True, verbose=verbose)
        
        self._started=False

    def start(self):
        """
        Start up the master and slave L2CAP firmware
        """

        iprint("Starting master")
        self._mst_l2cap.start(self._mst_dm)
        iprint("Starting slave")
        self._slv_l2cap.start(self._slv_dm)

        self._started = True

    def connect(self, psm=5):
        """
        Create a connection between the master and slave.
        PSM (protocol service multiplexer) is essentially an L2CAP client ID.
        It must be odd and the values 1 and 3 are reserved for SDP and RFCOMM
        respectively.
        """
        if not self._started:
            self.start()
        
        self._slv_dm.listen()
        
        # We get a pair of connection objects to be used in later functions
        self._mst_l2cap.connect(self._slv_l2cap,
                                psm=psm, 
                                bd_addr=self._slv_dm.local_bd_addr)
        self._conns[self._mst_l2cap.get_latest_conn_or_disconn()] = \
                                self._slv_l2cap.get_latest_conn_or_disconn()
        
        
    def disconnect(self, cid):
        
        self._mst_l2cap.disconnect(self._slv_l2cap, cid)        
        del self._conns[self._mst_l2cap.get_latest_conn_or_disconn()]

class L2CAPLoopingConnector(L2CAPConnector):
    """
    Applicaton that runs the message_loop to do an L2CAP connection.  If
    necessary it will perform initialisation (protocol registration, etc) first.
    """
    
    def __init__(self, mst_dev, slv_dev, verbose=False):
        
        L2CAPConnector.__init__(self)

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
        self._mst_l2cap = L2CAPLib(self._mst_utils, 
                                   verbose=self._verbose, tag="master l2cap")
        self._slv_l2cap = L2CAPLib(self._slv_utils, 
                                   verbose=self._verbose, tag="slave l2cap")

        # Set state variables for keeping track of master and slave progress
        self._slv_ready = False
        self._slv_listening = False
        self._mst_ready = False
        
        self._next_cid = 0x80

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
            self._mst_l2cap_task = self._mst_utils.create_task(self._mst_handler)
            self._slv_l2cap_task = self._slv_utils.create_task(self._slv_handler)
            
            self._mst_dm.reset_app_task(self._mst_dm_task)
            self._slv_dm.reset_app_task(self._slv_dm_task)
            self._mst_l2cap.reset_app_task(self._mst_l2cap_task)
            self._slv_l2cap.reset_app_task(self._slv_l2cap_task)
            
            # Set the per-protocol bluestack handlers for the main polling loop
            # handler
            BluestackUtils(self._mst_utils).set_bluestack_handler(
                                              dm=self._mst_dm.handler,
                                              l2cap=self._mst_l2cap.handler)
            BluestackUtils(self._slv_utils).set_bluestack_handler(
                                              dm=self._slv_dm.handler,
                                              l2cap=self._slv_l2cap.handler)
        
    def loop(self):
        poll_loop()

    def end_loop(self, mst_or_slv):
        """
        Free the tasks associated with a connection or disconnection loop
        """
        utils = getattr(self, "_%s_utils"%mst_or_slv)
        utils.remove_msg_type_task("BlueStack")
        for prot in ("dm", "l2cap"):
            attr = "_%s_%s_task"%(mst_or_slv, prot)
            task = getattr(self, attr)
            utils.delete_task(task)
            delattr(self, attr)
                                
    def connect(self, psm=5):
        """
        Trigger the connection sequence, assuming
         - Registration is complete
         - Slave's local bd_addr has been obtained
         - Slave is in listening mode

        PSM (protocol service multiplexer) is essentially an L2CAP client ID.
        It must be odd and the values 1 and 3 are reserved for SDP and RFCOMM
        respectively.
        """
        
        self.prepare_loop()
        
        self._psm = psm
        
        if not self._mst_ready:
            iprint("[mst] Start: registering with DM")
            self._mst_dm.register()
        if not self._slv_ready:
            iprint("[slv] Start: registering with DM")
            self._slv_dm.register()
        
        if self._slv_ready and self._mst_ready:
            iprint("[mst] Initiating connection")
            self._mst_l2cap.auto_connect_init(psm=psm,
                                              cid=self._next_cid,
                                              bd_addr=self._slv_dm.local_bd_addr)
       
        num_tries = 0     
        success = False     
        while (num_tries < self.num_connection_attempts) and (success == False):
            try:    
                self.loop()
                success = True
            except L2capError as e:
                num_tries += 1
                iprint("Caught L2CAP exception '%s'. Connection retry number %d" % (e, num_tries))
                #Have another go
                self._mst_l2cap.clear_conn_state()
                self._mst_l2cap.auto_connect_init(psm=psm,
                                              cid=self._next_cid,
                                              bd_addr=self._slv_dm.local_bd_addr)
                success = False
         
        self._next_cid += 1           
        self._conns[self._mst_l2cap.get_latest_conn_or_disconn()] = \
                                self._slv_l2cap.get_latest_conn_or_disconn()                                
    
    def disconnect(self, master_cid):

        if not master_cid in self._conns:
            iprint("Unrecognised master CID.  Known values are: ")
            iprint(" ".join([str(cid) for cid in mst_cids]))
            return False
        
        self.prepare_loop()        
        self._mst_l2cap.disconnect_req(master_cid)
        self.loop()
        del self._conns[self._mst_l2cap.get_latest_conn_or_disconn()]
        
    def _mst_handler(self, msg):
        """
        Sequence on master is:
            - DM register
            - L2CAP register
            - L2CAP connect
        """
        if msg["t"] == self._mst_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[mst] DM registration complete: registering with L2CAP")
                self._mst_l2cap.register(psm=self._psm)
        
        elif msg["t"] == self._mst_l2cap_task: 
            if msg["id"] == L2CAPAppMsgs.REGISTRATION_COMPLETE:
                iprint("[mst] L2CAP registration complete: ready to connect")
                self._mst_ready = True
                if self._slv_listening and self._mst_ready:
                    self.connect(psm=self._psm)

            elif msg["id"] == L2CAPAppMsgs.CONNECTION_COMPLETE:
                # Master is now connected: stop looping
                iprint("[mst] L2CAP connection complete")
                self.end_loop("mst")

            elif msg["id"] == L2CAPAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[mst] L2CAP disconnection complete")
                self.end_loop("mst")

    def _slv_handler(self, msg):
        """
        Sequence on slave is:
           - DM register
           - DM request local bd addr
           - L2CAP register
           - DM enter listening mode
           - L2CAP connect
        """
        if msg["t"] == self._slv_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[slv] DM registeration complete: requesting Bluetooth address")
                self._slv_dm.request_local_bd_addr()
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[slv] Bluetooth address obtained: registering with L2CAP")
                self._slv_l2cap.register(psm=self._psm)
            elif msg["id"] == DMAppMsgs.LISTENING:
                iprint("[slv] Listening: ready to connect")
                self._slv_listening = True
                if self._slv_listening and self._mst_ready:
                    self.connect(psm=self._psm)
        
        elif msg["t"] == self._slv_l2cap_task: 
            if msg["id"] == L2CAPAppMsgs.REGISTRATION_COMPLETE:
                iprint("[slv] L2CAP registered: entering listening mode")
                self._slv_ready = True
                self._slv_dm.listen()

            elif msg["id"] == L2CAPAppMsgs.CONNECTION_COMPLETE:
                # Slave is now connected: stop looping
                iprint("[slv] L2CAP connection complete")
                self._slv_listening = False # assume it has stopped now
                self.end_loop("slv")
                
            elif msg["id"] == L2CAPAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[slv] L2CAP disconnection complete")
                self.end_loop("slv")


