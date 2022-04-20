############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Generate raw TRB debug transactions using trbtrans
"""

from csr.wheels import gstrm
from csr.transport.trbtrans import Trb, TrbErrorDriverIOTimeout, \
                                        TrbErrorDriverBufferWrapped,\
                                        TrbOpcodes
from csr.wheels.bitsandbobs import byte_reverse_dword, bytes_to_dwords, \
                                        bytes_to_dwords_be, dwords_to_bytes_be,\
                                        chunks
from csr.transport.trbtrans import Transaction
from csr.transport.trbtrans import TrbAction, TrbActionType
from ctypes import c_ubyte
import re
import sys
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.model import interface

class ExtOpcodes:
    """
    The TBUS debug extension opcodes that we care about (see CS-129670-SP)
    """
    DEBUG_WRITE = 0
    DEBUG_WRITE_REQ = 1
    DEBUG_WRITE_RSP = 2
    DEBUG_READ_REQ = 3
    DEBUG_READ_RSP = 4
    
class TrbRawTransaction:
    '''
    fields property is a dictionary of bit position and bit length of the
    fields within a raw transaction. These come from the chip register 
    documentation.
    e.g. file://///root.pri/FILEROOT/UnixHomes/home/csra68100/emulator_releases/dev/csra68100_partial_emu_1_140801_10/digital_results/system_bus/html/main.html
    '''
    fields = {
               "opcode":    (92, 4),
               "src_id":    (88, 4),
               "src_block": (84, 4),
               "dest_id":   (80, 4),
               "dest_block":(76, 4),
               "tag":       (72, 4),
               "ext_op":    (68, 4)
               }
    
    opcode_names = {
                0     :"IDLE",    
                1     :"VM_LOOKUP_REQ",  
                2     :"VM_LOOKUP_RESP",
                3     :"VM_WRITEBACK", 
                4     :"DATA_WRITE",
                5     :"DATA_WRITE_REQ",
                6     :"DATA_WRITE_RESP",
                7     :"DATA_READ_REQ",  
                8     :"DATA_READ_RESP",
                9     :"EXTENDED",    
                10   :"SAMPLE",
                11   :"DEBUG",
                15   :"DEEP_SLEEP_SIGNAL", # Not actually a transaction
                # This is a deep sleep signal that the driver has picked up as
                # an event (interrupt) and put into a log as if it was a
                # transaction but using an otherwise unsed OpCode
                
                "9.0"  :"INTERRUPT_EVENT",
                "9.1"  :"TIME_SYNC",
                "9.2"  :"TIME_SYNC_RESP",
                "9.3"  :"MESSAGE",
                "9.4"  :"MESSAGE_RESP",
                "9.5"  :"SYS_RESET",
                "9.6"  :"HIT_BREAKPOINT",
                "9.7"  :"COEXISTENCE",
                "9.8"  :"RESERVATION",
                "9.9"  :"SAMPLE_INFO",
                "9.10" :"PING",                 
                "11.0" :"DEBUG_WRITE", 
                "11.1" :"DEBUG_WRITE_REQ", 
                "11.2" :"DEBUG_WRITE_RESP", 
                "11.3" :"DEBUG_READ_REQ", 
                "11.4" :"DEBUG_READ_RESP", 
                "11.5" :"DROP_NOTIFICATION"    
                    }
    
    opcode_codes = {v:k for k,v in opcode_names.items()}
    
    DeepSleepSignals = { 1 : "Deep Sleep Entry",
                         2 : "Deep Sleep Exit",
                         3 : "Deep Sleep Entry+Exit",
                         4 : "Deep Sleep Wake"}
    
TRB_STATUS_CODES = {0 : "Success",
                    1 : "Subsystem power off",
                    2 : "Subsystem asleep",
                    3 : "Routing error",
                    4 : "Lock Error",
                    7 : "Unmapped Buffer",
                    8 : "Mapping pages for read",
                    9 : "Out of free pages",
                    10 : "Debug Timeout",
                    11 : "Access protection",
                    12: "No memory here",
                    13: "Wrong length",
                    14: "Not writable",
                    15: "Bad alignment"}

class TrbExtendedOpcode(object):
    INTERRUPT_EVENT = 0
    TIME_SYNC = 1
    TIME_SYNC_RESP = 2
    MESSAGE = 3
    MESSAGE_RESP = 4
    SYS_RESET = 5
    HIT_BREAKPOINT = 6
    COEXISTENCE = 7
    RESERVATION = 8
    SAMPLE_INFO = 9
    PING = 10
    
class TrbStatusException(RuntimeError):
    """
    Exception indicating a non-zero return status from a transaction
    """
    def __init__(self, msg, id):
        
        RuntimeError.__init__(self, msg)
        self.id = id

    @property
    def id_string(self):
        return TRB_STATUS_CODES[self.id]
    
    def __str__(self):
        return self.message + " (status = 0x%x: %s)" % (self.id, self.id_string)


class TrbRaw(object):
    """
    Class that wraps around a TRB providing a simple API for selected raw TRB 
    transactions
    """
    
    def __init__(self, trb):
        """
        A TrbRaw needs a TrbTransConnection object to talk to 
        """
        from csr.dev.framework.connection.trb import TrbTransConnection, \
                                                 QSTrbTransConnection
        if isinstance(trb, TrbTransConnection):
            # clone the received Trb() instance so that this object can be used
            # in a separate thread from the main Pydbg sessions.
            #self._trb = trb.trb.clone()
            self._trb = trb.trb # let's not - it stops device.reset working
        elif isinstance(trb, Trb):
            self._trb = trb
        else:
            raise TypeError("TrbRaw: expected TrbTransConnection "
                            "- got '%s'" % type(trb))
        
        self._bridge_id = self._trb.get_bridge_subsys_id()

        from . import trbtrans
        self.TransactionToDriver = trbtrans.Transaction

    def debug_read(self, subsys, address, width, blockId=0):
        """
        Do a single debug read of the given width (must be appropriate to the
        subsystem) at a given address in a given subsystem 
        """
        return self.debug_read_rpt(subsys, address, width, 1,
                                       False, dst_blockId=blockId)[0]
    
    def debug_read_with_timestamp(self, subsys, address, width):
        """
        Do a single debug read of the given width (must be appropriate to the
        subsystem) at a given address in a given subsystem, return a timestamp
        along with the read value
        """
        return self.debug_read_rpt(subsys, address, width, 1,
                                       True)[0]
    
    def debug_repeated_read_with_timestamps(self, subsys, address, 
                                               width, num_rpt):
        """
        Repeat a single debug read of the given width (must be appropriate to the
        subsystem) at a given address in a given subsystem, return timestamps
        along with the read values
        """
        return self.debug_read_rpt(subsys, address, width, num_rpt,
                                       True)

    def interrupt_transaction(self, src_ssid, src_bl_id_dest_subsys, dest_bl_id_tag, status):
        """
        Return a single Bus Interrupt transaction to be sent on the Transaction
        Bus
        """
        txn = self.TransactionToDriver()
        txn.timestamp = 0
        txn.opcode_and_src_subsys_id = (TrbOpcodes.OPCODE_EXTENDED << 4) | src_ssid
        txn.src_block_id_and_dest_subsys_id = src_bl_id_dest_subsys
        txn.dest_block_id_and_tag = dest_bl_id_tag << 4
        txn.payload[0] = (TrbExtendedOpcode.INTERRUPT_EVENT << 4) | (status >> 12)
        txn.payload[1] = (status >> 4) & 0xff
        txn.payload[2] = (status << 4) & 0xff
        return txn

    def message_transaction(self, src_ssid, src_bl_id_dest_subsys, dest_bl_id_tag, message, msg_len):
        """
        Return a single Bus Message transaction to be sent on the Transaction
        Bus. message is expected to be a uint16 array containing the payload,
        msg_len is the message length in octets
        """
        data = [0, 0, 0, 0]
        if (msg_len > 0):
            data[3] = message[3]
        if (msg_len > 2):
            data[2] = message[2]
        if (msg_len > 4):
            data[1] = message[1]
        if (msg_len > 6):
            data[0] = message[0]
        
        txn = self.TransactionToDriver()
        txn.timestamp = 0
        txn.opcode_and_src_subsys_id = (TrbOpcodes.OPCODE_EXTENDED << 4) | src_ssid
        txn.src_block_id_and_dest_subsys_id = src_bl_id_dest_subsys
        txn.dest_block_id_and_tag = dest_bl_id_tag
        txn.payload[0] = (TrbExtendedOpcode.MESSAGE << 4)
        txn.payload[1] = (data[3] & 0xff00)  >> 8
        txn.payload[2] = (data[3] & 0xff)
        txn.payload[3] = (data[2] & 0xff00)  >> 8
        txn.payload[4] = (data[2] & 0xff)
        txn.payload[5] = (data[1] & 0xff00)  >> 8
        txn.payload[6] = (data[1] & 0xff)
        txn.payload[7] = (data[0] & 0xff00)  >> 8
        txn.payload[8] = (data[0] & 0xff)
        return txn

    def vm_lookup_transaction(self, src_ssid, src_bl_id_dest_subsys, dest_bl_id_tag, buf_handle, rw_flag, offset_only):
        """
        Return a single VM Lookup transaction to be sent on the Transaction Bus
        """
        txn = self.TransactionToDriver()
        txn.timestamp = 0
        txn.opcode_and_src_subsys_id = (TrbOpcodes.OPCODE_VM_LOOKUP_REQUEST << 4) | src_ssid
        txn.src_block_id_and_dest_subsys_id = src_bl_id_dest_subsys
        txn.dest_block_id_and_tag = dest_bl_id_tag
        txn.payload[0] = (buf_handle & 0xFF) >> 1
        txn.payload[1] = (buf_handle & 0x01) << 7
        txn.payload[1] |= (rw_flag & 0x01) << 6
        txn.payload[1] |= (offset_only & 0x01) << 5
        return txn

    def vm_offset_lookup_transaction(self, src_ssid, src_bl_id_dest_subsys, dest_bl_id_tag, buf_handle, rw_flag, buf_offset):
        """
        Return a single VM Offset Lookup transaction to be sent on the
        Transaction Bus
        """
        txn = self.TransactionToDriver()
        txn.timestamp = 0
        txn.opcode_and_src_subsys_id = (TrbOpcodes.OPCODE_VM_LOOKUP_REQUEST << 4) | src_ssid
        txn.src_block_id_and_dest_subsys_id = src_bl_id_dest_subsys
        txn.dest_block_id_and_tag = dest_bl_id_tag
        txn.payload[0] = 0x80 + ((buf_handle & 0xFF) >> 1)
        txn.payload[1] = (buf_handle & 0x01) << 7
        txn.payload[1] |= (rw_flag & 0x01) << 6
        txn.payload[1] |= (buf_offset & 0x3F000) >> 12
        txn.payload[2] |= (buf_offset & 0xFF0) >> 4
        txn.payload[3] |= (buf_offset & 0xF) << 4
        return txn

    def write_req_transaction(self, src_ssid, src_bl_id_dest_subsys, address, width, data):
        """
        Return a single data write transaction of the given width (must be
        appropriate to the subsystem) at a given address in a given subsystem.
        Data should be a single integer.
        """
        txn = self.TransactionToDriver()
        txn.timestamp = 0
        txn.opcode_and_src_subsys_id = TrbOpcodes.OPCODE_DATA_WRITE_REQUEST << 4 | src_ssid
        txn.src_block_id_and_dest_subsys_id = src_bl_id_dest_subsys
        txn.dest_block_id_and_tag = 0
        txn.payload[3] = (address & 0xff)
        txn.payload[2] = (address & 0xff00)     >> 8
        txn.payload[1] = (address & 0xff0000)   >> 16
        txn.payload[0] = (address & 0xff000000) >> 24
        txn.payload[4] = (width - 1) << 5
        txn.payload[8] = (data & 0xff)
        txn.payload[7] = (data & 0xff00)     >> 8
        txn.payload[6] = (data & 0xff0000)   >> 16
        txn.payload[5] = (data & 0xff000000) >> 24        
        return txn    

    def read_req_transaction(self, src_ssid, src_bl_id_dest_subsys, dest_bl_id_tag, address, width):    
        """
        Return a single read transaction of the given width (must be
        appropriate to the subsystem) at a given address in a given subsystem
        """
        txn = self.TransactionToDriver()
        txn.timestamp = 0
        txn.src_block_id_and_dest_subsys_id = src_bl_id_dest_subsys
        txn.dest_block_id_and_tag = dest_bl_id_tag
        opcode = TrbRawTransaction.opcode_codes["DATA_READ_REQ"]
        txn.opcode_and_src_subsys_id = opcode << 4 | self._bridge_id
        txn.payload[0] = (address & 0xff000000) >> 24
        txn.payload[1] = (address & 0xff0000)   >> 16
        txn.payload[2] = (address & 0xff00)     >> 8
        txn.payload[3] = (address & 0xff)
        txn.payload[4] = (width-1) << 5
        return txn
                
    def data_write(self, subsys, address, width, data, write_only=True):
        """
        Do a single data write request of the given width (must be appropriate to the
        subsystem) at a given address in a given subsystem.  data should be a
        single integer.  If write_only is True, no attempt is made to read the
        response or the transaction status.
        """
        
        txn = self.write_req_transaction(self._bridge_id, subsys, address, width, data)
        resp = self.multi_txn(txn, 1, write_only=write_only)

        if not write_only:
            status = self.get_txn_status(resp[0])
            if status != 0:
                raise TrbStatusException("Data write failed", status)
        
    def debug_write(self, subsys, address, width, data, write_only=False, blockId=0):
        """
        Do a single debug write of the given width (must be appropriate to the
        subsystem) at a given address in a given subsystem.  data should be a
        single integer.  If write_only is True, no attempt is made to read the
        response or the transaction status.
        """
        
        txn = self.TransactionToDriver()
        txn.opcode_and_src_subsys_id = 11 << 4 | self._bridge_id
        txn.src_block_id_and_dest_subsys_id = subsys
        txn.dest_block_id_and_tag = blockId <<4 | 0
        txn.payload[0] = ExtOpcodes.DEBUG_WRITE_REQ << 4 | (width-1) << 2
        txn.payload[4] = (address & 0xff)
        txn.payload[3] = (address & 0xff00)     >> 8
        txn.payload[2] = (address & 0xff0000)   >> 16
        txn.payload[1] = (address & 0xff000000) >> 24
        txn.payload[8] = (data & 0xff)
        txn.payload[7] = (data & 0xff00)     >> 8
        txn.payload[6] = (data & 0xff0000)   >> 16
        txn.payload[5] = (data & 0xff000000) >> 24

        resp = self.multi_txn(txn, 1, write_only=write_only)

        if not write_only:
            status = self.get_txn_status(resp[0])
            if status != 0:
                raise TrbStatusException("Debug write failed", status)
        
    
    def debug_read_rpt(self, subsys, address, width, num_rpt,
                          with_timestamps = False, dst_blockId=0):
        """
        Construct a repeated raw debug read, optionally collecting timestamps
        from the response PDUs
        """
        
        txn = (self.TransactionToDriver * num_rpt)()
        
        for i in range(num_rpt):
            txn[i].opcode_and_src_subsys_id = 11 << 4 | self._bridge_id
            txn[i].src_block_id_and_dest_subsys_id = subsys
            txn[i].dest_block_id_and_tag = dst_blockId << 4 | 0
            txn[i].payload[0] = ExtOpcodes.DEBUG_READ_REQ << 4 | (width-1) << 2
            txn[i].payload[4] = (address & 0xff)                           # A
            txn[i].payload[3] = (address & 0xff00)     >> 8                # d
            txn[i].payload[2] = (address & 0xff0000)   >> 16               # d
            txn[i].payload[1] = (address & 0xff000000) >> 24               # r
            
        returned_data = self.multi_txn(txn, num_rpt)
        return self.get_read_data_rpt(returned_data, num_rpt, with_timestamps)

    def build_txn_sq_mem(self, subsys, block_id, width, read_write_list, use_responsless_writes=True):
        """
        Simple function for building sequences of memory writes
        """
        length = len(read_write_list)
        txn_seq = (self.TransactionToDriver * length)()
        for i, txn_req in enumerate(read_write_list):
            txn_seq[i].src_block_id_and_dest_subsys_id = subsys
            txn_seq[i].dest_block_id_and_tag = block_id << 4
            if txn_req[0]: # This is a read - the tuple will have two elements
                _, address = txn_req
                opcode = TrbRawTransaction.opcode_codes["DATA_READ_REQ"]
            else:# This is a write - the tuple will have three elements
                _, address, data = txn_req
                if use_responsless_writes:
                    opcode = TrbRawTransaction.opcode_codes["DATA_WRITE"]
                else:
                    opcode = TrbRawTransaction.opcode_codes["DATA_WRITE_REQ"]
                txn_seq[i].payload[5] = (data & 0xff000000) >> 24
                txn_seq[i].payload[6] = (data & 0xff0000)   >> 16
                txn_seq[i].payload[7] = (data & 0xff00)     >> 8
                txn_seq[i].payload[8] = (data & 0xff)
            txn_seq[i].opcode_and_src_subsys_id = opcode << 4 | self._bridge_id
            txn_seq[i].payload[0] = (address & 0xff000000) >> 24
            txn_seq[i].payload[1] = (address & 0xff0000)   >> 16
            txn_seq[i].payload[2] = (address & 0xff00)     >> 8
            txn_seq[i].payload[3] = (address & 0xff)
            txn_seq[i].payload[4] = (width-1) << 5
        return txn_seq
    
    def build_txn_sequence(self, subsys, width, read_write_list, num_rpt=1):
        """
        Simple function for building sequences of debug reads and writes
        """
        length = len(read_write_list) * num_rpt
        txn_seq = (self.TransactionToDriver * length)()
        for i in range(num_rpt):
            
            for j, txn_req in enumerate(read_write_list):
                ij = i*len(read_write_list) + j
                txn_seq[ij].opcode_and_src_subsys_id = 11 << 4 | self._bridge_id
                txn_seq[ij].src_block_id_and_dest_subsys_id = subsys
                txn_seq[ij].dest_block_id_and_tag = 0
                if txn_req[0]: # This is a read - the tuple will have two elements
                    _, address = txn_req
                    txn_seq[ij].payload[0] = ExtOpcodes.DEBUG_READ_REQ << 4 | (width-1) << 2
                else:
                    _, address, data = txn_req # This is a write - three 
                    txn_seq[ij].payload[0] = ExtOpcodes.DEBUG_WRITE_REQ << 4 | (width-1) << 2
                    txn_seq[ij].payload[8] = (data & 0xff)
                    txn_seq[ij].payload[7] = (data & 0xff00)     >> 8
                    txn_seq[ij].payload[6] = (data & 0xff0000)   >> 16
                    txn_seq[ij].payload[5] = (data & 0xff000000) >> 24
                txn_seq[ij].payload[4] = (address & 0xff)                           # A
                txn_seq[ij].payload[3] = (address & 0xff00)     >> 8                # d
                txn_seq[ij].payload[2] = (address & 0xff0000)   >> 16               # d
                txn_seq[ij].payload[1] = (address & 0xff000000) >> 24               # r
            
        return txn_seq
    
    def multi_txn(self, txn, num_txns, write_only=False, timeout_ms=200):
        """
        Helper function to submit a block of transactions and return the
        corresponding block of responses
        """
        self._trb.write_raw_transactions(txn, num_txns)
        if write_only:
            return True
        transactions, num_got, wrapped = self._trb.read_raw_transactions(
                                                         num_txns, timeout_ms)
        return transactions[:num_got]

    def get_txn_status(self, resp):
        """
        Return the transaction status from the response.  It's consistently in
        the lower nibble of the first payload byte for the responses we care
        about.
        """
        return resp.payload[0] & 0xf

    def get_read_data(self, read_resp, with_timestamps = False):

        status = self.get_txn_status(read_resp)
        if status != 0:
            raise TrbStatusException("Read transaction failed", status)

        data = (read_resp.payload[5] << 24 |
                 read_resp.payload[6] << 16 |
                 read_resp.payload[7] << 8 |
                 read_resp.payload[8])
        
        if with_timestamps:
            # For some reason timestamps come out backwards
            ts = ((read_resp.timestamp >> 24) & 0xff | 
                  ((read_resp.timestamp >> 16) & 0xff) << 8 |
                  ((read_resp.timestamp >> 8) & 0xff) << 16 |
                  (read_resp.timestamp & 0xff) << 24)
            return (ts, data)
        
        return data


    def get_read_data_rpt(self, read_resp, num_rpt, with_timestamps=False):
        """
        Helper function to extract the data from a block of debug reads
        """
        
        return [self.get_read_data(read_resp[i], with_timestamps) \
                                                      for i in range(num_rpt)]

    def get_stream_route(self):
        ''' 
        Return a tuple of (ssid, block id, tag) that this stream is using
        for routing. That asynchronous transactions sent by the chip using
        those parameters will be received by this stream.
        It does this by sending a read transaction for curator RAM and 
        looking at the header of the returned packet. Obviously it would be
        better if the driver could just tell us this information - 
        http://ukbugdb/DS-136
        '''
        subsys = 0    
        address = 0x8000 * 2
        width = 2
        
        txn = self.TransactionToDriver()
        txn.opcode_and_src_subsys_id = 11 << 4 | self._bridge_id
        txn.src_block_id_and_dest_subsys_id = subsys
        txn.dest_block_id_and_tag = 0
        txn.payload[0] = ExtOpcodes.DEBUG_READ_REQ << 4 | (width-1) << 2
        txn.payload[4] = (address & 0xff)                           # A
        txn.payload[3] = (address & 0xff00)     >> 8                # d
        txn.payload[2] = (address & 0xff0000)   >> 16               # d
        txn.payload[1] = (address & 0xff000000) >> 24               # r
            
        returned_data = self.multi_txn(txn, 1)
        blk_id_and_tag = returned_data[0].dest_block_id_and_tag
        blk_id = blk_id_and_tag >> 4
        tag = blk_id_and_tag & 0xf
        return self._bridge_id, blk_id, tag


def extract_fields_be(pdu_words, field_defs, word_width=8):
    ''' Used for extracting field values from a PDU. Given a list of 
    words and a list of field tuples this method returns a dictionary 
    of field names and their values. 
    The field tuples are of the format ( <field name str>, <bitwidth> ).
    ASSUMPTION: The pdu_words are big endian and the fields are aligned
    to the highest bit. This is typical of field definitions for
    transactions.
    '''
    fields = dict()
    bits_so_far = 0
    for field_def in field_defs:
        name, bitwidth = field_def
        word_num = bits_so_far//word_width
        start_bit = bits_so_far % word_width
        mask = (1 << bitwidth)-1
        pdu_value = 0
        bits_read = 0
        bits_needed = start_bit + bitwidth 
        while bits_needed > bits_read:
            pdu_value = (pdu_value << word_width) + pdu_words[word_num]
            bits_read += word_width
            word_num += 1
        ls_bit_pos = (bits_read - start_bit) - bitwidth
        field_value = (pdu_value >> ls_bit_pos) & mask
        bits_so_far += bitwidth
        fields[name] = field_value
    return fields

class TimedLog(object):
    '''
    Class to allow logs (of strings) with timestamps to have a consistent
    view and to be combined into a single view.
    '''
    def __init__(self, initial_log=None):
        self.log = initial_log or list()
        self.last_timestamp = None
            
    def add(self, time_str_tuple_list):
        self.log += time_str_tuple_list
        self.log.sort(key=lambda t: t[0])

    def timestamp_us(self, entry):
        return entry[0]/10.0

    def msg_str(self, entry):
        return entry[1]

    def filter(self, filter):
        ''' Return a new TimedLog object with just the entries that contain
        the given string '''
        return TimedLog([a for a in self.log if filter in self.msg_str(a)])

    def show(self, prev_timestamp=None, report=False):
        log_lines = []
        for entry in self.log:
            timestamp_us = self.timestamp_us(entry)
            msg_str = self.msg_str(entry)
            delta_t = timestamp_us - prev_timestamp if prev_timestamp is not None else 0
            log_lines.append(" ".join([format(timestamp_us, "15,.1f"), 
                             format(delta_t, "10,.1f"), msg_str]))
            prev_timestamp = timestamp_us
        self.last_timestamp = prev_timestamp

        formatted_log = interface.Code("\n".join(log_lines))
        if report:
            return formatted_log
        TextAdaptor(formatted_log, gstrm.iout)

class DecodedTransaction(object):
    '''
    Representation of a raw transaction obtained from read_raw_transaction
    or one constructed for write_raw_transaction or a list of octets or a 
    list of dwords or a string representation of either. The lists of octets
    or dwords can optionally include a 4-octet timestamp.
    It decodes some of the payloads according to the opcode.
    Timestamps are in a different octet order on MRTS cmopared with scarlet.
    The optional mrts parameter allows them to be decoded correctly
    '''
    def __init__(self, transaction, ignore_time_from_ssid=None, mrts=False):
        if isinstance(transaction, str):
            transaction = [int(a,16) for a in transaction.split()]
            if len(transaction) == 4 or len(transaction) == 3:
                transaction = dwords_to_bytes_be(transaction) 
        if isinstance(transaction, list): 
            t = Transaction()
            if len(transaction) == 16:
                t.timestamp = bytes_to_dwords(transaction[0:4])[0]
                transaction = transaction[4:]
            elif len(transaction) != 12:
                raise RuntimeError("Can only initialise from length 3, 4, 12 or 16")
            t.opcode_and_src_subsys_id = transaction[0]
            t.src_block_id_and_dest_subsys_id = transaction[1]
            t.dest_block_id_and_tag = transaction[2]
            for i,a in enumerate(transaction[3:]):
                t.payload[i]=a
            transaction = t

        self.timestamp       = transaction.timestamp if mrts else byte_reverse_dword(transaction.timestamp)
        self.opcode          = transaction.opcode_and_src_subsys_id >> 4
        self.src_ss_id       = transaction.opcode_and_src_subsys_id & 0xf
        self.src_block_id    = transaction.src_block_id_and_dest_subsys_id >> 4
        self.dest_ss_id      = transaction.src_block_id_and_dest_subsys_id & 0xf
        self.dest_block_id   = transaction.dest_block_id_and_tag >> 4
        self.tag             = transaction.dest_block_id_and_tag & 0xf
        self.payload         = transaction.payload
        if self.src_ss_id == ignore_time_from_ssid:
            self.timestamp = 0 
        if self.opcode == TrbOpcodes.OPCODE_DEBUG or \
                    self.opcode == TrbOpcodes.OPCODE_EXTENDED:
            self.extended_opcode = transaction.payload[0] >> 4

    def __repr__(self):
        '''
        Show a string representation of the decoded transaction with its 
        timestamp
        '''
        return "time : %10d " % (self.timestamp) + self.description_string()

    def type_and_routing_string(self):
        if self.opcode == TrbOpcodes.OPCODE_DEBUG or \
            self.opcode == TrbOpcodes.OPCODE_EXTENDED:
            op_str = "%d.%d" % (self.opcode, self.extended_opcode)
            op_lookup = op_str
        else:
            op_str = "%d" % (self.opcode)
            op_lookup = self.opcode
        op_str += " " + TrbRawTransaction.opcode_names.get(op_lookup, "UNKNOWN")
        op = "Op " + op_str
         
        return (op + " " +
                      "source %d.%d " % (self.src_ss_id, self.src_block_id) +
                      "dest %d.%d " % (self.dest_ss_id, self.dest_block_id) +
                      "tag: {0:#x} ".format(self.tag))

    def description_string(self):
        '''
        Decode the fields of the transaction into a string representation
        '''
        desc_string =  self.type_and_routing_string()
        if self.opcode == TrbOpcodes.OPCODE_DEBUG:
            # Decode further
            if self.extended_opcode in (ExtOpcodes.DEBUG_WRITE_REQ,
                                        ExtOpcodes.DEBUG_READ_REQ,
                                        ExtOpcodes.DEBUG_WRITE):
                access_width = ((self.payload[0] & 0xf) >> 2) + 1
                address = bytes_to_dwords_be(self.payload[1:5])[0]
                desc_string += "width: %d address: 0x%08x " % (access_width, address)
            if self.extended_opcode in (ExtOpcodes.DEBUG_READ_RSP,
                                        ExtOpcodes.DEBUG_WRITE_RSP):
                status = (self.payload[0] & 0xf)
                desc_string += "status: %d (%s) " % (status, 
                                                TRB_STATUS_CODES[status])
            if self.extended_opcode in (ExtOpcodes.DEBUG_WRITE_REQ,
                                        ExtOpcodes.DEBUG_READ_RSP,
                                        ExtOpcodes.DEBUG_WRITE):
                value = bytes_to_dwords_be(self.payload[5:9])[0]
                desc_string += "value: 0x%08x" % value
        elif self.opcode == TrbOpcodes.OPCODE_DATA_READ_REQUEST:
            access_width = (self.payload[4] >> 5) + 1
            address = bytes_to_dwords_be(self.payload[0:4])[0]
            desc_string += "width: %d address: 0x%08x " % (access_width, address)
        elif self.opcode == TrbOpcodes.OPCODE_DATA_WRITE_REQUEST or \
                self.opcode == TrbOpcodes.OPCODE_DATA_WRITE:
            access_width = (self.payload[4] >> 5) + 1
            address = bytes_to_dwords_be(self.payload[0:4])[0]
            data = bytes_to_dwords_be(self.payload[5:9])[0]
            desc_string += "width: %d address: 0x%08x data 0x%x" % (access_width, address, data)
        elif self.opcode == TrbOpcodes.OPCODE_DATA_READ_RESPONSE:
            status = self.payload[0] >> 4
            n_bytes = ((self.payload[0] >> 1) & 7) + 1
            data = " ".join(["%02x" % x for x in self.payload[9:9-(n_bytes+1):-1]])
            desc_string += "Status: '%s' Data: %s" % (
                                            TRB_STATUS_CODES[status], data)
        elif self.opcode == TrbOpcodes.OPCODE_VM_LOOKUP_REQUEST:
            vm_req_no_offset_fields = (("type_offset", 1),
                                         ("buffer", 8),
                                         ("rwb", 1),
                                         ("offset_only", 1)) 
            vm_req_offset_fields =    (("type_offset", 1),
                                         ("buffer", 8),
                                         ("rwb", 1),
                                         ("offset", 18)) 
            fields = extract_fields_be(self.payload, vm_req_offset_fields) 
            if fields["type_offset"]:
                desc_string += ("OFFSET buffer 0x%x read %d offset 0x%x" % 
                   (fields["buffer"], fields["rwb"], fields["offset"]))
            else:
                fields =extract_fields_be(self.payload, vm_req_no_offset_fields)
                desc_string += ("NO OFFSET buffer 0x%x read %d offset only %d" % 
                   (fields["buffer"], fields["rwb"], fields["offset_only"]))
        elif self.opcode == TrbOpcodes.OPCODE_VM_LOOKUP_RESPONSE:
            vm_resp_no_offset_fields = (("with_offset", 1),
                                         ("unused", 3),
                                         ("status", 4),
                                         ("address", 32),
                                         ("validity_window", 10),
                                         ("offset", 18))
            vm_resp_offset_fields = (("with_offset", 1),
                                         ("unused", 3),
                                         ("status", 4),
                                         ("address", 32),
                                         ("validity_window", 10),
                                         ("validity_window_offset", 10))
            fields = extract_fields_be(self.payload, vm_resp_no_offset_fields)
            desc_string += ("%sOFFSET %s addr 0x%x Valid: %d " % 
                           (["NO ",""][fields["with_offset"]], 
                            TRB_STATUS_CODES[fields["status"]], 
                            fields["address"], 
                            fields["validity_window"]))
            if fields["with_offset"]:
                fields = extract_fields_be(self.payload, vm_resp_offset_fields)
                desc_string += "Valid ofs %d" % fields["validity_window_offset"]
            else:
                desc_string += "Offset 0x%x" % fields["offset"]
        elif self.opcode == TrbRawTransaction.opcode_codes["DEEP_SLEEP_SIGNAL"]:
            desc_string += TrbRawTransaction.DeepSleepSignals[self.payload[0]]
        else:
            desc_string +="payload: " + " ".join(["%02x" %a for a in self.payload])

        return desc_string

    def time_description_tuple(self):
        ''' Return the transaction as a tuple of the timestamp integer and 
        the description string suitable for passing to a TimedLog object. 
        '''
        return self.timestamp, self.description_string()

def decode_trb_log_line(line, ignore_time_from_ssid=None, mrts=False):
    ''' Decode a single line representing one or more transactions and return 
    a tuple of the host timestap value and a list of the transactions,
    See the decode_trb_log() method for a description of the optional parameters
    '''
    # List of 16 two digit hex numbers comprising timestamp and transaction 
    trb_log_write_re = re.compile(" " + "([0-9a-fA-F]{2}) "*15 
                                        + "([0-9a-fA-F]{2})")
    # List of 12 two digit hex numbers comprising just the transaction
    # e.g. a driver log from trans.trb.start_log()
    trb_log_write_re2 = re.compile(" " + "([0-9a-fA-F]{2}) "*11 
                                        + "([0-9a-fA-F]{2})")
    # List of 4 eight digit hex numbers comprising timestamp and transaction 
    trb_log_write_re3 = re.compile(" " + "([0-9a-fA-F]{8}) "*3 
                                        + "([0-9a-fA-F]{8})")
    # An eight hex digit timestamp followed by 12 two digit hex bytes of 
    # transaction
    trb_log_read_re = re.compile(" ([0-9a-fA-F]{8}) " 
                                 + "([0-9a-fA-F]{2}) "*11 
                                 + "([0-9a-fA-F]{2})")

    # procfs log from MRTS - 
    # http://wiki.europe.root.pri/Hydra_drivers/HydraTrbRawLogs
    host_log_timestamp = re.compile("\[[ ]*([0-9]+.[0-9]+)\] *")
    # Driver log from trans.trb.start_log()
    host_log_timestamp2 = re.compile(r"([0-9]+\.[0-9]+) *")
    
    host_ts_match = (host_log_timestamp.match(line) or
                    host_log_timestamp2.match(line))
    host_ts = None
    if host_ts_match:
        try:
            host_ts = float(host_ts_match.group(1))
        except ValueError:
            pass
        # Remove the timestamp characters
        line = line[host_ts_match.end():]
    rdwr = re.match(r"(Write|Read)\s+(\d+)", line)
    if rdwr:
        iter = int(rdwr.group(2))
        line = line[rdwr.end():] 
    else:
        iter = 1
    
    transactions=[]
    for _ in range(iter):
        
        if rdwr and rdwr.group(1)=='Write':
            # This is a log from trans.trb.start_log() with multiple
            # transactions so we have to limit it to only taking the
            # length of one transaction which is 12 two digit hex numbers
            x = trb_log_write_re2.search(line)
        else:
            x = (trb_log_write_re.search(line) or 
                 trb_log_write_re2.search(line) or
                 trb_log_write_re3.search(line))
        
        trans_str = line 
        if x:
            trans_str = " ".join(x.groups())
        else:
            x = trb_log_read_re.search(line)
            if x:
                trans_str = " ".join(["".join(a) for a in chunks(x.groups())[0]] 
                                                            + x.groups()[1:])
        try:
            tr = DecodedTransaction(trans_str, ignore_time_from_ssid, mrts)
        except (RuntimeError, TypeError, ValueError, IndexError):
            tr = None
        transactions.append(tr)
        line = line[x.end():] if x else ""
    
    return host_ts, transactions
        
def decode_trb_log(in_filename, out_filename=None, ignore_time_from_ssid=None, mrts=False):
    '''
    Takes a text file and annotates it with decoding of any TrB transactions
    it identifies in each line. Various patterns are matched and they need
    to be expanded to cope with new variations as we find them.
    Returns a list of text lines or writes to a given output file. If the 
    output file name has an html extension then the output will be in html
    format with different colouring for the decode annotations.
    The driver doesn't put timestamps in the first four bytes of outgoing
    transactions. Instead they have random data. Setting ignore_time_from_ssid
    to the ssid of the host keeps these values out of the log.
    The MRTS debug interface has a different clock to the scarlet. Setting
    optional parameter mrts to True will use the MRTS time scaling.
    '''
    def timestamp_to_ms(timestamp):
        ''' 
        Scale a timestamp from a transaction into milliseconds as a float and
        a string representation.
        The factor of 10 is for the scaling that scarlet uses
        for the timestamps. It is also in the TimedLog class
        in bitsandbobs.py. MRTS has a scale of 62.5
        '''
        t_delta_ms = timestamp /(1000.0 * (62.5 if mrts else 10))
        t_delta_str = "(+%.3fms) " % (t_delta_ms)
        return t_delta_ms, t_delta_str

    outlines = []

    
    if out_filename and out_filename.endswith(".html"):
        eol = "<br>"
        decode_eol = "<br>\n"
        decode_start = '<span style="color:blue">  '
        signal_decode_start = '<span style="color:green">  '
        decode_end = '</span>'
        time_gap = '<span style="color:lightgreen"> ==================== </span>' + decode_eol
    else:
        eol = ""
        decode_eol = "\n"
        decode_start = "  "
        signal_decode_start = "  "
        decode_end = ""
        time_gap = '====================' + decode_eol
    last_t = None; last_host_ts=None
    with open(in_filename,"r") as fd:
        for line in fd:
            src_line = line
            t_delta_str = ""
            host_t_delta_ms = t_delta_ms = 0

            host_ts, transactions = decode_trb_log_line(src_line, ignore_time_from_ssid, mrts)
            tr = transactions[0]
            
            if host_ts is not None:
                if last_host_ts:
                    host_t_delta_ms = (host_ts - last_host_ts)*1000
                    t_delta_str = ("[+%dms] " % host_t_delta_ms) + t_delta_str
                last_host_ts = host_ts

            if tr and tr.timestamp:
                if last_t:
                    t_delta_ms, t_delta_str = timestamp_to_ms(tr.timestamp - last_t)
                last_t = tr.timestamp

            if tr and (host_t_delta_ms > 200 or (t_delta_ms > 200 and host_ts is None)):
                outlines.append(time_gap)
                
            outlines.append(src_line + eol)
            
            if tr:
                ds = TrbRawTransaction.opcode_codes["DEEP_SLEEP_SIGNAL"]
                start = signal_decode_start if tr.opcode==ds else decode_start
                outlines.append(start + t_delta_str + "%s" % tr + decode_end + decode_eol)
                for tr in transactions[1:]:
                    t_delta_str = ""
                    if tr and tr.timestamp:
                        if last_t:
                            t_delta_ms, t_delta_str = timestamp_to_ms(tr.timestamp - last_t)
                        last_t = tr.timestamp
                    outlines.append(decode_start + t_delta_str + "%s" % tr + decode_end + decode_eol)
                    
    if out_filename:
        with open(out_filename, "w") as fd:
            for line in outlines:
                fd.write(line)
    else:
        return outlines
