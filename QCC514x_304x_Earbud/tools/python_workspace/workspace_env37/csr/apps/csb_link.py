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
from csr.dev.fw.trap_api.bluestack_utils import BluestackUtils

class CsbLoopingLink(object):
    """
    Applicaton that runs the message_loop to create csb link  If necessary,
    it will perform initialisation (protocol registration, etc) first.
    """
    RETRY_LIMIT = 10

    def __init__(self, csb_tx_dev, csb_rx_dev, verbose=False):
        """
        Initialise this object, and set up basic things in the firmware too,
        such as the Bluetooth upper layers task
        """
        self._verbose=verbose
        
        self._csb_tx_apps = csb_tx_dev.chip.apps_subsystem
        self._csb_rx_apps = csb_rx_dev.chip.apps_subsystem
        
        self._csb_tx_utils = self._csb_tx_apps.p1.fw.trap_utils
        self._csb_rx_utils = self._csb_rx_apps.p1.fw.trap_utils

        # Create the Bluestack library instances
        self._csb_tx_dm = DMLib(self._csb_tx_utils,
                             verbose=self._verbose, tag="csb_tx dm")
        self._csb_rx_dm = DMLib(self._csb_rx_utils,
                             verbose=self._verbose, tag="csb_rx dm")

        # Set state variables for keeping track of master and slave progress
        self._csb_rx_ready = False
        self._csb_rx_complete = False
        self._csb_tx_ready = False
        self._csb_tx_complete = False
        self._csb_link_setup_complete = False
    
    def toggle_verbose(self):
        self._verbose = not self._verbose

    def loop(self):
        self.retry_count = 0
        poll_loop()

    def end_loop(self, csb_tx_or_csb_rx):
        """
        Free the tasks associated with loop
        """
        utils = getattr(self, "_%s_utils"%csb_tx_or_csb_rx)
        utils.remove_msg_type_task("BlueStack")
        attr = "_%s_%s_task"%(csb_tx_or_csb_rx, "dm")
        task = getattr(self, attr)
        utils.delete_task(task)
        delattr(self, attr)

    def prepare_loop(self, csb_tx_or_rx, handler):
        """
        Prepare loop if not previously registered
        """
        if csb_tx_or_rx == "csb_tx" and not hasattr(self, "_csb_tx_dm_task"):
            self._csb_tx_dm_task = self._csb_tx_utils.create_task(handler)
            self._csb_tx_dm.reset_app_task(self._csb_tx_dm_task)
            BluestackUtils(self._csb_tx_utils).set_bluestack_handler(
                                                    dm=self._csb_tx_dm.handler)
        elif csb_tx_or_rx == "csb_rx" and not hasattr(self, "_csb_rx_dm_task"):
            self._csb_rx_dm_task = self._csb_rx_utils.create_task(handler)
            self._csb_rx_dm.reset_app_task(self._csb_rx_dm_task)
            BluestackUtils(self._csb_rx_utils).set_bluestack_handler(
                                                    dm=self._csb_rx_dm.handler)

    def setup(self, lt_addr=0x01):
        """
        Setup csb link between transmitter and receiver if required
        """        
        if not self._csb_rx_ready or not self._csb_tx_ready:
            self._csb_tx_lt_addr = lt_addr
            self.prepare_loop("csb_tx", self._csb_tx_setup_handler)
            self.prepare_loop("csb_rx", self._csb_rx_setup_handler)
            iprint("[csb_tx] Start: registering with DM")
            self._csb_tx_dm.register()
            iprint("[csb_rx] Start: registering with DM")
            self._csb_rx_dm.register()
            self.loop()
            
        if self._csb_rx_complete and self._csb_tx_complete:
            self._csb_link_setup_complete = True
        
    def drop(self):
        """
        Disable csb on both transmitter and receiver 
        """
        if not self._csb_link_setup_complete:
            iprint("csb link doesn't exist")
        self._csb_rx_dm.set_csb_receive(False);
        self._csb_tx_dm.set_csb(False, self._csb_tx_lt_addr)

    def get_afh_map_info(self, csb_tx=True, ret=True):
        """
        Waits for afh map available info event 
        """
        if not self._csb_link_setup_complete:
            iprint("csb_link doesn't exist")
            return
        if csb_tx:
            self.prepare_loop("csb_tx", self._csb_tx_setup_handler)
        else:
            self.prepare_loop("csb_rx", self._csb_rx_setup_handler)
        self.loop()
        
        if ret:
            return self._csb_tx_afh_map_info
    
    def transfer_afh_map_info(self, ret=True):
        """
        Informs receiver about new afh map info by 
        1) Starting sync train on transmitter and
        2) Receiving sync train on receiver 
        """
        if not self._csb_link_setup_complete:
            iprint("csb_link doesn't exist")
            return
        self.prepare_loop("csb_tx", self._csb_tx_setup_handler)
        self.prepare_loop("csb_rx", self._csb_rx_setup_handler)
        self._csb_tx_dm.start_sync_train_req()
        self._csb_rx_dm.receive_sync_train_req(
                                        bd_addr=self._csb_tx_dm.local_bd_addr)
        self.loop()
        return self._csb_rx_afh_map_info

    def _csb_tx_setup_handler(self, msg):
        """
        Sequence on Csb transmitter :
            - Registration to DM 
            - Getting local BT address
            - Set BT version
            - Set Lt address
            - Enable Csb
            - Write Synchronization train parameters
            - Start Synchronization train
        """
        if msg["t"] == self._csb_tx_dm_task:

            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[csb_tx] DM registration completed: requesting bd address")
                self._csb_tx_dm.request_local_bd_addr()
                
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[csb_tx] BD address obtained: setting BT version")
                self._csb_tx_dm.set_bt_version()

            elif msg["id"] == DMAppMsgs.BT_VER_SET:
                iprint("[csb_tx] BT version set completed: setting lt_addr")
                self._csb_tx_dm.set_reserved_lt_addr(self._csb_tx_lt_addr)

            elif msg["id"] == DMAppMsgs.SET_RESERVED_LT_ADDR_COMPLETE:
                iprint("[csb_tx] Set reserved lt_addr completed: enable csb transmitter")
                self._csb_tx_dm.set_csb(True, self._csb_tx_lt_addr)

            elif msg["id"] == DMAppMsgs.SET_CSB_COMPLETE:
                iprint("[csb_tx] Csb transmitter enabled: writing sync train params")
                self._csb_tx_dm.write_sync_train_param_req()
                
            elif msg["id"] == DMAppMsgs.WRITE_SYNCHRONIZATION_TRAIN_PARAM_COMPLETE:
                iprint("[csb_tx] Write sync train param completed: starting sync train")
                self._csb_tx_dm.start_sync_train_req()
                self._csb_tx_ready = True
                # Start receive sync train request on receiver if required                
                if not self._csb_rx_ready:
                    self._csb_rx_ready = True
                    iprint("[csb_rx] Sending receive sync train request")
                    self._csb_rx_dm.receive_sync_train_req(
                                    bd_addr=self._csb_tx_dm.local_bd_addr)
                    
            elif msg["id"] == DMAppMsgs.START_SYNCHRONIZATION_TRAIN_COMPLETE:
                iprint("[csb_tx] Start sync train complete")
                self._csb_tx_complete = True
                self.end_loop("csb_tx")
                
            elif msg["id"] ==  DMAppMsgs.CSB_AFH_MAP_AVAILABLE_IND:
                iprint("[csb_tx] Afh map available indication received")
                self._csb_tx_afh_map_info = self._csb_tx_dm._afh_map_info
                self.end_loop("csb_tx")

    def _csb_rx_setup_handler(self, msg):
        """
        Sequence on Csb receiver :
            - Registration to DM 
            - Getting local BT address
            - Set BT version
            - Receive synhronization train
            - Enable Csb receive 
        """
        if msg["t"] == self._csb_rx_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[csb_rx] DM registration complete: requesting bd address")
                self._csb_rx_dm.request_local_bd_addr()
                
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[csb_rx] BD address obtained: setting BT version")
                self._csb_rx_dm.set_bt_version()
            
            elif msg["id"] == DMAppMsgs.BT_VER_SET:
                iprint("[csb_rx] Set BT version complete")
                # wait for transmitter to be ready before sending
                # receive sync train request
                if self._csb_tx_ready:
                    self._csb_rx_ready = True
                    iprint("[csb_rx] Sending synchronization train request")
                    self._csb_rx_dm.receive_sync_train_req(
                                        bd_addr=self._csb_tx_dm.local_bd_addr)
          
            elif msg["id"] == DMAppMsgs.RECEIVE_SYNCHRONIZATION_TRAIN_FAILED:
                iprint("[csb_rx] Receive sync train failed")
                if self._csb_tx_ready:
                    self.retry_count += 1
                    iprint("[csb_rx] Sending sync train request: %d" % self.retry_count)
                    # Quit loop when retry limit reached
                    if self.retry_count < self.RETRY_LIMIT:
                        self._csb_rx_dm.receive_sync_train_req(
                                            bd_addr=self._csb_tx_dm.local_bd_addr)
                    else:
                        self.end_loop("csb_rx")

            elif msg["id"] == DMAppMsgs.RECEIVE_SYNCHRONIZATION_TRAIN_COMPLETE:
                iprint("[csb_rx] Receive sync train complete")
                self._csb_rx_afh_map_info = self._csb_rx_dm._sync_train_info.map 
                if not self._csb_rx_complete:
                    iprint("[csb_rx] Enable csb receiver")
                    self._csb_rx_dm.set_csb_receive(True)
                else:
                    self.end_loop("csb_rx")

            elif msg["id"] == DMAppMsgs.SET_CSB_RECEIVE_COMPLETE:
                iprint("[csb_rx] Enable csb receiver complete")
                self._csb_rx_complete = True
                self.end_loop("csb_rx")
