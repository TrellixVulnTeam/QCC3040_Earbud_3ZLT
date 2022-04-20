#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Encapsulate PyDBG app's call methods."""
import logging

from ksp.lib.exceptions import FirmwareError
from ksp.lib.firmware.base import Firmware
from ksp.lib.logger import method_logger

logger = logging.getLogger(__name__)


class Kymera(Firmware):
    """Wraps around pydbg apps1 method to call the functions in the firmware.

    Args:
        apps1 (apps1 pydbg obj): apps1 pydbg object.
    """
    def _set_timeout(self, seconds):
        """Call the set_timeout built-in method.

        Help on method set_timeout in module csr.dev.fw.call:
        set_timeout(self, timeout) method of csr.wheels.bitsandbobs.Call_sub_0
            instance
        Set the timeout for all function calls through this object
        Args:
            seconds(int): Maximum time to wait for completion of calls.
        """
        try:
            self._connection.fw.call.set_timeout(seconds)
        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

    def _string_to_charptr(self, str_txt):
        """Creates a new variable for the given string.

        Create a Pydbg _variable object in target memory, containing
        the string as a C string (terminated with a 0 character).
        """
        try:
            chars = self._connection.fw.call.pnew("char", len(str_txt) + 1)
            for i in list(range(len(str_txt))):
                chars[i].value = ord(str_txt[i])
            chars[len(str_txt)].value = ord('\0')

        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

        return chars

    @method_logger(logger)
    def _free(self, mem_object):
        """Call the 'free' firmware function.

        Args:
            mem_object: int memory address or _Variable object such as
                obtained from pnew.
        """
        try:
            self._connection.fw.call.free(mem_object)
        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

    def _set_dm(self, start, end, words):
        """Set Data Memory.

        Args:
            start (int): Starting address.
            end (int): Ending address.
            words (list): List of integers.
        """
        try:
            self._connection.dm[start:end] = words
        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

    def _allocate(self, size, owner):
        """Allocate program memory.

        Args:
            size (int)
            owner (int)

        Returns:
            int: Memory address.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        try:
            return self._connection.fw.call.pmalloc_trace(
                size,
                owner
            )
        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

    @method_logger(logger)
    def find_file(self, start, name):
        """Find the file in the firmware.

        Args:
            start: int, starting file index, normally 1.
            name: str, file name.
        Returns:
            int: File index if found, 0 otherwise.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        name_str = self._string_to_charptr(name)
        try:
            file_index = self._connection.fw.call.FileFind(
                start, name_str, len(name_str) - 1)

        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

        finally:
            self._free(name_str)

        return file_index

    @method_logger(logger)
    def load_downloadable(self, index, download_type):
        """Loads the downloadable in the firmware.

        Args:
            index (int): The file index which is found using find_file
                method.
            download_type (int): Value to specify how capabilities in the bundle
                will be loaded. Following are the valid values and their
                function:

                0: Capability bundle downloaded for exclusive use in P0.
                   Trying to create an operator on P1 of capability downloaded
                   using this type will be unsuccessful.
                1: Capability bundle downloaded to P0’s PM RAM, but operators
                   for capabilities from the bundle can be created on P1.
                2: Capability bundle downloaded for exclusive use in P1. Trying
                   to create an operator on P0 of capability downloaded using
                   this type will be unsuccessful.
                3: Capability bundle downloaded to P1’s PM RAM, but operators
                   for capabilities from the bundle can be created on P0.

        Return:
            int: handle for the loaded downloadable.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        try:
            bdl = self._connection.fw.call.OperatorBundleLoad(
                index, download_type
            )
        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

        return bdl

    @method_logger(logger)
    def unload_downloadable(self, handle, timeout=10):
        """Unload the loaded downloadable.

        Args:
            handle: Handle which comes from load_downloadable.
            timeout (int): Time out in seconds.
        Return:
            bool: True if successful.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        try:
            self._set_timeout(timeout)
            success = self._connection.fw.call.OperatorBundleUnload(handle)

        except Exception as error:  # pylint: disable=broad-except
            raise FirmwareError(error)

        return success

    @method_logger(logger)
    def create_operator(self, cap_id, proc_num=None):
        """Creates the operator from the given capability ID.

        Args:
            cap_id (int): Capability ID.
            proc_num (int): optional, audio processor core number, 0..1,
                default None means processor 0.

        Return:
            int: Kymera Operator ID.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        op_id = 0
        if proc_num:
            # Key 2, proc 1
            # typedef struct
            # {
            #     uint16 key;         /*!< Key for OperatorCreate. */
            #     uint32 value;       /*!< Value for the key. */
            # } OperatorCreateKeys;
            #
            with KymeraBufferAddress(
                    self._connection, [2, 0, proc_num, 0], 8) as params:
                try:
                    op_id = self._connection.fw.call.OperatorCreate(
                        cap_id, 1, params)
                except Exception as error:  # pylint: disable=broad-except
                    raise FirmwareError(error)
        else:
            try:
                op_id = self._connection.fw.call.OperatorCreate(cap_id, 0, 0)
            except Exception as error:  # pylint: disable=broad-except
                raise FirmwareError(error)

        if not op_id:
            raise FirmwareError("Could no create the operator.")

        return op_id

    @method_logger(logger)
    def destroy_operator(self, op_id):
        """Destroy an operator.

        Call the OperatorDestroyMultiple trap with converted arguments:
        bool OperatorDestroyMultiple(uint16 n_ops, Operator * oplist,
                                     uint16 * success_ops)
        Args:
            op_id (int): Kymera operator ID.

        Return:
            bool: True if successful.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        response = False
        with KymeraBufferAddress(
                self._connection, [op_id], 2*len([op_id])) as op_ad:
            destroyed_ops = KymeraBufferAddress(self._connection, [], 2)
            with destroyed_ops:
                try:
                    response = self._connection.fw.call.OperatorDestroyMultiple(
                        1,
                        op_ad,
                        destroyed_ops.__enter__()
                    )
                except Exception as error:  # pylint: disable=broad-except
                    raise FirmwareError(error)

        return response

    @method_logger(logger)
    def start_operator(self, op_id):
        """Start an operator.

        Call the OperatorStartMultiple trap:
        bool OperatorStartMultiple(uint16 n_ops, Operator * oplist,
                                   uint16 * success_ops)
        The arguments are converted.
        Args:
            op_id (int): Kymera Operator ID.

        Returns:
            bool: True if successful.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        response = False
        with KymeraBufferAddress(
                self._connection, [op_id], 2*len([op_id])) as op_addr:
            try:
                response = self._connection.fw.call.OperatorStartMultiple(
                    1,
                    op_addr,
                    0
                )
            except Exception as error:  # pylint: disable=broad-except
                raise FirmwareError(error)

        return response

    @method_logger(logger)
    def stop_operator(self, op_id):
        """Stop the started operator.

        Args:
            op_id: Kymera operator ID.

        Returns:
            bool: True if successful, False otherwise.

        Raises:
            FirmwareError: Some unexpected happen in the firmware.
        """
        response = False
        with KymeraBufferAddress(
                self._connection, [op_id], 2*len([op_id])) as op_addr:
            try:
                response = self._connection.fw.call.OperatorStopMultiple(
                    1,
                    op_addr,
                    0
                )
            except Exception as error:  # pylint: disable=broad-except
                raise FirmwareError(error)

        return response

    def connect_stream(self, source_op, sink_op):
        """Connects a stream between two operators.

        Args:
            source_op (int): Source Operator ID.
            sink_op (int): Sink Operator ID.

        Returns:
            int: The connection stream ID.

        Raises:
            FirmwareError: If the connection fails to form.
        """
        source = source_op | 0x2000
        sink = sink_op | 0xa000
        try:
            stream_id = self._connection.fw.call.StreamConnect(source, sink)
        except Exception as error: # pylint: disable=broad-except
            raise FirmwareError(error)

        if not stream_id:
            raise FirmwareError("Connecting stream failed!")

        return stream_id

    def disconnect_stream(self, source_op, sink_op):
        """Disconnect the stream between two operators.

        Args:
            source_op (int): Source Operator ID.
            sink_op (int): Sink Operator ID.

        Raises:
            FirmwareError: If disconnecting the streams fail.
        """
        source = source_op | 0x2000
        sink = sink_op | 0xa000
        try:
            self._connection.fw.call.StreamDisconnect(source, sink)
        except Exception as error: # pylint: disable=broad-except
            raise FirmwareError(error)

    def operator_message(self, op_id, request, response_size=1):
        """Send the operator a message and return the response.

        Args:
            op_id (int): The ID of the operator.
            request (list): List of integers. Each integer must be a 16 bits
                wide value.
            response_size (int): Expected number of words in return.

        Return:
            list: List of integers.

        Raises:
            FirmwareError: If sending the message was not successful.
            FirmwareError: If result does not agree with the request.
            FirmwareError: If result is empty. The result should always return
                the request message ID.
        """
        if response_size < 1:
            raise ValueError("The response size should be bigger than 0.")

        apps_call = self._connection.fw.call

        result = []
        # Add 2 to the response_size to take header into account.
        with apps_call.create_local("uint16", len(request)) as buf_snd, \
                apps_call.create_local("uint16", response_size + 2) as buf_rcv:
            for (index, value) in enumerate(request):
                if 0 > value > 0xffff:
                    raise FirmwareError(
                        "Only 16 bits wide values should be passed in the "
                        "request. Tried to send {} instead.\n".format(
                            hex(value)
                        )
                    )

                buf_snd[index].value = value

            success = apps_call.OperatorMessage(
                op_id,
                buf_snd.address, buf_snd.size,
                buf_rcv.address, buf_rcv.size
            )
            if not success:
                raise FirmwareError(
                    "Failed to send a message to operator with id {}.".format(
                        hex(op_id)
                    )
                )

            # Removing the space for the header.
            result = [x.value for x in buf_rcv][:-2]

        if not result:
            raise FirmwareError(
                "The result is empty. The result should return the message ID."
            )
        if result[0] != request[0]:
            raise FirmwareError(
                "The response does not contain the message ID. Received "
                "response is {}".format(result)
            )
        return result


# pylint: disable=too-few-public-methods
class KymeraBufferAddress(Kymera):
    """A buffer address management for a ``unint16`` memory location in memory.

    It is a context manager class. During the instantiation it allocates
    buffers and sets values of the words in place. Once the user exits
    from the context, the buffer will be released.

    Args:
        connection (obj): apps1 pydbg object.
        address (Integral): The address in the apps1 memory.
        words (list): A list of words.
        size (int): Size of the buffer.
        owner (int): The owner identifier.
    """
    def __init__(self, connection, words, size, owner=0):
        super().__init__(connection)
        self._words = words
        self._size = size
        self._owner = owner

        self._address = None

    def __enter__(self):
        self._address = self._allocate(self._size, self._owner)
        for offset, word in enumerate(self._words):
            start = self._address + 2 * offset
            end = self._address + 2 * offset + 2
            self._set_dm(start, end, [word & 0xFF, word >> 8])

        return self._address

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._free(self._address)
