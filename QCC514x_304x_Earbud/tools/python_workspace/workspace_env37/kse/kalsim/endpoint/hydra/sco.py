#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra sco endpoint class"""

import logging

from kats.core.endpoint_base import EndpointBase
from kats.library.registry import get_instance


class EndpointHydraSco(EndpointBase):
    """Hydra sco endpoint

    This is an endpoint that is destroyed in the stream as part of the hydra sco
    data service creation/destruction.

    Args:
        kymera (kats.kymera.kymera.kymera_base.KymeraBase): Instance of class Kymera
        endpoint_type (str): Type of endpoint "source" or "sink"
        stream (int): Hydra sco stream index
    """

    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'sco'

    def __init__(self, kymera, endpoint_type, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self.__stream = kwargs.pop('stream')

        # initialise values
        self.__args = []
        for entry in args:
            if not isinstance(entry, list):
                raise RuntimeError('arg %s invalid should be a list' % (entry))
            if len(entry) != 2:
                raise RuntimeError('arg %s invalid should be list of 2 elements' % (entry))
            self.__args.append(entry)

        self.__args += list(kwargs.items())

        super().__init__(kymera, endpoint_type)

    def create(self, *_, **__):
        stream = get_instance('stream_sco', self.__stream)
        self._create('sco', [stream.get_hci_handle(), 0x0000])

    def config(self):
        for entry in self.__args:
            self.config_param(entry[0], entry[1])

        super().config()

    def destroy(self):
        """Destroyed as part of SCO Processing Service stop_service."""
