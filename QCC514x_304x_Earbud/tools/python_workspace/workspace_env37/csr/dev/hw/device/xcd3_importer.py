############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import sys
import io
import array
import re
import csr
from ...hw.chip_version import ChipVersion
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import open_bytes_compressed
from csr.dev.hw.device.device_factory import DeviceFactory
from csr.dev.hw.subsystem.hydra_subsystem import SimpleHydraSubsystem
from csr.dev.hw.address_space import ExtremeAccessCache, AddressSpace
from csr.dev.env.standalone_env import StandaloneFirmwareEnvironment
from csr.dev.hw.core.meta.i_layout_info import XapDataInfo

if sys.version_info >= (3,):
    int_type = (int)
else:
    # Python 2
    int_type = (int, long)


def stash_stdin(loader):
    '''
    Use any stdin we stashed in loader.__class__ on an earlier load
    (or we could have rewound to its beginning
    just to load it again, which is wasteful of time).
    This assumes no clever switching of stdin is being used to deliver
    different xcd files on different calls to the XCD importer.
    '''
    if hasattr(loader.__class__, '_stdin'):
        loader.buffer = loader.__class__._stdin
    else:
        # stdin.read() returns a string like object but we want to handle everything
        # as bytes for the initial input before decoding at parsing time so we encode here.
        try:
            # In Py3 sys.stdin has underlying buffer attribute which is bytes-like file object
            loader.buffer = sys.stdin.buffer.read()
        except AttributeError:
            loader.buffer = sys.stdin.read()
        loader.__class__._stdin = loader.buffer

class XCD3Importer (object):
    """\
    Constructs virtual devices and loads their state from an XCD3 coredump.
    """
    def import_device(self, xcd3_path, emulator_build=None):
        """\
        Returns virtual device constructed from XCD3 file.
        """
        # Reuse existing xcd3 parser/loader logic (though its a bit messy)
        #
        xcd3 = XCD3DeviceLoader(xcd3_path)        
        
        # File contains raw chip version. 
        #
        chip_version = ChipVersion(xcd3.chip_version)        
        
        # Construct virtual device ready to receive the coredumped state. Using
        # a special cache policy that intercepts all accesses - including
        # writes to read only registers!
        #
        device = DeviceFactory.create(chip_version, None, ExtremeAccessCache,
                                      emulator_build=emulator_build)
        setattr(device, 'device_url', 'xcd3:'+xcd3_path)
        # Pump dumped state into the device reusing standard loader. 
        #
        # NB. The virtual device should model bank state well enough for this
        # to mostly work.
        #
        loaded = xcd3.load_into_device(device, write_readonly_registers=True)
        if not loaded:
            return None
        # TODO: assume once loaded don't need the underlying file
        xcd3.close()
        return device

class XCD1Importer (object):
    
    def import_device(self, xcd1_path):
        
        xcd1 = XCD1DeviceLoader(xcd1_path)
        
        chip_version = ChipVersion(xcd1.chip_version)
        
        device = DeviceFactory.create(chip_version, None, ExtremeAccessCache)
        setattr(device, 'device_url', 'xcd1:'+xcd1_path)
        loaded = xcd1.load_into_device(device, write_readonly_registers=True)
        if not loaded:
            return None
        return device
    

