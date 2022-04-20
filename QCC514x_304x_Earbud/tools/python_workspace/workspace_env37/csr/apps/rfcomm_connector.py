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
from .libs.rfcomm import RFCOMMAppMsgs, RFCOMMLib, RfcommConnectionError, \
                                                    RfcommRemoteRefusal

class RFCOMMConnector(object):
    """
    RFCOMM Connection App.  Clients can register server channels, connect
    the client to them, and disconnect existing connections.
    
    WARNING: this class caches information about the current state of the RFCOMM.
    It should be constructed with an uninitialised RFCOMM module and destroyed
    if the RFCOMM (most likely the whole DUT) is reset.
    """
    
    def __init__(self, mst_dev, slv_dev, verbose=False):
        
        mst_apps1 = mst_dev.chip.apps_subsystem.p1
        self._mst_utils = mst_apps1.fw.trap_utils
        self._mst_dm = DMLib(self._mst_utils, linear=False, verbose=verbose,
                             tag="client")
        self._mst_rfcomm = RFCOMMLib(self._mst_utils, linear=False, verbose=verbose,
                                     tag="client")
        
        slv_apps1 = slv_dev.chip.apps_subsystem.p1
        self._slv_utils = slv_apps1.fw.trap_utils
        self._slv_dm = DMLib(self._slv_utils, linear=False, verbose=verbose,
                             tag="server")
        self._slv_rfcomm = RFCOMMLib(self._slv_utils, linear=False, verbose=verbose,
                                     tag="server")
        
        self._mst_ready = False
        self._slv_ready = False
        self._slv_listening = False
        self._registered_channels = []
        self._conns = {}
        self.num_connection_attempts = 5
        
    def register(self, channel):
        """
        Register the given channel if it hasn't been already
        """
        if channel in self._registered_channels:
            iprint("[slv]: Channel %d already registered: skipping" % channel)
            return
        if not self._slv_ready:
            iprint("[slv] Start: registering with DM")
            self.prepare_loop("slv")
            self._slv_dm.register()
            self.loop()
            
        self.prepare_loop("slv")
        self._slv_rfcomm.register(channel=channel)
        self.loop()
        self._registered_channels.append(channel)
        
    def connect(self, channel, auto_register=True):
        """
        Create a client connection to the specified channel.
        
        This function will start the sequence with basic Bluestack set-up (e.g. 
        DM registration, bd_addr access, RFCOMM init) if necessary; otherwise it
        will start with the client connect request.  If the auto_register flag
        is True, this function will cause the channel to be registered by the
        server if necessary first.
        """

        self._requested_channel = channel
        prep_for = []
        if not self._mst_ready:
            prep_for.append("mst")
        if not self._slv_ready:
            prep_for.append("slv")
        if not self._mst_ready or not self._slv_ready:
            self.prepare_loop(*prep_for)
            if not self._mst_ready:
                iprint("[mst] Start: registering with DM")
                self._mst_dm.register()
            if not self._slv_ready:
                iprint("[slv] Start: registering with DM")
                self._slv_dm.register()
            self.loop()
        
        if auto_register:
            iprint("[slv] Auto-registering channel %d" % channel)
            self.register(channel)
        
        iprint("[mst] Initiating connection")
        self.prepare_loop()
        self._mst_rfcomm.client_connect(server_channel = channel,
                                            bd_addr=self._slv_dm.local_bd_addr)
        
        num_tries = 0
        success = False    
        while (num_tries < self.num_connection_attempts):
            try:
                if success == True:
                    break 
                self.loop()
            except RfcommRemoteRefusal:
                # Retrying is no good here: the client has probably been
                # configured wrongly
                raise
            except RfcommConnectionError as e:
                num_tries += 1
                iprint ("Caught RFCOMM exception '%s'. Connection retry number "
                                            "%d" % (e, num_tries))
                #Have another go   
                self._mst_rfcomm.clear_conn_state()
                self.prepare_loop()
                self._mst_rfcomm.client_connect(server_channel = channel,
                                            bd_addr=self._slv_dm.local_bd_addr)
                success = False 
            else:
                success = True
        
        self._conns[self._mst_rfcomm.get_latest_conn_or_disconn()] = \
                                self._slv_rfcomm.get_latest_conn_or_disconn() 

        
    def disconnect(self, conn_id):
        
        self.prepare_loop()
        self._mst_rfcomm.disconnect(conn_id)
        self.loop()
        mst_disconn = self._mst_rfcomm.get_latest_conn_or_disconn()
        assert mst_disconn in self._conns, "Disconnect on known channel"
        del self._conns[mst_disconn]
            
        
    def prepare_loop(self, *args):
        """
        Prepare the loop if it is currently not prepared.  The caller must
        specify whether to prepare for the master (client) looping, the slave 
        (server) looping, or both.  The prepare_loop call must be matched by
        end_loop call(s); however, note that prepare_loop should only be called
        once per loop, i.e. you can't prepare master and slave separately.
        """
        loop_over = args if args else ("mst", "slv")
        
        for mst_or_slv in loop_over:
            if not hasattr(self, "_%s_dm_task" % mst_or_slv):
                # Create "app tasks" for the bluestack library handlers to report back
                # to
                utils = getattr(self, "_%s_utils" % mst_or_slv)
                bluestack_hdlrs = {}
                for prot in ("dm", "rfcomm"):
                    protlib = getattr(self, "_%s_%s" % (mst_or_slv, prot))
                    task = utils.create_task(getattr(self, "_%s_handler" % mst_or_slv)) 
                    setattr(self, "_%s_%s_task" % (mst_or_slv, prot), task)
                    protlib.reset_app_task(task)
                    bluestack_hdlrs[prot] = protlib.handler
                # Set the per-protocol bluestack handlers for the main polling 
                #loop handler
                utils.bluestack.set_bluestack_handler(**bluestack_hdlrs)


    def loop(self):
        try:
            poll_loop()
        except:
            self.end_loop()
            raise
        
    def end_loop(self, *args):
        """
        Free the tasks associated with a loop.  This frees the tasks that
        prepare_loop created, so the cumulative calls to end_loop must match the
        call to prepare loop
        """
        loop_over = args if args else ("mst", "slv")
            
        for mst_or_slv in loop_over:
            try:
                utils = getattr(self, "_%s_utils"%mst_or_slv)
                utils.remove_msg_type_task("BlueStack")
                for prot in ("dm", "rfcomm"):
                    attr = "_%s_%s_task"%(mst_or_slv,prot)
                    task = getattr(self, attr)
                    utils.delete_task(task)
                    delattr(self, attr)
            except AttributeError:
                pass


    def _mst_handler(self, msg):
        """
        Sequence on master is:
            - DM register
            - RFCOMM register
            - RFCOMM init
            - RFCOMM client connect
        """
        if msg["t"] == self._mst_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[client] DM registration complete: initialising RFCOMM")
                self._mst_rfcomm.init()
        
        elif msg["t"] == self._mst_rfcomm_task: 
            if msg["id"] == RFCOMMAppMsgs.INIT_COMPLETE:
                iprint("[client] Initialisation complete: ready to connect")
                self._mst_ready = True
                self.end_loop("mst")

            elif msg["id"] == RFCOMMAppMsgs.CONNECTION_COMPLETE:
                # Master is now connected: stop looping
                iprint("[client] RFCOMM connection complete")
                self.end_loop("mst")

            elif msg["id"] == RFCOMMAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[client] RFCOMM disconnection complete")
                self.end_loop("mst")

            elif msg["id"] == RFCOMMAppMsgs.ERROR_IND:
                iprint("[client] Error reported. Aborting.")
                self.end_loop()


    def _slv_handler(self, msg):
        
        if msg["t"] == self._slv_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[server] DM registration complete: obtaining BD ADDR")
                self._slv_dm.request_local_bd_addr()
                
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[server] Bluetooth address obtained.  Initialising RFCOMM")
                self._slv_rfcomm.init()
                
            elif msg["id"] == DMAppMsgs.LISTENING:
                iprint("[server] Listening: ready to connect")
                self._slv_listening = True
                self.end_loop("slv") # End of initialisation loop
        
        elif msg["t"] == self._slv_rfcomm_task: 
            if msg["id"] == RFCOMMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[server] RFCOMM registration complete")
                self.end_loop("slv") # End of channel registration loop

            elif msg["id"] == RFCOMMAppMsgs.INIT_COMPLETE:
                # Master is now connected: stop looping
                iprint("[server] Initialisation complete")
                self._slv_ready = True
                self._slv_dm.listen()

            elif msg["id"] == RFCOMMAppMsgs.CONNECTION_COMPLETE:
                iprint("[server] RFCOMM connection complete")
                self.end_loop("slv") # End of server-side connection loop

            elif msg["id"] == RFCOMMAppMsgs.DISCONNECTION_COMPLETE:
                iprint("[server] RFCOMM disconnection complete")
                self.end_loop("slv")

            elif msg["id"] == RFCOMMAppMsgs.ERROR_IND:
                iprint("[server] Error reported. Aborting.")
                self.end_loop()


    def get_conn_ids(self):
        """
        Return a list of client conn IDs
        """
        return self._conns.keys()

    def get_connected_id(self, mst_conn_id):
        """
        Get the server conn ID for the given client conn ID
        """
        return self._conns[mst_conn_id] if mst_conn_id in self._conns else None

    def get_conn(self, mst_conn_id):
        """
        Get the client and server connection objects given the client's conn ID
        """
        if mst_conn_id not in self._conns:
            return None

        return (self._mst_rfcomm.get_conn(mst_conn_id),
                self._slv_rfcomm.get_conn(self._conns[mst_conn_id]))

