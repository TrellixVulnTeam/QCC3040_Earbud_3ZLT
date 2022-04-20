############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 - 2022 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################


class UnknownDongleID(RuntimeError):
    """
    Unknown dongle ID passed as argument to the select method.
    """

class DongleManager(object):
    """
    This is the class that is used when managing multiple transport dongles
    attached to the same PC.
    If there are multiple dongles connected, it expects the relevant
    arguments to be passed in, to be able to select the correct one.
    """
    def __init__(self):
        self._dongle_kwargs_list = []
        self._current_id = None

    def register(self, conn_manager, **dongle_kwargs):
        new_id = len(self._dongle_kwargs_list)
        self._dongle_kwargs_list.append((conn_manager, dongle_kwargs))
        return new_id

    def select(self, dongle_id):
        if self._current_id == dongle_id:
            # The dongle is already connected. Nothing to do here.
            return
        if dongle_id >= len(self._dongle_kwargs_list):
            # Dongle ID not found.
            raise UnknownDongleID

        conn_manager, kwargs = self._dongle_kwargs_list[dongle_id]
        api = conn_manager.api
        # Invalidate any existing connections
        conn_manager.invalidate_conns()
        # Need to reopen the DLL session with a different dongle.
        if self._current_id is not None:
            api.close()
        
        api.open(**kwargs)
        self._current_id = dongle_id