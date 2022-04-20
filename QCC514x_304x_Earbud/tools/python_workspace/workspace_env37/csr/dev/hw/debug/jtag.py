############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import time
from csr.wheels import build_be, flatten_be, pack_unpack_data_le, \
    flatten_le, build_le, timeout_clock, pack_unpack_data_be

def from_bits(bit_list):
    return build_le(bit_list, word_width=1)

def to_bits(value, num_bits):
    return flatten_le(value, word_width=1, num_words=num_bits)

class JTAGController(object):
    def __init__(self, ir_length, **scan_chain):
        self._ir_length = ir_length
        self._irpre = scan_chain.get("IRPre", 0)
        self._irpost = scan_chain.get("IRPost", 0)
        self._drpre = scan_chain.get("DRPre", 0)
        self._drpost = scan_chain.get("DRPost", 0)
        self.ir_state = None

    def raw_ir_scan(self, ir_bits):
        raise NotImplementedError

    def raw_dr_scan(self, dr_bits):
        raise NotImplementedError

    def ir_scan(self, ir_value):
        self.raw_ir_scan([1]*self._irpre + to_bits(ir_value, self._ir_length) + [1]*self._irpost)
        self.ir_state = ir_value

    def dr_scan(self, dr_bits):
        scan_in = [0]*self._drpre + dr_bits + [0]*self._drpost
        scanned_out = self.raw_dr_scan(scan_in)
        return scanned_out[self._drpost:self._drpost + len(dr_bits) + self._drpre]

class JTAGUtil(object):

    def __init__(self, jtag_driver):
        self._jtag_driver = jtag_driver

    def get_num_devices(self):

        self._jtag_driver.reset()
        self._jtag_driver.ir_scan_bits([1]*100)
        self._jtag_driver.dr_scan_bits([0]*100)
        scanned_out = self._jtag_driver.dr_scan_bits([1]*100)
        return scanned_out.index(1)

    def get_id_codes(self):
        """
        Retrieve ID codes by resetting the TAP state machine and scanning
        n*32 0s through the DR chain for increasing n until 32 0s are seen
        at the end of the list.  This implies there are n-1 devices in the
        scan chain, and we split the shifted bits into n-1 32-bit
        values and return them as a list (in TDI-end-first order)
        """
        self._jtag_driver.reset()
        num_devices = 1
        # Keep scanning an extra 32 bits every time all the IDCODEs we get back
        # are non-zero
        while True:
            scanned_out = self._jtag_driver.dr_scan_bits([0]*num_devices*32)
            id_codes = pack_unpack_data_le(scanned_out, 1, 32)
            if id_codes[-1] == 0:
                return id_codes[:-1][::-1] # Reverse so we list them from TDI to TDO
            num_devices += 1


def word_to_bits_be(value):
    return flatten_be(value, word_width=1, num_words=32)
def word_from_bits_be(bits_array):
    return build_be(bits_array, word_width=1)