class XCD3DeviceLoader (object):
    """\
    XCD3 core dump loader.
    
    Parses xcd3 and Loads coredumped state into a device (physical or virtual)
    
    Tries to infer firmware from id in coredump.    
    """
    def __init__(self, xcd3_file):
        
        self._loader = XCD3Loader(mem=None, 
                            filename = xcd3_file,
                            strict_syntax=False, 
                            quiet=True)                

        self._header_fields = self._loader.read_header()

    @property
    def dump_type(self):
        return self._header_fields['AT']

    @property
    def chip_version(self):
        v = int(self._header_fields['AV'][4:8], 16)
   
        # Chip Version is end swapped in some xcd files!
        if not v:
            v = int(self._header_fields['AV'][0:4], 16)
            
        return v

    def read_header(self):
        """\
        Read header field dictionary
        """
        return self._loader.read_header() 

    def load_into_device(self, device, write_readonly_registers=False):
        """\
        Load the core dump into the specified device.
        
        Options:- 
        
        - write_readonly_registers: Write values to read only registers if told
        to. This allows loading of normally unwritable state into "software"
        device for analysis.
        
        Returns success or failure.
        """
        # Future:- 
        # - Handle LPC dumps here (when they happen)
        # - Handle multi chip (e.g. HAPS54) dumps here (if ever)
        # 
        chip = device.chips[0]
        
        # Load hostio subsystem
        #
        # If the BTSubsystem is generic it means it won't support loading of
        # host SS registers.
        if not hasattr(chip, "bt_subsystem") or not isinstance(chip.bt_subsystem, 
                                                               SimpleHydraSubsystem):
            iprint("Loading hostio state...")
            
            host_ss = chip.host_subsystem
            # The coredump file attempts to hide that it has been produced via the
            # debug transport, but doesn't really manage it. data is written out in 
            # natural words but addresses are not consistently correct for the 
            # processor memory map (indeed, for the host subsystem there isn't a 
            # "processor memory map", of course). In summary:
            # 
            # The Host subsystem is (effectively) dumped against the SPI memory map,
            # regardless of whether it was actually dumped via SPI or TRB.
            ok = self._loader.load_subsys_by_name("HOSTIO", "HOSTIO", 
                                                  mem = host_ss.spi_in,
                                                  layout_info=XapDataInfo())
            if not ok:
                # We may not be interested in the Hostio
                iprint("WARNING: No Hostio subsystem dump found")
            else:
                host_ss.has_data_source = True
        
        # Load curator subsystem w.r.t. its _SPI_ space (to reuse legacy logic)
        #
        iprint("Loading curator state...")
        cur_ss = chip.curator_subsystem
        core = cur_ss.core
        # The coredump file attempts to hide that it has been produced via the
        # debug transport, but doesn't really manage it. data is written out in 
        # natural words but addresses are not consistently correct for the 
        # processor memory map (indeed, for the host subsystem there isn't a 
        # "processor memory map", of course). In summary:
        # 
        # The Curator is (effectively) dumped against the SPI memory map,
        # regardless of whether it was actually dumped via SPI or TRB.
        ok = self._loader.load_subsys_by_name("CURATOR", "CURATOR", 
                                              mem = cur_ss.spi_in,
                                              layout_info=core.info.layout_info)
        if not ok:
            # We may not be interested in the Curator
            iprint("WARNING: No Curator subsystem dump found")
            # ... but we'd like to have the MMU_REG_ACCESS_TIMEOUT_VALUE register
            # set so that the check on bad register reads won't trip over
            cur_ss.core.fields.MMU_REG_ACCESS_TIMEOUT_VALUE.set_default()
        else:
            cur_ss.core.dump_build_id = self._loader.fw_build_id
            cur_ss.core.dump_build_string = self._loader.fw_build_string
            cur_ss.has_data_source = True

        if hasattr(chip, "bt_subsystem"):
            iprint("Loading bt state...")
            bt_ss = chip.bt_subsystem
            core = bt_ss.core
            # The coredump file attempts to hide that it has been produced via the
            # debug transport, but doesn't really manage it. data is written out in 
            # natural words but addresses are not consistently correct for the 
            # processor memory map (indeed, for the host subsystem there isn't a 
            # "processor memory map", of course). In summary:
            # 
            # The Curator is (effectively) dumped against the SPI memory map,
            # regardless of whether it was actually dumped via SPI or TRB.
            ok = self._loader.load_subsys_by_name("BT", "BT", 
                                                  mem = bt_ss.trb_in,
                                                  layout_info=core.info.layout_info)

            if not ok:
                # We may not be interested in the BT
                iprint("WARNING: No Bluetooth subsystem dump found")
            else:
                bt_ss.core.dump_build_id = self._loader.fw_build_id
                bt_ss.core.dump_build_string = self._loader.fw_build_string
                bt_ss.has_data_source = True

                # Populate unmapped registers, if there are any
                regs = self._loader.subloader.unmapped_regs
                try:
                    for i in range(12):
                        core.r[i] = regs['R'+str(i)]
                    #and other core registers
                    #mentioned in the coredump
                    core.pc = regs['PC']
                    core.lr = regs['LR']
                    core.sp = regs['SP']
                    core.msp = regs['MSP']
                    core.psp = regs['PSP']
                    core.xpsr = regs['XPSR']
                    core.special = regs['SPECIAL']
                except (AttributeError,KeyError):
                    # old coredumps didn't have these or were empty.
                    pass

        if hasattr(chip, "apps_subsystem"):
            # 
            # The Apps is always dumped against the TRB memory map
            app_ss = chip.apps_subsystem

            # The method is_coredump_old_format() fails if the subsystem is not present, 
            # yet it is called before we determine if the subsystem is present, for now
            # just catch this..             
            try:
                if self._loader.is_coredump_old_format("APP"):
                    iprint("Loading apps state (old format)...")
                    # It is old format coredump so need to load TRB view map                
                    ok = self._loader.load_subsys_by_name("APP", "APP", mem = app_ss.trb_in,
                                                          layout_info=app_ss.p0.info.layout_info)
                    if not ok:
                        # Apps isn't required
                        iprint("WARNING: No Apps subsystem dump found")
                    else:
                        app_ss.p0.dump_build_id = self._loader.fw_build_id
                        app_ss.p0.dump_build_string = self._loader.fw_build_string
                        app_ss.p0.has_data_source = True
                else:
                    # It is new format coredump so need to load processor view instead of trb mem map
                    iprint("Loading apps state...")

                    ok = self._loader.load_subsys_by_name("APP", "APP", mem = app_ss.p0.dm,
                                                          layout_info=app_ss.p0.info.layout_info, processor=0) 
                    if not ok:
                        # Apps P0 isn't required
                        iprint("WARNING: No Apps subsystem P0 dump found")
                    else:
                        app_ss.p0.dump_build_id = self._loader.fw_build_id
                        app_ss.p0.dump_build_string = self._loader.fw_build_string
                        app_ss.p0.has_data_source = True
                    
                    ok = self._loader.load_subsys_by_name("APP", "APP", mem = app_ss.p1.dm,
                                                          layout_info=app_ss.p1.info.layout_info, processor=1) 
                    if not ok:
                        # Apps P1 isn't required
                        iprint("WARNING: No Apps subsystem P1 dump found")
                    else:
                        app_ss.p1.dump_build_id = self._loader.fw_build_id
                        app_ss.p1.dump_build_string = self._loader.fw_build_string
                        app_ss.p1.has_data_source = True
            except XCD3FormatCheckException:
                iprint("WARNING: No APPS subsystem found whilst trying to determine coredump format.")

            # Do for AUDIO as done for APP
            try:
                audio_ss = chip.audio_subsystem
            except AttributeError:
                # We're on a partial emulator
                audio_ss = device.chips[1].audio_subsystem

        if hasattr(chip, "audio_subsystem"):
            # B-221270. This could do with a rework..
            # The method is_coredump_old_format() fails if the subsystem is not present, 
            # yet it is called before we determine if the subsystem is present, for now
            # just catch this..             
            try:
                if self._loader.is_coredump_old_format("AUDIO"):
                    iprint("Loading audio states (old format)...")
                    # It is old format coredump so need to load TRB view map                
                    ok = self._loader.load_subsys_by_name("AUDIO", "AUDIO", mem = audio_ss.trb_in,
                                                          layout_info=audio_ss.p0.info.layout_info)
                    if not ok:
                        # Audio isn't required
                        iprint("WARNING: No Audio subsystem dump found")
                else:
                    # It is new format coredump so need to load processor view instead of trb mem map
                    #
                    # However note that if private memory is not enabled the
                    # dump will only contain a single view of memory, and we
                    # will end up loading the same thing three times.  That's a
                    # bit silly, of course, but because of a wrinkle in how the
                    # AudioVMCore supports private memory it causes the right
                    # thing to happen, namely P0 and P1 both get to see the
                    # same thing in the range [0x0000,0x1000).
                    iprint("Loading audio state...")
                    # Provide processor as an empty string to indicate load up to 'P 0'
                    ok = self._loader.load_subsys_by_name("AUDIO", "AUDIO", mem = audio_ss.p0.dm,
                                                          layout_info=audio_ss.p0.info.layout_info, processor="")
                    if not ok:
                        # Audio P0 isn't required
                        iprint("WARNING: No Audio subsystem dump found")
                    else:
                        audio_ss.core.dump_build_id = self._loader.fw_build_id
                        audio_ss.core.dump_build_string = self._loader.fw_build_string
                        audio_ss.core.has_data_source = True
                   
                    # Provide processor as 0 to indicate load 'P 0'
                    ok = self._loader.load_subsys_by_name("AUDIO", "AUDIO", mem = audio_ss.p0.dm,
                                                          layout_info=audio_ss.p0.info.layout_info, processor=0) 
                    if not ok:
                        # Audio P0 isn't required
                        iprint("WARNING: No Audio subsystem P0 dump found")
                    # Provide processor as 1 to indicate load 'P 1'
                    ok = self._loader.load_subsys_by_name("AUDIO", "AUDIO", mem = audio_ss.p1.dm,
                                                          layout_info=audio_ss.p1.info.layout_info, processor=1) 
                    if not ok:
                        # Audio P1 isn't required
                        iprint("WARNING: No Audio subsystem P1 dump found")
                    else:
                        audio_ss.p1.has_data_source = True

            except XCD3FormatCheckException:
                iprint("WARNING: No Audio subsystem found whilst trying to determine coredump format.")
        
        return True

    def close(self):
        """
        Python3 does not like leaving the xcd3 filestream open;
        so we need an explicit close
        """
        
        self._loader.close()

