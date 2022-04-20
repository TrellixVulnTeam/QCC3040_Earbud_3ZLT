############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import re
from csr.wheels import iprint, CLang, global_streams as gstrm, timeout_clock

class IsLoadableToRAM(object):

    def load_to_ram(self, quiet=False, skip_if_loaded=True, verify_load=True, run_control=True):
        """
        Simple firmware loader.  Writes contents of loadable ELF sections to
        their given physical addresses, assuming that this can be achieved by
        a simple write operation (i.e. the destination is RAM).
        """
        def verify_section(start_addr, end_addr, data, chunk_size=None):
            if chunk_size is None:
                return self._core.data[start_addr:end_addr] == data[:]
            
            # read in chunks to trade off between efficiency of access vs
            # cost of reading past the first difference, where differences
            # are expected
            read = 0
            remaining = len(data)
            while remaining:
                nbytes_to_read = min(chunk_size, remaining)
                if self._core.data[start_addr+read:
                                   start_addr+read+nbytes_to_read] != data[read:read+nbytes_to_read]:
                    return False
                read += nbytes_to_read
                remaining -= nbytes_to_read
            return True

        if run_control:
            self._core.pause()
        for section in self.build_info.elf_code.sections:
            if not quiet:
                iprint("Loading section '{}': {} bytes at 0x{:x}".format(section.name, len(section.data), section.paddr))

            section_data = section.data.tolist()
            start_addr = section.paddr
            end_addr = section.paddr+len(section.data)
            if not skip_if_loaded or not verify_section(start_addr, end_addr, section_data, chunk_size=None):
                self._core.data[start_addr:end_addr] = section_data
                if verify_load and not verify_section(start_addr, end_addr, section_data):
                    raise RuntimeError("Section failed to verify")
            else:
                if not quiet:
                    print("Skipping: section already loaded")

        if run_control:
            self._core.pc = self.env.elf.get_program_entry_point()


