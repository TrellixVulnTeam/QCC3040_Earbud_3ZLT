#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Capability generic handler"""

import logging

from kats.framework.library.log import log_input
from kats.library.capability import get_capability_names, get_capability_id, get_capability_name, \
    get_capability_msgs
from kats.library.registry import get_instance
from ..capability_base import CapabilityBase


class CapabilityGenericInfo:  # pylint: disable=too-few-public-methods
    """Helper class to extract information from the system capability_description information"""

    def __init__(self):
        self._cap_description = get_instance('capability_description')

    def enum_interface(self):
        """Return all capabilities handled in the generic capability handler

        Returns:
            list[list[str,int,any,str]]: Interface, capability id, class, platform
        """
        caps = get_capability_names(self._cap_description)
        ret = []
        for entry in caps:
            ret.append(
                [entry,  # interface
                 get_capability_id(self._cap_description, entry),  # cap_id
                 CapabilityGeneric,  # class
                 CapabilityGeneric.platform])  # platform
        return ret


class CapabilityGeneric(CapabilityBase):
    """Capability base class. Should be subclassed by every capability extension.

    This builds an interface on top of kymera to handle capabilities and operators.
    kymera interface is a single interface of multiple operators while CapabilityBase is
    an instance that handle a single operator.

    Args:
        cap (int or str): Capability
        kymera (kats.kymera.kymera.kymera_base.KymeraBase): Instance of class Kymera
    """
    platform = 'common'
    interface = 'generic'
    cap_id = None

    def __init__(self, cap, kymera, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        self._kymera = kymera

        cap_data = get_instance('capability_description')
        try:
            self._capability = get_capability_name(cap_data, cap)
        except Exception:  # pylint: disable=broad-except
            self._capability = str(cap)
        self.cap_id = get_capability_id(cap_data, cap)
        self._cap_msg = get_capability_msgs(cap_data, cap)
        self._op_id = None
        self.__args = []

        for entry in args:
            if not isinstance(entry, list):
                raise RuntimeError('arg %s invalid should be a list' % (entry))
            if len(entry) != 2:
                raise RuntimeError('arg %s invalid should be list of 2 elements' % (entry))
            if not isinstance(entry[1], list):
                raise RuntimeError('arg %s invalid should be a list' % (entry[1]))
            if not isinstance(entry[0], int) and entry[0] not in self._cap_msg:
                raise RuntimeError('arg %s invalid msg unknown' % (entry[0]))
            self.__args.append(entry)
        for entry in kwargs:
            if not isinstance(kwargs[entry], list):
                raise RuntimeError('kwarg %s invalid should be a list' % (kwargs[entry]))
            if entry not in self._cap_msg:
                raise RuntimeError('kwarg %s invalid msg unknown' % (entry))
            self.__args.append([entry, kwargs[entry]])

        super().__init__(self._capability, kymera, *args, **kwargs)

    @log_input(logging.INFO)
    def config(self):
        for entry in self.__args:
            op_id = self._cap_msg.get(entry[0], entry[0])
            self.send_recv_operator_message([op_id] + entry[1])
