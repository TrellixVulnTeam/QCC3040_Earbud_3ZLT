############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.model.base_component import BaseComponent


class GenericSubsystem(BaseComponent):
    """
    The generic subsystem common base class for zeagle chips
    Relies on CORE_TYPE being defined in a derived class.
    """
    # pylint: disable=no-member

    def __init__(self, chip, access_cache_type, io_struct=None):
        BaseComponent.__init__(self)
        self._access_cache_type = access_cache_type
        self._io_struct = io_struct
        # join up the object traversal hierarchy for pydbg
        # self.chip is chip.bt_subsystem.chip
        self.chip = chip

    @property
    def core(self):
        """
        Bluetooth Subsystem's one and only cpu Core.
        """

        try:
            self._core
        except AttributeError:
            self._core = self.CORE_TYPE(
                self, self._access_cache_type, self._io_struct)
            try:
                self._core.populate
            except AttributeError:
                pass
            else:
                self._core.populate(self._access_cache_type)
        return self._core

    @property
    def name(self):
        """Name used in UI for this subsystem"""
        return "generic"

    @property
    def subcomponents(self):
        # implements base class property
        return {"core" : "_core"}
    
    def load_register_defaults(self):
        self.core.has_data_source=True
        self.core.load_register_defaults()