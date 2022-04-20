############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

"""
Implementation of host side of SWD protocol
"""

from csr.wheels import pack_unpack_data_le, build_le

class SWDLineBits(object):
    """
    Generic list of bits to be sent/received on SWDIO
    """
    def __init__(self, bit_list, dir_bits):
        self.bit_list = bit_list
        self.dir_bits = dir_bits

    def __add__(self, other):
        return SWDLineBits(self.bit_list+other.bit_list, 
                           self.dir_bits+other.dir_bits)
    def __radd__(self, other):
        return SWDLineBits(self.bit_list+other.bit_list, 
                           self.dir_bits+other.dir_bits)
    def __iadd__(self, other):
        self.bit_list += other.bit_list
        self.dir_bits += other.dir_bits

class SWDDriveBits(object):
    """
    List of bits to be driven on SWDIO
    """
    def __init__(self, bit_list):
        self.bit_list = bit_list
    @property
    def dir_bits(self):
        return [1]*len(self.bit_list)
    def __add__(self, other):
        if isinstance(other, SWDDriveBits):
            return SWDDriveBits(self.bit_list + other.bit_list)
        return SWDLineBits(self.bit_list+other.bit_list,
                           self.dir_bits+other.dir_bits)

class SWDReceiveBits(object):
    """
    Number of bits to receive on SWDIO
    """
    def __init__(self, num_bits):
        self.num_bits = num_bits
    @property
    def bit_list(self):
        return [0]*self.num_bits
    @property
    def dir_bits(self):
        return [0]*self.num_bits
    def __add__(self, other):
        if isinstance(other, SWDReceiveBits):
            return SWDReceiveBits(self.num_bits + other.num_bits)
        return SWDLineBits(self.bit_list+other.bit_list,
                           self.dir_bits+other.dir_bits)

class SWDInvalidACK(RuntimeError):
    """
    Saw ACK bits other than [0,0,1], [0,1,0], [1,0,0]
    """

class SWD(object):
    """
    Provides general SWD operations based on a transport-specific
    driver object, which provides a "drive_line" method which takes
    an SWDXXXBits object and drives/receives bits accordingly, returning
    whatever bits are returned at the end.
    """
    RESET = SWDDriveBits([1]*56)
    LINE_RESET = SWDDriveBits([1]*56) + SWDReceiveBits(8)
    JTAG_SWD_SWITCH = SWDDriveBits([0,1,1,1] + 
                                   [1,0,0,1] +
                                   [1,1,1,0] + 
                                   [0,1,1,1])

    OK_ACK_BITS = [1,0,0]
    WAIT_ACK_BITS = [0,1,0]
    FAULT_ACK_BITS = [0,0,1]
    OK = 1
    WAIT = 2
    FAULT = 4

    def __init__(self, swd_driver):
        self._driver = swd_driver

    def _header(self, reg_addr, read_not_write, ap_not_dp=False):
        """
        Construct drive bits for an SWD protocol header
        """
        read_data = [ 1,   # start 
                      int(ap_not_dp),   # APnDP
                      int(read_not_write),   # RnW
                      (reg_addr & 0x4) >> 2, # A[2:...
                      (reg_addr & 0x8) >> 3, # ..3]
                    ]
        parity = sum(read_data[1:]) % 2 # start bit not included in parity check
        read_data += [parity,
                   0,   # stop
                   1,   # park
                   ]
        return SWDDriveBits(read_data)

    def _write_payload(self, value):
        """
        Construct drive bits for an SWD write transaction payload (data + parity)
        """
        write_reg_data = pack_unpack_data_le([value], from_width=32, to_width=1)
        write_reg_data += [sum(write_reg_data) % 2] # parity
        return SWDDriveBits(write_reg_data)

    def line_reset_and_write(self, reg_addr, reg_value):
        """
        Implement line reset and the write the given address (typically, DP.TARGETSEL)
        """    
        header = self._header(reg_addr, read_not_write=False, 
                              ap_not_dp=False)

        self._driver.drive_line(self.LINE_RESET + 
                                header + 
                                SWDReceiveBits(5) + 
                                self._write_payload(reg_value))

    def line_reset(self):
        """
        Sends the line reset bit-sequence (56 bits high followed by 8 bits low/idle)
        """
        self._driver.drive_line(self.LINE_RESET)

    def jtag_to_swd_switch(self):
        """
        Sends reset followed by JTAG->SWD switch followed by another reset
        """
        self._driver.drive_line(self.RESET + self.JTAG_SWD_SWITCH + self.RESET)

    def jtag_to_swd_switch_to_wake(self):
        """
        Sends jtag_to_swd_switch and then reads DPIDR.  If a valid ACK is seen it
        implies the chip is awake and the value of DPIDR is returned.  Otherwise None
        is returned.
        """
        bit_list = (self.RESET + self.JTAG_SWD_SWITCH + self.RESET + 
                    self._header(0, ap_not_dp=False, read_not_write=True) + SWDReceiveBits(38))
        read_data_bits = self._driver.drive_line(bit_list)
        ack = build_le(read_data_bits[0:3], word_width=1)
        if ack == self.OK:
            return pack_unpack_data_le(read_data_bits[3:35],1,32)[0]

    def read_apdp_register(self, reg_addr, ap_not_dp=False):
        """
        Executes an SWD read transaction, checking the ACK and only returning the data if
        successful (otherwise an exception is raised)
        """
        self._driver.drive_line(self._header(reg_addr, ap_not_dp=ap_not_dp, read_not_write=True))
        read_data_bits = self._driver.drive_line(SWDReceiveBits(36+8))[:36]
        ack = build_le(read_data_bits[0:3], word_width=1)
        if ack == self.OK:
            return pack_unpack_data_le(read_data_bits[3:35],1,32)[0]
        if ack == self.WAIT:
            raise NotImplementedError("Saw WAIT ack!")
        if ack == self.FAULT:
            raise NotImplementedError("Saw FAULT ack!")
        raise SWDInvalidACK("Invalid ack {} seen".format(ack))

    def write_apdp_register(self, reg_addr, value, ap_not_dp=False):
        """
        Executes an SWD write transaction, checking the ACK and only sending the data if successful
        """
        self._driver.drive_line(self._header(reg_addr, ap_not_dp=ap_not_dp, read_not_write=False))
        ack = build_le(self._driver.drive_line(SWDReceiveBits(5))[:3], word_width=1)
        if ack == self.OK:
            self._driver.drive_line(self._write_payload(value)) # payload + 8 trailing clocks
            pass
        elif ack == self.WAIT:
            raise NotImplementedError("Saw WAIT ack!")
        elif ack == self.FAULT:
            raise NotImplementedError("Saw FAULT ack!")
        else:
            raise SWDInvalidACK("Invalid ack {} seen".format(ack))