# xcd2 core file loader

class xcd2error(Exception):
    """
    Exception class for syntactic problems detected in XCD2-format
    files/blocks
    """
    def __init__(self,value):
        self.value = value
        
    def __str__(self):
        return (self.value)

class _ShortDirectiveData(Exception):
    """
    A directive's data block was shorter than the length field.  For some 
    directives this is OK.
    """

class XCD2Loader(object):
    """ Loads an xcd2-format core dump from a supplied filestream
    """
    @property
    def fw_build_id(self):
        """Build ID if any found, else None (of last loaded subsystem)"""
        return self._fw_build_id
    
    @property
    def fw_build_string(self):
        """Build String if any found, else None (of last loaded subsystem)"""
        return self._fw_build_string
    
    @property
    def unmapped_regs(self):
        return self._unmapped_regs

    xap_reg_mem_map = {
      "AH" : 0xffe0, 
      "AL" : 0xffe1,
      "UXH" : 0xffe2, 
      "UX" : 0xffe3, 
      "UXL" : 0xffe3, 
      "UY" : 0xffe4, 
      "IXH" : 0xffe5, 
      "IX" : 0xffe6, 
      "IXL" : 0xffe6, 
      "IY" : 0xffe7,
      "FLAGS" : 0xffe8
     }
    
    
    def __init__(self,mem,filestream,strict_syntax=False,quiet=False,version=0,
                 layout_info=None):
        self.mem = mem
        self.filestream = filestream
        self.strict_syntax = strict_syntax
        self.bank_select_stack = []
        self.quiet = quiet
        self.version = version
        self.layout_info = layout_info
        
        self.ignore_list = ("CM","AV","AT","XCD2", "XCD1","#") #Directives we don't 
        #care about
        self.warn_list = ("P") #Directives we don't understand
            
        self.skip_list = ("DS","DE","DJ","DP") #Directives with hex data 
        #following that we want to skip
        self.cur_block = None
        self._unmapped_regs = {}
    
    ##############################################################
    # Utility functions
    def _report(self,message):
        if not self.quiet:
            iprint(message)
    
    ##############################################################
    # State checks
    def _check_inside_block(self):
        return self.cur_block and not self.cur_block.finished()

    def _check_directive_expected(self):
        """ Check that receiving a directive in the current line is valid
        """
        if self.cur_block:
            self.cur_block.assert_finished(True)

    def _check_completion_valid(self):
        """ Check that the object is in a state that could correspond to completion
        """
        if self.cur_block:
            self.cur_block.assert_finished(False)
        if self.bank_select_stack:
            warning = "Finished read without having exited banked mode!"
            if self.strict_syntax:
                raise xcd2error(warning)
            else:
                self._report("Syntax warning: %s" % warning)
    ##############################################################

    ##############################################################
    # Workhorse functions for particular types of load
    #
    #Potential extension:: figure out what to do with an explicit CPU register directive
    def _load_cpu_register(self,line_words):
        if len(line_words) != 2:
            raise xcd2error("Encountered CPU register directive with %d args!" % 
                            len(line_words))
        #Look up the memory-mapped address in the XAP register dictionary and write it
        try:
            addr = self.xap_reg_mem_map[line_words[0]]
            self._load_word([addr,line_words[1]])
        except KeyError:
            self._unmapped_regs[line_words[0]] = int(line_words[1], 16)

            #Some of the possible XAP register directives don't correspond to anything
            #in the memory map.
            self._report("Ignoring XAP register %s" % line_words[0])

    #Write a single word to memory at the supplied address
    def _load_word(self,line_words):
        if len(line_words) != 2:
            raise xcd2error("Encountered register directive with %d args!" % 
                            len(line_words))

        address = line_words[0]
        value = line_words[1]
        if not isinstance(address, int_type):
            address = int(address,16)
        if not isinstance(value, int_type):
            value = int(value,16)

        self._report("xcd2loader: Loading 0x%04x into address 0x%04x" % (value,address))
        # Serialise the word into bytes to make it suitable for writing to the
        # raw interface
        bytes = self.layout_info.serialise(value, 
                                           self.layout_info.data_word_bits //
                                             self.layout_info.addr_unit_bits)
        end_address = address + len(bytes)
        self.mem[address:end_address] = bytes
        

    #Set up flags for a new bank select section
    def _enter_bank_select_mode(self,line_words):
        #We could use the bank select register and number of blocks
        #that are specified on this line as checks against what follows
        #but there doesn't seem much point.        
        self._report("xcd2loader: Entering bank-select mode")
        self.bank_select_stack.append((int(line_words[0],16),
                                          int(line_words[1],16)))

    #Loads a single bank select register setting
    def _load_bank_select(self,line_words):
        
        if len(line_words) != 5:
            raise xcd2error("Register write directive has %d arguments!" % len(line_words))
        
        #Write the value specified to the address specified after masking it
        address = int(line_words[0],16)
        if self.bank_select_stack and self.strict_syntax: 
            bank_select_addr, _ = self.bank_select_stack[-1]
            if address != bank_select_addr:
                raise xcd2error("Address for bank select register load (0x%04x) doesn't match current bank's select register address (0x%04x)!" % 
                                (address,bank_select_addr))
                
        value = int(line_words[4],16)
        mask = int(line_words[2],16)
        value = value & mask #Is the mask supposed to be "positive" or "negative"?  Let's assume positive.
        self._load_word([address,value])
            
    #Sets flags etc indicating current bank select has been completed
    def _exit_bank_select_mode(self):
        if self.bank_select_stack:
            # Restore the bank select register to its dump-time value
            self._load_word(self.bank_select_stack.pop())
            if not self.bank_select_stack:
                self._report("xcd2loader: Leaving bank-select mode")
        else:
            warning = "Unmatched end bank-select directive"
            if self.strict_syntax:
                raise xcd2error(warning)
            else:
                self._report("Syntax warning: %s" % warning)

    def _report_unknown_directive(self,line_words):
        self._report("Warning: directive %s not implemented: ignoring" % line_words[0])
        
    # End of load functions
    ################################################################################

    ###########################################################################
    # Start of version handling functions
    def _set_version_number(self,line_words):
        #We will either see an 8-digit hex number, or a 3-4 or 9+ digit
        #decimal, assuming nobody with a two-digit UID has built the Curator
        #firmware before now (which shouldn't have happened - Unix UIDs between
        #1 and 100 (at least) are conventionally reserved for system use).
        if len(line_words[0]) == 8:
            self._fw_build_id = int(line_words[0],16)
        else:
            self._fw_build_id = int(line_words[0],16)
        if self.version != 0:
            if self.version == self._fw_build_id:
                self._report("xcd2loader: Firmware version IDs match.")
            else:
                raise xcd2error("ERROR: fw on chip has version ID %d, but coredump reports version ID %d" % 
                                (self.version,self._fw_build_id))
            
    def _set_version_string(self,line_words):
        self._fw_build_string = " ".join(line_words)
        if self._fw_build_string[0] == '"' and self._fw_build_string[-1] == '"':
            # Strip off leading and trailing quotes
            self._fw_build_string = self._fw_build_string[1:-1]
        self._report("Dump firmware version string: %s" % self._fw_build_string)
        

    def _check_for_version_comments(self,line_words):
        if line_words[0] == "Build":
            if line_words[1] == "string:":
                self._set_version_string(line_words[2:])
            elif line_words[1] == "ID:":
                self._set_version_number(line_words[2:])
    # End of version checking functions
    ###########################################################################
    
    ################################################################################
    # Dispatch method
    # Expects to receive a list of words constituting a single line of the file
    def _parse_line(self,line):
        """ Examines the next line in the current context
        """
        #Ignore empty lines and unimportant directives
        if not line:
            self._report("Syntax warning: Empty line!")
            return
         
        try:
            first_token, line_remainder = line.split(None, 1)
        except ValueError:
            first_token = line.rstrip()
        
        if self._check_inside_block():
            try:
                self.cur_block.load_line(line)
            except _ShortDirectiveData as e:
                if self.cur_block.allow_short:
                    # In this case cur_block knew it might see less data than
                    # indicated, so we should try the line again
                    self._parse_line(line)
                else:
                    raise xcd2error("Data for previous directive unexpectedly "
                                    "truncated by '%s'" % line.rstrip())

        elif first_token == "DD":
            #RAM block
            self._check_directive_expected()
            self.cur_block = self.DataBlock( self.mem,self.quiet,line,
                                                          self.layout_info)
        elif first_token == "DC":
            #Constants block
            self._check_directive_expected()
            self.cur_block = self.DataBlock(self.mem,self.quiet,line,
                                            self.layout_info)
        elif first_token[0] == "#":
            # Comments of this sort can occur at any time,
            # so don't even check if we're at the end of a block.
            pass
        elif first_token in self.ignore_list:
            self._check_directive_expected()
            #Horrible hack to work around my old horribly hacky way of inserting
            #the version number/string
            if first_token == "CM":
                self._check_for_version_comments(line_remainder.split()) 
        elif first_token in self.warn_list:
            self._check_directive_expected()
            self._report_unknown_directive(line.split())
            
        elif first_token == "R":
            #CPU register
            self._check_directive_expected()
            self._load_cpu_register(line_remainder.split())
            
        elif first_token == "RR":
            #Single register
            self._check_directive_expected()
            self._load_word(line_remainder.split())

        elif first_token == "DR":
            #Register block
            self._check_directive_expected()
            self.cur_block = self.DataBlock(self.mem,self.quiet,line,
                                            self.layout_info)

        elif first_token in self.skip_list:
            #Various kinds of address space we don't know/care about
            self._check_directive_expected()
            self.cur_block = self.DataBlock(self.mem,self.quiet,
                                            line, self.layout_info,
                                            ignore=True,
                                            # DP directive can provide less data
                                            # than the length field indicates
                                            allow_short=(first_token=="DP"))

        elif first_token == "DK":
            #Keyhole memory
            self._check_directive_expected()
            self.cur_block = self.DataBlock(self.mem,self.quiet,
                                            line, self.layout_info,
                                            ignore=True,nargs=5)
        elif first_token == "BS":
            #Bank select
            self._check_directive_expected()
            self._enter_bank_select_mode(line_remainder.split())
        elif first_token == "RW":
            #Write register (for bank selection)
            self._check_directive_expected()
            self._load_bank_select(line_remainder.split())
        elif first_token == "BE":
            #End of bank select
            self._check_directive_expected()
            self._exit_bank_select_mode()
            
        elif first_token == "II":
            self._check_directive_expected()
            self._set_version_number(line_remainder.split())
        elif first_token == "IS":
            self._check_directive_expected()
            self._set_version_string(line_remainder.split())
            
        else:
            raise xcd2error("Can't parse line: %s" % line)
    
    def loaduntil(self,term_token=None):
        """ Read from the internal filestream until the supplied token is encountered
        """
        self._fw_build_id = None
        self._fw_build_string = None
        
        try:
            for line in self.filestream:
                try:
                    line = line.decode("utf-8").lstrip()
                except AttributeError:
                    line = line.lstrip()
                except UnicodeDecodeError:
                    if line[0:2] == b"IS":
                        self._set_version_string(["Corrupted version string"])
                        continue
                if term_token and line.startswith(term_token):
                    break
                self._parse_line(line)
                
            self._check_completion_valid()
            return True

        except xcd2error as e:
            iprint("Syntax error: %s. Exiting." % str(e))
            return False
        
    
    def load(self):
        """ Read the entire internal filestream to the end
        """
        return self.loaduntil()

    class DataBlock(object):
        """Encapsulate state of a single data block as it's parsed.
        Parsing is actually in xcdloader._parse_line."""

        def __init__(self,mem,quiet,line,layout_info,ignore=False,nargs=2,
                     allow_short=False):
            """Sets up a data-block load.
            If ignore, sets flag to read without loading."""
            line_words = line.split()
            self.name = line_words[0]
            line_words = line_words[1:]
            self.mem = mem
            self.layout_info = layout_info
            if len(line_words) != nargs:
                raise xcd2error("Encountered data block directive with %d args when %d expected!" % (len(line_words),nargs))

            self.ignore = ignore
            self.allow_short = allow_short

            self.start_address = int(line_words[0],16)
            self.tot_length = int(line_words[1], 16)
            if not self.ignore:
                if not quiet:
                   iprint("xcd2loader.DataBlock: Loading %s, length 0x%s, starting at 0x%s" % (self.name,line_words[1],line_words[0]))
                self.address = self.start_address
            else:
                self.address = 0

            self.cur_length = self.tot_length
            self._line_bytes = []
            self.n_bytes = (self.layout_info.data_word_bits //
                            self.layout_info.addr_unit_bits)


        def load_line(self,line):
            """Writes a line (arbitrary length) of *addressable units* into memory.
            Unless in ignore mode, in  which case they are, well, ignored."""

            line_words = line.split()
            length = len(line_words)
            # Check that the length of the line to write fits into the
            # remaining space in the block
            if self.cur_length < length:
                self.block_error("Data block length specification too short for actual data block!")

            if length > 0 and len(line_words[0]) < 4:
                # Probably encountered a directive
                if self.allow_short:
                    self.cur_length = 0
                raise _ShortDirectiveData

            for word in line_words:
                self._line_bytes += self.layout_info.serialise(int(word,16), self.n_bytes)
            # Adjust remaining length of block
            self.cur_length = self.cur_length - length
            if self.cur_length == 0 and not self.ignore:
                #write the values
                try:
                    self.mem[self.address:self.address+len(self._line_bytes)] = self._line_bytes
                except AddressSpace.NoAccess:
                    # This part of the address space isn't legal in the pydbg
                    # chip model.  That's not a problem - the coredump contains
                    # quite a lot of meaningless addresses, e.g. in between
                    # register buses.
                    pass


        def finished(self):
            """Check the data block has been fully read."""
            return self.cur_length == 0

        def block_error(self, message):
            """Raise an error on the data block, with information about it."""
            raise xcd2error("%s\nBlock declared as %s %04x %04x" %
                            (message, self.name,
                             self.start_address, self.tot_length))

        def assert_finished(self, more_data):
            """Ensure block is finished.
            If more_data, there's another directive coming.
            Otherwise, there's no more data at all left, except if the dump
            ended with a directive that could be given short data."""
            if self.cur_length != 0 and (more_data or not self.allow_short):
                if more_data:
                    msg = "Premature directive!"
                else:
                    msg = "Finished read while inside a data block!"
                msg += "\nRemaining length of current data block is %d" % (self.cur_length)
                self.block_error(msg)