class SupportsVVTest(IsLoadableToRAM):
    """
    Mixin that allows "vv_log"-style log messaging to be scraped out of the
    given buffer (defaults to "OutBuffer_cpu").

    This inherits from IsLoadableToRAM because it is expected that vv_test
    firmware will always be RAM-loadable.

    Some details from vv_msg.h:

    /// Message Code Format:
    ///
    /// +------+----------+-----------------+---------------------------------+
    /// | SEV  |  STATE   |   FILE_NUMBER   |           LINE_NUMBER           |
    /// +------+----------+-----------------+---------------------------------+
    /// 31   29 28      24 23             16 15                              0
    ///
    ///
    /// SEV         - 3-bit message severity (8 possible values; see enum).
    ///
    /// STATE       - 5-bit message state is five bits (32 possible values; see
    ///               enum).
    ///
    /// FILE_NUMBER - 8-bit file number (256 possible files).  The '#define'
    ///               VV_MSG_FILE_NUMBER MUST exist in each source code file
    ///               that uses vv_msg().
    ///
    ///               File numbers are grouped into the following categories:
    ///
    ///               Values 0-127 are for use by test code (128 files).
    ///
    ///               Values 128-191 are reserved for common test code (64 files).
    ///
    ///               Values 192-255 are reserved for QNPL code (64 files).
    ///
    /// LINE_NUMBER - 16-bit line number (allows files up to 64K lines).
    ///

    typedef enum
    {
        SEV_ERROR       = 0,
        SEV_WARNING     = 1,
        SEV_INFO        = 2,
        SEV_FATAL       = 3,
        SEV_DEBUG       = 4
    
    } VV_MSG_SEVERITY;
    
    //******************************************************************************
    /// @brief QNPL Message States
    ///
    /// The maximum number of states supported is 32 (0-31) since the cooresponding
    /// field in the message code is only 5-bits wide.
    //******************************************************************************
    typedef enum
    {
        ST_BOOT        = 0,
        ST_INIT        = 1,
        ST_FUNCTION    = 2,
        ST_SUBFUNCTION = 3,
        ST_IRQ         = 4,
        ST_FIQ         = 5,
        ST_EXCEPTION   = 6
    
    } VV_MSG_STATE;
    """

    VV_MSG_SEVERITY = {0:"ERROR", 1:"WARNING", 2:"INFO", 3:"FATAL", 4:"DEBUG"}
    VV_MSG_STATE = {0:"BOOT", 1:"INIT", 2:"FUNCTION", 3:"SUBFUNCTION", 
                    4:"IRQ", 5:"FIQ", 6:"EXCEPTION"}

    SUPPORTED_SEMIHOSTING_TYPES = ("BufferE",)
    VVTEST_SEMIHOSTING_TYPE = None

    def _decode_vv_msg(self, msg):
        """
        Decode a vv message that is a hex-formatted 32-bit number with a trailing
        colon based on the explicit vv_msg encoding.  Pass other strings through,
        with a trailing newline as the basic structure of a line is <id>: <msg>
        output in two stages.
        """
        match = re.match("^([a-fA-F0-9]{8}):\s*$",msg)
        if not match:
            return msg + "\n" # arbitrary text
        msg_code = int(match.group(1),16)
        sev_code = msg_code >> 29
        sev = self.VV_MSG_SEVERITY.get(sev_code, "UNKNOWN ({})".format(sev_code))
        state_code = (msg_code>>24)&0x1f
        state = self.VV_MSG_STATE.get(state_code, "UNKNOWN ({})".format(state_code))
        filenum = (msg_code>>16)&0xff
        linenum = (msg_code)&0xff
        return "{}: {}: (file:{} line:{}) :: ".format(sev, state, filenum, linenum)


    def vv_test(self, **semihost_args):
        """
        Load firmware to RAM and then execute vv_test_run.

        For description of parameters see vv_test_run.
        """
        semihost_type = semihost_args.pop("semihosting_type", self.VVTEST_SEMIHOSTING_TYPE)
        if semihost_type is None:
            raise TypeError("{} should define an attribute VVTEST_SEMIHOSTING_TYPE to specify"
                            " the default semi-hosting method.  Possible values are {}".format(
                                    self.__class__, ", ".join(self.SUPPORTED_SEMIHOSTING_TYPES)))
        if semihost_type not in self.SUPPORTED_SEMIHOSTING_TYPES:
            raise NotImplementedError("Semi-hosting type '{}' not supported. Supported types are {}".format(
                    semihost_type, ", ".join(self.SUPPORTED_SEMIHOSTING_TYPES)))

        self.load_to_ram()
        if semihost_type == "BufferE":
            self.vv_test_run_buffere(**semihost_args)
        else:
            assert False, ("Semi-hosting type '{}' listed as supported but no run command "
                            "implemented!".format(semihost_type))

    def vv_test_run_buffere(self, tohost_buf="OutBuffer_cpu", 
                            out_stream=gstrm.iout, timeout=None):
        """
        Run the processor and scrape out contents of the supplied vv_msg 
        buffer symbol, using Trace32's "BufferE" protocol

        :param tohost_buf, optional: Name of the buffer into which the vv_msg
         strings are written.  Defaults to 'OutBuffer_cpu'.
        :param out_stream, optional: Open file-like object to which the vv_msg
         strings are output.  Defaults to global_stream.iout which itself 
         defaults to sys.stdout.
        :param timeout, optional: Timeout in seconds after a message has been
         received after which the processor is halted.  Provides a simple
         auto-stop feature.  If not provided, the processor must be halted
         manually by the user typing Ctrl-C.
        """

        try:
            outbuffer = self.env.vars[tohost_buf]
        except KeyError as exc:
            raise ValueError("vv_msg buffer symbol '{}' "
                             "not found".format(tohost_buf))
        # The first word of the buffer is used as a length field and the remainder as
        # a character buffer of length indicated in the length field. 
        length = self.env.cast(outbuffer.address, "unsigned int")
        length.value = 0
        self._core.run()

        def read_and_clear_buffer():
            current_length = length.value
            if  current_length != 0:
                # Construct an object representing the populated part of the character buffer
                outbuf = self.env.cast(outbuffer.address + length.size, 
                                       "unsigned char", 
                                       array_len=current_length)
                data = outbuf.value
                # First clear the characters we've just read
                outbuf.value = [0]*current_length 
                # Then clear the length word, allowing the firmware to write more
                # characters.  DON'T CHANGE THE ORDER!
                length.value = 0

                # Convert the character values to characters
                return CLang.get_string(data)

        try:
            if timeout is not None:
                t0 = timeout_clock()
            while True:
                string = read_and_clear_buffer()
                if string is not None:
                    out_stream.write(self._decode_vv_msg(string))
                    if timeout is not None:
                        # We got a log message so restart the timer
                        t0 = timeout_clock()
                elif timeout is not None and timeout_clock() - t0 > timeout:
                    # It has been a long time since we got 
                    # a log message - assume we're done
                    break
        except KeyboardInterrupt:
            pass

        self._core.pause()




