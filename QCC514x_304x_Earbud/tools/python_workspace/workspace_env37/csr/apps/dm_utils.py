############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from time import sleep
from csr.wheels.global_streams import iprint
from csr.wheels.polling_loop import poll_loop
from .libs.dm import DMAppMsgs, DMLib
from csr.dev.fw.trap_api.bluestack_utils import BluestackUtils
import logging

class DMUtils(object):
    """
    Base class prividing access to the DM library to perform switch role, scan
    enable and write link policy settings. The class provides a polling loop
    and app tasks callbacks (_mst_handler and _slv_handler) for the
    DM handler to report back to. The supported functions are:
        - dm_register
        - get_local_bdaddr
        - scan_enable
        - write_link_policy_settings
        - switch_role
    """

    def __init__(self, mst_trap_utils, slv_trap_utils, logger=None):
        """
        Initialise DMUtils, create mst and slv DMs.
        """
        if logger == None:
            self._log = logging.getLogger("dm")
            self._log.setLevel(logging.INFO)
        else:
            self._log = logger

        self._mst_trap_utils = mst_trap_utils
        self._slv_trap_utils = slv_trap_utils

        # Create the DM library instances
        self._mst_dm = DMLib(self._mst_trap_utils, linear=False, verbose=True, logger=logger)
        self._slv_dm = DMLib(self._slv_trap_utils, linear=False, verbose=True, logger=logger)

        # Set state variables for keeping track of mst and slv progress
        self._mst_dm_ready = False
        self._slv_dm_ready = False

    def dm_register(self, mst_or_slv):
        """
        Register the DM object for mst or slv.
        """
        dmanager = getattr(self, "_%s_dm" % mst_or_slv)
        self.prepare_loop(mst_or_slv)
        dmanager.register()
        self.loop()

    def get_local_bdaddr(self, mst_or_slv):
        """
        Get the local bdaddr for mst or slv.
        """
        if not getattr(self, "_%s_dm_ready" % mst_or_slv):
            self.dm_register(mst_or_slv)

        if mst_or_slv == 'mst':
            bd_addr = self._mst_trap_utils.bluestack.get_local_bdaddr()
            self._mst_dm_bd_addr = bd_addr
        else:
            bd_addr = self._slv_trap_utils.bluestack.get_local_bdaddr()
            self._slv_dm_bd_addr = bd_addr

        return bd_addr

    def scan_enable(self, mst_or_slv, inq_scan, page_scan):
        """
        Scan enable on mst or slv device.
        """
        if not getattr(self, "_%s_dm_ready" % mst_or_slv):
            self.dm_register(mst_or_slv)

        self.prepare_loop(mst_or_slv)
        if mst_or_slv == 'mst':
            self._mst_dm.scan_enable(inq_scan=inq_scan, page_scan=page_scan)
        else:
            self._slv_dm.scan_enable(inq_scan=inq_scan, page_scan=page_scan)

        self.loop()

        if mst_or_slv == 'mst':
            ret = self._mst_dm_hci_write_scan_enable
        else:
            ret = self._slv_dm_hci_write_scan_enable

        return ret

    def init_dm_sm(self, mst_or_slv):
        """
        Intialise DM Security Manager on mst or slv device.
        """
        if not getattr(self, "_%s_dm_ready" % mst_or_slv):
            self.dm_register(mst_or_slv)

        self.prepare_loop(mst_or_slv)
        if mst_or_slv == 'mst':
            self._mst_dm.init_dm_sm()
        else:
            self._slv_dm.init_dm_sm()

        self.loop()

        if mst_or_slv == 'mst':
            ret = self._mst_dm_sm_initialised
        else:
            ret = self._slv_dm_sm_initialised

        return ret

    def sniff_mode_req(self, mst_or_slv, bd_addr, enter=True ):
        """
        Change policy settings to exit sniff. Set link_policy_settings: 0x05
        """
        ret = self.write_link_policy_settings(mst_or_slv, bd_addr)
        if not ret:
            return False
        if mst_or_slv == 'mst':
            dmlib = self._mst_dm
        else:
            dmlib = self._slv_dm
            
        if enter:
            dmlib.enter_sniff_mode_req(bd_addr=bd_addr)
        else:
            dmlib.exit_sniff_mode_req(bd_addr=bd_addr)
        return ret
    
    def write_link_policy_settings(self, mst_or_slv, bd_addr):
        """
        Set link_policy_settings: 0x05
        """
        if not getattr(self, "_%s_dm_ready" % mst_or_slv):
            self.dm_register(mst_or_slv)

        self.prepare_loop(mst_or_slv)
        if mst_or_slv == 'mst':
            self._mst_dm.write_link_policy_settings(bd_addr=bd_addr, link_policy_settings=0x05)
        else:
            self._slv_dm.write_link_policy_settings(bd_addr=bd_addr, link_policy_settings=0x05)

        self.loop()

        if mst_or_slv == 'mst':
            ret = self._mst_dm_hci_write_link_policy_settings
        else:
            ret = self._slv_dm_hci_write_link_policy_settings

        return ret

    def switch_role(self, mst_bd_addr, role='slave'):
        """
        Switch role.
        """
        if not self._mst_dm_ready:
            self.dm_register('mst')

        self.prepare_loop('mst')
        self._mst_dm.switch_role(bd_addr=mst_bd_addr, role=role)
        self.loop()

        return self._mst_dm_hci_switch_role
    
    def sm_bond_req(self):
        """
        Bond master and slave devices 
        """
        if not self._mst_dm_ready or not self._mst_dm_sm_initialised:
            return False
        
        self.prepare_loop('mst')
        self.prepare_loop('slv')
        self._mst_dm.sm_bond_request(bd_addr=self._slv_dm_bd_addr)# self._slv_dm.local_bd_addr())
        self.loop()
        return self._sm_bond_ready

    def _sm_io_cap_response(self, mst_or_slv):
        """
        Send sm io capability request response
        """
        if mst_or_slv == 'mst':
            self._mst_dm.sm_io_cap_req_rsp(tp_addrt = self._mst_dm.get_tp_addrt_in_sm_io_req_ind())
        else:
            self._slv_dm.sm_io_cap_req_rsp(tp_addrt = self._slv_dm.get_tp_addrt_in_sm_io_req_ind())

    def loop(self):
        """
        Poll loop for responses from DM.
        """
        poll_loop()

    def end_loop(self, mst_or_slv):
        """
        Free the mst or slv tasks associated with the loop.
        """
        utils = getattr(self, "_%s_trap_utils" % mst_or_slv)
        utils.remove_msg_type_task("BlueStack")
        attr = "_%s_dm_task" % (mst_or_slv)
        task = getattr(self, attr)
        utils.delete_task(task)
        delattr(self, attr)

    def prepare_loop(self, mst_or_slv='mst'):
        """
        Prepare the loop if it is currently not prepared.
        """
        # Create "app tasks" for the bluestack library handlers to report back to.
        # Then set the per-protocol bluestack handlers for the main polling loop handler.
        if not hasattr(self, "_%s_dm_task" % mst_or_slv):
            if mst_or_slv == 'mst':
                self._mst_dm_task = self._mst_trap_utils.create_task(self._mst_handler)
                self._mst_dm.reset_app_task(self._mst_dm_task)

                BluestackUtils(self._mst_trap_utils).set_bluestack_handler(dm=self._mst_dm.handler)
            else:
                self._slv_dm_task = self._slv_trap_utils.create_task(self._slv_handler)
                self._slv_dm.reset_app_task(self._slv_dm_task)

                BluestackUtils(self._slv_trap_utils).set_bluestack_handler(dm=self._slv_dm.handler)

    def _mst_handler(self, msg):
        """
        mst DM handler apps task callback. Check for _CFM messages and end
        poll loop when received.
        """
        if msg["t"] == self._mst_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                self._mst_dm_ready = True
                iprint("[mst] DM registration complete")
                self._mst_dm.set_bt_version()
                
            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_SCAN_ENABLE_CFM:
                self._mst_dm_hci_write_scan_enable = True
                iprint("[mst]: Write scan enable complete")
                
            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_SCAN_ENABLE_CFM_FAILED:
                self._mst_dm_hci_write_scan_enable = False
                iprint("[mst]: Write scan enable failed")
                
            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM:
                self._mst_dm_hci_write_link_policy_settings = True
                iprint("[mst]: Write policy settings complete")

            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM_FAILED:
                self._mst_dm_hci_write_link_policy_settings = False
                iprint("[mst]: Write policy settings failed")

            if msg["id"] == DMAppMsgs.DM_HCI_SWITCH_ROLE_CFM:
                iprint("[mst]: Role switch complete")
                self._mst_dm_hci_switch_role = True
                
            if msg["id"] == DMAppMsgs.DM_HCI_SWITCH_ROLE_CFM_FAILED:
                self._mst_dm_hci_switch_role = False
                iprint("[mst]: Role switch failed")

            if msg["id"] == DMAppMsgs.SM_INIT_CFM:
                self._mst_dm_sm_initialised = True
                iprint("[mst]: SM Init complete")
                
            if msg["id"] == DMAppMsgs.SM_INIT_CFM_FAILED:
                self._mst_dm_sm_initialised = False
                iprint("[mst]: SM Init failed")

            if msg["id"] == DMAppMsgs.SM_IO_CAPABILITY_REQUEST_IND:
                iprint("[mst]: SM IO capability request")
                self._sm_io_cap_response('mst')
                # Need to wait for SM_BONDING_CFM before ending loop 
                return
                
            if msg["id"] == DMAppMsgs.SM_BONDING_CFM:
                self._sm_bond_ready = True
                iprint("[mst]: SM Bond complete")
                
            if msg["id"] == DMAppMsgs.SM_BONDING_CFM_FAILED:
                self._sm_bond_ready = False
                iprint("[mst]: SM Bond failed")
                self.end_loop('slv')
        
        self.end_loop('mst')

    def _slv_handler(self, msg):
        """
        slv DM handler apps task callback. Check for _CFM messages and end
        poll loop when received.
        """
        if msg["t"] == self._slv_dm_task:
            if msg["id"] == DMAppMsgs.REGISTRATION_COMPLETE:
                self._slv_dm_ready = True
                iprint("[slv] DM registration complete")
                self._slv_dm.set_bt_version()
                
            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_SCAN_ENABLE_CFM:
                self._slv_dm_hci_write_scan_enable = True
                iprint("[slv] Write scan enable complete")

            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_SCAN_ENABLE_CFM_FAILED:
                self._slv_dm_hci_write_scan_enable = False
                iprint("[slv] Write scan enable failed")

            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM:
                iprint("[slv]: Write policy settings complete")
                self._slv_dm_hci_write_link_policy_settings = True

            if msg["id"] == DMAppMsgs.DM_HCI_WRITE_LINK_POLICY_SETTINGS_CFM_FAILED:
                self._slv_dm_hci_write_link_policy_settings = False
                iprint("[slv]: Write policy settings failed")
 
            if msg["id"] == DMAppMsgs.DM_HCI_SWITCH_ROLE_CFM:
                self._slv_dm_hci_switch_role = True
                iprint("[slv]: Role switch complete")
 
            if msg["id"] == DMAppMsgs.DM_HCI_SWITCH_ROLE_CFM_FAILED:
                self._slv_dm_hci_switch_role = False
                iprint("[slv]: Role switch failed")
                
            if msg["id"] == DMAppMsgs.SM_INIT_CFM:
                self._slv_dm_sm_initialised = True
                iprint("[slv]: SM Init complete")
                
            if msg["id"] == DMAppMsgs.SM_INIT_CFM_FAILED:
                self._slv_dm_sm_initialised = False
                iprint("[slv]: SM Init failed")

            if msg["id"] == DMAppMsgs.SM_IO_CAPABILITY_REQUEST_IND:
                iprint("[slv]: SM IO capability request")
                self._sm_io_cap_response('slv')
            
        self.end_loop('slv')