#
# xcd3 core file loader

class XCD3HeaderParseError(RuntimeError):
    pass

class XCD3FormatCheckException(RuntimeError):
    pass

class XCD3Loader(object):
    """ Loads an xcd3 core dump file by finding the appropriate
        section and sending it to the xcd2 loader
        
        We can either receive a filename, in which case we open
        and close the stream ourselves, or else an already-opened
        stream which we don't close.
        
        We can receive a memory object for writing to either in the
        constructor, which becomes the default, or as an optional
        argument to the subsystem load function.  This enables memory 
        writes to be done in different ways for different subsystem
        loads.  All that is required of the 

        Note: with one exception, we report status via boolean return
        values.  The exception is when we can't open the file we've
        been passed.
        
        
        Potential extension:: implement loading of XAP registers
        Potential extension:: redesign this class 
        - the file vs stream stuff is a mess?
        - what are the invariants? too much conditional stuff.
        - where is the format/header parsed/checked?
    """
    
    def __init__(self,mem,filename = None,strict_syntax=False,quiet=False):

        self.mem = mem        
        self.filename = filename
        self.strict_syntax = strict_syntax
        self.quiet = quiet
        self._subloader = None
        
        if filename:
            self.close_on_exit = True
            if filename == '-':
                stash_stdin(self)
        else:
            self.close_on_exit = False
        
    def _report(self,message):
        if not self.quiet:
            iprint(message)
    
    def _set_filestream(self,filestream):
        if filestream:
            self.filestream = filestream
            self.close_on_exit = False
        elif self.filename:
            if self.filename == '-':
                self.filestream = io.BytesIO(self.buffer)
            else:
                try:
                    if (hasattr(self, 'filestream') and self.filestream and
                            self.close_on_exit):
                        self.filestream.close()
                    
                    self.filestream = open_bytes_compressed(self.filename)
                except IOError:
                    iprint("XCD3Loader: ERROR: Couldn't open %s" % self.filename)
                    raise
        else:
            iprint("XCD3Loader: ERROR: No filename or stream given; aborting")
            return False
        
        return True    

    def read_header(self, filestream = None):
        """\
        Read the xcd3 file header and extract tags
        """        
        # XCD3
        # AT MULTI
        # AV 01440000     # NB trailing 0000?
        #
        # Or 
        #
        # XCD3
        # # Linux Coredump V 0.0.0.5100 Test (1541135)
        # AT MULTI
        # AV 00002044

        if not self._set_filestream(filestream):
            raise RuntimeError("Failed to set filestream (whatever that means)")

        # Horrible manual parse 3 lines
        
        fields = {}
        
        state = 'XCD3'
        
        for rawline in self.filestream:
            
            # Decode our bytes object into a string-like one.
            line = rawline.decode("utf-8").strip()

            # First line must be 'XCD3' (not comment nor blank)
            if state == 'XCD3':                
                if line == 'XCD3':
                    state = 'AT'
                    continue
                else:
                    if re.match(r"XCD\d", line):
                        raise XCD3HeaderParseError("Expected an XCD3 file: "
                                                   "format appears to be %s" % line)
                    raise XCD3HeaderParseError("XCD3: expected 'XCD3' Found '%s'" % line)
                
            # Skip blank lines
            if not line:
                continue
            
            # Skip comment lines
            if line[0] == '#':
                continue
            
            # Split remaining lines into words
            words = line.split()
            
            if state == 'AT':
                if words[0] == 'AT':
                    fields['AT'] = words[1]
                    state = 'AV'
                    continue
                else:
                    raise XCD3HeaderParseError("XCD3: expected 'AT ...' Found '%s'" % str(words))
                
            elif state == 'AV':
                if words[0] == 'AV':
                    fields['AV'] = words[1]
                    state = 'DONE'
                    break
                else:
                    raise XCD3HeaderParseError("XCD3: expected 'AV ...' Found '%s'" % str(words))
        
        return fields

    def _seek(self, tag, rewind=False):
        """Read the file as far as the tag
        Checks rewind option to get file pointer to exact same spot where tag is found .
        """
        while True:
            if rewind == True:
                fp = self.filestream.tell()
            try:
                line = self.filestream.readline().decode('utf-8')
            except UnicodeDecodeError:
                # Can't decode line so carry on
                pass

            if not line:
                return False
            if line.startswith(tag):
                if rewind == True:
                    self.filestream.seek(fp, 0)
                return True
        return False

    @property
    def subloader(self):
        return self._subloader

    def _load(self, end_marker, version, layout_info):
        # call the xcd2 loader to load until the end marker
        self._report("XCD3Loader: Firing xcd2loader.")
        subloader = XCD2Loader(self.mem,self.filestream,self.strict_syntax,self.quiet,version,
                               layout_info = layout_info)
        #Get the subloader to read until it encounters the next subsystem marker or the end of the file
        ret = subloader.loaduntil(end_marker)
        self._subloader = subloader
        # Horrid hack to retrieve fw build id
        try:
            self.fw_build_id = subloader.fw_build_id
            self.fw_build_string = subloader.fw_build_string
        except AttributeError:
            #If we didn't find build info, we can't save it...
            pass
        return ret

    def is_coredump_old_format(self, tag, filestream = None):
        """Get the first address from the coredump.
        If it starts with 8 then it is definitely old format using TRB addresses. Return True.
        Other return False specifying it is new format.
        """
        #Open filestream
        if not self._set_filestream(filestream):            
            return False
        
        self._report("XCD3Loader: Set filestream")

        coredump_old = False

        if self._seek('SS '+tag):
            if self._seek('DD', True):# rewind set to get file pointer to be at DD line on return

                line = self.filestream.readline().decode("utf-8")
                l = line.split()
                if l[1][0] == "8":
                   coredump_old = True
                if l[1][0] == "0":
                   coredump_old = False
            else:
                raise XCD3FormatCheckException("XCD3Loader: No DD Found")
        else:
            raise XCD3FormatCheckException("XCD3Loader: No SS Found")

        if self.close_on_exit:
            self.filestream.close()
            
        return coredump_old

    def _load_subsystem(self, subsys_tag, version, layout_info, processor=None):
        # Read the file as far as the subsys_tag and then call the xcd2 reader
        if self._seek('SS '+subsys_tag):
            if processor is not None:
                if processor == "":
                    # processor is an empty string, so load up to 'P 0'
                    return self._load('P 0', version, layout_info)
                elif self._seek('P ' + str(processor)):
                    return self._load('P 1' if processor == 0 else 'SS',version,layout_info)
                else:
                  iprint("XCD3Loader: Couldn't find 'P %s' directive in core file!" % processor)
                  return False
            else:
                return self._load("SS", version, layout_info)
        else:
            iprint("XCD3Loader: Couldn't find 'SS %s' directive in core file!" % subsys_tag)
            return False #Tag wasn't found

    def load_subsys_by_name(self,name,tag,filestream = None,mem = None, version = 0,
                            layout_info=None, processor=None):
        """Load the section corresponding to the SS tag
        Returns (fw-id, fw-string) if it can find them.
        """
        #Use non-default memory if requested
        if mem is not None:
            orig_mem = self.mem
            self.mem = mem
        
        self._report("XCD3Loader: Loading %s core dump..." % name)
        
        if not self._set_filestream(filestream):
            if mem is not None:
                self.mem = orig_mem
            return False
        
        self._report("XCD3Loader: Set filestream")

        return_val = self._load_subsystem(tag, version, 
                                          layout_info, processor)
        
        if return_val:
            self._report("XCD3Loader: %s core dump load succeeded" % name)            
        else:
            self._report("XCD3Loader: %s core dump load failed" % name)

        if self.close_on_exit:
            self.filestream.close()
            
        if mem is not None:
            self.mem = orig_mem
        return return_val

    def close(self):
        """
        Python3 does not like leaving the xcd3 filestream open;
        so we need an explicit close
        """
        
        if self.close_on_exit:
            try:
                self.filestream
            except AttributeError:
                pass
            else:
                self.filestream.close()
                self.filestream = None

