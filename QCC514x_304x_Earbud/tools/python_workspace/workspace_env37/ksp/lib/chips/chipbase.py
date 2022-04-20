#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Boilerplate for various chip interfaces."""
import logging
import threading

import ksp.lib.namespace as ns
from ksp.lib.exceptions import OperatorError, FirmwareError, CommandError
from ksp.lib.logger import method_logger
from ksp.lib.firmware.operator.ksp_operator_manager import KSPOperatorManager
from ksp.lib.firmware.kymera import Kymera
from ksp.lib.trb import Ksp

logger = logging.getLogger(__name__)
TRB_NUMBER_OF_TRANSACTIONS = 100


# The number of instance attributes related to `GenericChip` class is
# expected and necessary for a boiler plate.
# pylint: disable=too-many-instance-attributes
class GenericChip(object):
    """A boilerplate class for chips implementations.

    Args:
        device (object): Device instance object comes from pydbg library.
    """
    IS_EDKCS = True

    @method_logger(logger)
    def __init__(self, device):
        self._dongle_name = device.transport.dongle_name
        self._dongle_id = device.transport.trb.get_dongle_details().id

        _, apps1 = (
            device.chip.apps_subsystem.p0,
            device.chip.apps_subsystem.p1
        )
        self._firmware = Kymera(apps1)

        self._read_stop_event = threading.Event()
        self._read_data_process = None

        self._op_manager = None
        self._ksp_operator = None
        self._ksp_trb = None

    @method_logger(logger)
    def start_probe(self, output_filename, config):
        """Starts the probe based on the given configurations.

        Args:
            output_filename (str): A filename that output which KSP will
                write into it.
            config (dict): A dictionary configuration which the keys are
                exactly the same as KymeraStreamProbeCLI's configuration.

        Raises:
            CommandError: When there is a probe running.
        """
        self._reset()

        self._init_ksp_trb()
        self._init_operator(config)

        self._start_ksp_trb(output_filename)
        try:
            self._op_manager.start()

        except OperatorError:
            # Communication to the KSP cap is failed. Stop the reader.
            self._stop_ksp_trb()
            raise

    @method_logger(logger)
    def stop_probe(self):
        """Stops the running probe.

        Raises:
            CommandError: When the probe is already stopped.
        """
        try:
            self._stop_ksp_op()
            self._stop_ksp_trb()

        except CommandError:
            raise CommandError("The probe is already stopped.")
        except FirmwareError as error:
            raise CommandError(
                "Cannot stop the probe gracefully. The application may need "
                " to restart. Error: {}".format(error)
            )

        finally:
            self._reset()

    @method_logger(logger)
    def start_op(self, config):
        """Configure and start the KSP operator.

        Args:
            config (dict): Stream configurations.
        Raises:
            CommandError: When the operator is already running.
            CommandError: Something goes wrong when configuring the
                operator.
        """
        if self._op_manager:
            raise CommandError("KSP is already being configured.")

        try:
            self._init_operator(config)
            self._op_manager.start()

        except CommandError as error:
            logger.warning(error)
            raise CommandError("Unable to start the KSP operator.")

        logger.info("KSP operator is successfully being setup and running.")

    @method_logger(logger)
    def stop_op(self):
        """Stop and unload the ksp downloadable operator.

        Raises:
            CommandError: Something goes wrong when stopping the operator.
        """
        try:
            self._op_manager.stop()
            self._op_manager = None

        except FirmwareError as error:
            logger.warning(error)
            raise CommandError("Unable to stop the KSP operator")

    @method_logger(logger)
    def start_trb(self, output_filename):
        """Start the trb link to save the data flowing in.

        Args:
            output_filename (str): The filename and the location of where
                the received data should be saved.
        """
        self._init_ksp_trb()
        self._start_ksp_trb(output_filename)

    @method_logger(logger)
    def stop_trb(self):
        """Stop the trb link to the chip."""
        try:
            self._stop_ksp_trb()

        except CommandError:
            raise CommandError("The probe is already stopped.")

    @method_logger(logger)
    def _is_op_running(self):
        """Checks whether the KSP operator is running.

        Returns:
            bool: True if the operator is running, False otherwise.
        """
        if self._op_manager and self._op_manager.is_running():
            return True

        return False

    @method_logger(logger)
    def _is_trb_running(self):
        """Checks whether the TRB is running.

        Returns:
            bool: True if the TRB is running, False otherwise.
        """
        if self._read_data_process and self._read_data_process.is_alive():
            return True

        return False

    @method_logger(logger)
    def _start_ksp_trb(self, output_filename):
        self._read_data_process = threading.Thread(
            target=self._ksp_trb.read_data,
            args=(
                output_filename,
                self._read_stop_event,
            ),
            kwargs={'verbose': True}
        )

        self._read_data_process.start()

    @method_logger(logger)
    def _stop_ksp_trb(self):
        if self._ksp_trb is None:
            raise CommandError("KSP TRB is not running.")

        if self._read_data_process:
            self._read_stop_event.set()
            self._read_data_process.join()

            self._read_stop_event.clear()
            self._read_data_process = None

        self._ksp_trb = None

    @method_logger(logger)
    def _stop_ksp_op(self):
        if self._op_manager is None:
            raise CommandError("KSP operator is not running.")

        self._op_manager.stop()
        # By deleting the Operator Manager, the downloadable will be unloaded.
        del self._op_manager
        self._op_manager = None

    @method_logger(logger)
    def _reset(self):
        if self._is_op_running():
            self._stop_ksp_op()

        if self._is_trb_running():
            self._stop_ksp_trb()

    @method_logger(logger)
    def _init_ksp_trb(self):
        self._ksp_trb = Ksp(
            device=self._dongle_name,
            device_id=self._dongle_id,
            num_transactions=TRB_NUMBER_OF_TRANSACTIONS,
        )

    @method_logger(logger)
    def _init_operator(self, config):
        try:
            self._op_manager = KSPOperatorManager(
                self._firmware,
                edkcs=self.IS_EDKCS,
                builtin_cap=config[ns.USE_BUILTIN_CAP]
            )
        except FirmwareError as error:
            raise CommandError(error)

        self._op_manager.config(config[ns.STREAMS])
