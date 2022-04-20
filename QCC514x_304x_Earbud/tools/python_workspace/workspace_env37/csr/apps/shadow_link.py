############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from time import sleep
from csr.wheels.polling_loop import poll_loop
from csr.apps.libs.sdm import SDMAppMsgs, SDMLib, SDMException
from csr.dev.fw.trap_api.bluestack_utils import BluestackUtils
import logging

class ShadowLink(object):
    """
    Base class providing access to the SDM library to perform create shadow acl
    link, set BREDR slave address, disconnect shadow link. The class provides
    a polling loop and app tasks callbacks (_primary_handler and 
    _secondary_handler) for the SDM handler to report back to.
    The supported functions are:
        - create_shadow_acl_link
        - set_bredr_slave_address
        - disconnect_shadow_link
    """

    def __init__(self, primary_trap_utils, secondary_trap_utils, logger=None):
        """
        Initialise ShadowLink, create Primary and Secondary SDMs.
        """
        if logger == None:
            self._log = logging.getLogger("sdm")
            self._log.setLevel(logging.INFO)
        else:
            self._log = logger

        self._primary_trap_utils = primary_trap_utils
        self._secondary_trap_utils = secondary_trap_utils

        # Create the SDM library instances
        self._primary_sdm = SDMLib(self._primary_trap_utils, linear=False, verbose=True, logger=logger)
        self._secondary_sdm = SDMLib(self._secondary_trap_utils, linear=False, verbose=True, logger=logger)

        # Set state variables for keeping track of primary and secondary progress
        self._primary_sdm_ready = False
        self._secondary_sdm_ready = False

    def sdm_register(self, primary_or_secondary):
        """
        Register the SDM object for primary or secondary.
        """
        sdm = getattr(self, "_%s_sdm" % primary_or_secondary)
        self.prepare_loop(primary_or_secondary)
        sdm.register()
        self.loop()

    def create_shadow_acl_link(self, shadowed_bd_addr, secondary_bd_addr):
        """
        Create shadow ACL link.
        """
        if not self._primary_sdm_ready:
            self.sdm_register('primary')
        if not self._secondary_sdm_ready:
            self.sdm_register('secondary')

        self._primary_sdm.handle = 0
        self._secondary_sdm.handle = 0
        self.prepare_loop('primary')
        self.prepare_loop('secondary')
        self._primary_sdm.create_shadow_acl_link(shadowed_bd_addr=shadowed_bd_addr, \
                                                 secondary_bd_addr=secondary_bd_addr)
        self.loop()

        return (self._primary_sdm.handle, self._secondary_sdm.handle)

    def create_shadow_sco_link(self, shadowed_bd_addr, secondary_bd_addr):
        """
        Create shadow ACL link.
        """
        if not self._primary_sdm_ready:
            self.sdm_register('primary')
        if not self._secondary_sdm_ready:
            self.sdm_register('secondary')

        self._primary_sdm.handle = 0
        self._secondary_sdm.handle = 0
        self.prepare_loop('primary')
        self.prepare_loop('secondary')
        self._primary_sdm.create_shadow_sco_link(shadowed_bd_addr=shadowed_bd_addr, \
                                                 secondary_bd_addr=secondary_bd_addr)
        self.loop()

        return (self._primary_sdm.handle, self._secondary_sdm.handle)

    def set_bredr_slave_address(self, remote_bd_addr, new_bd_addr):
        """
        Set the slave bredr address. Always done on primary.
        """
        if not self._primary_sdm_ready:
            self.sdm_register('primary')

        self.prepare_loop('primary')
        self._primary_sdm.set_bredr_slave_address(remote_bd_addr=remote_bd_addr, new_bd_addr=new_bd_addr)
        self.loop()

        return self._primary_sdm_set_bdedr_slave_address

    def disconnect_shadow_link(self, handle, primary_or_secondary='primary'):
        """
        Disconnect the shadow ACL link. Always done on primary.
        """
        if not getattr(self, "_%s_sdm_ready" % primary_or_secondary):
            self.sdm_register(primary_or_secondary)

        self.prepare_loop(primary_or_secondary)
        if primary_or_secondary == 'primary':
            self._primary_sdm.disconnect_shadow_link(handle)
        else:
            self._secondary_sdm.disconnect_shadow_link(handle)
        self.loop()

        if primary_or_secondary == 'primary':
            ret = self._primary_sdm_shadow_link_disconnect
        else:
            ret = self._secondary_sdm_shadow_link_disconnect

        return ret

    def loop(self):
        """
        Poll loop for responses from SDM.
        """
        poll_loop()

    def end_loop(self, primary_or_secondary):
        """
        Free the tasks associated with the loop
        """
        utils = getattr(self, "_%s_trap_utils" % primary_or_secondary)
        utils.remove_msg_type_task("Sdm")
        attr = "_%s_sdm_task" % (primary_or_secondary)
        task = getattr(self, attr)
        utils.delete_task(task)
        delattr(self, attr)

    def prepare_loop(self, primary_or_secondary='primary'):
        """
        Prepare the loop if it is currently not prepared.
        """
        # Create "app tasks" for the bluestack library handlers to report back to.
        # Then set the per-protocol bluestack handlers for the main polling loop handler
        if not hasattr(self, "_%s_sdm_task" % primary_or_secondary):
            if primary_or_secondary == 'primary':
                self._primary_sdm_task = self._primary_trap_utils.create_task(self._primary_handler)
                self._primary_sdm.reset_app_task(self._primary_sdm_task)

                BluestackUtils(self._primary_trap_utils).set_bluestack_handler(sdm=self._primary_sdm.handler)
            else:
                self._secondary_sdm_task = self._secondary_trap_utils.create_task(self._secondary_handler)
                self._secondary_sdm.reset_app_task(self._secondary_sdm_task)

                BluestackUtils(self._secondary_trap_utils).set_bluestack_handler(sdm=self._secondary_sdm.handler)

    def _primary_handler(self, msg):
        """
        Primary DM handler apps task callback. Check for _CFM messages and end poll loop
        when received. When creating an ACL or SCO wait for the _CFM message on primary
        and _IND on secondary. End loop for primary and secondary if both handles are vaild.
        When ACL or SCO link connections fail, _CFM_FAILED is retuned so end both loops.
        """
        if msg["t"] == self._primary_sdm_task:
            if msg["id"] == SDMAppMsgs.REGISTRATION_COMPLETE:
                self._primary_sdm_ready = True
                self.end_loop('primary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_CFM:
                # If both connection handles are valid (not 0 or 0xFFFF) then end primary and secondary loops.
                if self._primary_sdm.handle not in (0, 0xFFFF) and self._secondary_sdm.handle not in (0, 0xFFFF):
                    self.end_loop('primary')
                    self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_CFM_FAILED:
                self.end_loop('primary')
                self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_CFM:
                # If both connection handles are valid (not 0 or 0xFFFF) then end primary and secondary loops.
                if self._primary_sdm.handle not in (0, 0xFFFF) and self._secondary_sdm.handle not in (0, 0xFFFF):
                    self.end_loop('primary')
                    self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_CFM_FAILED:
                self.end_loop('primary')
                self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SET_BREDR_SLAVE_ADDRESS_CFM:
                self._primary_sdm_set_bdedr_slave_address = True
                self.end_loop('primary')

            if msg["id"] == SDMAppMsgs.SDM_SET_BREDR_SLAVE_ADDRESS_CFM_FAILED:
                self._primary_sdm_set_bdedr_slave_address = False
                self.end_loop('primary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_LINK_DISCONNECT_CFM:
                self._primary_sdm_shadow_link_disconnect = True
                self.end_loop('primary')

    def _secondary_handler(self, msg):
        """
        Secondary SDM handler apps task callback. Check for _CFM messages and end poll loop
        when received. When creating an ACL or SCO wait for the _IND message on secondary
        and _CFM on primary. End loop for primary and secondary if both handles are vaild.
        When ACL or SCO link connections fail, _IND_FAILED is retuned so end both loops.
        """
        if msg["t"] == self._secondary_sdm_task:
            if msg["id"] == SDMAppMsgs.REGISTRATION_COMPLETE:
                self._secondary_sdm_ready = True
                self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_IND:
                # If both connection handles are valid (not 0 or 0xFFFF) then end primary and secondary loops.
                if self._primary_sdm.handle not in (0, 0xFFFF) and self._secondary_sdm.handle not in (0, 0xFFFF):
                    self.end_loop('primary')
                    self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ACL_LINK_CREATE_IND_FAILED:
                self.end_loop('primary')
                self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_IND:
                # If both connection handles are valid (not 0 or 0xFFFF) then end primary and secondary loops.
                if self._primary_sdm.handle not in (0, 0xFFFF) and self._secondary_sdm.handle not in (0, 0xFFFF):
                    self.end_loop('primary')
                    self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_ESCO_LINK_CREATE_IND_FAILED:
                self.end_loop('primary')
                self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SET_BREDR_SLAVE_ADDRESS_CFM:
                self._secondary_sdm_set_bdedr_slave_address = True
                self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SET_BREDR_SLAVE_ADDRESS_CFM_FAILED:
                self._secondary_sdm_set_bdedr_slave_address = False
                self.end_loop('secondary')

            if msg["id"] == SDMAppMsgs.SDM_SHADOW_LINK_DISCONNECT_CFM:
                self._secondary_sdm_shadow_link_disconnect = True
                self.end_loop('secondary')

