#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Module for capturing KSP packets sent by audio subsystem via TRB."""
# B-299423: This module needs refactoring.
import logging
import sys
import time

from trbtrans.trbtrans import Trb, TrbErrorBridgeLinkIsDown

from ksp.lib.logger import method_logger, function_logger

logger = logging.getLogger(__name__)


class Ksp(Trb):
    """Reads KSP packets received from transaction bridge.

    Args:
        device (str): trb device name.
        num_transactions (int): number of transaction to poll each time.
        wait_time (float): Timeout for waiting for data in seconds. To
            make it more precise, fraction of seconds is also possible.
    """

    # pylint: disable=too-many-arguments
    @method_logger(logger)
    def __init__(self, device="scar",
                 device_id=None,
                 num_transactions=100,
                 wait_time=None):
        Trb.__init__(self)
        self.sample_stream_open(device, dongle_id=device_id)
        logger.info(
            "Connected to device:%s:%s.",
            device,
            0 if device_id is None else device_id
        )
        self.num_transactions = num_transactions
        self._wait_time = wait_time

    def read_data(self, file_name, stop_event, verbose=False):
        """Reads raw data into file.

        Args:
            file_name (str): output file.
            stop_event (threading.Event): if true prints the number of bytes
                received so far. This is used when the reader used in a thread.
            verbose (bool): if true prints the number of bytes received so far.
        """
        # Read raw transactions.
        total_bytes = self._read_raw_transactions_to_file(
            file_name,
            stop_event,
            src_block_id=0x7,
            src_subsys_id=0x3,
            log_other_messages=False,
            verbose=verbose
        )
        return total_bytes

    # pylint: disable=too-many-statements,too-many-branches,too-many-locals,too-many-arguments
    @method_logger(logger)
    def _read_raw_transactions_to_file(self,
                                       file_name,
                                       stop_event,
                                       src_block_id=0x7,
                                       dest_subsys_id=None,
                                       opcode=None,
                                       src_subsys_id=0x3,
                                       log_other_messages=False,
                                       verbose=False):
        """Reads raw transactions, filter and write them to file.

        Args:
            file_name (str): file name for recording raw data.
            stop_event (threading.Event): if true prints the number of bytes
                received so far. This is used when the reader used in a thread.
            src_block_id (int): expected source block id, set to None to accept
                all.
            dest_subsys_id (int): destination subsystem id, set to None to
                accept all.
            opcode (int): opcode of received transaction, set to None to accept
                all.
            src_subsys_id (int): source block id, set to None to accept all.
            log_other_messages (bool): Log messages which do not match the
                src_block_id.
            verbose (bool): if true prints the number of bytes received so far.

        Returns:
            int: Number of bytes written to the output file.
        """
        def_time_out_ms = 20
        total_bytes = 0
        total_bytes_rem = 0

        last_read_time = time.time()

        # Open output file.
        try:
            fid = open(file_name, "wb")
        except IOError:
            logger.error("Cannot open file %s.", file_name)
            return total_bytes

        logger.info("TRB link is established.")
        while True:
            if stop_event.is_set():
                break

            # Read transactions.
            try:
                results, count, is_wrapped = self.read_raw_transactions(
                    self.num_transactions,
                    def_time_out_ms)
            except TrbErrorBridgeLinkIsDown as error:
                logger.error(error)
                break

            # If no transaction received update timeout.
            wait_time = time.time() - last_read_time
            if count == 0 and self._wait_time and wait_time > self._wait_time:
                # Wait time is defined and we passed that without
                # receiving any data. Stop the read immediately.
                logger.warning(
                    (
                        "Stopping the TRB link as it was idle for more "
                        "than %s seconds."
                    ),
                    self._wait_time
                )
                break
            if count == 0:
                continue

            # There is something to read. Record the time to reset the
            # idle time.
            last_read_time = time.time()

            # If wrapped, links become unreliable, so exit.
            if is_wrapped:
                raise RuntimeError(
                    "TRB driver shows wrapping, read data isn't valid. Reset "
                    "the device."
                )

            # Filter received messages.
            for msg in results:
                # See if NULL transaction.
                if msg.timestamp == 0 and \
                        msg.src_block_id_and_dest_subsys_id == 0 and \
                        msg.opcode_and_src_subsys_id == 0 and \
                        msg.payload[:] == [0] * len(msg.payload[:]):
                    continue

                # Check src_block_id.
                if src_block_id is not None:
                    m_src_block_id = msg.src_block_id_and_dest_subsys_id >> 4
                    if src_block_id != m_src_block_id:
                        if log_other_messages:
                            logger.warning(
                                "src_block_id not matching: %s.", total_bytes
                            )
                            logger.debug("msg=%s", _extract_msg_content(msg))

                        continue

                # Check dest_subsys_id.
                if dest_subsys_id is not None:
                    m_dest_subsys_id = msg.src_block_id_and_dest_subsys_id & 0xf
                    if m_dest_subsys_id != dest_subsys_id:
                        if log_other_messages:
                            logger.warning(
                                "dest_subsys_id not matching: %s", total_bytes
                            )
                            logger.debug("msg=%s", _extract_msg_content(msg))

                        continue

                # Read opcode.
                if opcode is not None:
                    m_opcode = msg.opcode_and_src_subsys_id >> 4
                    if m_opcode != opcode:
                        if log_other_messages:
                            logger.warning(
                                "opcode not matching: %s.",
                                total_bytes
                            )
                            logger.debug("msg=%s", _extract_msg_content(msg))

                        continue

                # Check src_subsys_id.
                if src_subsys_id is not None:
                    m_src_subsys_id = msg.opcode_and_src_subsys_id & 0xf
                    if m_src_subsys_id != src_subsys_id:
                        if log_other_messages:
                            logger.warning(
                                "src_subsys_id not matching: %s",
                                m_src_subsys_id
                            )
                            logger.debug("msg=%s", _extract_msg_content(msg))
                        continue

                # Check flags, specific to KSP operator.
                if (msg.payload[0] & 0x3F) != 0x2C:
                    if log_other_messages:
                        logger.warning(
                            "flags not matching: 0x{0:x}".format(msg.payload[0])
                        )
                        logger.debug("msg=%s", _extract_msg_content(msg))
                    continue

                # Write payload to file.
                payload = bytearray(msg.payload[1:])
                fid.write(payload)
                total_bytes += len(payload)

                # Print number of bytes received in verbose mode.
                if verbose:
                    total_bytes_rem += len(payload)
                    if total_bytes_rem > (1 << 14):
                        total_bytes_rem -= (1 << 14)
                        sys.stdout.write(
                            "\rBytes written to the output file: {0}".format(
                                total_bytes
                            )
                        )
                        sys.stdout.flush()

        if verbose:
            sys.stdout.write('\r')

        fid.close()

        return total_bytes


@function_logger(logger)
def _extract_msg_content(msg):
    """Returns the content of a TRB message.

    Args:
        msg (TRB message): a received TRB message.

    Returns:
        str: The TRB message content.
    """
    m_src_block_id = msg.src_block_id_and_dest_subsys_id >> 4
    m_dest_subsys_id = msg.src_block_id_and_dest_subsys_id & 0xf
    m_opcode = msg.opcode_and_src_subsys_id >> 4
    m_src_subsys_id = msg.opcode_and_src_subsys_id & 0xf
    payload = msg.payload[:]

    msg_content = "src_block_id={0},".format(m_src_block_id)
    msg_content += "dest_subsys_id={0},".format(m_dest_subsys_id)
    msg_content += "opcode={0},".format(m_opcode)
    msg_content += "src_subsys_id={0},".format(m_src_subsys_id)
    msg_content += " ".join(["{0:02x}".format(val) for val in payload])

    return msg_content
