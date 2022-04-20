#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Interact with the KSP operator in the firmware."""
import logging

from ksp.lib import namespace as ns
from ksp.lib.data_types import KspDataType
from ksp.lib.exceptions import CommandError, OperatorError, FirmwareError
from ksp.lib.firmware.operator.convertors import stream_to_words
from ksp.lib.logger import method_logger
from ksp.lib.types import TransformIDList

logger = logging.getLogger(__name__)


class KSPOperator(object):
    """Manage the KSP operator.

    Args:
        firmware (Firmware obj): Firmware instance.
        cap_id (int): capability ID.
        processor (int): Select the processor 0 or 1.
    """
    VERSION_REQUEST_ID = 0x1000

    @method_logger(logger)
    def __init__(self, firmware, cap_id, processor=0):
        self._firmware = firmware
        self._cap_id = cap_id
        self._processor = processor

        self.op_id = 0
        # self._op_id2 = 0
        self._running = False
        self._number_of_active_streams = 0
        self._streams = {}

    @property
    def version(self):
        """KSP Capability Version property."""
        if self.op_id == 0:
            raise CommandError("The KSP operator is not created.")

        try:
            # The number of words for the response is set to 5. 3 for
            # (Request ID, Major Version, Minor Version) and 2 for the safety
            # in apps1. Setting the exact 3 would panic the apps1.
            res = self._firmware.operator_message(
                self.op_id,
                [self.VERSION_REQUEST_ID],
                5
            )

            # Remove the Request ID and convert the rest to tuple.
            return '{major}.{minor}'.format(major=res[1], minor=res[2])

        except FirmwareError as error:
            raise CommandError(error)

    def is_running(self):
        """Returns the status of the operator.

        Returns:
            bool: True if it's running, False otherwise.
        """
        return self._running

    @method_logger(logger)
    def config(self, streams):
        """Configures a stream.

        Args:
            streams (dict): dictionary of streams.

        Raises:
            FirmwareError: If the configuration did not happen.
        """
        for stream_number, stream in streams.items():
            data_type = KspDataType[stream[ns.STREAMS_DATA_TYPE]]
            samples = stream.get(ns.STREAMS_SAMPLES, 0)
            # No use at the moment set it to 0 always.
            channel_info = 0
            metadata = stream.get(ns.STREAMS_METADATA)
            channels = TransformIDList(stream[ns.STREAMS_TRANSFORM_IDS])
            buffer_size = stream.get(ns.STREAMS_BUFFER_SIZE, 0)
            timed_data = stream.get(ns.STREAMS_TIMED_DATA, '')

            words_to_send = stream_to_words(
                stream_number,
                samples,
                channel_info,
                data_type,
                channels,
                metadata,
                timed_data
            )

            try:
                self._set_buffer_size(stream_number, buffer_size)
                self._firmware.operator_message(self.op_id, words_to_send)

            except FirmwareError as error:
                raise OperatorError(
                    "Configuring stream %s has failed. Error: %s" % (
                        stream_number,
                        error
                    )
                )

            self._number_of_active_streams += 1

    @method_logger(logger)
    def __del__(self):
        if self._running:
            try:
                self.stop()
            except FirmwareError as error:
                logger.error(error)

        if self.op_id != 0:
            try:
                self.destroy()
            except OperatorError as error:
                logger.error(error)

    @method_logger(logger)
    def create(self):
        """Create KSP the Operator.

        Raises:
            FirmwareError: When the creation fails.
        """
        if self.op_id != 0:
            logger.warning("Can't create the operator. It already exists.")
            return

        try:
            self.op_id = self._firmware.create_operator(
                self._cap_id,
                self._processor
            )

            logger.info(
                "KSP Operator is created. ID=%s, Version=%s",
                hex(self.op_id),
                self.version
            )

        except FirmwareError as error:
            self.op_id = 0
            raise OperatorError(
                "Creating the KSP operator is failed with '%s'." % error
            )

        if self.op_id == 0:
            raise OperatorError("Creating the KSP operator is failed.")

    @method_logger(logger)
    def destroy(self):
        """Destroy the KSP Operator.

        Raises:
            OperatorError: When destroying the operator fails.
        """
        if self.op_id == 0:
            # Quietly return.
            assert self._number_of_active_streams == 0
            assert not self._running
            return

        if self._running:
            raise OperatorError("Cannot destroy the running operator.")

        response = self._firmware.destroy_operator(self.op_id)
        if response == 0:
            raise OperatorError("The operator is not destroyed.")

        # Destroyed!
        self.op_id = 0
        self._number_of_active_streams = 0

    def _set_buffer_size(self, stream_number, buffer_size):
        if self.version < '1.3' and buffer_size != 0:
            logger.warning(
                "Internal buffer cannot be set with this version of the "
                "capability. The entered value is ignored."
            )
            return
        if self.version < '1.3':
            # The buffer size is set to 0 but the capability cannot support
            # the operator message. Nothing to do here.
            return

        try:
            self._firmware.operator_message(
                self.op_id,
                [
                    0x2023,
                    stream_number,
                    buffer_size
                ]
            )
            if buffer_size == 0:
                logger.info(
                    "Buffer size for stream %d is set to default.",
                    stream_number
                )
            else:
                logger.info(
                    "Buffer size for stream %d is set to %u",
                    stream_number,
                    buffer_size
                )

        except FirmwareError:
            logger.warning("Cannot set the internal buffer size.")
            raise

    @method_logger(logger)
    def start(self):
        """Start the KSP operator."""
        if not self.op_id:
            raise CommandError("KSP Operator is not created.")
        if self._running:
            raise CommandError("KSP Operator is already running.")

        response = self._firmware.start_operator(self.op_id)

        if response == 0:
            raise OperatorError(
                "Starting KSP operator failed with %s return id." % response
            )

        self._running = True

    @method_logger(logger)
    def stop(self):
        """Stop the KSP operator."""
        ret = self._firmware.stop_operator(self.op_id)
        if ret == 0:
            raise OperatorError(
                "Stopping KSP operator failed with %s return id." % ret
            )

        self._running = False
