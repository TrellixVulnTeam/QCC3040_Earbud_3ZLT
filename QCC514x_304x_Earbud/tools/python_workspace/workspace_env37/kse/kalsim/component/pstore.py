#
# Copyright (c) 2018 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""PSKeys storage server"""

import logging
from functools import partial

from kats.framework.library.file_util import load
from kats.framework.library.log import log_input, log_output
from kats.kymera.kymera.generic.accmd import ACCMD_RESP_STATUS_OK, ACCMD_RESP_STATUS_CMD_FAILED, \
    ACCMD_CMD_ID_PS_READ_REQ, ACCMD_CMD_ID_PS_WRITE_REQ, ACCMD_CMD_ID_PS_DELETE_REQ, \
    ACCMD_CMD_ID_PS_READ_RESP, ACCMD_CMD_ID_PS_WRITE_RESP, ACCMD_CMD_ID_PS_DELETE_RESP
from kats.library.registry import get_instance

READ_DATA_MAX = 32


class PStore:
    """Persistent Store service class

    This class provides a persistent storage server to support kalsim firmware images.
    On instantiation it registers itself with firmware and listens for requests coming
    (read, write, delete).

    Initial storage state format::

        [
            [
                1,
                [
                    0,
                    1
                ]
            ],
            [
                2,
                [
                    4
                ]
            ]
        ]

    Containing two keys, key id 1 with value [0, 1] and key id 2 with value [4]

    Args:
        filename (str): Filename containing Store initial data
        register (bool): Register pstore against kymera
        delay (float): Time in seconds to delay response to kymera (in kymera/firmware domain)
    """

    def __init__(self, filename, register=True, delay=0):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log

        self._delay = delay
        self.__uut = get_instance('uut')
        self.__accmd = get_instance('accmd')
        self.__accmd.register_receive_callback(self._received_accmd)

        # load store initial state
        self.__data = {}
        if filename:
            data = load(filename)
            for entry in data:
                self.__data[entry[0]] = entry[1]

        if register:
            self.__accmd.ps_register()  # register as ps store

    @log_input(logging.DEBUG, formatters={'data': '0x%04x'})
    def _received_accmd(self, data):
        cmd_id, seq_no, payload = self.__accmd.receive(data)

        handlers = {
            ACCMD_CMD_ID_PS_READ_REQ: self._received_read_req,
            ACCMD_CMD_ID_PS_WRITE_REQ: self._received_write_req,
            ACCMD_CMD_ID_PS_DELETE_REQ: self._received_delete_req,
        }
        if cmd_id in handlers:
            handlers[cmd_id](seq_no, payload)

    def _delayed_send_resp(self, cmd_id, payload, sequence_num, timer_id):
        _ = timer_id
        self.__accmd.send(cmd_id, payload, sequence_num=sequence_num)

    def _send_resp(self, cmd_id, payload=None, sequence_num=None):
        if self._delay:
            self.__uut.timer_add_relative(self._delay,
                                          partial(self._delayed_send_resp,
                                                  cmd_id,
                                                  payload,
                                                  sequence_num))
        else:
            self.__accmd.send(cmd_id, payload, sequence_num=sequence_num)

    @log_input(logging.DEBUG, formatters={'payload': '0x%04x'})
    def _received_read_req(self, seq_no, payload):
        """Process a PS_READ_REQ received message containing

            24 bits key id (padded to 32 bits)
            16 bit offset

        Args:
            seq_no (int): Sequence number
            payload (list[int]): Message payload words
        """
        if len(payload) == 3:
            key_id = payload[0] | payload[1] << 16
            offset = payload[2]
            if key_id not in self.__data or len(self.__data[key_id]) < offset:
                self._log.warning('received PS_READ_REQ key_id:%s offset:%s data not available',
                                  key_id, offset)
                self._send_resp(ACCMD_CMD_ID_PS_READ_RESP, [ACCMD_RESP_STATUS_CMD_FAILED, 0],
                                seq_no)
            else:
                data = self.__data[key_id][offset:]
                total_length = len(self.__data[key_id])
                data = data if len(data) <= READ_DATA_MAX else data[:READ_DATA_MAX]
                self._log.info('received PS_READ_REQ key_id:%s offset:%s total_length:%s data:%s',
                               key_id, offset, total_length, str(data))
                self._send_resp(ACCMD_CMD_ID_PS_READ_RESP,
                                [ACCMD_RESP_STATUS_OK, total_length] + data,
                                seq_no)
        else:
            self._log.warning('received PS_READ_REQ length:%s invalid', len(payload))

    @log_input(logging.DEBUG, formatters={'payload': '0x%04x'})
    def _received_write_req(self, seq_no, payload):
        """Process a PS_WRITE_REQ received message containing

            24 bits key id (padded to 32 bits)
            16 bit total length (in words)
            16 bit offset
            data

        Args:
            seq_no (int): Sequence number
            payload (list[int]): Message payload words
        """
        if len(payload) > 4 and payload[2] >= (payload[3] + len(payload) - 4):
            key_id = payload[0] | payload[1] << 16
            total_length = payload[2]
            offset = payload[3]
            data = payload[4:]
            self._log.info('received PS_WRITE_REQ key_id:%s offset:%s data:%s',
                           key_id, offset, str(data))

            if offset == 0:
                self.__data[key_id] = [0] * total_length
            # TODO we could verify that len(data) doesn't go beyond total_length
            self.__data[key_id][offset:offset + len(data)] = data

            self._send_resp(ACCMD_CMD_ID_PS_WRITE_RESP, [ACCMD_RESP_STATUS_OK], seq_no)
        else:
            self._log.warning('received PS_WRITE_REQ length:%s invalid', len(payload))

    @log_input(logging.DEBUG, formatters={'payload': '0x%04x'})
    def _received_delete_req(self, seq_no, payload):
        """Process a PS_DELETE_REQ received message containing

            24 bits key id (padded to 32 bits)

        Args:
            seq_no (int): Sequence number
            payload (list[int]): Message payload words
        """
        if len(payload) == 2:
            key_id = payload[0] | payload[1] << 16
            self._log.info('received PS_DELETE_REQ key_id:%s', key_id)
            if key_id in self.__data:
                del self.__data[key_id]
            self._send_resp(ACCMD_CMD_ID_PS_DELETE_RESP, [ACCMD_RESP_STATUS_OK], seq_no)
        else:
            self._log.warning('received PS_DELETE_REQ length:%s invalid', len(payload))

    @log_output(logging.DEBUG, formatters={'return': '0x%04x'})
    def ps_read(self, key_id):
        """Read current storage key

        Args:
            key_id (int): Key identifier

        Returns:
            list[int]: Key value, empty list for inexistent key
        """
        return self.__data.get(key_id, [])
