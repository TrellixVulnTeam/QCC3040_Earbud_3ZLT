############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2018 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from csr.wheels.global_streams import iprint
from csr.wheels.polling_loop import poll_loop
from .libs.dm import DMAppMsgs, DMLib
from csr.dev.fw.trap_api.bluestack_utils import BluestackUtils

class LE2Setup(object):
    """
    Applicaton that runs the message_loop to configure PHY. If necessary,
    it will perform initialisation (protocol registration, etc) first.
    """
    def __init__(self, le2_tx_dev, le2_rx_dev, verbose=False):
        """
        Initialise this object, and set up basic things in the firmware too,
        such as the Bluetooth upper layers task
        """
        self._verbose=verbose
        
        self._le2_tx_apps = le2_tx_dev.chip.apps_subsystem
        self._le2_rx_apps = le2_rx_dev.chip.apps_subsystem
        
        self._le2_tx_utils = self._le2_tx_apps.p1.fw.trap_utils
        self._le2_rx_utils = self._le2_rx_apps.p1.fw.trap_utils

        # Create the Bluestack library instances
        self._le2_tx_dm = DMLib(self._le2_tx_utils,
                             verbose=self._verbose, tag="le2_tx dm")
        self._le2_rx_dm = DMLib(self._le2_rx_utils,
                             verbose=self._verbose, tag="le2_rx dm")

        # Set state variables for keeping track of master and slave progress
        self.dm_ulp_set_phy = False
        self.dm_ulp_set_default_phy = False
    
    def toggle_verbose(self):
        self._verbose = not self._verbose

    def loop(self):
        poll_loop()

    def end_loop(self, le2_tx_or_le2_rx):
        """
        Free the tasks associated with loop
        """
        utils = getattr(self, "_%s_utils"%le2_tx_or_le2_rx)
        utils.remove_msg_type_task("BlueStack")
        attr = "_%s_%s_task"%(le2_tx_or_le2_rx, "dm")
        task = getattr(self, attr)
        utils.delete_task(task)
        delattr(self, attr)

    def prepare_loop(self, le2_tx_or_rx, handler):
        """
        Prepare loop if not previously registered
        """
        if le2_tx_or_rx == "le2_tx" and not hasattr(self, "_le2_tx_dm_task"):
            self._le2_tx_dm_task = self._le2_tx_utils.create_task(handler)
            self._le2_tx_dm.reset_app_task(self._le2_tx_dm_task)
            BluestackUtils(self._le2_tx_utils).set_bluestack_handler(
                                                    dm=self._le2_tx_dm.handler)
        elif le2_tx_or_rx == "le2_rx" and not hasattr(self, "_le2_rx_dm_task"):
            self._le2_rx_dm_task = self._le2_rx_utils.create_task(handler)
            self._le2_rx_dm.reset_app_task(self._le2_rx_dm_task)
            BluestackUtils(self._le2_rx_utils).set_bluestack_handler(
                                                    dm=self._le2_rx_dm.handler)
            
    def set_2le_ulp_default_phy_req(self, **cfg_dict):
        """
        Setup the 2LE ULP default PHY
        """
        ret = False
        self.prepare_loop("le2_tx", self._le2_tx_setup_handler)
        self._le2_tx_dm.set_ulp_default_phy_req(**cfg_dict)
        self.loop()
        if(self.dm_ulp_set_default_phy):
            ret = True
        return ret
            
        
    def set_2le_ulp_phy_req(self, **cfg_dict):
        """
        Setup the 2LE ULP PHY
        """
        ret = False
        self.prepare_loop("le2_tx", self._le2_tx_setup_handler)
        cfg_dict["bdaddress"] = self._le2_tx_dm.local_bd_addr
        self._le2_tx_dm.set_ulp_phy_req(**cfg_dict)
        self.loop()
        if(self.dm_ulp_set_phy):
            ret = True
        return ret

    def setup(self, lt_addr=0x01):
        """
        Setup 2LE link between transmitter and receiver if required
        """        
        self._le2_tx_lt_addr = lt_addr
        self.prepare_loop("le2_tx", self._le2_tx_setup_handler)
        self.prepare_loop("le2_rx", self._le2_rx_setup_handler)
        iprint("[le2_tx] Start: registering with DM")
        self._le2_tx_dm.register()
        iprint("[le2_rx] Start: registering with DM")
        self._le2_rx_dm.register()
        self.loop()

    def _le2_tx_setup_handler(self, msg):
        """
        Sequence on le2 transmitter :
            - Registration to DM 
            - Getting local BT address
            - Set BT version
            - configure default PHY
            -configure PHY
        """
        if msg["t"] == self._le2_tx_dm_task:

            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[le2_tx] DM registration completed: requesting bd address")
                self._le2_tx_dm.request_local_bd_addr()
                
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[le2_tx] BD address obtained: setting BT version")
                self._tx_bd_addr = self._le2_tx_dm.local_bd_addr
                self._le2_tx_dm.set_bt_version()

            elif msg["id"] == DMAppMsgs.BT_VER_SET:
                iprint("[le2_tx] BT version set completed")
                self.end_loop("le2_tx")
                
            elif msg["id"] == DMAppMsgs.DM_ULP_SET_DEFAULT_PHY_CFM:
                iprint("[le2_tx] 2LE ULP set default PHY REQ confirmed")
                self.dm_ulp_set_default_phy = True
                self.end_loop("le2_tx")
            
            elif msg["id"] == DMAppMsgs.DM_ULP_SET_DEFAULT_PHY_CFM_FAILED:
                iprint("[le2_tx] 2LE ULP set default PHY REQ confirm failed")
                self.dm_ulp_set_default_phy = False
                self.end_loop("le2_tx")
                
            elif msg["id"] == DMAppMsgs.DM_ULP_SET_PHY_CFM:
                iprint("[le2_tx] 2LE ULP set PHY REQ confirmed")
                self.dm_ulp_set_phy = True
                self.end_loop("le2_tx")
   
            elif msg["id"] == DMAppMsgs.DM_ULP_SET_PHY_CFM_FAILED:
                iprint("[le2_tx] 2LE ULP set PHY REQ confirm failed")
                self.dm_ulp_set_phy = False
                self.end_loop("le2_tx")
       

    def _le2_rx_setup_handler(self, msg):
        """
        Sequence on le2 receiver :
            - Registration to DM 
            - Getting local BT address
            - Set BT version
        """
        if msg["t"] == self._le2_rx_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                iprint("[2le_rx] DM registration complete: requesting bd address")
                self._le2_rx_dm.request_local_bd_addr()
                
            elif msg["id"] == DMAppMsgs.LOCAL_BD_ADDR_OBTAINED:
                iprint("[le2_rx] BD address obtained: setting BT version")
                self._rx_bd_addr = self._le2_rx_dm.local_bd_addr
                self._le2_rx_dm.set_bt_version()
            
            elif msg["id"] == DMAppMsgs.BT_VER_SET:
                iprint("[le2_rx] Set BT version complete")
                self.end_loop("le2_rx")

          
            