class XCD1DeviceLoader(object):
    """
    Load an XCD1 file using the XCD2 loader 
    """
    def __init__(self, filename):
        
        self._filename = filename
        self._chip_version = None

        if filename == '-':
            stash_stdin(self)

    def _load(self, mem):
        if self._set_filestream():
            xcd2loader = XCD2Loader(mem, self.filestream, layout_info=XapDataInfo(),
                                    quiet=True)
            return xcd2loader.load(), xcd2loader.fw_build_id

        # Error raised or reported by set_filestream
        return False

    def _set_filestream(self):
        if self._filename:
            if self._filename == '-':
                self.filestream = io.BytesIO(self.buffer)
            else:
                try:
                    self.filestream = open_bytes_compressed(self._filename)
                except IOError:
                    iprint("XCD1Loader: ERROR: Couldn't open %s" % self._filename)
                    raise
        else:
            iprint("XCD1Loader: ERROR: No filename or stream given; aborting")
            return False

        return True

    @property
    def chip_version(self):
        
        if self._chip_version is None:
            # Read the dump into a big array so we can grab the chip version
            # register
            mem = [0xffff]*0x10000
            self._load(mem)
            self._chip_version = mem[0xfe81]
             
        return self._chip_version
        
    def load_into_device(self, device, write_readonly_registers=True):
        
        loaded, build_id = self._load(device.chip.spi_in)
        if loaded:
            device.chip.core.dump_build_id = build_id
            device.has_data_source = True
        return loaded
