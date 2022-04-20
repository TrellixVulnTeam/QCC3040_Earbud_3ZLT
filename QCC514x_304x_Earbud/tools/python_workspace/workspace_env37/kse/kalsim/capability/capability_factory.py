#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Capability handler class"""

import logging

from kats.framework.library.factory import find_subclass
from kats.framework.library.log import log_input, log_output, log_exception
from kats.library.module import get_parent_module
from kats.library.registry import register_instance, unregister_instance
from .capability_base import CapabilityBase

PLUGIN_PLATFORM = 'platform'
PLUGIN_CLASS = 'class'

CAPABILITY_UNKNOWN = ''


class CapabilityFactory:
    """kats/external Capability factory

    This class handles capabilities outside kalimba.
    It supports a factory interface, where capabilities are classified in platforms
    When this class starts it autodiscovers all the capability types available for
    a set of platforms.
    Discovered capabilities should subclass CapabilityBase in a subpackage.

    Args:
        platform (list[str]): Platforms available
    """

    def __init__(self, platform):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self._plugins = {}  # registered plugins

        mod_name = get_parent_module(__name__, __file__)  # get parent module
        try:
            class_list = find_subclass(mod_name, CapabilityBase)  # get all available interfaces
            for entry in class_list:
                if entry.platform in platform:
                    self._register(entry.platform, entry.interface, entry)
        except ImportError:
            self._log.info('no operators found')

    def _register(self, platform, interface, plugin):
        self._log.info('registering local operator interface:%s platform:%s class:%s',
                       plugin.interface, platform, plugin.__name__)
        self._plugins[interface] = {
            PLUGIN_PLATFORM: platform,
            PLUGIN_CLASS: plugin
        }

    @log_input(logging.INFO)
    @log_exception
    def get_class(self, capability):
        """Search in the discovered plugins for a operator and return its class

        Args:
            operator (str): Operator name to search

        Returns:
            any: Operator class

        Raises:
            ValueError: If unable to find operator
        """
        if capability in self._plugins:  # search by name (interface)
            operator = self._plugins[capability][PLUGIN_CLASS]
            return operator
        raise ValueError('unable to find capability:%s' % (capability))

    @log_input(logging.INFO)
    @log_exception
    def get_instance(self, capability, *args, **kwargs):
        """Search in the discovered plugins for a capability and return an instance to it

        Args:
            capability (str): Capability name to search

        Returns:
            any: Capability instance

        Raises:
            ValueError: If unable to find capability
        """
        if capability in self._plugins:  # search by name (interface)
            operator = self._plugins[capability][PLUGIN_CLASS](*args, **kwargs)
            register_instance('koperator', operator)
            register_instance('koperator_' + capability, operator)
            return operator
        raise ValueError('unable to find capability:%s' % (capability))

    @log_input(logging.INFO)
    @log_exception
    def put_instance(self, capability):
        """Destroy operator instance

        Args:
            capability (any): Capability instance
        """
        unregister_instance('koperator_' + capability.interface, capability)
        unregister_instance('koperator', capability)
        del capability

    @log_output(logging.INFO)
    @log_exception
    def enum_interface(self):
        """Get a list of registered capabilities

        Returns:
            dict:
                str: Capability name/interface
                    any: class
                    str: platform
        """
        return self._plugins
