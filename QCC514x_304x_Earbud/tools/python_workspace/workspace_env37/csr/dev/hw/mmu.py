############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
CSR Memory Management Unit Components.
"""
# This module could do with splitting up but we are stuck with the API names
#pylint: disable=too-many-lines, invalid-name

import csv
import time
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.wheels import PureVirtualError, open_or_stdout, detect_ctrl_z
from csr.dev.model.base_component import BaseComponent
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.hw.register_field.register_field import AdHocBitField
from csr.wheels.bitsandbobs import pack_unpack_data_le, bytes_to_words, \
    words_to_bytes, hex_array_str_columns
from csr.dev.hw.address_space import AddressSlavePort, WriteRequest,\
    AddressSpace
from csr.dev.hw.core.meta.i_layout_info import XapLEDataInfo, Kalimba32DataInfo
from csr.dev.model.interface import Group, Code


try:
    # Python 2
    int_type = (int, long)
except NameError:
    # Python 3
    int_type = int

class MMUSize(object):
    """constants for sizes of MMU blocks"""
    #pylint:disable=bad-whitespace,too-few-public-methods
    MMU_BUFFER_SIZE_128                      = 0x0
    MMU_BUFFER_SIZE_256                      = 0x1
    MMU_BUFFER_SIZE_512                      = 0x2
    MMU_BUFFER_SIZE_1024                     = 0x3
    MMU_BUFFER_SIZE_2048                     = 0x4
    MMU_BUFFER_SIZE_4096                     = 0x5
    MMU_BUFFER_SIZE_8192                     = 0x6
    MMU_BUFFER_SIZE_16384                    = 0x7
    MMU_BUFFER_SIZE_32K                      = 0x8
    MMU_BUFFER_SIZE_64K                      = 0x9
    MMU_BUFFER_SIZE_128K                     = 0xA
    MMU_BUFFER_SIZE_256K                     = 0xB
    MMU_BUFFER_SIZE_512K                     = 0xC


class MMU(BaseComponent):
    """\
    Memory Management Unit (Abstract Base)

    Just a placeholder till it becomes clear what, if anything, all types
    of MMUs have in common.
    """

    # BaseComponent compliance

    @property
    def title(self):
        return 'MMU'


class HydraMMU(MMU):
    """\
    Hydra Memory Management Unit
    """
    #pylint: disable=too-many-instance-attributes, too-many-public-methods
    # Potential extension:: Replace memory_type + memory_offset with a single
    # memory region property.

    def __init__(self, subsystem):
        """\
        Construct a HydraMMU.

        Params:-
        - subsystem: The subsystem containing the MMU control registers
        and memory (shared or otherwise).

        Notes:- - Register access is probably OK via the first core's memory
        map in both single and multi-cpu subsystems.
        """
        self._subsystem = subsystem
        self._initRegs()

        # Derived classes may set a dictionary whose
        # usage is subsystem specific, e.g. keyed by
        # 'used', 'unused', 'percent_used', otherwise it is empty.
        self._dynamic_mmu_pages = {}

        # Derived classes can set a dictionary describing the handles;
        # usage is subsystem specific, e.g. keyed by
        # 'used', 'unused', 'precent_used',
        # 'name', 'start', 'end', 'size', otherwise it is empty.
        self._mmu_handles = {}

        # Derived classes can set a list of static mmu pages.
        self._static_mmu_pages = None

        # Derived classes can set a dictionary describing wastage;
        #usage is subsystem specific, e.g. keyed by
        # 'name', 'start', 'end', 'size.
        self._mmu_alignment_wastage = None

        # Derived classes can set a dict describing the free list;
        # usage is subsystem specific, e.g. keyed by
        # 'name', 'start', 'end', 'size.
        self._mmu_freelist = None

    def __str__(self):
        """Pretty print."""
        # Dumps the registers
        return "MMU< regs %s >" % repr(self.regs)

    # MMU compliance

    @property
    def subcomponents(self):
        # Subcomponents we want reports to visit...
        return {"freeList" : "_freeList"}

    def handleTable_refresh(self):
        """
        Refresh the Handle Table - clear the handleTable so that
        all the cached info about particular handles is forced
        to be refreshed, and reread the MMU_PROC_* registers
        """
        if hasattr(self, "_handleTable"):
            delattr(self, "_handleTable")
        self._initRegs()

    def _on_reset(self):
        """
        When the device is reset, clear the handleTable so that all the cached
        info about particular handles is forced to be refreshed, and reread the
        MMU_PROC_* registers
        """
        # This is done by invoking the handleTable_refresh which does the
        # same functionality.
        self.handleTable_refresh()

    # Extensions

    # Pure virtual properties that depend on the implementation.
    # Values are provided by the child classes overriding these
    # functions - e.g. SINGLE_XAP_MMU or DUAL_XAP_MMU
    @property
    def subsystem(self):
        """The subsystem that this MMU is for"""
        # This need not be restricted to being protected and making it public
        # stops pylint moaning about _protected_access.
        return self._subsystem

    @property
    def page_size(self):
        """\
        MMU page size in octets.
        """
        raise PureVirtualError(self)

    @property
    def default_num_handles(self):
        """
        A per-subsystem constant equal to the "normal" number of handles that
        subsystem's firmware allocates
        """
        raise PureVirtualError(self)

    @property
    def num_handles(self):
        """
        The true number of handles the firmware has allocated.  Defaults to
        default_num_handles if no firmware information is available
        """
        return self.default_num_handles

    @property
    def num_handle_bits(self):
        """
        The number of bits in an MMU handle.  Has been 64 for a long time now.
        """
        return 64

    @property
    def max_page_number(self):
        """The maximum page number supported by this MMU"""
        raise PureVirtualError(self)

    @property
    def mmu_data(self):
        """
        The Data-space containing MMU tables etc.
        May be shared.
        """
        raise PureVirtualError(self)

    @property
    def memory_offset(self):
        """\
        What is offset of MMU data wrt mmu_data ?
        FIXME: hide the offset by making mmu_data a SubRegion
        """
        raise PureVirtualError(self)

    @property
    def regs(self):
        """List of related hardware registers."""
        return self.freeList.regs + self._miscRegs

    @property
    def handleTable(self):
        """The Buffer Handle Table"""
        # Construct lazily
        try:
            self._handleTable
        except AttributeError:
            self._handleTable = MMUHandleTable(self)
        return self._handleTable

    @property
    def freeList(self):
        """The Free Page List"""
        # Construct lazily
        try:
            self._freeList
        except AttributeError:
            self._freeList = MMUFreeList(self)
        return self._freeList

    @property
    def _layout_info(self):
        raise PureVirtualError(self)

    @property
    def layout_info(self):
        """The layout information for the memory for self._core"""
        # There's nothing particularly protected about the layout so make it
        # public to prevent lots of protected-access messages from pylint.
        return self._layout_info

    @property
    def octets_per_word(self):
        """Number of octets in a word on this processors"""
        return self._layout_info.addr_unit_bits // 8

    @property
    def _core(self):
        """Core containing MMU control registers"""
        try:
            self._core_
        except AttributeError:
            self._core_ = self._subsystem.cores[0]
        return self._core_

    @property
    def core(self):
        """The Core containing MMU control registers"""
        # There's nothing particularly protected about a core so make it public
        # to prevent lots of protected-access messages from pylint.
        return self._core

    @property
    def _fields(self):
        """Map containing the MMU control registers"""
        return self._core.field_refs

    def _initRegs(self):
        """Initialise MMU Register References"""

        # Potential extension:: Move out of here into specific sub blocks that use them

        # Shorthands
        fields = self._fields

        self._miscRegs = []
        try:
            self.proc_read_port_buffer = fields['MMU_PROC_READ_PORT_BUFFER']
            self.proc_read_port_page = fields['MMU_PROC_READ_PORT_PAGE']
            self.proc_write_port_buffer = fields['MMU_PROC_WRITE_PORT_BUFFER']
            self.proc_write_port_page = fields['MMU_PROC_WRITE_PORT_PAGE']
            self.proc_io_log_addr = fields['MMU_PROC_IO_LOG_ADDR']
            self._miscRegs.append(self.proc_read_port_buffer)
            self._miscRegs.append(self.proc_read_port_page)
            self._miscRegs.append(self.proc_write_port_buffer)
            self._miscRegs.append(self.proc_write_port_page)
            self._miscRegs.append(self.proc_io_log_addr)
        except KeyError:
            # Some MMUs don't have these
            pass


    def readMemoryBlock(self, address, length):
        """
        read a block of data of some element size from a memory block starting
        at address
        """
        return self.mmu_data[address : address+length]

    def writeMemoryBlock(self, address, data):
        """
        write a block of data of some element size to a memory starting
        at address
        """
        self.mmu_data[address : address + len(data)] = data

    def readMemory(self, address):
        """read data of some size from a memory at address"""
        return self.mmu_data[address]

    def writeMemory(self, address, value):
        """write data value of some size to memory at address"""
        self.mmu_data[address] = value

    # Reports
    #
    # Possibly expensive & possibly readonly views

    def _generate_report_body_elements(self):

        elements = []

        # Buffer Handle summary
        #
        try:
            elements.append(self.buf_list_report())
        except Exception as exc: #pylint: disable=broad-except
            elements.append(interface.Code('Error in buf_list: %s' % exc))

        # Configured Buffer summaries
        #
        for handle in self.handleTable.records:
            if handle.isConfigured:
                try:
                    elements.append(self.buf_read_report(handle.index))
                except Exception as exc: #pylint: disable=broad-except
                    elements.append(interface.Code('Error in buf_read(%d): %s'
                                                   % (handle.index, exc)))

        elements.append(self.handle_owners_report())

        return elements

    def buf_list(self, nonEmpty=None, showPages=False, validate=False,
                 showSpecial=False):
        """\
        Wrapper on buf_list_report that just outputs to stdout for command-line
        use.
        """
        report = self.buf_list_report(
            nonEmpty, showPages, validate, showSpecial)
        TextAdaptor(report, gstrm.iout)

    def buf_list_report(self, nonEmpty=None, showPages=False,
                        validate=False, showSpecial=False):
        #This routine could do with factoring, hence:
        #pylint: disable=too-many-locals, too-many-branches, too-many-statements
        #pylint: disable=too-many-nested-blocks

        #pylint: disable=line-too-long
        """Create summary of buffer handle state.

        For each created buffer, shows the buffer handle,
        the total size, the size currently in use and
        and the hardware offset (unless it is zero and the
        buffer is empty) -- all in octets and in hex.

        Example:
          bh size  used  offs   bh size  used  offs   bh size  used  offs   bh size  used  offs
          01 01000 01000 00852  02 00200 00200 00144  03 00100 00100 00048  04 00040 00040 00020
          05 00100 00100 -----  07 00080 00080 0002c

        An optional first argument called nonEmpty can be passed;
        if set to True only non-empty buffers are shown.

        An optional second argument called showPages can be passed;
        if set to True the page number is printed for each page
        mapped into the buffer.

        An optional third argument called validate indicates whether MMU
        pages should be validated.  The lowest and highest pages in use
        are indicated, together with any pages in the intervening range
        that are unused or that occur more than twice.  There should be
        only unused pages which should coincide with the pages on the free
        list (apart from contiguous free pages below the lowest page
        found in a buffer).

        An optional fourth argument called showSpecial can be passed;
        if set to True zombie and reserved buffers are shown (this is
        a UniFi-ism).
        """
        #pylint: enable=line-too-long
        # For quick start just render as plain text 'code' element.
        #
        # Potential extension:: generate richer report model
        #
        mmu = self
        text = ''

        # Refresh the handleTable and reread the MMU_PROC_* registers.
        mmu.handleTable_refresh()

        if validate:
            seen = [0] * mmu.max_page_number
            highest_page = 0
            lowest_page = mmu.max_page_number
            multiple = False

        if showPages:
            text += "bh size  used  offs  pages\n"
        else:
            text += ("bh size  used  offs   bh size  used  offs   bh size "
                     " used  offs   bh size  used  offs\n")
        nprints = 0
        linestring = ""
        total_usedsize = 0
        if mmu.max_page_number > 255:
            pagefmt = "%04x "
            nopage = "---- "
        else:
            pagefmt = "%02x "
            nopage = "-- "

        for handle in mmu.handleTable.records:
            printed = False
            if handle.bufferOffset == 0:
                offsetstr = "-----"
            else:
                offsetstr = "%05x" % handle.bufferOffset
            if handle.isReserved and showSpecial:
                linestring += "%02x reserved " % handle.index
                printed = True
            elif handle.isInitialised and showSpecial:
                linestring += "%02x initialised " % handle.index
                printed = True
            elif handle.isFree and showSpecial:
                linestring += "%02x free " % handle.index
                printed = True
            elif handle.isConfigured:
                totsize = handle.bufferPages * mmu.page_size
                pages = handle.pageTable.records
                usedsize = sum(x > 0 for x in pages) * mmu.page_size
                if not nonEmpty or usedsize != 0:
                    if usedsize == 0:
                        linestring += ("%02x %05x ----- %s  " %
                                       (handle.index, totsize, offsetstr))
                    else:
                        linestring += ("%02x %05x %05x %s  " %
                                       (handle.index, totsize,
                                        usedsize, offsetstr))
                        # need to account for data buffers having two handles
                        total_usedsize += usedsize
                    printed = True
                    if showPages and usedsize != 0:
                        linestring += "(%04x: " % handle.pageTableOffset
                        for page in pages:
                            if page == 0:
                                linestring += nopage
                            else:
                                linestring += pagefmt % page
                        linestring += ")"
                    if validate and usedsize != 0:
                        for page in pages:
                            if page == 0:
                                continue
                            if page < lowest_page:
                                lowest_page = page
                            if page > highest_page:
                                highest_page = page
                            if seen[page]:
                                multiple = True
                            seen[page] += 1
            elif showSpecial:
                linestring += "%02x unknown state" % handle.index
                printed = True

            if printed:
                nprints += 1
                if nprints == 4 or showPages:
                    text += linestring + '\n'
                    nprints = 0
                    linestring = ""

        if nprints != 0:
            text += linestring + '\n'

        if validate:
            if lowest_page == mmu.max_page_number:
                text += "No MMU pages in buffers.\n"
            else:
                text += "Lowest used MMU pages is %02x\n" % lowest_page
                text += "Highest used MMU page is %02x\n" % highest_page
                text += "Unused MMU pages in this range:"
                unused = 0
                for i in range(highest_page - lowest_page + 1):
                    page = lowest_page + i
                    if seen[page] == 0:
                        unused = unused + 1
                        text += "%02x " % page
                if unused > 0:
                    text += '\n'
                else:
                    text += "None\n"
                if multiple:
                    text += "!Multiply mapped pages:\n"
                    for i in range(highest_page - lowest_page + 1):
                        page = lowest_page + i
                        if seen[page] > 1:
                            text += "%02x(%d)" % (page, seen[page])
                    text += '\n'

                freePages = self.freeList.freePages
                used_and_freed = [page for page in freePages if seen[page]]
                if used_and_freed:
                    text += ("ERROR ! %d pages are on the free list and "
                             "mapped into buffers : %s" %(
                                 len(used_and_freed),
                                 " ".join([hex(page) for page in
                                           used_and_freed])))
                min_page = min(freePages + [lowest_page])
                max_page = max(freePages + [highest_page])
                text += ("MMU Pages occupy memory from 0x%x to 0x%x (%d bytes)"
                         % (min_page * self.page_size,
                            ((max_page+1)* self.page_size) -1,
                            ((max_page+1-min_page)* self.page_size)))

        # Bundle the raw text into a mini-report
        #
        report = interface.Group('Buffer Summary')
        report.append(interface.Code(text))
        return report

    def buf_read(self, handle_num, show_page_addresses=False, start=0,
                 len=None, show_owner=False, suppress=False):
        #'len' overrides built-in
        #pylint: disable=too-many-arguments,redefined-builtin
        """\
        Wrapper on buf_read_report that just outputs to stdout for command-line
        use.
        Optional parameters:
         - show_page_addresses: shows the memory addresses of the pages being
           displayed.
         - start: gives an offset into the buffer at which to start dumping the
           contents.
         - len: specifies the length of the buffer data to dump in bytes.
         - show_owner: show buffer owner details
         - suppress: the displaying of repeated 00 data
        """
        report = self.buf_read_report(handle_num,
                                      show_page_addresses=show_page_addresses,
                                      start=start,
                                      len=len,
                                      show_owner=show_owner,
                                      suppress=suppress)
        TextAdaptor(report, gstrm.iout)

    def buf_offset(self, handle_num):
        """return the current offset of the buffer with handle handle_num"""
        mmu = self

        # Refresh the handleTable and reread the MMU_PROC_* registers.
        mmu.handleTable_refresh()

        record = mmu.handleTable.records[handle_num]
        offset = record.bufferOffset

        return offset

    def buf_size(self, handle_num):
        """return the size of the buffer with handle handle_num"""
        mmu = self

        # Refresh the handleTable and reread the MMU_PROC_* registers.
        mmu.handleTable_refresh()

        record = mmu.handleTable.records[handle_num]

        return record.bufferSize

    def buf_read_pages(self, handle_num, start=0, length=None):
        """\
        Return buffer pages with content.
        [(page_offset, [page_data]),...])
        """
        mmu = self

        # Refresh the handleTable and reread the MMU_PROC_* registers.
        mmu.handleTable_refresh()

        record = mmu.handleTable.records[handle_num]

        octets_per_word = self._layout_info.addr_unit_bits // 8

        pages = []

        if start % mmu.page_size:
            # not on a page boundary
            if length:
                length += start % mmu.page_size
            start -= start % mmu.page_size

        for i, cur_page_value in enumerate(record.pageTable):
            if cur_page_value != 0:
                page_size_words = mmu.page_size // octets_per_word
                page_start_offset = i * mmu.page_size
                if page_start_offset < start:
                    continue
                if length and page_start_offset >= length+start:
                    continue
                cur_page_address = mmu.memory_offset + (cur_page_value *
                                                        page_size_words)
                cur_page_values = mmu.readMemoryBlock(cur_page_address,
                                                      page_size_words)

                if self._layout_info.addr_unit_bits == 16:
                    pages.append((page_start_offset,
                                  words_to_bytes(cur_page_values)))
                else:
                    pages.append((page_start_offset, cur_page_values))

        return pages

    def _buf_stream(self, handle_num):
        '''
        Stream data from MMU buffer by constantly polling offset and
        fetching new data when appears.
        '''
        mmu = self

        buf_size = mmu.buf_size(handle_num)

        new_offset = buf_offset = mmu.buf_offset(handle_num)

        while True:
            while new_offset == buf_offset:
                new_offset = mmu.buf_offset(handle_num)

            length = (new_offset + buf_size - buf_offset) % buf_size

            if new_offset > buf_offset:
                pages = mmu.buf_read_pages(handle_num, buf_offset, length)
            else:
                # handle wrap around by calling buf_read_pages() twice
                pages = mmu.buf_read_pages(handle_num, buf_offset)
                pages += mmu.buf_read_pages(handle_num, 0, new_offset)

            data = []
            for page_index, page in enumerate(pages):
                (_, page_data) = page

                if page_index == 0:
                    # remove excess leading data
                    leading_bytes = buf_offset % mmu.page_size
                    del page_data[0:leading_bytes]

                if page_index == len(pages)-1:
                    # remove excess trailing data
                    trailing_bytes = mmu.page_size - (new_offset%mmu.page_size)
                    del page_data[-trailing_bytes:]

                data += page_data

            text = hex_array_str_columns(data)

            report = interface.Group('MMU handle 0x%x offset 0x%x length %d' %
                                     (handle_num, buf_offset, length))
            report.append(interface.Code(text))
            TextAdaptor(report, gstrm.iout)

            buf_offset = new_offset

    def buf_stream(self, handle_num):
        '''
        Stream data from MMU buffer by constantly polling offset and
        fetching new data as it appears until CTRL+C is pressed.
        '''
        try:
            self._buf_stream(handle_num)
        except KeyboardInterrupt:
            pass

    def buf_read_report(self, handle_num, show_page_addresses=False, start=0,
                        len=None, show_owner=False, suppress=False):
        #'len' overrides built-in
        #pylint: disable=too-many-arguments,redefined-builtin
        #This routine needs refactoring, hence:
        #pylint: disable=too-many-locals
        """\
        Dump buffer summary and content.

        '>' Marks current offset position.

        Example:
        Buffer 0x5 | Offset 0x0 | Size 0x2 | Pages 4
          0000 : >05 00 04 00 00 00 00 00 10 02 0c 00 00 00 00 00
          0010 :  10 02 14 00 00 00 00 00 10 02 08 00 00 00 00 00
          ...
        Optional parameters:
         - show_page_addresse: shows the memory addresses of the pages being
            displayed.
         - show_owner: show buffer owner details (zeagle only)
         - suppress: suppress the displaying of repeated 00 data (zeagle only)
        Note:
        If start is in the middle of a page you won't get that page printed
        at all.

        """
        #pylint:disable=unused-argument

        # Potential extension:: Looks like there is more than just presentation logic in here...
        #
        # Potential extension:: generate richer report model
        #
        mmu = self
        text = ''

        # Refresh the handleTable and reread the MMU_PROC_* registers.
        mmu.handleTable_refresh()

        record = mmu.handleTable.records[handle_num]
        buf_pages = record.bufferPages
        offset = record.bufferOffset

        octets_per_word = self._layout_info.addr_unit_bits // 8

        text += ("Buffer 0x%x | Offset 0x%x | Size 0x%x | Pages %d\n" %      \
                        (handle_num, offset, record.bufferSize, buf_pages))

        for i, cur_page_value in enumerate(record.pageTable):
            if cur_page_value != 0:
                page_size_words = mmu.page_size // octets_per_word
                page_start_offset = i * mmu.page_size
                if page_start_offset < start:
                    continue
                if len and page_start_offset >= len+start:
                    continue
                if show_page_addresses:
                    iprint("Page %d = 0x%x | RAM location 0x%x" %
                           (i, cur_page_value,
                            (mmu.memory_offset +
                             cur_page_value * page_size_words)))

                cur_page_address = mmu.memory_offset + (cur_page_value *
                                                        page_size_words)
                cur_page_values = mmu.readMemoryBlock(cur_page_address,
                                                      page_size_words)
                if self._layout_info.addr_unit_bits == 16:
                    text += hex_array_str_columns(
                        words_to_bytes(cur_page_values),
                        start_offset=page_start_offset,
                        cursor=offset)
                else:
                    text += hex_array_str_columns(
                        cur_page_values,
                        start_offset=page_start_offset,
                        cursor=offset)

        # Bundle the raw text into a mini-report
        #
        report = interface.Group('Buffer %d' % handle_num)
        report.append(interface.Code(text))
        return report

    def handle_owners_report(self):
        """
        Return a report of the firmware modules that are deemed to own
        particular MMU handles
        """
        return interface.Code("Handle owners report not implemented for %s" %
                              self.__class__.__name__)

    @property
    def unusedFreeListRam(self):
        """
        Returns unused MMU page RAM in bytes
        """
        val = self.freeList.unusedPageCount * self.page_size
        return val

    @property
    def usedFreeListRam(self):
        """
        Returns used MMU page RAM in bytes
        """
        val = self.freeList.usedPageCount * self.page_size
        return val

    @property
    def unusedHandleRam(self):
        """
        Returns unused MMU handle RAM in bytes
        """
        val = (self.num_handle_bits // 8) * self.handleTable.unusedHandleCount
        return val

    def _prepare_usage_info(self):
        """
        Set up basic information about memory usage by the MMU.  This needs to
        be implemented by subclasses because it depends on subsystem-specific
        details of how the MMU is configured
        """
        raise PureVirtualError(self)

    def _generate_memory_report_component(self):

        self._prepare_usage_info()
        #MMU pages used
        if self._dynamic_mmu_pages:
            page_size_words = self.page_size // (self._core.data.word_bits // 8)
            mmu_pages_unused = self.freeList.unusedPageCount * page_size_words
            mmu_pages_total = self.freeList.pageCount * page_size_words
            mmu_pages_used = mmu_pages_total - mmu_pages_unused
            self._dynamic_mmu_pages["used"] = mmu_pages_used
            self._dynamic_mmu_pages["unused"] = mmu_pages_unused
            self._dynamic_mmu_pages["percent_used"] = (
                float(mmu_pages_used) / mmu_pages_total) * 100

        # Now the handles used
        if self._mmu_handles:
            mmu_handles_total_number = self.handleTable.numHandles
            mmu_handles_unused = self.handleTable.unusedHandleCount
            mmu_handles_used = mmu_handles_total_number - mmu_handles_unused
            handle_words = self.handleTable.numHandleWords
            self._mmu_handles["used"] = mmu_handles_used * handle_words
            self._mmu_handles["unused"] = mmu_handles_unused * handle_words
            try:
                self._mmu_handles["percent_used"] = (
                    float(mmu_handles_used) / mmu_handles_total_number) * 100
            except ZeroDivisionError:
                self._mmu_handles["percent_used"] = None
            self._mmu_handles["comment"] = (
                "Num Handles(Total:%d Used:%d Unused:%d) + Null:1" %
                (mmu_handles_total_number, mmu_handles_used,
                 mmu_handles_unused))

        return [[[self._mmu_handles, self._static_mmu_pages,
                  self._dynamic_mmu_pages, self._mmu_alignment_wastage,
                  [self._mmu_freelist]]]]


class XapHydraMMU(HydraMMU):
    """Intermediate class that provides XAP specific layout info for a Hydra MMU
    """
    @property
    def _layout_info(self):
        return XapLEDataInfo()

class SingleXapHydraMMU(XapHydraMMU):
    """ Subsystems with a single xap have mmu handles, page tables, freelist and
    pages in conventional memory which is mapped in from 0x8000 onwards.
    """
    @property
    def page_size(self):
        return 64

    @property
    def default_num_handles(self):
        return 128

    @property
    def max_page_number(self):
        return self.mmu_data_memory_size // self.page_size

    @property
    def mmu_data(self):
        # MMU tables are mapped into core data space at 0x8000
        return self._core.data

    @property
    def mmu_data_memory_size(self):
        ''' Size of region of memory holding the MMU in bytes '''
        return 64*1024

    @property
    def memory_offset(self):
        return 0x8000

class BTMMU(SingleXapHydraMMU):
    """Hydra Bluetooth MMU based on a XAP architecture"""
    def _prepare_usage_info(self):
        """
        Set up basic information about memory usage by the MMU
        """
        try:
            # handles
            # The BT subsystem FW lacks the symbols we get from the other
            #  subsystems, so we have to bake assumptions in here
            start = self._core.fw.env.abs["MMU_MEM_START"]
            end = self._core.fw.env.abs["MMU_FREE_LIST"]

            self._mmu_handles = {"name" : "MMU_HANDLES",
                                 "start" : start,
                                 "end" :  end,
                                 "size" : end - start}
            # mmu_freelist
            start = self._core.fw.env.abs["MMU_FREE_LIST"]
            freelist_end_addr = (self._core.fields.MMU_FREE_LIST_END_ADDR +
                                 self._core.fw.env.abs["MMU_MEM_START"])
            size = freelist_end_addr - start
            self._mmu_freelist = {"name" : "MMU_FREE_LIST",
                                  "start" : start,
                                  "end" :  freelist_end_addr,
                                  "size" : size}
            # pages
            page_size_words = (self.page_size //
                               (self._core.data.word_bits // 8))
            temp = freelist_end_addr + (page_size_words - 1)
            mmu_pages_start = temp - (temp % (page_size_words))
            # "-1" for the NULL page
            size = (len(self.freeList.records) - 1) * page_size_words
            self._dynamic_mmu_pages = {"name" : "MMU_PAGES",
                                       "start" : mmu_pages_start,
                                       "end" :  mmu_pages_start + size,
                                       "size" : size}

            mmu_pages_unused = self.freeList.unusedPageCount * page_size_words
            #We have a NULL page at the end, so we need a "-1" here
            mmu_pages_total = (len(self.freeList.records) - 1) * page_size_words
            mmu_pages_used = mmu_pages_total - mmu_pages_unused
            self._dynamic_mmu_pages["used"] = mmu_pages_used
            self._dynamic_mmu_pages["unused"] = mmu_pages_unused
            self._dynamic_mmu_pages["percent_used"] = (
                float(mmu_pages_used) / mmu_pages_total) * 100

            # alignment wastage
            start = freelist_end_addr
            end = mmu_pages_start
            size = end - start
            self._mmu_alignment_wastage = {"name" : "MMU page wastage",
                                           "start" : start,
                                           "end" : end,
                                           "size" : size}


        except KeyError:
            self._mmu_handles = {}
            self._mmu_freelist = None
            self._dynamic_mmu_pages = {}
            self._mmu_alignment_wastage = None

        # BT doesn't use static MMU pages
        self._static_mmu_pages = []

    @property
    def freeList(self):
        """The Free Page List"""
        # Construct lazily
        try:
            self._freeList
        except AttributeError:
            self._freeList = BTMMUFreeList(self)
        return self._freeList

    @property
    def handleTable(self):
        """The Buffer Handle Table"""
        # Construct lazily
        try:
            self._handleTable
        except AttributeError:
            self._handleTable = BTMMUHandleTable(self)
        return self._handleTable


class CuratorMMU(SingleXapHydraMMU):
    """
    The Curator has fewer handles than the default of 128 (which is correct for
    BT)
    """
    def _prepare_usage_info(self):
        """
        Set up basic information about memory usage by the MMU
        """
        try:
            self._mmu_handles = self._subsystem.core.sym_get_range(
                'MEM_MAP_MMU_HANDLES', start_tag="_BEGIN", end_tag="_END")
            self._static_mmu_pages = self._subsystem.core.sym_get_range(
                'MEM_MAP_STATIC_MMU', start_tag="_BEGIN", end_tag="_END")
            self._dynamic_mmu_pages = self._subsystem.core.sym_get_range(
                'MEM_MAP_MMU_PAGES', start_tag="_BEGIN", end_tag="_END")

            freelist_addr = self._core.fw.env.cus["mmu_freelist.c"].localvars[
                "mmu_freelist_pages"].address
            freelist_size = self._core.fw.env.cus["mmu_freelist.c"].localvars[
                "mmu_freelist_pages"].size
            self._mmu_freelist = {"name" : "mmu_freelist",
                                  "start" : freelist_addr,
                                  "end" : freelist_addr + freelist_size,
                                  "size" : freelist_size}

            # MMU pages have to be on PAGE_SIZE boundary
            start = self._core.sym_get_value("MEM_MAP_RAMRUN_END")
            end = self._core.sym_get_value("MEM_MAP_STATIC_MMU_BEGIN")
            self._mmu_alignment_wastage = {"name":"MMU page wastage",
                                           "start":start,
                                           "end":end,
                                           "size":end - start}
        except AttributeError:
            self._mmu_handles = {}
            self._static_mmu_pages = None
            self._dynamic_mmu_pages = {}
            self._mmu_alignment_wastage = None
            self._mmu_freelist = None

    @property
    def default_num_handles(self):
        return 64

    @property
    def num_handles(self):
        """
        Try to calculate the number of handles based on known symbols in the
        ELF, if it's available
        """
        try:
            addrs_per_hdl_rec = (self.num_handle_bits //
                                 self._layout_info.addr_unit_bits)
            return (self._core.fw.env.abs["MMU_HANDLE_RAM_SIZE"] //
                    addrs_per_hdl_rec - 1)
        except (AttributeError, KeyError):
            return self.default_num_handles

class DualXapHydraMMU(XapHydraMMU):
    """ Subsystems with two xaps have mmu handles, page tables, freelist and
    pages in SHARED memory starting from offset zero
    """
    @property
    def page_size(self):
        return 256

    @property
    def default_num_handles(self):
        return 128

    @property
    def max_page_number(self):
        return self.mmu_data_memory_size // self.page_size

    @property
    def mmu_data(self):
        # MMU tables are mapped into share RAM at 0x0000
        return self._core.shared_ram

    @property
    def mmu_data_memory_size(self):
        ''' Size of region of memory holding the MMU in bytes '''
        return 512*1024

    @property
    def memory_offset(self):
        return 0

    @property
    def octets_per_word(self):
        return self._layout_info.addr_unit_bits // 8


class DualKalimbaHydraMMU(HydraMMU):
    """MMU for hydra chip with dual kalimba processors"""
    @property
    def page_size(self):
        ''' Size of an MMU page in bytes '''
        return 128

    @property
    def default_num_handles(self):
        return 255

    @property
    def max_page_number(self):
        return self.mmu_data_memory_size // self.page_size

    @property
    def mmu_data(self):
        # M
        return self._core.dm

    @property
    def mmu_data_memory_size(self):
        ''' Size of region of memory holding the MMU in bytes '''
        return 256*1024

    @property
    def memory_offset(self):
        # This the offset into shared RAM, where the MMU stuff is found
        return 0x20000


class AppsMMU(DualKalimbaHydraMMU):
    """MMU for hydra chip apps processor"""

    def _prepare_usage_info(self):
        """
        Set up basic information about memory usage by the MMU
        """
        try:
            self._mmu_handles = self._core.sym_get_range('MEM_MAP_MMU_HANDLES')
            self._dynamic_mmu_pages = self._core.sym_get_range(
                'MEM_MAP_MMU_PAGES')
            self._static_mmu_pages = []

            # MMU pages have to be on PAGE_SIZE boundary
            start = self._core.sym_get_value("MEM_MAP_SHARED_MEMORY_END")
            end = self._core.sym_get_value("MEM_MAP_MMU_PAGES_START")
            self._mmu_alignment_wastage = {"name":"MMU page wastage",
                                           "start":start,
                                           "end":end,
                                           "size":end - start}

            freelist_addr = self._core.fw.env.cus["mmu_freelist.c"].localvars[
                "mmu_freelist_pages"].address
            freelist_size = self._core.fw.env.cus["mmu_freelist.c"].localvars[
                "mmu_freelist_pages"].size
            self._mmu_freelist = {"name" : "mmu_freelist",
                                  "start" : freelist_addr,
                                  "end" : freelist_addr + freelist_size,
                                  "size" : freelist_size}

        except KeyError:
            self._mmu_handles = {}
            self._mmu_freelist = None
            self._dynamic_mmu_pages = {}
            self._static_mmu_pages = None
            self._mmu_alignment_wastage = None

    class _FieldRegNameTranslator(object):
        """Utility for translating names of fields in registers"""
        #pylint: disable=too-few-public-methods
        def __init__(self, field_refs):
            self._field_refs = field_refs

        def __getitem__(self, name):

            apps_name = name.replace("MMU_", "VM_")
            return self._field_refs[apps_name]

    @property
    def memory_offset(self):
        """
        Apps subsystems in general have shared memory start at 0x1000000.
        CSRA68100 D00 is an exception (see subclass)
        """
        return 0x10000000

    @property
    def _fields(self):
        try:
            self.__fields
        except AttributeError:
            self.__fields = self._FieldRegNameTranslator(self._core.field_refs)
        return self.__fields

    @property
    def _layout_info(self):
        return Kalimba32DataInfo()

    @property
    def num_handles(self):
        try:
            addrs_per_hdl_rec = (self.num_handle_bits //
                                 self._layout_info.addr_unit_bits)
            return ((self._core.fw.env.abs["MEM_MAP_MMU_HANDLES_END"] -
                     self._core.fw.env.abs["MEM_MAP_MMU_HANDLES_START"]) //
                    addrs_per_hdl_rec)
        except (AttributeError, KeyError):
            return self.default_num_handles

    @property
    def pages_in_use_per_handle(self):
        """Return the number of pages currently in use by each handle index."""
        page_counts = []
        for handle in self.handleTable.records:
            try:
                pages = sum(x > 0 for x in handle.pageTable.records)
            except MMUHandleRecord.NotConfiguredError:
                pages = 0
            page_counts.append(pages)
        return page_counts

    @property
    def ram_in_use_per_handle(self):
        """
        Return the amount of RAM in bytes currently in use by each handle index.
        """
        return [pages * self.page_size for pages in
                self.pages_in_use_per_handle]

    @property
    def _handle_owners_by_index(self):
        """
        Return a tuple of two items describing owners of handles and any
        problems encountered when tracing owners.

        The first element in the tuple is a list of handle owners where the
        list index is the MMU index.

        The second element is a list of interface objects describing problems
        encountered (typically, exceptions thrown) while building the first
        list. In particular, this can indicate the first list has items
        missing because something went wrong.
        """
        handle_owners = {}
        handles, missing = self._subsystem.gather_mmu_handles()
        for cur_ss_handles in handles:
            for handle in cur_ss_handles[1]:
                existing_string = ""
                if handle in list(handle_owners.keys()):
                    # If we already have a owner, append this new one
                    existing_string = handle_owners[handle & 0xff] + " : "
                handle_owners[handle & 0xff] = (existing_string +
                                                cur_ss_handles[0])
        handle_owners[0] = "NULL HANDLE"
        return handle_owners, missing

    def poll_ram_in_use_per_handle(
            self, filename="-", period_s=0.5, poll_handle_owners=False):
        """Poll the amount of RAM in use per MMU handle.

        Periodically outputs the amount of RAM in use per MMU handle in CSV
        format.
        Press Ctrl-z to exit polling.

        Args:
            filename (optional): Defaults to "-" which means use stdout as the
                file handle.
                Otherwise data is written to the provided filename. If the file
                already exists it is overwritten.

            period_s (float, optional): Optionally set the polling period in
                seconds.

            poll_handle_owners (optional): By default this is set to False and
                the handle owners are only output at the start of the function.
                If this is set to True the handle owners are output along with
                each sample. Note that this will increase the processing
                overhead so the sampling period may need to increase.
        """
        with open_or_stdout(filename, "wb") as f:
            iprint("Polling MMU RAM usage (Ctrl-z to stop).")

            writer = csv.writer(f)

            def handle_owners_list():
                '''returns a list of handle owners'''
                handle_owners, _ = self._handle_owners_by_index
                return [handle_owners.get(i, "Unknown") for i in
                        range(self.num_handles)]

            # Use the current handle owners for the CSV header.
            # Handle owners may vary over time so they may not be accurate for
            # the entire polling session.
            writer.writerow(["Time (s)"] + handle_owners_list())

            # Start polling
            start = time.time()
            while True:
                before = time.time()
                timestr = "{0:.3f}".format(before - start)

                if poll_handle_owners:
                    writer.writerow([timestr] + handle_owners_list())
                writer.writerow([timestr] + self.ram_in_use_per_handle)
                detect_ctrl_z()

                # The duration of reading the RAM usage is not negligible so
                # include it in our sleep calculation.
                duration = time.time() - before
                time.sleep(max(period_s - duration, 0))

    def handle_owners(self):
        '''
        Produce report to stdout of the firmware owners of handles in this MMU
        '''
        report = self.handle_owners_report()
        TextAdaptor(report, gstrm.iout)

    def handle_owners_report(self):
        '''
        Returns report object of the firmware owners of handles in this MMU
        '''
        handle_owners, missing = self._handle_owners_by_index

        #Create report object
        report = interface.Group('Buffer owners')
        output_table = interface.Table(["Handle", "Size", "Used", "Value"])
        for handle in self.handleTable.records:
            if handle.isConfigured:
                index = handle.index
                size = handle.bufferSize
                pages = handle.pageTable.records
                usedsize = sum(x > 0 for x in pages) * self.page_size
                text = ""
                try:
                    text = handle_owners[index]
                except KeyError:
                    text = "Unknown"

                row = []
                row.append("0x%04x" % index)
                row.append("0x%05x" % size)
                row.append("0x%05x" % usedsize)
                row.append(text)
                output_table.add_row(row)

        report.append(output_table)

        if missing:
            mgroup = interface.Group('Missing')
            mgroup.append(interface.Text(
                "The owners of some handles may be missing as the "
                "following exceptions were generated while trying to "
                "identify MMU handle owners:"))
            mgroup.extend(missing)
            report.append(mgroup)

        return report

class CSRA68100AppsD00MMU(AppsMMU):
    """Provides MMU for Apps processor on CSRA68100 D00 chip"""
    @property
    def memory_offset(self):
        return 0x20000

class AudioBAC(DualKalimbaHydraMMU):
    """
    A class of Hydra MMU for a dual Kalimba processor representing an AudioBAC
    """
    def __init__(self, subsystem):
        #Note: doesn't call super because does nto want to call _initRegs
        #pylint: disable=super-init-not-called
        self._subsystem = subsystem
        self._core_ = subsystem.cores[0]
        self.fields = self.core.fields
        self._values_dict = self.core.info.io_map_info.misc_io_values
        try:
            self._page_size = 2**self._values_dict["BAC_PAGE_BIT_WIDTH"]
        except KeyError:
            # The name above disappeared in a pre-D01 digits release; the name
            # below is the best approximation, I think.
            self._page_size = 2**self._values_dict["BAC_STND_PAGE_BIT_WIDTH"]
        self.__layout_info = Kalimba32DataInfo()

    @property
    def subcomponents(self):
        """
        An AudioBAC has no freeList subcomponent, unlike normal Hydra MMUs
        """
        return {}

    @property
    def page_size(self):
        return self._page_size
    @property
    def _layout_info(self):
        return self.__layout_info

    @property
    def memory_offset(self):
        return self.core.fields["BAC_BUFFER_HANDLE_BLOCK_OFFSET"]

    @property
    def num_handle_bits(self):
        return 12 * 8

    def _handle_value(self, handle_address):
        """ Read the handle from memory as a 96 bit value """
        mem = self.core.dm[handle_address:handle_address+12]
        h = 0
        for i, h_byte in enumerate(mem):
            h |= h_byte << (8*i)
        return h

    @property
    def handleTable(self):
        """The Buffer Handle Table - override to create a BAC MMU handle
        table in place of the conventional hydra one. """
        # Construct lazily
        try:
            self._handleTable
        except AttributeError:
            self._handleTable = self.MMUHandleTable(self)
        return self._handleTable

    def _generate_memory_report_component(self):
        return []

    class MMUHandleTable(object):
        """        MMU Handle Table        """
        def __init__(self, mmu):
            """ Construct HandleTable
            Params:-
            - mmu: The containing MMU object.
            """
            self._mmu = mmu
            self._numHandleWords = (self._mmu.num_handle_bits //
                                    self._mmu.layout_info.addr_unit_bits)
            self._beginAddr = self._mmu.memory_offset
            self._numHandles = 64
            self._records = self._makeRecords()

        def __repr__(self):
            """Pretty print instances of this class."""
            return "MMU.HandleTable< addr 0x%X, numHandles 0x%X >" % (
                self._beginAddr, self._numHandles)

        @property
        def records(self):
            """List of all handle records."""
            return self._records

        def __getitem__(self, ix):
            """Allow indexing of handle records as convenience"""
            return self.records[ix & 0xff]

        def _makeRecords(self):
            """Make list of all records in the table"""
            records = []
            for i in range(self._numHandles):
                records.append(self._makeRecord(i))
            return records

        def _makeRecord(self, ix):
            """\
            Make i-th Handle Record
            """
            recordAddr = self._beginAddr + (ix * self._numHandleWords)
            return self._mmu.MMUHandleRecord(self._mmu, ix, recordAddr)

        @property
        def numHandles(self):
            """Number of handles in the table"""
            return self._numHandles

        @property
        def numHandleWords(self):
            """Number of handle words in the table"""
            return self._numHandleWords

    class MMUHandleRecord(object):
        """Type for an MMU handle in AudioBAC"""
        def __init__(self, mmu, idx, addr):
            value = mmu._handle_value(addr) #pylint: disable=protected-access
            self._value = value
            self._addr = addr
            self._mmu = mmu
            self.index = idx & 0xff
            self.fields = {}
            for field, num_bits in [("start", 32), ("offset", 18),
                                    ("validity_tag", 1), ("size", 12),
                                    ("sample_size", 2), ("packing_mode", 1),
                                    ("shift", 5), ("sign_extension", 1),
                                    ("byte_swap", 1),
                                    ("host_access_protection", 1),
                                    ("access_protection", 1),
                                    ("mode", 1)]:
                bit_offset = self.value("BAC_BUFFER_" + field)
                self.fields[field] = (int((value >> bit_offset) &
                                          ((1<<num_bits)-1)))

            # work out how big the buffer is
            if self.fields["mode"] == self.buffer_val(
                    "MODE_POWER_OF_2_NO_OF_PAGES"):
                self.fields["size_bytes"] = (self._mmu.page_size
                                             * (2**self.fields["size"]))
            else:
                self.fields["size_bytes"] = (self._mmu.page_size
                                             * self.fields["size"])

        def value(self, enum_name):
            """ Helper function to extract values from the register definitions
            """
            #pylint: disable=protected-access
            return self._mmu._values_dict["$" + enum_name.upper() + "_ENUM"]

        def buffer_val(self, buffer_enum_name):
            """ Helper function to extract values from the BAC_BUFFER
            register definitions """
            return self.value("BAC_BUFFER_" + buffer_enum_name)

        @property
        def bufferOffset(self):
            """returns the current offset in the buffer"""
            return self.fields["offset"]

        @property
        def isConfigured(self):
            """ For compliance with HydraMMU """
            return self.fields["start"] != 0

        @property
        def isReserved(self):
            """ For compliance with HydraMMU """
            return False

        @property
        def isInitialised(self):
            """ For compliance with HydraMMU """
            return self.isConfigured

        @property
        def isFree(self):
            """ For compliance with HydraMMU """
            return not self.isConfigured

        @property
        def bufferPages(self):
            """ For compliance with HydraMMU - an alias for the buffer size """
            return self.fields["size_bytes"] // self._mmu.page_size

        @property
        def pageTable(self):
            """ The BAC buffers don't have a page table so we just make
            one up with each 'page' being the start address in memory of
            each 64 byte section """
            return self.DummyPageTable(self._mmu, self.fields["start"],
                                       self.fields["size_bytes"])

        class DummyPageTable(object):
            """Type for a dummy page table"""
            #pylint: disable=too-few-public-methods
            def __init__(self, mmu, start_addr, size_bytes):
                self._page_size = mmu.page_size
                self._start_addr = start_addr
                self._size_bytes = size_bytes

            @property
            def records(self):
                """returns a range representing the records in the page table"""
                return range(self._start_addr,
                             self._start_addr + self._size_bytes,
                             self._page_size)

        @property
        def pageTableOffset(self):
            """On a paged buffer this is the address of the page table. For
            the BAC we just return the address of the buffer memory to keep
            the buf_list code quiet """
            return self.fields["start"]

        def __repr__(self):
            fld = self.fields
            val = self.buffer_val
            # now tidy up the values
            if fld["sample_size"] == val("SAMPLE_8_BIT"):
                sampSize = 8
            elif fld["sample_size"] == val("SAMPLE_16_BIT"):
                sampSize = 16
            elif fld["sample_size"] == val("SAMPLE_24_BIT"):
                sampSize = 24
            elif fld["sample_size"] == val("SAMPLE_32_BIT"):
                sampSize = 32
            else:
                raise ValueError("Unknown sample size %d" % fld["sample_size"])

            packing = ('DISABLED' if fld['packing_mode'] ==
                       val('PACKING_DISABLED') else 'ENABLED')
            signExtn = ('DISABLED' if fld['sign_extension'] ==
                        val('SIGN_EXTENSION_DISABLED') else 'ENABLED')
            byteSwap = ('DISABLED' if fld['byte_swap'] ==
                        val('BYTE_SWAP_DISABLED') else 'ENABLED')
            hostPtcn = ('DISABLED' if fld['host_access_protection'] ==
                        val('HOST_ACCESS_PROTECTION_DISABLED') else 'ENABLED')
            rmotPtcn = ('DISABLED' if fld['access_protection'] ==
                        val('ACCESS_PROTECTION_DISABLED') else 'ENABLED')
            bufMode = ('power of 2' if fld['mode'] ==
                       val('MODE_POWER_OF_2_NO_OF_PAGES') else 'arbitrary')
            valid = ('VALID' if fld['validity_tag'] ==
                     val('VALID') else 'INVALID')

            # display nicely:
            output = [
                "Buffer Start      : %08X"        % fld["start"],
                "Offset            : %08X"        % fld["offset"],
                "Valid             : %s"          % valid,
                "Size              : %d (raw %d)" % (fld["size_bytes"],
                                                     fld["size"]),
                "Sample size       : %d bits"     % sampSize,
                "Packing           : %s"          % packing,
                "Sign extension    : %s"          % signExtn,
                "Byte swap         : %s"          % byteSwap,
                "Host protection   : %s"          % hostPtcn,
                "Remote protection : %s"          % rmotPtcn,
                "Buffer mode       : %s"          % bufMode]

            return "\n".join(output)

        def set_handle_bit(self, field, value):
            ''' Set a named bit field within a handle to the given value
            (either 1 or 0).
            e.g. audio.mmu.handleTable[9].set_handle_bit("access_protection", 0)
            '''
            bit_offset = self.value("BAC_BUFFER_" + field)
            byte_number = bit_offset // 8
            byte_bit_pos = bit_offset % 8
            handle = self._mmu.core.dm[self._addr: self._addr+12]
            if value:
                handle[byte_number] |= 1 << byte_bit_pos
            else:
                handle[byte_number] &= ~(1 << byte_bit_pos)
            self._mmu.core.dm[self._addr: self._addr+12] = handle

    def handle(self, idx):
        """Looks up idx in a handle table and returns the handle"""
        return self.MMUHandleTable(self)[idx]

    def handles(self, max_handle=16):
        """Returns a list of handles"""
        return [self.handle(i) for i in range(max_handle)]

    def buf_read_report( #pylint: disable=too-many-arguments
            self,
            idx,
            show_page_addresses=False,
            start=0,
            len=None, #pylint: disable=redefined-builtin
            show_owner=False,
            suppress=False):
            #pylint: disable=arguments-differ
            # because AudioBAc expects idx not handle
        """
        returns a report for content of a buffer specified by idx
        """
        handle = self.handle(idx)
        text = ("Buffer 0x%x | Offset 0x%x | Size 0x%x\n" %
                (idx, handle.fields["offset"], handle.fields["size_bytes"]))

        start_offset = handle.fields["start"] + start
        end_offset = handle.fields["start"] + handle.fields["size_bytes"]
        if len and end_offset > (start_offset + len):
            end_offset = (start_offset + len)
        text += hex_array_str_columns(
            self.core.dm[start_offset: end_offset],
            start_offset=start,
            cursor=handle.fields["offset"])
        # Bundle the raw text into a mini-report
        #
        report = interface.Group('Buffer %d' % idx)
        report.append(interface.Code(text))
        return report

    def _generate_report_body_elements(self):
        """
        Generate a report suitable for Audio BAC as the default one isn't.
        """
        elements = []

        # Configured Buffer summaries
        #
        for handle in self.handleTable.records:
            if handle.isConfigured:
                try:
                    elements.append(self.buf_read_report(handle.index))
                except Exception as exc: #pylint: disable=broad-except
                    elements.append(interface.Code('Error in buf_read(%d): %s'
                                                   % (handle.index, exc)))
        return elements


class MMUFreeList(BaseComponent):
    """\
    MMU Free Page List Arena
    """

    def __init__(self, mmu):
        """\
        Construct FreeList Component.

        Params:-
        - mmu: The containing MMU object.
        """

        self._mmu = mmu
        self._mmuOffset = mmu.memory_offset

        # Shorthands
        fields = mmu._fields

        # Cache control field references ()
        self.addrReg = fields['MMU_FREE_LIST_ADDR']
        self.nextAddrReg = fields['MMU_FREE_LIST_NEXT_ADDR']
        self.endAddrReg = fields['MMU_FREE_LIST_END_ADDR']

    def __repr__(self):
        """Pretty print."""
        # If configured dump the list, else the registers
        if self.isInitialised:
            return "MMUFreeList< records %s >" % repr(self.records)
        return "MMUFreeList< regs %s >" % repr(self.regs)

    # BaseComponent compliance

    @property
    def title(self):
        return 'MMUFreeList'

    # Extensions

    @property
    def regs(self):
        """List of related hardware registers."""
        return [self.addrReg, self.nextAddrReg, self.endAddrReg]

    @property
    def isInitialised(self):
        """Do the hardware registers contain plausible values?"""
        # These registers are initialised to zero on reset. Any non-zero
        # value shows that the firmware has initialised them.
        return self._beginAddr or self._endAddr or self._nextAddr

    @property
    def records(self):
        """\
        List of all records
        """
        # The list is generated on the fly and could race.
        # Could retry to get stable but this could still be a lie so
        # don't bother. - coherence must be dealt with in general/global way
        # at transaction/cache level - not here.

        if not self.isInitialised:
            raise RuntimeError("MMU Free List not initialised")

        records = []

        # Snapshot nextAddr as an index into the records
        next_page_index = ((self._nextAddr - self._beginAddr) *
                           self._mmu.layout_info.addr_unit_bits // 16)

        # Note: endAddr is inclusive
        null_page_size = 16 // self._mmu.core.data.word_bits
        record_values = pack_unpack_data_le(
            self._mmu.readMemoryBlock(
                self._beginAddr,
                (self._endAddr - self._beginAddr) + null_page_size),
            self._mmu.layout_info.addr_unit_bits, 16)
        for index, val in enumerate(record_values):
            offset = index - next_page_index
            records.append(MMUFreeListRecord(val, offset))

        return records

    @property
    def freePages(self):
        """ Return a list of page numbers that are the free pages on the
        free list
        """
        # We could parse the record property to extract the pages we want,
        # but that would mean iterating over it to get just the values out.
        # It seems more efficient to just read the memory since it comes
        # in the format we want.

        # Only sample _nextAddr once, it reads a volatile hardware register.
        next_addr = self._nextAddr
        length = self._endAddr - next_addr
        return pack_unpack_data_le(self._mmu.readMemoryBlock(next_addr, length),
                                   self._mmu.layout_info.addr_unit_bits, 16)

    @property
    def freePageCount(self):
        """
        Returns a count of the number of free pages.
        """
        return ((self._endAddr - self._nextAddr) *
                self._mmu.layout_info.addr_unit_bits // 16)

    # Protected / BaseComponent compliance

    def _generate_report_body_elements(self):

        elements = []

        if self.isInitialised:
            text = ''
            for rec in self.records:
                text += repr(rec)
            elements.append(interface.Code(text))
        else:
            elements.append(interface.Code('Not initialised'))

        return elements


    # Private

    @property
    def _beginAddr(self):
        """
        Address in data-space of first free list record.
        Derived from hardware reg.
        """
        return self._getAddressFromRegister(self.addrReg)

    @property
    def _endAddr(self):
        """
        Address in data-space of last free list record - inclusive.
        Derived from hardware reg.
        """
        return self._getAddressFromRegister(self.endAddrReg)

    @property
    def _nextAddr(self):
        """
        Address in data-space of next free list record.
        Derived from hardware reg.
        """
        return self._getAddressFromRegister(self.nextAddrReg)

    def _getAddressFromRegister(self, reg):
        """
        Read MMU-relative address from specified register and map to
        data-space-relative address.
        """
        mmuAddr = reg.read()
        cpuAddr = mmuAddr + self._mmuOffset
        return cpuAddr

    @staticmethod
    def _get_next_freelist_page_id(cur_page_id):
        return cur_page_id + 1

    @property
    def unusedPageCount(self):
        """
        We initialise the freelist from high page number to low page number.
        The churn of pages being used/freed is at the start of the list, so any
        pages at the end of the list which are sequential in number are very
        likely to never have been used
        """
        free_list = self.freePages
        expected_value = free_list[-1]
        unused_pages = 0

        for i in reversed(free_list):
            if i == expected_value:
                unused_pages += 1
                expected_value = self._get_next_freelist_page_id(expected_value)
            else:
                break
        return unused_pages

    @property
    def pageCount(self):
        """
        Returns the number of pages available to the MMU
        """
        # We have a NULL page at the end, hence the -1
        val = len(self.records) - 1
        return val

    @property
    def usedPageCount(self):
        """
        Returns the number of pages used by the MMU
        """
        val = self.pageCount - self.unusedPageCount
        return val

class BTMMUFreeList(MMUFreeList):
    """
    Unlike the Curator and Apps SS, the BT free page list starts with
    the highest numbered entry at the lowest address
    """
    #pylint: disable=too-few-public-methods
    def _get_next_freelist_page_id(self, cur_page_id):
        return cur_page_id - 1

class MMUFreeListRecord(object):
    """\
    MMU FreeList Record

    At present these are value only objects, they don't know
    what they represent.
    """
    #pylint: disable=too-few-public-methods
    def __init__(self, value, offsetToNext):

        self.value = value
        self.offsetToNext = offsetToNext

    def __repr__(self):
        """\
        Pretty print instances of this class.
        """
        return "%X%s" % (self.value, self._offsetRepr())

    def _offsetRepr(self):
        """\
        Symbol used to indicate relation to next free ptr
        """
        if self.offsetToNext < 0:
            sym = "-"
        elif self.offsetToNext > 0:
            sym = "+"
        else:
            sym = "="
        return sym

class MMUHandleRecord(object):
    """\
    MMU Handle Record
    """
    #pylint: disable=too-many-instance-attributes
    # port     subclass - This is hydra specific
    # Potential extension:     objectify states

    # Special State Codes
    _STATE_FREE = 0
    _STATE_RESERVED = 1
    _STATE_INITIALISED = 0xff

    class NotConfiguredError(RuntimeError):
        """\
        Raised if there's any attempt to use fields from an uninitialised
        HandleRecord.
        """

    def __init__(self, mmu, ix, addr):
        """Construct."""
        self._ix = ix       # for info only
        self._addr = addr
        self._mmu = mmu

        # Build bitfields for the various fields in the handle record.
        self._layout_info = mmu.layout_info
        self._num_words = 64 // self._layout_info.addr_unit_bits
        addrs_per_32_bits = 32 // self._layout_info.addr_unit_bits

        self._pageTableOffsetBitField = AdHocBitField(
            self._mmu.core.data,
            self._layout_info,
            self._addr,
            0, 24)
        self._bufferOffsetBitField = AdHocBitField(
            self._mmu.core.data,
            self._layout_info,
            self._addr + addrs_per_32_bits,
            0, 24)
        self._bufferSizeBitFieldLS = AdHocBitField(
            self._mmu.core.data,
            self._layout_info,
            self._addr,
            24, 8)
        self._bufferSizeBitFieldMS = AdHocBitField(
            self._mmu.core.data,
            self._layout_info,
            self._addr + addrs_per_32_bits,
            24, 3)
        self._bufferModeBitField = AdHocBitField(
            self._mmu.core.data,
            self._layout_info,
            self._addr, 31, 1)

    def __str__(self):
        """Pretty print."""
        return "MMU.HandleRecord< index 0x%X, state < %s > >" % \
                (self.index, self._stateRepr())

    def _stateRepr(self):
        """Pretty print state-dependent info."""
        if self.isConfigured:
            text = ("CONFIGURED, bufferSize 0x%X, pageTableOffset 0x%X, "
                    "bufferOffset 0x%X" % (self.bufferSize,
                                           self.pageTableOffset,
                                           self.bufferOffset))
        elif self.isFree:
            text = "FREE"
        elif self.isReserved:
            text = "RESERVED"
        elif self.isInitialised:
            text = "INITIALISED"
        else:
            text = "UNKNOWN, data %s" % repr(self.words)
        return text

    @property
    def index(self):
        """Index of this Record relative to the Handle Table."""
        return self._ix

    @property
    def isConfigured(self):
        """Is this Record configured and ready/in use?"""
        # EnCoded as page table offset != 0
        offset = self.pageTableOffset
        # Test offset is less than 64k to stop it from falsely
        # identifying handles in WLAN bootloader ROM
        # in practice handles are normally allocated at the
        # start of the mmu memory rather than the end so this
        # shouldn't stop anything working.
        return offset != 0 and offset < 0x10000

    @property
    def isFree(self):
        """Is this Record free to be re-assigned?"""
        return self._isInSpecialState(self._STATE_FREE)

    @property
    def isReserved(self):
        """Is this Record reserved but not yet configured?"""
        return self._isInSpecialState(self._STATE_RESERVED)

    @property
    def isInitialised(self):
        """Is this Record in its initiali state and hence has never been
        used?"""
        return self._isInSpecialState(self._STATE_INITIALISED)

    @property
    def pageTableOffset(self):
        """Raw Page Table Offset field value: first 24 bits of the record """
        return self._pageTableOffsetBitField.read()

    @property
    def bufferOffset(self):
        """Raw Buffer Offset field value: bits 32:56"""
        return self._bufferOffsetBitField.read()

    @bufferOffset.setter
    def bufferOffset(self, value):
        """
        Set the buffer offset, wrapping it if necessary.  This is used by
        MMUPagedBuffer.
        """
        value = value % self.bufferSize
        self._bufferOffsetBitField.write(value)

    def refresh(self):
        """ Refresh cached values to cope with firmware re-using the
        handle
        """
        try:
            del self._bufferSize
        except AttributeError:
            pass
        try:
            del self._bufferSizeField
        except AttributeError:
            pass
        try:
            del self._bufferPages
        except AttributeError:
            pass


    @property
    def bufferSize(self):
        """Raw Buffer Size field value: bits 24:31 and 56:59"""
        try:
            self._bufferSize
        except AttributeError:
            if self._bufferModeBitField.read() == 1:
                self._bufferSize = self.bufferSizeField
            else:
                self._bufferSize = ((1 << self.bufferSizeField) *
                                    self._mmu.page_size)

        return self._bufferSize

    @property
    def bufferSizeField(self):
        """The buffer size"""
        try:
            self._bufferSizeField
        except AttributeError:
            self._bufferSizeField = (self._bufferSizeBitFieldMS.read() << 8 |
                                     self._bufferSizeBitFieldLS.read())
        return self._bufferSizeField

    @property
    def bufferPages(self):
        """\
        Buffer Size pages if isConfigured.

        Raises:-
        - RuntimeError: if this record is not configured.
        """
        if not self.isConfigured:
            raise self.NotConfiguredError("MMU HandleRecord %x not configured"
                                          % self._ix)
        try:
            self._bufferPages
        except AttributeError:
            if self._bufferModeBitField.read() == 0:
                self._bufferPages = 1 << self.bufferSizeField
            else:
                self._bufferPages = self.bufferSize // self._mmu.page_size

        return self._bufferPages

    @property
    def pageTableDataAddress(self):
        """\
        Derive Page Table address wrt data-space.

        Raises:-
        - RuntimeError: if this record is not configured.
        """
        if not self.isConfigured:
            raise self.NotConfiguredError("MMU HandleRecord %x not configured"
                                          % self._ix)
        mmuMemoryOffset = self._mmu.memory_offset
        return (mmuMemoryOffset + self.pageTableOffset *
                (16 // self._layout_info.addr_unit_bits))

    @property
    def pageTable(self):
        """Derive Page Table if this record isConfigured"""
        return MMUPageTable(self._mmu, self.pageTableDataAddress,
                            self.bufferPages)

    @property
    def pagedBuffer(self):
        """\
        Return interface to the paged buffer space implemented by this
        handle.
        """
        return MMUHWPagedBuffer(self._mmu, self)

    @property
    def words(self):
        """List of raw word values from this record"""
        return self._mmu.readMemoryBlock(self._addr, self._num_words)

    def __getitem__(self, ix):
        """Allow direct indexing of record words as convenience"""
        return self.words[ix]

    def _isInSpecialState(self, specialState):
        """Is this record in the specified special state?"""
        # Special states are encoded in buffersize code field when
        # not in use.
        return ((not self.isConfigured) and
                (specialState == self.bufferSizeField))

class MMUPageTable(object):
    """\
    MMU Page Table
    """
    # PORT: subtype for packed words etc.

    def __init__(self, mmu, dataAddr, numPages):
        """\
        Construct Page Table descriptor.
        """
        self._mmu = mmu
        self._dataAddr = dataAddr
        self._numPages = numPages

    def __repr__(self):
        """Pretty print."""
        # Dumps all the records
        return "MMU.PageTable< records %s >" % repr(self.records)

    def __getitem__(self, ix):
        """Alias for records[ix]"""
        return self._getRecord(ix)

    @property
    def records(self):
        """List of all Page Table Record values (not models)."""
        recs = []
        for pageIndex in range(self._numPages):
            recs.append(self._getRecord(pageIndex))
        return recs

    def __len__(self):
        return self._numPages

    @property
    def numPages(self):
        """The number of pages in the MMU page table"""
        return self._numPages

    @property
    def pagedBuffer(self):
        """\
        Return interface to the paged buffer space implemented by this
        page table.
        """
        return MMUPagedBuffer(self._mmu, self)

    def _getRecord(self, pageIndex):
        """
        Get value of specified page record
        """

        # PORT: Currently assumes one page id per memory word.

        if pageIndex >= self._numPages:
            raise IndexError("MMU Page Index (%d) out of range (%d)" %
                             (pageIndex, self._numPages))

        linfo = self._mmu.layout_info
        recordAddr = self._dataAddr + pageIndex * (16 // linfo.addr_unit_bits)
        numAddrPerRecord = 16 // linfo.addr_unit_bits
        return linfo.deserialise(self._mmu.readMemoryBlock(recordAddr,
                                                           numAddrPerRecord))

class MMUPagedBuffer(object):
    """
    Word indexable view of an MMU Paged Buffer.

    Reading and writing to the buffer is done via aliased memory
    space (not MMU ports) so this will not trigger paging and will
    raise MemoryNotPaged for unmapped addresses.
    """
    ID_UNMAPPED_PAGE = 0

    class MemoryNotPaged(RuntimeError):
        """\
        Exception raised on attempt to access unpaged memory.
        """
        def __init__(self, index, page):
            super(MMUPagedBuffer.MemoryNotPaged, self).__init__()
            self.index = index
            self.page = page

        def __str__(self):

            return "memory at index 0x%X (page 0x%X) not paged." % \
                                                    (self.index, self.page)

    def __init__(self, mmu, pageTable):

        self._mmu = mmu
        self._pageTable = pageTable
        self._buf_size = mmu.page_size * len(self._pageTable)

    def __getitem__(self, octetIndex_or_slice):

        if isinstance(octetIndex_or_slice, int_type):
            return self._read_octet(octetIndex_or_slice)
        return self._read_slice(octetIndex_or_slice)

    def _read_octet(self, octetIndex):
        """\
        Access one octet of paged memory by index.
        Raises:-
        - PageNotMapped: If underlying page not mapped.
        """
        # Potential extension:: support slicing
        # Potential extension:: Consider None for unpaged memory addresses? - perhaps not.

        sl = slice(octetIndex, octetIndex+1)
        return self._read_slice(sl)

    def _read_slice(self, slice): #pylint: disable=redefined-builtin
        #pylint: disable=too-many-locals


        if slice.step is not None and slice.step != 1:
            raise IndexError("MMUPagedBuffer: only support single-strided "
                             "slicing!")

        length = slice.stop - slice.start
        buf_size = self._pageTable.numPages * self._mmu.page_size
        while length < 0:
            length += buf_size
        if buf_size < length:
            raise IndexError("MMUPagedBuffer: slice too large for buffer "
                             "(%d > %d)!" % (length, buf_size))

        octetIndex = slice.start % self._buf_size
        data = [0]*length

        length_read = 0

        while length_read < length:
            pageIndex = octetIndex // self._mmu.page_size
            octetOffset = octetIndex % self._mmu.page_size

            pageID = self._pageTable[pageIndex]
            if pageID == MMUPagedBuffer.ID_UNMAPPED_PAGE:
                iprint("Unmapped: %s" % self._pageTable)
                raise self.MemoryNotPaged(octetIndex, pageIndex)

            pageAddr = self._pageIdToAddr(pageID)
            addr_size = (self._mmu.layout_info.addr_unit_bits // 8)
            wordAddr = pageAddr + octetOffset // addr_size
            wordLen = (self._mmu.page_size -
                       octetOffset + addr_size - 1) // addr_size

            cur_page_values = self._mmu.readMemoryBlock(wordAddr, wordLen)

            read_from_page = min(self._mmu.page_size - octetOffset,
                                 length - length_read)
            if self._mmu.layout_info.addr_unit_bits == 8:
                data[length_read:length_read + read_from_page] = \
                        cur_page_values[0:read_from_page]
            elif self._mmu.layout_info.addr_unit_bits == 16:
                #Unpack words out of memory into the data buffer
                for iword, word in enumerate(
                        cur_page_values[0:read_from_page // 2]):
                    data[length_read+2*iword:length_read+2*iword+2] = (
                        [word & 0xff, word >> 8])
                # If we wanted an odd number of octets, we'll have missed the
                # last one
                if read_from_page % 2 == 1:
                    data[length_read + read_from_page - 1] = (
                        cur_page_values[(read_from_page + 1) // 2 - 1] & 0xff)

            length_read += read_from_page
            octetIndex = (octetIndex + read_from_page) % self._buf_size

        return data

    def __setitem__(self, octetIndex_or_slice, data):

        if isinstance(octetIndex_or_slice, int_type):
            return self._write_word(octetIndex_or_slice, data)
        return self._write_slice(octetIndex_or_slice, data)

    def _write_word(self, octetIndex, value):
        """\
        Write one word of paged memory by index.
        Raises:-
        - PageNotMapped: If underlying page not mapped.
        """
        # Potential extension:: support slicing
        # Potential extension:: Consider None for unpaged memory addresses? - perhaps not.

        pageIndex = octetIndex // self._mmu.page_size
        # This variable is not used anywhere.
        # Raised B-290062.
        # pylint: disable=unused-variable
        wordOffset = octetIndex % self._mmu.page_size

        pageID = self._pageTable[pageIndex]
        if pageID == MMUPagedBuffer.ID_UNMAPPED_PAGE:
            iprint("Unmapped: %s" % self._pageTable)
            raise self.MemoryNotPaged(octetIndex, pageIndex)

        pageAddr = self._pageIdToAddr(pageID)
        octetAddr = pageAddr + octetOffset # pylint: disable=undefined-variable

        return self._mmu.writeMemory(octetAddr, value)

    def _write_slice(self,
                     slice, #pylint: disable=redefined-builtin
                     data):

        if slice.step is not None and slice.step != 1:
            raise ValueError("MMUPagedBuffer: only support single-strided"
                             " slicing!")

        length = slice.stop - slice.start
        if len(data) < length:
            raise ValueError("MMUPagedBuffer: too little data supplied for "
                             "slice!")

        octetIndex = slice.start

        length_written = 0

        while length_written < length:
            pageIndex = octetIndex // self._mmu.page_size
            octetOffset = octetIndex % self._mmu.page_size

            pageID = self._pageTable[pageIndex]
            if pageID == MMUPagedBuffer.ID_UNMAPPED_PAGE:
                iprint("Unmapped: %s" % self._pageTable)
                raise self.MemoryNotPaged(octetIndex, pageIndex)
            if pageID < 10:
                # Simple catch for accidentally overwriting the free list
                # Potential extension:: use actual free list pages according to free list
                # registers
                iprint("BadPage: %s" % self._pageTable)
                raise self.MemoryNotPaged(octetIndex, pageIndex)

            pageAddr = self._pageIdToAddr(pageID)
            octetAddr = pageAddr + octetOffset
            wordAddr = pageAddr + (octetOffset // self._mmu.octets_per_word)

            write_to_page = min(self._mmu.page_size - octetOffset,
                                length - length_written)
            if self._mmu.octets_per_word == 1:
                self._mmu.writeMemoryBlock(
                    octetAddr,
                    data[length_written:length_written+write_to_page])
            elif self._mmu.octets_per_word == 2:
                if octetAddr & 1 or write_to_page & 1:
                    raise NotImplementedError("Offset %d, length %d" % (
                        octetAddr, write_to_page))
                self._mmu.writeMemoryBlock(
                    wordAddr,
                    bytes_to_words(
                        data[length_written:length_written+write_to_page]))

            length_written += write_to_page
            octetIndex = (octetIndex + write_to_page) % self._buf_size

    def _pageIdToAddr(self, pageID):
        """\
        Map page id to address wrt MMU memory space.
        """

        pageOffset = pageID * (self._mmu.page_size // self._mmu.octets_per_word)
        return self._mmu.memory_offset + pageOffset

    @property
    def mapped_range(self):
        """
        Report the range of buffer indices that are currently backed by mapped
        pages.  This may loop around from high to low, of course.
        """
        buf_range = [None, None]
        for ipage in range(len(self._pageTable)):
            if buf_range[0] is None:
                if self._pageTable[ipage] != 0:
                    # First mapped page (counting from 0)
                    buf_range[0] = ipage * self._mmu.page_size
            elif buf_range[1] is None:
                if self._pageTable[ipage] == 0:
                    # First unmapped page after mapping
                    buf_range[1] = ipage * self._mmu.page_size
            else:
                if self._pageTable[ipage] != 0:
                    # If we encounter another mapped page then this is the real
                    # start - the mapped pages must loop around from high to low
                    buf_range[0] = ipage * self._mmu.page_size
        if buf_range[1] is None and buf_range[0] is not None:
            buf_range[1] = len(self._pageTable) * self._mmu.page_size
        return buf_range


class MMUHWPagedBuffer(MMUPagedBuffer):
    """
    Models the hardware's view of a paged buffer, allowing reads or writes at
    the buffer hw offset.  This view is based on a particular handle, rather
    than just on a page table as the parent class is.
    """

    def __init__(self, mmu, handleRecord, offset_is_write_point=True):
        """Construct."""

        MMUPagedBuffer.__init__(self, mmu, handleRecord.pageTable)
        handleRecord.refresh()
        self._handleRecord = handleRecord
        # Should we treat the offset as a write handle?
        self._offset_is_write_point = offset_is_write_point



    def read(self, length):
        """read and return length values from MMU paged buffer"""
        if self._offset_is_write_point:
            raise RuntimeError("MMUPagedBuffer: not configured for hardware "
                               "reads!")
        offset = self._handleRecord.bufferOffset
        self._handleRecord.bufferOffset += length
        return self[offset:offset + length]

    def write(self, data):
        """write data values to the MMU paged buffer"""
        if not self._offset_is_write_point:
            raise RuntimeError("MMUPagedBuffer: not configured for hardware "
                               "writes!")
        offset = self._handleRecord.bufferOffset
        self[offset:offset+len(data)] = data
        self._handleRecord.bufferOffset += len(data)

class MMUHandleTable(object):
    """\
    MMU Handle Table
    """
    # PORT: Subclass - This is hydra specific

    def __init__(self, mmu):
        """\
        Construct HandleTable

        Params:-
        - mmu: The containing MMU object.
        """
        self._mmu = mmu
        self._numHandleWords = (self._mmu.num_handle_bits //
                                self._mmu.layout_info.addr_unit_bits)

        beginAddr = self._mmu.memory_offset

        # We don't have a code symbol for this so use the default but
        # we need to make sure it doesn't cause them to run into the
        # free list
        core = mmu.subsystem.cores[0]
        fields = core.field_refs

        free_list_position = fields['MMU_FREE_LIST_ADDR'].read()

        self._beginAddr = beginAddr
        self._numHandles = min(self._mmu.num_handles,
                               free_list_position // self._numHandleWords)
        self._records = self._makeRecords()

    def __repr__(self):
        """Pretty print instances of this class."""
        return "MMU.HandleTable< addr 0x%X, numHandles 0x%X >" % (
            self._beginAddr, self._numHandles)

    @property
    def records(self):
        """List of all handle records."""
        return self._records

    def __getitem__(self, ix):
        """Allow indexing of handle records as convenience"""
        return self.records[ix]

    def _makeRecords(self):
        """Make list of all records in the table"""
        records = []
        for i in range(self._numHandles):
            records.append(self._makeRecord(i))
        return records

    def _makeRecord(self, ix):
        """\
        Make i-th Handle Record
        """
        recordAddr = self._beginAddr + (ix * self._numHandleWords)
        return MMUHandleRecord(self._mmu, ix, recordAddr)

    def create_test_mmu(self, page_location, page_list):
        '''
        Hacky method to create an mmu for use by python testing independent
        of firmware. In fact if this is used with firmware running it will
        probably trash other buffers and/or the free list.
        '''
        #That being so, well just turn off pylint moan about protected access
        #seeing as test code can be expected to go under the hood.
        #pylint: disable=protected-access
        ix = max(len(self._records), 1)
        if self._numHandles <= ix:
            self._numHandles = ix + 1
        handle_addr = self._beginAddr + self._numHandleWords * ix

        new_mmu = MMUHandleRecord(self._mmu, ix, handle_addr)
        new_mmu._pageTableOffsetBitField.write(page_location)
        new_mmu._bufferOffsetBitField.write(0)
        i = 0
        while 2**i < len(page_list):
            i += 1
        new_mmu._bufferSizeBitFieldLS.write(i)
        for i, page in enumerate(page_list):
            self._mmu.writeMemory(
                new_mmu.pageTableDataAddress + i *
                (16 // self._mmu.layout_info.addr_unit_bits), page)
        self._records.append(new_mmu)
        return MMUPagedBuffer(self._mmu, new_mmu.pageTable), ix

    @property
    def unusedHandleCount(self):
        """
        We magic number the handles at initialisation. Using the handles
        destroys that magic number (and freeing doesn't replace it) so we can
        see which handles have never been used
        """
        unused_h = 0
        for handle in self.records:
            if handle.isInitialised:
                unused_h += 1
        return unused_h

    @property
    def numHandles(self):
        """Number of handles in the table"""
        return self._numHandles

    @property
    def numHandleWords(self):
        """Number of handle words in the table"""
        return self._numHandleWords

class BTMMUHandleTable(MMUHandleTable):
    """
    MMU Handle table for Bluetooth processor.
    """
    @property
    def unusedHandleCount(self):
        """
        BT has a variable which tracks this for us
        """
        # For future reference note that we also have handle_counter which shows
        # the current number instead of the max ever used
        return (self._numHandles -
                self._mmu.core.fw.gbl.mmu_hrec.max_handle_cnt.value)

class AppsVMWindow(AddressSlavePort):
    """
    MMU emulation layer that presents the address range [0x1000000:0x20000000)
    in the Apps memory map by mapping access requests onto MMUPagedBuffer reads
    and writes for the appropriate buffer, automatically handling the mapping
    in of pages for writes where necessary
    """
    def __init__(self, core, *args, **kw_args):

        super(AppsVMWindow, self).__init__(*args, **kw_args)
        self._core = core
        self._map_fn = None

    def resolve_access_request(self, access_request):
        """
        Translate an access request to the VM window into a request to the
        underlying MMU pages.  Note that this doesn't allow for pages to be
        mapped in - the processor has to do that.
        """
        # First, grab an MMUPagedBuffer for the buffer corresponding to this
        # access
        start_addr = access_request.region.start
        handle = start_addr >> 20

        mmu = self._core.subsystem.mmu
        handle_rec = mmu.handleTable[handle]
        pagedBuf = handle_rec.pageTable.pagedBuffer

        size = handle_rec.bufferSize
        real_start_addr = start_addr % size
        #real_end_addr = access_request.region.stop % size

        access_size = access_request.region.stop - access_request.region.start
        if isinstance(access_request, WriteRequest):
            # Need to see if the required pages are mapped in and map them in if
            # not
            start_page = real_start_addr // mmu.page_size
            # This may be off the end of the page list, but we'll correct for
            # this below where necessary
            end_page = (real_start_addr + access_size - 1) // mmu.page_size
            for page in range(start_page, end_page+1):
                if handle_rec.pageTable[page % len(handle_rec.pageTable)] == 0:
                    page_start_offset = handle << 20 | page*mmu.page_size
                    if self._map_fn is None:
                        try:
                            self._map_fn = self._core.fw.call.\
                                trap_api_test_map_page_at
                        except AttributeError:
                            try:
                                # We might be on P0, in which case we probably
                                # don't have this function, but P1 may do...
                                # Have a go and see if it works...
                                self._map_fn = self._core.subsystem.p1.fw.\
                                    call.trap_api_test_map_page_at
                            except AttributeError:
                                raise AddressSpace.WriteFailure(
                                    "No firmware support for mapping in MMU "
                                    "pages: can't complete requested write")
                    self._map_fn(page_start_offset)

            pagedBuf[real_start_addr:real_start_addr+access_size] = \
                access_request.data

        else:
            # Turn the access into a series of reads from successive pages
            access_request.data = pagedBuf[
                real_start_addr:real_start_addr+access_size]

    def _extend_access_path(self, access_path):
        """
        No extension to be done here: the VM window doesn't map to lower-level
        address buses that we care about
        """
        pass
