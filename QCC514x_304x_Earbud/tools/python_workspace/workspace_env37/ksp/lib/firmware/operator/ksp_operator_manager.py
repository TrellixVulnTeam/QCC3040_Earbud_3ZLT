#
# Copyright (c) 2021 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Configuring KSP operator(s).

This module orchestrate the configuration and start/stop command for KSP
operators in two cores of Kalimba.
"""
import logging

import ksp.lib.namespace as ns
from ksp.lib.exceptions import OperatorError, FirmwareError, CommandError
from ksp.lib.firmware.downloadable.ksp_downloadable import KSPDownloadable
from ksp.lib.firmware.operator.ksp_operator import KSPOperator

logger = logging.getLogger(__name__)


class KSPOperatorManager(object):
    """
    Manages KSP Operators.

    Takes the responsibility of loading the downloadable into memory and
    config, start, and stop operators.

    Raises:
        DownloadableError: If something is wrong with loading the
            downloadable.
    """
    # KSP capability IDs.
    DOWNLOADABLE_CAP_ID = 0x4093
    BUILTIN_CAP_ID = 0x00BF

    def __init__(self, firmware, edkcs=False, builtin_cap=False):
        self._builtin_cap = builtin_cap
        self._edkcs = edkcs
        self._cap_id = None
        self._firmware = firmware

        self._downloadable = None
        self._operator_p0 = None
        self._operator_p1 = None

    def is_running(self):
        """Returns the status of the operator.

        Returns:
            bool: True if it's running, False otherwise.
        """
        if self._operator_p1:
            return (
                self._operator_p0.is_running() and
                self._operator_p1.is_running()
            )

        if self._operator_p0:
            return self._operator_p0.is_running()

        return False

    def config(self, streams):
        """Configure KSP Operator(s) based on the given streams.

        Loads the downloadable and, based on the given streams, it may configure
        both the operators in P1 and P0. If there is no stream for P1 operator,
        configuration happens only to P0's operator.

        Raises:
            CommandError: When the streams is empty.
        """
        if not streams:
            raise CommandError("The given stream is empty.")

        self._load_downloadable()

        p0_streams, p1_streams = self.get_streams_for_p0_p1(streams)
        self._configure_p0(p0_streams)
        if p1_streams:
            self._configure_p1(p1_streams)

    def start(self):
        """Start the probe.

        To use this method, stream(s) must be already configured. If the
        configured stream is only concerns P0 operator, this method will not
        start the operator in P1.

        Based on the configured stream(s), it makes the connection between the
        operators in P1 and P0 and start the operator(s).

        Raises:
            OperatorError: Fails to start operator(s).
        """
        if self._operator_p0 is None:
            raise CommandError("There is no operator to start.")

        if self.is_running():
            raise CommandError("The operator(s) still running.")

        try:
            if self._operator_p1:
                logger.debug("Connecting P1 operator to P0 operator.")
                stream_connect_id = self._firmware.connect_stream(
                    self._operator_p1.op_id, self._operator_p0.op_id
                )
                logger.debug("Stream ID: %d", stream_connect_id)

            logger.debug("Starting the P0 operator.")
            self._operator_p0.start()
            if self._operator_p1:
                logger.debug("Starting the P1 operator.")
                self._operator_p1.start()

        except OperatorError as error:
            logger.error(error)
            raise

    def stop(self):
        """Stop the probe."""
        if not self.is_running():
            raise CommandError("Operator(s) not running.")

        try:
            self._operator_p0.stop()
            if self._operator_p1:
                self._operator_p1.stop()
                self._firmware.disconnect_stream(
                    self._operator_p1.op_id, self._operator_p0.op_id
                )

        except (FirmwareError, OperatorError) as error:
            logger.error(error)
            raise

        finally:
            self._operator_p0 = None
            self._operator_p1 = None

            logger.info("KSP operator is being stopped.")
            try:
                self._unload_downloadable()
            except FirmwareError as error:
                logger.error(
                    "Cannot unload the downloadable. Error: {}".format(error)
                )
                raise

    def _configure_p0(self, streams):
        self._operator_p0 = KSPOperator(
            self._firmware, cap_id=self._cap_id, processor=0
        )
        logger.debug("Creating the operator in P0 and configure it.")
        try:
            self._operator_p0.create()
            self._operator_p0.config(streams)
        except (FirmwareError, OperatorError) as error:
            logger.error(error)
            raise

    def _configure_p1(self, streams):
        logger.debug("Creating the operator in P1 and configure it.")
        self._operator_p1 = KSPOperator(
            self._firmware, cap_id=self._cap_id, processor=1
        )
        try:
            self._operator_p1.create()
            self._operator_p1.config(streams)
        except (FirmwareError, OperatorError) as error:
            logger.error(error)
            raise

    def _unload_downloadable(self):
        # Check if the downloadable object is available, unload it.
        if self._downloadable:
            self._downloadable.unload()

        self._downloadable = None

    def _load_downloadable(self):
        if self._builtin_cap:
            self._cap_id = self.BUILTIN_CAP_ID
        else:
            self._cap_id = self.DOWNLOADABLE_CAP_ID
            self._downloadable = KSPDownloadable(
                self._firmware, edkcs=self._edkcs
            )
            self._downloadable.load()

    @staticmethod
    def get_streams_for_p0_p1(streams):
        """Generates separate streams for P0 and P1.

        Args:
            streams (dict): A dictionary of streams. The key is stream number
                and the value is the properties of the corresponding stream.

        Returns:
            tuple: (streams corresponding to P0, streams corresponding to P1).
                Both streams are in dictionary type.
        """
        p0_streams = {}
        p1_streams = {}
        for stream_number, stream in streams.items():
            processor = stream.get(ns.STREAMS_PROCESSOR, 0)
            if processor == 0:
                p0_streams[stream_number] = stream
            else:
                # P1's stream.
                p1_streams[stream_number] = stream

        logger.debug("P0 streams: %s", p0_streams)
        logger.debug("P1 streams: %s", p1_streams)
        return p0_streams, p1_streams

    def __del__(self):
        # Deleting the operators
        if self.is_running():
            self.stop()

        if hasattr(self, '_operator_p0'):
            del self._operator_p0
            self._operator_p0 = None
        if hasattr(self, '_operator_p1'):
            del self._operator_p1
            self._operator_p1 = None
