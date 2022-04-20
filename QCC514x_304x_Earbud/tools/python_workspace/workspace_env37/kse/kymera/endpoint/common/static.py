#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Static endpoint class"""

import logging

from kats.core.endpoint_base import EndpointBase


class EndpointStatic(EndpointBase):
    """Static Endpoint

    This is an endpoint that never gets created or destroyed

    Args:
        kymera (kats.kymera.kymera.kymera_base.KymeraBase): Instance of class Kymera
        endpoint_type (str): Type of endpoint source or sink
        endpoint_id (int): Endpoint identifier
    """

    platform = ['common']
    interface = 'static'

    def __init__(self, kymera, endpoint_type, endpoint_id):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self.__endpoint_id = endpoint_id

        super().__init__(kymera, endpoint_type)

    def get_id(self):
        return self.__endpoint_id

    def create(self, *_, **__):
        """We don't need to worry about creating static endpoints."""

    def config(self):
        """Static endpoint configuration is not supported"""

    def destroy(self):
        """We don't need to worry about destroying static endpoints."""
