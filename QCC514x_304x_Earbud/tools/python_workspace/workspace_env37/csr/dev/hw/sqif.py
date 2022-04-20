# Copyright (c) 2016 Qualcomm Technologies International, Ltd.
#   %%version
"""
@file
SQIF hardware module object file.

@section Description
This module implements the SqifInterface object for the subsystems with
a single serial flash interfaces and SqifInterfaces object for those with
two interface. The SqifInterface object provides a few basic methods to
interact with the SQIF hardware block and the attached SQIF device directly.
"""
import time
from csr.wheels.global_streams import iprint
from csr.dev.hw.address_space import AddressSpace

class SqifInterfaces(list,object):
    '''
    Class representing two SQIF blocks within a subsystem.
    '''
    def __init__(self, cores):
        list.__init__(self, (SqifInterface(cores[0], 0),SqifInterface(cores[1], 1)))



class SqifInterface(object):
    '''
    Class representing SQIF hardware settings.
    '''
    def __init__(self, core,bank=None):
        self._bank = bank
        self._core = core

        #Storage for SQIF configuration registers.
        self._store_store_sqif_inst = 0
        self._store_sqif_conf = 0
        self._store_sqif_conf2 = 0
        self._store_sqif_ctrl = 0
        self._clkgen_en = 0

        #Store references for commonly used registers
        self._sqif_peek = self._core.field_refs["SQIF_PEEK"]
        self._sqif_peek_go = self._core.field_refs["SQIF_PEEK_GO"]
        self._sqif_poke = self._core.field_refs["SQIF_POKE"]
        self._sqif_poke_last = self._core.field_refs["SQIF_POKE_LAST"]
        if self._bank is not None:
            self._apps_sys_sqif_sel = self._core.field_refs["APPS_SYS_SQIF_SEL"]

    def store_config(self):
        '''
        Store the current SQIF configuration.

        It is generally assumed that the Curator will configure
        SQIF with in the subsystems. Therefore, the configuration
        should be stored before the subsystem is reset.
        '''
        if self._bank is not None:
            bank_select = self._apps_sys_sqif_sel.read()
            self._apps_sys_sqif_sel.write(self._bank)

        self._store_sqif_inst = self._core.fields["SQIF_INST"]
        self._store_sqif_conf = self._core.fields["SQIF_CONF"]
        self._store_sqif_conf2 = self._core.fields["SQIF_CONF2"]
        self._store_sqif_ctrl = self._core.fields["SQIF_CTRL"]

        if self._bank is not None:
            #Restore the SQIF bank select
            self._apps_sys_sqif_sel.write(bank_select)

        self._clkgen_en = self._core.fields["CLKGEN_ENABLES"]

    def restore_config(self):
        '''
        Restore the saved SQIF configuration.
        '''
        if self._bank is not None:
            bank_select = self._apps_sys_sqif_sel.read()
            self._apps_sys_sqif_sel.write(self._bank)

        self._core.fields["SQIF_INST"] = self._store_sqif_inst
        self._core.fields["SQIF_CONF"] = self._store_sqif_conf
        self._core.fields["SQIF_CONF2"] = self._store_sqif_conf2
        self._core.fields["SQIF_CTRL"] = self._store_sqif_ctrl

        if self._bank is not None:
            #Restore the SQIF bank select
            self._apps_sys_sqif_sel.write(bank_select)

        self._core.fields["CLKGEN_ENABLES"] = self._clkgen_en

    def minimal_config(self, verbose=True):
        """
        Absolute minimum register configuration to be able to access SQIF
        This will allow peek and poke commands to be sent to SQIF but isn't
        enough to actually run code, or even read memory!

        The user MUST reset the device to get back to a workable system.
        """
        if self._bank is not None:
            #SQIF clock enable.
            if self._bank == 0:
                self._core.fields["CLKGEN_ENABLES"] = 0x0008
            else:
                self._core.fields["CLKGEN_ENABLES"] = 0x0020

            #Set the bank register and leave it set to the SQIF configured
            self._apps_sys_sqif_sel.write(self._bank)

        #In case the SQIF HW has never been configured enable the minimum.
        self._core.fields.SQIF_CONF2.SQIF_CONF2_SQIF_EN = 1
        self._core.fields.SQIF_CONF2.SQIF_CONF2_GRAB_PADS = 1
        self._core.fields.SQIF_CONF2.SQIF_CONF2_ENDIANNESS = 1

        #If the curator is using the dummy override this will 'break' PAP
        # access so clear this and set the latency cycles in the old way.
        #
        # NOTE: If the Curator every changes the number of cycles this will
        # fail, but we can't support all values the override can be set to
        # in the SQIF_CONF register.
        if self._core.fields.SQIF_CONF2.SQIF_CONF2_DUMMY_OVR != 0:
            self._core.fields.SQIF_CONF2.SQIF_CONF2_DUMMY_OVR = 0
            self._core.fields.SQIF_CONF.SQIF_CONF_DUM = 3

        if verbose:
            iprint('\nMinimal SQIF configuration set!  \nA device reset may be needed to get back to a working system.')

    def bulk_erase_helper(self, bank = 0, byte_address = None, show_progress=True):
        '''
        Poke the erase commands out to the serial flash, and poll for complete

        This sequence is ONLY intended to work on Spansion MirrorBit devices,
        since the SQIF commands are hardcoded here.

        NB: This should really be changed to read curator MIBs or offload programming to
            Curator via tool_cmd. For now, Mask added to SR read to only check bit 0 which
            is the WIP bit for most devices.

            Macronix has the QE bit in this register and when it is set, sstr will
            never be 0.
        '''

        #Write WREN command
        self._sqif_poke_last.write(0x06)
        if byte_address is None:
            if show_progress:
                iprint('Write BULK erase command, this will take sometime')
            self._sqif_poke_last.write(0x60)  #Chip erase command
            poll = 5
        else:
            erase_cmd = 0xD8 #Sector erase command
            if byte_address < 0x8000: #Special case sector is subdivided
                erase_cmd = 0x20  #Block erase command

            if show_progress:
                iprint('Write SECTOR erase command for address 0x%x' % byte_address)
            self._sqif_poke.write(erase_cmd)   #Sector erase command
            self._sqif_poke.write((byte_address & 0xFF0000) >> 16)
            self._sqif_poke.write((byte_address & 0x00FF00) >> 8)
            self._sqif_poke_last.write(byte_address & 0x0000FF)
            poll = 0.25
        #write "Read status command" to SQIF_POKE so CS remains asserted
        self._sqif_poke.write(0x05)

        if show_progress:
            iprint('Sleep for %ds and read back status... zzZZ ' % poll)

        n=0
        while True:
            time.sleep(poll-0.1)

            self._sqif_peek_go.write(0x0)
            time.sleep(0.1)

            #Added mask to check the WIP bit only
            status = (self._core.fields["SQIF_PEEK"] & 0x1)

            #Partially decode the status
            sstr = "Unexpected error"
            if status == 1:
                sstr = "Busy"
            if status == 0:
                sstr = "Complete"
            n = n + 1

            if show_progress:
                iprint('After %ds status is %x (%s)'%(n*poll, status, sstr))
            if status == 0 or n>100:
                break

        # Do one last read to clear chip select
        self._sqif_peek_go.write(0x1)
        time.sleep(0.1)

        status = self._sqif_peek.read()

        if show_progress:
            iprint('Erased in %ds'%(n*poll))

    def chip_id(self, report=True):
        '''
        Poke the Chip ID commands out to the serial flash, and read back the ID.
        '''
        if self._bank is not None:
            bank_select = self._apps_sys_sqif_sel.read()
            self._apps_sys_sqif_sel.write(self._bank)

        #Write read JEDEC ID command to SQIF_POKE so CS remains asserted
        self._sqif_poke.write(0x9F)

        #Read back the response
        self._sqif_peek_go.write(0x0)
        byte1 = self._sqif_peek.read()
        self._sqif_peek_go.write(0x0)
        byte2 = self._sqif_peek.read()
        self._sqif_peek_go.write(0x1)
        byte3 = self._sqif_peek.read()

        #Restore the SQIF bank select
        if self._bank is not None:
            self._apps_sys_sqif_sel.write(bank_select)

        if report:
            iprint('Manufacturers ID %#0.2x  Device ID %#0.2x%0.2x'%(byte1,byte2,byte3))
        else:
            return int(byte1)

    def mode_bit_reset(self):
        '''
        Poke the Mode Bit Reset command out to the serial flash.

        This ONLY intended to work on Spansion MirrorBit devices,
        since the SQIF commands are hardcoded here.
        '''
        if self._bank is not None:
            bank_select = self._apps_sys_sqif_sel.read()
            self._apps_sys_sqif_sel.write(self._bank)

        #Write MBR to SQIF_POKE_LAST
        self._sqif_poke_last.write(0xFF)

        #Write RSTEN and RST to SQIF_POKE_LAST
        self._sqif_poke_last.write(0x66)
        self._sqif_poke_last.write(0x99)

    @property
    def sqif_enabled(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        try:
            return self._core.fields.SQIF_CONF2.SQIF_CONF2_SQIF_EN.read()
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def ddr_enabled(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        try:
            return self._core.fields.SQIF_CONF2.SQIF_CONF2_DDR_MODE_EN.read()
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def clk_divider(self):
        '''
        Returns Clock divider, or 0 if the subsystems isn't powered
        '''
        try:
            # This field is zero based (0=clk/1, 1=clk/2, 2=clk/3, etc.)
            # So add one to the field value we return to get the real divider.
            return self._core.fields.SQIF_CTRL.SQIF_CTRL_CLK_RATE_FLASH.read() + 1
        except AddressSpace.ReadFailure:
            return 0

    @property
    def in_continuous_mode(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        try:
            if (self._core.fields.SQIF_INST.SQIF_INST_MOD == 0xa5) and \
               (self._core.fields.SQIF_CONF.SQIF_CONF_CMD == 0):
                 return True
            else:
                 return False
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def sqif_width(self):
        '''
        Returns width or "OFF" if the subsystems isn't powered
        '''
        width_names = ["C1A1D1", "C1A1D2", "C1A1D4", "C1A2D2", "C1A4D4", "C4A4D4", "C2A2D2"]
        try:
            sqif_width = self._core.fields.SQIF_CONF.SQIF_CONF_WID.read()
            return width_names[sqif_width]
        except AddressSpace.ReadFailure:
            return "OFF"


    @property
    #Assume an 80MHz SQIF clock. Subsystems which go faster will override this
    def clk_rate(self):
        '''
        Returns the SQIF clock rate
        '''
        return 80

class AppsSqifInterface(SqifInterface):
    @property
    def clk_rate(self):
        '''
        Returns the SQIF clock rate
        '''
        return 120

class AppsP0SqifInterface(AppsSqifInterface):
    @property
    def sqif_enabled(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P0
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(0)
            return self._core.fields.SQIF_CONF2.SQIF_CONF2_SQIF_EN.read()
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def ddr_enabled(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P0
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(0)
            return self._core.fields.SQIF_CONF2.SQIF_CONF2_DDR_MODE_EN.read()
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def clk_divider(self):
        '''
        Returns Clock divider, or 0 if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P0
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(0)
            # This field is zero based (0=clk/1, 1=clk/2, 2=clk/3, etc.)
            # So add one to the field value we return to get the real divider.
            return self._core.fields.SQIF_CTRL.SQIF_CTRL_CLK_RATE_FLASH.read() + 1
        except AddressSpace.ReadFailure:
            return 0

    @property
    def in_continuous_mode(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P0
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(0)
            if (self._core.fields.SQIF_INST.SQIF_INST_MOD == 0xa5) and \
               (self._core.fields.SQIF_CONF.SQIF_CONF_CMD == 0):
                 return True
            else:
                 return False
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def sqif_width(self):
        '''
        Returns width or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P0
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(0)
            width_names = ["C1A1D1", "C1A1D2", "C1A1D4", "C1A2D2", "C1A4D4", "C4A4D4", "C2A2D2"]
            sqif_width = self._core.fields.SQIF_CONF.SQIF_CONF_WID.read()
            return width_names[sqif_width]
        except AddressSpace.ReadFailure:
            return "OFF"

class AppsP1SqifInterface(AppsSqifInterface):
    @property
    def sqif_enabled(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P1
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(1)
            return self._core.fields.SQIF_CONF2.SQIF_CONF2_SQIF_EN.read()
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def ddr_enabled(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P1
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(1)
            return self._core.fields.SQIF_CONF2.SQIF_CONF2_DDR_MODE_EN.read()
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def clk_divider(self):
        '''
        Returns clock divider or 0 if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P1
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(1)
            # This field is zero based (0=clk/1, 1=clk/2, 2=clk/3, etc.)
            # So add one to the field value we return to get the real divider.
            return self._core.fields.SQIF_CTRL.SQIF_CTRL_CLK_RATE_FLASH.read() + 1
        except AddressSpace.ReadFailure:
            return 0

    @property
    def in_continuous_mode(self):
        '''
        Returns True,False or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P1
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(1)
            if (self._core.fields.SQIF_INST.SQIF_INST_MOD == 0xa5) and \
               (self._core.fields.SQIF_CONF.SQIF_CONF_CMD == 0):
                 return True
            else:
                 return False
        except AddressSpace.ReadFailure:
            return "OFF"

    @property
    def sqif_width(self):
        '''
        Returns width or "OFF" if the subsystems isn't powered
        '''
        #Need to set SQIF register bank select to P1
        try:
            self._core.fields.APPS_SYS_SQIF_SEL.write(1)
            width_names = ["C1A1D1", "C1A1D2", "C1A1D4", "C1A2D2", "C1A4D4", "C4A4D4", "C2A2D2"]
            sqif_width = self._core.fields.SQIF_CONF.SQIF_CONF_WID.read()
            return width_names[sqif_width]
        except AddressSpace.ReadFailure:
            return "OFF"

class AudioSqifInterface(SqifInterface):
    @property
    def clk_rate(self):
        '''
        Returns the SQIF clock rate
        '''
        return 120

class AppsSqifInterfaces(list,object):
    '''
    Class representing two SQIF blocks within a subsystem.
    '''
    def __init__(self, cores):
        list.__init__(self, (AppsP0SqifInterface(cores[0], 0), AppsP1SqifInterface(cores[1], 1)))
