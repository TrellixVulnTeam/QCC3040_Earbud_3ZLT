############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels import PureVirtualError
from csr.dev.model.base_component import BaseComponent


class BaseChip (BaseComponent):
    """\
    Chip Model (Base)    
    """

    # BaseComponent compliance

    @property
    def title(self): 
        # default to Device + name
        return '%s chip' % self.full_name
    
    # Extensions
    
    @property
    def full_name(self):
        """
        """
        return self.name.lower()
    
    @property
    def name(self):
        """\
        Chip Family Name
        """
        raise PureVirtualError(self)

    def load_register_defaults(self):
        raise PureVirtualError(self)
    

    @property
    def data_space_owners(self):
        """
        Abstraction of cores to include things that have their own
        data space even if they aren't actually processors
        
        By default just cores.
        """
        return self.cores


class ChipBase(BaseComponent):
    """
    Base chip class for registering subsystems and subcomponents to a chip
    """
    def __init__(self, access_cache_type, io_struct=None, raw_version=None):

        self.access_cache_type = access_cache_type
        self._raw_version = raw_version
        self.device = None
        self._subcomponents = {}
        self._subsystems = []
        self._cores = []
        self._io_struct = io_struct

    def set_device(self, device):
        """set the chip's device object"""
        self.device = device

    def register_subcomponents(self, subcmpts_dict):
        for key in subcmpts_dict:
            if key in self._subcomponents:
                # subcomponent already exists, raise an error!
                raise ValueError("Error: Trying to register a subcomponent twice!.")
        self._subcomponents.update(subcmpts_dict)

    def register_subsystem(self, subsystem):
        self._subsystems.append(subsystem)

    @property
    def subcomponents(self):
        """
        property accessor for the subcomponents
        """
        return self._subcomponents

    @property
    def subsystems(self):
        """
        property accessor for the subsystems
        """
        return self._subsystems

    def load_register_defaults(self):
        """
        Load register defaults on all the subsystems
        """
        for ss in self.subsystems:
            ss.load_register_defaults()

    @property
    def cores(self):
        return [ss.core for ss in self.subsystems]


class HasAppsMixin(object):
    """
    Mixin class for adding Apps subsystem to a chip
    """

    def __init__(self):
        # Create an APPS subsystem attribute
        self._apps_subsystem = self.APPS_SUBSYSTEM_TYPE(self,
                                                        self.access_cache_type,
                                                        self._io_struct)
        self.register_subsystem(self._apps_subsystem)
        self.register_subcomponents({"apps_subsystem": "_apps_subsystem"})

    @property
    def apps_subsystem(self):
        """\
        Apps Subsystem Proxy
        """
        return self._apps_subsystem


class HasBTMixin(object):
    """
    Mixin class for adding BT subsystem to a chip
    """

    def __init__(self):
        # Create a BT subsystem attribute
        self._bt_subsystem = self.BT_SUBSYSTEM_TYPE(self,
                                                    self.access_cache_type,
                                                    self._io_struct)
        self.register_subsystem(self._bt_subsystem)
        self.register_subcomponents({"bt_subsystem": "_bt_subsystem"})

    @property
    def bt_subsystem(self):
        """\
        BT Subsystem Proxy
        """
        return self._bt_subsystem

class HasAudioMixin(object):
    """
    Mixin class for adding Audio subsystem to a chip
    """
    def __init__(self):

        self._audio_subsystem = self.AUDIO_SUBSYSTEM_TYPE(self, 
                                                      self.access_cache_type,
                                                      self._io_struct)
        self.register_subsystem(self._audio_subsystem)
        self.register_subcomponents({"audio_subsystem" : "_audio_subsystem"})

    @property
    def audio_subsystem(self):
        return self._audio_subsystem