class MSMTAPController(object):
    """
    Implements various MSMTAP register/memory access protocols, primarily JTAG2AHB.
    """

    IR_LENGTH = 11

    # In principle there are many JTAG2AHB transport channels.  We've just captured one here.
    
    JTAG_DAISY_CFG_CTRL = 0x18
    GENERIC_SPARE_0 = 0x25
    GENERIC_SPARE_1 = 0x26

    _instructions = {"JTAG_DAISY_CFG_CTRL" : 0b00000011000,
                     "GENERIC_SPARE_0" : 0b00000100101,
                     "GENERIC_SPARE_1" : 0b00000100110}
    _reg_bit_order_is_be = {"JTAG_DAISY_CFG_CTRL" : True}

    JTAG2AHB_READ = 0
    JTAG2AHB_WRITE = 1
    JTAG2AHB_PSEL = 1
    JTAG2AHB_READY = 1

    PREADY_TIMEOUT = 0.01 # seconds

    def __init__(self, jtag_controller, reg_lengths=None):
        """
        Operates via a generic JTAG controller object which is expected to have the following API:
         * ir_scan - scans in a given value to the Instruction Register
         * dr_scan - scans in a given list of bits to the current Data Register, and scans out 
          the same number of bits.
        (Note that it's the caller's responsibility to configure the JTAG controller object with the
        correct IR length for the JTAG2AHB TAP.  Perhaps that ought to be changed...)
        """
        self._jtag_controller = jtag_controller

        self._reg_lengths = {} if reg_lengths is None else reg_lengths


    def _wait_for_completion(self):
        t = timeout_clock()
        while True:
            read_bit = self._jtag_controller.dr_scan_bits([0])[0]
            if read_bit == self.JTAG2AHB_READY:
                break
            if timeout_clock() - t > self.PREADY_TIMEOUT:
                break
    
    def access_reg(self, reg_name, value=None):
        try:
            instruction = self._instructions[reg_name]
            reg_length = self._reg_lengths[reg_name]
            reg_is_be = self._reg_bit_order_is_be[reg_name]
        except KeyError:
            raise ValueError("Unknown/unsupported MSM-TAP register '{}'".format(reg_name))
        self._jtag_controller.ir_scan(instruction)
        pack_unpack = pack_unpack_data_be if reg_is_be else pack_unpack_data_le
        if value is None:
            # Read: needs two scans
            self._jtag_controller.dr_scan_bits([0]*reg_length)
            return pack_unpack(self._jtag_controller.dr_scan_bits([0]*reg_length), 
                                1, reg_length)[0]
        self._jtag_controller.dr_scan_bits(pack_unpack([value], reg_length, 1))
        
    def jtag_daisy_cfg_ctrl_read(self):
        """
        Read the JTAG_DAISY_CFG_CTRL_REG
        """
        return self.access_reg("JTAG_DAISY_CFG_CTRL")

    def jtag_daisy_cfg_ctrl_write(self, value):
        """
        Write the given value into the JTAG_DAISY_CFG_CTRL_REG
        """
        self.access_reg("JTAG_DAISY_CFG_CTRL", value)


    def jtag2ahb_read(self, start_addr, stop_addr):
        """
        Read the given address range (should be 4-byte aligned) by issuing a series of
        JTAG2AHB accesses (there is no support for "burst" accesses in JTAG2AHB it seems).

        This function issues the read instruction on GENERIC_SPARE_1 and then polls a single bit
        on GENERIC_SPARE_0 until a 1 is seen, at which point the full 33 bits are scanned
        out and bits 1:31 converted to the result.
        """
        data = []
        generic_spare_1 = self._instructions["GENERIC_SPARE_1"]
        generic_spare_0 = self._instructions["GENERIC_SPARE_0"]
        for addr in range(start_addr, stop_addr, 4):

            read_bits = [self.JTAG2AHB_PSEL] + word_to_bits_be(addr) + [self.JTAG2AHB_READ] + [0]*32

            self._jtag_controller.ir_scan(generic_spare_1)
            self._jtag_controller.dr_scan_bits(read_bits)
            self._jtag_controller.ir_scan(generic_spare_0)
            self._wait_for_completion()
            returned_bits = self._jtag_controller.dr_scan_bits([0]*33)[1:]

            # build big-endian
            data.append(word_from_bits_be(returned_bits))
            
            return data

    def jtag2ahb_write(self, start_addr, data_words):
        """
        Write the given words starting at the given address (should be 4-byte aligned).

        This function issues the write instruction on GENERIC_SPARE_1 and then polls a single bit
        on GENERIC_SPARE_0 until a 1 is seen.
        """

        generic_spare_1 = self._instructions["GENERIC_SPARE_1"]
        generic_spare_0 = self._instructions["GENERIC_SPARE_0"]

        stop_addr = start_addr + len(data_words)*4
        for addr, data_word in zip(range(start_addr, stop_addr, 4), data_words):
  
            write_bits = ([self.JTAG2AHB_PSEL] + word_to_bits_be(addr) + 
                          [self.JTAG2AHB_WRITE] + word_to_bits_be(data_word))

            self._jtag_controller.ir_scan(generic_spare_1)
            self._jtag_controller.dr_scan_bits(write_bits)
            self._jtag_controller.ir_scan(generic_spare_0)
            self._wait_for_completion()


class JTAG2AHBTransport(object):

    IR_LENGTH = 11

    def __init__(self, remote_api, controller_type, verbose=0, **kwargs):

        self._remote_api = remote_api
        self._verbose = verbose
        self._taps = {}
        self._controller_type = controller_type

    def get_tap_connection(self, tap_id):
        
        try:
            self._taps[tap_id]
        except KeyError:
            ctrl = self._controller_type(self._remote_api, 
                                         MSMTAPController.IR_LENGTH)
            self._taps[tap_id] = MSMTAPController(ctrl)
        return self._taps[tap_id]

    def memory_read(self, tap_id, start_addr, end_addr, peripheral=None):
        tap = self.get_tap_connection(tap_id)
        data_words = tap.jtag2ahb_read(start_addr, end_addr)
        return pack_unpack_data_le(data_words, 32, 8)

    def memory_write(self, tap_id, start_addr, data_bytes, peripheral=None):
        tap = self.get_tap_connection(tap_id)
        tap.jtag2ahb_write(start_addr, pack_unpack_data_le(data_bytes,8,32))
