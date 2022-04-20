#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Hydra a2dp endpoint class"""

import logging

from kats.core.endpoint_base import EndpointBase
from kats.library.registry import get_instance


class EndpointHydraA2dp(EndpointBase):
    """Hydra a2dp endpoint

    This is an endpoint that is created and destroyed in the stream as part of the hydra audio
    data service creation/destruction.
    From here we just get the endpoint id from the stream

    Args:
        kymera (kats.kymera.kymera.kymera_base.KymeraBase): Instance of class Kymera
        endpoint_type (str): Type of endpoint "source" or "sink"
        stream (int): Hydra a2dp stream index
    """

    platform = ['crescendo', 'stre', 'streplus', 'maorgen1', 'maor']
    interface = 'a2dp'

    def __init__(self, kymera, endpoint_type, stream):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self.__stream = stream
        self._log.warning('a2dp endpoints are obsolete, please use l2cap')

        super().__init__(kymera, endpoint_type)

    def get_id(self):
        stream = get_instance('stream_a2dp', self.__stream)
        return stream.get_endpoint_id()

    def create(self, *_, **__):
        """Created as part of Audio Data Service start_service."""

    def config(self):
        pass

    def destroy(self):
        """Destroyed as part of Audio Data Service stop_service."""
