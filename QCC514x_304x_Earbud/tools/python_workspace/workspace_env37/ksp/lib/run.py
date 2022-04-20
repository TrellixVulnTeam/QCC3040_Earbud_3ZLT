############################################################################
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
############################################################################
"""
KSP common body of run method, encapsulated
"""
import logging
import os
import sys
import time
from datetime import datetime

import ksp.lib.namespace as ns
from ksp.lib.chips.factory import chip_factory
from ksp.lib.exceptions import OperatorError, CommandError
from ksp.lib.extract import extract
from ksp.lib.logger import method_logger

logger = logging.getLogger(__name__)


# pylint: disable=too-few-public-methods
class KymeraStreamProbeRun:
    """Kymera Stream Probe common run method.

    Args:
        device (object): A device instance from pydbg.
    """
    TIMESTAMP_FORMAT = '%Y%m%d-%H%M%S'
    OUTPUT_FORMAT_TIMESTAMP = '{filename}_{time_stamp}{extension}'
    OUTPUT_FORMAT = '{filename}{extension}'

    @method_logger(logger)
    def __init__(self, device):
        super(KymeraStreamProbeRun, self).__init__()
        self._device = device
        self._config = {}
        self._ksp_pydbg = None
        self._chip = chip_factory(device)
        self._wait_input = False
        self._duration = 0

    @method_logger(logger)
    def _extract(self, output_filename):
        extract(
            output_filename,
            self._config[ns.STREAMS]
        )

    @method_logger(logger)
    def run_configured(self):
        """Run end to end configured probe.

        Common sequence of actions for a recording run of KSP:
        start, wait, stop, extract and cleanup. Configuration
        must have been completed before calling this.

        Returns:
            bool: True if successful, False otherwise.
        """
        output_filename = self.get_output_filename()
        if not self._check_output_file_permission(output_filename):
            return False

        try:
            self._chip.start_probe(
                output_filename,
                self._config,
            )
        except (OperatorError, CommandError) as error:
            logger.error(error)
            return False

        if self._wait_input:
            print("\nPress Enter to stop the probe.")
            try:
                sys.stdin.read(1)
            except KeyboardInterrupt:
                print('\n')
                logger.warning("Reading is interrupted.")
                # The operation is cancelled. No further processing is required.
                self._chip.stop_probe()
                return False

        elif self._duration:
            logger.info("Record for %d seconds...", self._duration)
            time.sleep(self._duration)

        try:
            self._chip.stop_probe()
            self._extract(output_filename)

        except (OperatorError, CommandError) as error:
            logger.error(error)
            return False

        return True

    def start_op(self):
        """Start the KSP operator with the current configuration."""
        try:
            self._chip.start_op(self._config)
            return True

        except CommandError as error:
            logger.warning(error)
            return False

    def stop_op(self):
        """Stop the KSP operator."""
        try:
            self._chip.stop_op()
            return True

        except CommandError as error:
            logger.warning(error)
            return False

    def start_reader(self):
        """Start the Reader to get data from the chip."""
        self._chip.start_trb(self._config[ns.OUTPUT_FILENAME])
        print("\nPress Enter to stop the Reader.")

        try:
            sys.stdin.read(1)

        except KeyboardInterrupt:
            print('\n')
            logger.warning("Reading is interrupted.")

        finally:
            self._chip.stop_trb()

    def get_output_filename(self):
        """Get the output filename from the configuration.

        If user sets the configuration to add date-time timestamp to the output
        filename, this method will add the current date-time timestamp with
        the format of %Y%m%d-%H%M to the filename.

        As an example, if the filename is `foo.lrw`, the result would be
        something similar to `20210105-1100_foo.lrw`.

        In case, the user did not specify the extension for the output filename
        this method will add '.lrw' to end of the filename.
        """
        # Extract filename and the extension, if the filename doesn't have an
        # extension, add `.lrw` to the final name.
        filename, extension = os.path.splitext(self._config[ns.OUTPUT_FILENAME])
        if extension.lower() != '.lrw':
            filename = self._config[ns.OUTPUT_FILENAME].strip()
            extension = '.lrw'

        if self._config[ns.ADD_DATETIME_TO_OUTPUT_FILE]:
            # Add date and time stamp to the end of the filename.
            time_stamp = datetime.now().strftime(self.TIMESTAMP_FORMAT)
            return self.OUTPUT_FORMAT_TIMESTAMP.format(
                filename=filename, time_stamp=time_stamp, extension=extension
            )

        # User has requested not to add timestamp to the output filename.
        return self.OUTPUT_FORMAT.format(
            filename=filename, extension=extension
        )

    @staticmethod
    def _check_output_file_permission(path):
        try:
            with open(path, 'w') as handler:
                handler.write('write test.')

            os.remove(path)
            return True
        except OSError as error:
            logger.error(
                "Cannot create the output file in the given location. "
                "Error: %s", error
            )

        return False
