############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Heap Analysis Base Class.

Module which implements the heap allocation logic.
"""
import abc
from collections import namedtuple

from ACAT.Analysis import Analysis
from ACAT.Core import Arch
from ACAT.Core import CoreUtils as cu
from ACAT.Core.exceptions import (
    AnalysisError, FatalAnalysisError, BundleMissingError,
    InvalidDmAddressError, DebugInfoNoVariableError
)

FreeBlocks = namedtuple('FreeBlocks', ['total_size', 'info'])


# pylint: disable=abstract-class-not-used
class Heap(Analysis.Analysis):
    """Encapsulates an analysis for heap memory usage.

    Args:
        **kwarg: Arbitrary keyword arguments.
    """
    heap_names = []
    max_num_heaps = len(heap_names)
    memory_type = "Not set"

    def __init__(self, **kwarg):
        Analysis.Analysis.__init__(self, **kwarg)
        # Look up the debuginfo once. Don't do it here though; we don't want
        # to throw an exception from the constructor if something goes
        # wrong.
        self.pmalloc_debug_enabled = None
        self.dmprofiling_enabled = None

    def display_configuration(self):
        """Prints out the heap configuration for both processors."""
        pass

    def display_memory_map(self):
        """Displays memory map based on current settings."""
        pass

    def display_heaps_blocks(self):
        """Display heaps blocks.

        Displays all the free and allocated blocks for all the heap
        available in the build.
        """
        for heap_num in range(self.max_num_heaps):
            # get information about the current heap.
            (available, heap_size, heap_start, _, heap_free_start) = \
                self._get_heap_property(heap_num)

            # Check if the heap is available in the build.
            if available:
                self.formatter.section_start(self.heap_names[heap_num])
                # Analyse the free blocks.
                free_blocks = self._free_blocks(
                    heap_free_start,
                    heap_start,
                    heap_size,
                    memory_type=self.memory_type
                )
                self.formatter.output_list(
                    free_blocks.info
                )
                # Search for allocated blocks using the magic value.
                (heapinfo, debuginfo) = self._alloc_blocks(
                    heap_start, heap_size,
                    memory_type=self.memory_type
                )
                self.formatter.output_list(heapinfo)
                # if debug is enabled print out the extra information.
                if self.pmalloc_debug_enabled:
                    self.formatter.section_start(
                        self.heap_names[heap_num] + ' Debug Info'
                    )
                    self.formatter.output_list(debuginfo)
                    self.formatter.section_end()

                self.formatter.section_end()

    def get_dmprofiling_heaps_blocks(self, tlist, tf_list, p_list, f_list):
        """List heaps blocks.

        Args:
            tlist:   A list of task-id 'owner' identifiers
            tf_list: A list of transform memory blocks.
            p_list:  A list of main pool block pointers
            f_list:  A list of file related block pointers
        """
        sz_list = [0] * len(tlist)
        blk_list = [[] for _ in range(len(tlist))]

        sz_unknown = 0

        self._lookup_debuginfo()
        for heap_num in range(self.max_num_heaps):
            # Get information about the current heap.
            available, heap_size, heap_start, _, heap_free_start = \
                self._get_heap_property(heap_num)

            # Check if the heap is available in the build.
            if available:
                # Search for allocated blocks using the magic value.
                sz, szunk, block_list = self._get_dmprofiling_alloc_blocks(
                    heap_start, heap_size, tlist, tf_list, p_list, f_list,
                    self.heap_names[heap_num],
                    memory_type=self.memory_type
                )
                for tskid in range(len(tlist)):
                    sz_list[tskid] = sz_list[tskid] + sz[tskid]
                    blk_list[tskid] = blk_list[tskid] + block_list[tskid]
                sz_unknown = sz_unknown + szunk

        return (sz_list, blk_list, sz_unknown)

    def run_all(self):
        """Performs analysis and spew the output to the formatter.

        Displays the heap memory usage and the pmalloc debug info.
        """
        # Look up the debug information.
        self._lookup_debuginfo()

        self.formatter.section_start(
            'Heap %s Memory Info' % (self.memory_type.upper())
        )

        self.display_configuration()
        self.display_memory_map()
        self.overview()
        self.display_heaps_blocks()

        self.formatter.section_end()

    def _get_heap_config_str(self, processor_number, memory_type = "DM"):
        """Get string representation of the heap configurations.

            In other words is the to string function for processor
            configuration.

        Args:
            processor_number

        Returns:
            processor configuration string.
        """
        return_str = ""
        total_size = 0
        for heap_num in range(self.max_num_heaps):
            (available, heap_size, heap_start, heap_end) = \
                self._get_heap_config(processor_number, heap_num)

            if memory_type == "PM" and heap_num == 1:
                if self._supports_discontinuous_heap():
                    return_str += "(PM heap P0 size includes the PM RAM and patch code sizes.)\n"
                if available:
                    # The configuration would be available for P1 PM heap 
                    # if P1 is off, so it could be incorrectly added to
                    # the total usable memory.
                    total_size += heap_size
            else:
                total_size += heap_size

            if available:
                return_str += "%-17s : " % self.heap_names[heap_num]
                return_str += "%s at 0x%08x - 0x%08x\n" % (
                    cu.mem_size_to_string(heap_size),
                    heap_start,
                    heap_end
                )

        return_str += "%-17s : %s" % (
            "Total size",
            cu.mem_size_to_string(total_size)
        )

        return return_str

    def _get_free_space(self):
        """Get total size of free blocks.

            This method reads the free blocks for all heaps and returns a
            string which contains a list with the total size of those free
            blocks.
        """
        # Look up the debug information.
        self._lookup_debuginfo()
        output_str = ""
        total_size = 0
        for heap_num in range(self.max_num_heaps):
            (available, heap_size, heap_start, _, heap_free_start) = \
                self._get_heap_property(heap_num)

            if available:
                free_blocks = self._free_blocks(
                    heap_free_start,
                    heap_start,
                    heap_size,
                    memory_type=self.memory_type
                )
                # display the heap name
                output_str += "%-17s : " % self.heap_names[heap_num]
                free_block_size = free_blocks.total_size
                if self.pmalloc_debug_enabled:
                    # Two words are used for guard.
                    free_block_size -= 2 * Arch.addr_per_word
                output_str += cu.mem_size_to_string(
                    free_block_size
                )
                total_size += free_block_size

                output_str += "\n"

        output_str += "%-17s : %s\n" % (
            "Total size",
            cu.mem_size_to_string(total_size)
        )

        return output_str

    def _get_overview_str(self, detailed=True):
        """Gets Overview string of the available heaps.

        Args:
            detailed (bool, optional): If set to True, a detailed view is
                provided.
        """
        total_heap, free_heap, min_free_heap = self.ret_get_watermarks()

        output_str = ""
        output_str += (
            "%s heap total size :\n" % (self.memory_type.upper())
        )
        if detailed:
            config_str = self._get_heap_config_str(self.chipdata.processor,
                                                   self.memory_type.upper())
            # add indentation for a nicer view.
            output_str += cu.add_indentation(config_str, 2) + "\n"
        else:
            watermark_str = "%-15s : " % "from config"
            watermark_str += cu.mem_size_to_string(total_heap)
            output_str += cu.add_indentation(watermark_str, 2) + "\n"

        output_str += (
            "%s heap current free:\n" % (self.memory_type.upper())
        )
        if detailed:
            free_space_str = self._get_free_space()
            output_str += cu.add_indentation(free_space_str, 2)

        if free_heap != -1:
            watermark_str = "%-15s : " % "from watermarks"
            watermark_str += cu.mem_size_to_string(free_heap)
        else:
            watermark_str = "Watermarks not available for this heap!"
        output_str += cu.add_indentation(watermark_str, 2) + "\n"

        output_str += (
            "%s heap minimum free:\n" % (self.memory_type.upper())
        )

        if min_free_heap != -1:
            watermark_str = "%-15s : " % "from watermarks"
            watermark_str += cu.mem_size_to_string(min_free_heap)
        else:
            watermark_str = "Watermarks not available for this heap!"
        output_str += cu.add_indentation(watermark_str, 2) + "\n"
        return output_str

    def overview(self):
        """Displays an overview of the available heaps."""
        output_str = self._get_overview_str(detailed=True)

        self.formatter.section_start(
            'Heap %s memory usage' % (self.memory_type.upper())
        )
        # use output_raw to keep the spaces.

        # display memory used by patches for PM Heap
        if self.memory_type.upper() == "PM":
            try:
                patch_analysis = self.interpreter.get_analysis(
                    "patches",
                    self.chipdata.processor
                )
                if patch_analysis.get_patch_level() > 0:
                    try:
                        patch_address_start = self.debuginfo.get_constant_strict(
                            '$__pm_patch_code_start_addr'
                        ).value
                    except DebugInfoNoVariableError:
                        patch_address_start = self.debuginfo.get_constant_strict(
                            '$PM_RAM_P0_CODE_START'
                        ).value
                    self.formatter.output_raw(
                        "\nPatch size : {0} at address: 0x{1:0>8x}\n".format(
                            cu.mem_size_to_string(
                                patch_analysis.get_patch_size(),
                                "o"
                            ),
                            patch_address_start
                        )
                    )
            except KeyError:
                # Patch analysis is not released to customers.
                pass

        self.formatter.output_raw(output_str)
        self.formatter.section_end()

    def get_watermarks(self):
        """Displays the memory overview.

        Returns the minimum available memory for the total, the free and
        the minimum free memories.
        """
        output_str = self._get_overview_str(detailed=False)
        # use output_raw to keep the spaces.
        self.formatter.output_raw(output_str)

    def is_address_valid(self, address):
        """Check if an address belongs to a heap.

        Args:
            address Address to verify.

        Returns:
            bool: True if the address belongs to a heap, or False
                otherwise.
        """
        for heap_number in range(self.max_num_heaps):
            (_, _, start, end, _) = self._get_heap_property(heap_number)
            if (address >= start) and (address <= end):
                return True

        return False

    def _free_blocks(self, address, heap_start, heap_size, memory_type="dm"):
        """Checks the free blocks.

        Args:
            address: Address to start with.
            heap_start
            heap_size
            memory_type (str, optional)

        Returns:
            A list describing the free memory allocations.
        """
        free_blocks_info = []
        address_history = []
        total_size = 0
        while address != 0:
            # Avoid infinite loop by checking if the node was already checked.
            if address not in address_history:
                address_history.append(address)
            else:
                self.formatter.error(
                    "Repeating nodes with address 0x%x. "
                    "Probably memory corruption" % address
                )
                return FreeBlocks(total_size, free_blocks_info)

            try:
                freeblock_size = self._get_node_length(address)
            except InvalidDmAddressError:
                self.formatter.error(
                    "Address 0x%x in %s cannot be access. "
                    "Heap cannot be analysed." %
                    (address, str(Arch.get_dm_region(address, False)))
                )

                return FreeBlocks(total_size, free_blocks_info)

            # verify if the address is valid
            if self.is_address_valid(address):
                # If the list node belongs to the current analysed heap
                # display info
                end_of_heap = heap_start + heap_size
                if (address >= heap_start) and (address <= end_of_heap):
                    desc_str = (
                        "Free block size : " +
                        cu.mem_size_to_string(freeblock_size, "o") +
                        " at address: 0x{0:0>8x}".format(address)
                    )
                    free_blocks_info.append(desc_str)
                    total_size += freeblock_size
            else:
                raise FatalAnalysisError(
                    " 0x%x is out of %s heap memory !" % (address, memory_type)
                )

            address = self._get_node_next(address)

        free_blocks_info.append(
            "Total heap free : " + cu.mem_size_to_string(total_size, "ow")
        )
        return FreeBlocks(total_size, free_blocks_info)

    def _get_magic(self, value):
        """Retrieves magic from heap block value.
        This fw-set value is (task-id<<24)|(magic)

        Args:
            value (int): Heap block value.
        """
        if self.dmprofiling_enabled:
            magic = value & 0xFFFFFF
        else:
            magic = value

        return magic

    @staticmethod
    def _get_owner_id(value):
        """Retrieves owner from heap block value.
        This fw-set value is (task-id<<24)|(magic)

        Args:
            value (int): Heap block value.
        """
        owner = value >> 24
        return owner

    def _read_alloc_block(self, block_address, memory_type):
        """Read alloc block.

        Args:
            block_address
            memory_type
        """
        length = self._get_node_length(block_address)
        magic = self._get_node_magic(block_address)
        file_address = self._get_node_file_address(block_address)
        line = self._get_node_line(block_address)

        if file_address is not None:
            if line == 0:
                # 'file' is actually the value of rlink when we called malloc.
                # We can look that up to work out roughly where the allocation
                # took place.
                try:
                    owner = self.debuginfo.get_source_info(
                        file_address)
                    owner_hint = (
                        owner.src_file + ', near line ' +
                        str(owner.line_number)
                    )
                except BundleMissingError:
                    owner_hint = (
                        "No source information." +
                        "Bundle is missing."
                    )
            else:
                try:
                    filename = self.debuginfo.read_const_string(
                        file_address
                    )
                except KeyError:
                    filename = "Filename cannot be read!"
                except BundleMissingError:
                    filename = (
                        "Filename cannot be read! " +
                        "Bundle is missing."
                    )
                owner_hint = (
                    "{0:s}, line {1:d}".format(filename, line)
                )
        else:
            owner_hint = ""

        return length, magic, owner_hint

    def _alloc_blocks(self, heap_address, heap_size, memory_type="dm"):
        """Reads and checks the allocated blocks.

        Args:
            heap_address: The heap start address.
            heap_size: The heap size.
            memory_type (str, optional)

        Returns:
             Two lists. One for the heap allocations the other for debug
                 information.
        """
        magic_val = 0xabcd01
        alloc_info = []
        debug_info = []

        try:
            heap_data = self._get_heap(heap_address, heap_size)
            magic_offset = self._get_magic_offset()
        except InvalidDmAddressError:
            self.formatter.error(
                "Address 0x%x in %s cannot be access. "
                "Heap cannot be analysed." %
                (heap_address, str(Arch.get_dm_region(heap_address, False)))
            )
            return alloc_info, debug_info

        total = 0
        # here index is the index of the magic word in heap_data. Since in
        # heap_pm we are working with 32-bit words and not strictly addresses,
        # for start, index should actually be
        # -Arch.addr_per_word/Arch.arrd_per_word, which is -1
        index = 0

        # Search through the entire heap block, looking for allocated blocks
        # based on the presence of the magic word
        while True:
            try:
                while index < (heap_size/4):
                    if self._get_magic(heap_data[index]) == magic_val:
                        break
                    index+=1
                if index >= (heap_size/4):
                    break
                address = heap_address + \
                    (index - magic_offset) * Arch.addr_per_word

                (length, mval, owner_hint) = \
                    self._read_alloc_block(address, memory_type)

                if self._get_magic(mval) != magic_val:
                    raise AnalysisError(
                        "Magic word not found at expected offset."
                    )
                owner = self._get_owner_id(mval)

                # Check if we are still in valid region
                in_region = (address + length) < (heap_address + heap_size)
                if length > 0 and in_region:
                    alloc_info.append(
                        "Allocated block size : " +
                        cu.mem_size_to_string(length, "o") +
                        " at address: 0x{0:0>8x}".format(address)
                    )
                    index = index + length // Arch.addr_per_word
                    total = total + length
                    if self.pmalloc_debug_enabled:
                        debug_info.append(
                            "Ptr: 0x{0:0>8x} size: ".format(address) +
                            cu.mem_size_to_string(length, "o") +
                            " allocated by: {0:s}".format(owner_hint)
                        )
                elif in_region:
                    alloc_info.append(
                        "Allocated block size : " +
                        cu.mem_size_to_string(length, "o") +
                        " at address: 0x{0:0>8x} *** invalid or corrupt entry: size 0".format(address)
                    )
                    index = index + 1
                else:
                    index = index + 1
            except ValueError:
                break

        alloc_info.append(
            "Total heap allocation : " + cu.mem_size_to_string(total, "ow")
        )

        return alloc_info, debug_info

    def _is_transform_dm_memory(self, address, length, name, tf_list):
        """Check if the address is in the transform list.
        
        Args:
            address: Heap block address.
            length: Size of heap block.
            name: The name (str) of the heap to be searched. (for 'verbose' block info)
            tf_list: A list of transform memory blocks.

        Returns:
             True or False
        """

        # Check if the address is in the transform list (of pointers)
        # Note that 'address' is a node pointer, the transform list
        # pointers are memory pointers, e.g. +8 octets off.
        found_trf = False
        if self.pmalloc_debug_enabled:
            block_offset = 8 + 12
        else:
            block_offset = 8
        for list in [1,2,3]:
            pos = 0
            while True:
                try:
                    idx = tf_list[list+13].index(address + block_offset, pos)
                    # Amend to heapmem format
                    tf_list[list][idx] = address
                    tf_list[list+4][idx] += length
                    tf_list[list+10][idx] = name + " " + tf_list[list+10][idx]
                    found_trf = True
                    pos = idx+1
                except ValueError:
                    break
                    
        return found_trf

    def _is_pool_dm_memory(self, address, length, name, p_list):
        """Check if the address is in the pool pointers list.
        
        Args:
            address: Heap block address.
            length: Size of heap block.
            name: The name (str) of the heap to be searched. (for 'verbose' block info)
            p_list:  A list of main pool block pointers

        Returns:
             True or False
        """

        # Check if the address is in the pool list (of pointers)
        # Note that 'address' is a node pointer, the transform list
        # pointers are memory pointers, e.g. +8 octets off.
        found_pool = False
        if self.pmalloc_debug_enabled:
            block_offset = 8 + 12
        else:
            block_offset = 8
        try:
            idx = p_list[0].index(address + block_offset)
            # Amend to heapmem format (do '-' to keep possible DM2 0xfff00000)
            p_list[1][idx] = p_list[1][idx] - block_offset
            p_list[2][idx] += length
            p_list[3][idx] = name + " " + p_list[3][idx]
            found_pool = True
        except ValueError:
            pass
                    
        return found_pool

    def _is_file_dm_memory(self, address, length, name, f_list):
        """Check if the address is in the file pointers list.
        
        Args:
            address: Heap block address.
            length: Size of heap block.
            name: The name (str) of the heap to be searched. (for 'verbose' block info)
            f_list:  A list of file related block pointers

        Returns:
             True or False
        """

        # Check if the address is in the file list (of pointers)
        # Note that 'address' is a node pointer, the transform list
        # pointers are memory pointers, e.g. +8 octets off.
        found_file = False
        if self.pmalloc_debug_enabled:
            block_offset = 8 + 12
        else:
            block_offset = 8
        for list in [1,2,3]:
            pos = 0
            while True:
                try:
                    idx = f_list[list+13].index(address + block_offset, pos)
                    # Amend to heapmem format
                    f_list[list][idx] = address
                    f_list[list+4][idx] += length
                    f_list[list+10][idx] = name + " " + f_list[list+10][idx]
                    found_file = True
                    pos = idx+1
                except ValueError:
                    break
                    
        return found_file

    def _get_dmprofiling_alloc_blocks(self, heap_address, heap_size, tlist, tf_list, p_list, f_list, name, memory_type="dm"):
        """Reads and checks the allocated blocks.

        Args:
            heap_address: The heap start address.
            heap_size: The heap size.
            tlist:  A list of task-id 'owner' identifiers.
            tf_list: A list of transform memory blocks.
            p_list:  A list of main pool block pointers
            f_list: A list of file related block pointers
            name: The name (str) of the heap to be searched. (for 'verbose' block info)
            memory_type (str, optional)

        Returns:
             Three lists. One for sizes (sum allocations for each tlist entry (owner)),
             an integer that is the sum of unknown allocations (owner not in tlist),
             and a list of a tlist entry's heap allocations (text verbose information)
        """
        magic_val = 0xabcd01

        # Initialise local vars
        index = 0
        sz_unknown = 0

        sz_list = [0] * len(tlist)
        block_list = [[] for i in range(len(tlist))]

        try:
            heap_data = self._get_heap(heap_address, heap_size)
            magic_offset = self._get_magic_offset()
        except InvalidDmAddressError:
            self.formatter.error(
                "Address 0x%x in %s cannot be access. "
                "Heap cannot be analysed." %
                (heap_address, str(Arch.get_dm_region(heap_address, False)))
            )
            return sz_list, sz_unknown, block_list

        total = 0
        # here index is the index of the magic word in heap_data. Since in
        # heap_pm we are working with 32-bit words and not strictly addresses,
        # for start, index should actually be
        # -Arch.addr_per_word/Arch.arrd_per_word, which is -1

        # Search through the entire heap block, looking for allocated blocks
        # based on the presence of the magic word
        while True:
            try:
                while index < (heap_size/4):
                    if self._get_magic(heap_data[index]) == magic_val:
                        break
                    index+=1
                if index >= (heap_size/4):
                    break
                address = heap_address + \
                    (index - magic_offset) * Arch.addr_per_word

                length, mval, owner_hint = \
                    self._read_alloc_block(address, memory_type)

                if self._get_magic(mval) != magic_val:
                    raise AnalysisError(
                        "Magic word not found at expected offset."
                    )
                owner = self._get_owner_id(mval)

                if not self._is_transform_dm_memory(address, length, name, tf_list) and \
                   not self._is_pool_dm_memory(address, length, name, p_list) and \
                   not self._is_file_dm_memory(address, length, name, f_list):
                    sz_unknown = sz_unknown + length
                    for tskid in range(len(tlist)):
                        if (tlist[tskid]&0xFF) == owner:
                            sz_unknown = sz_unknown - length
                            sz_list[tskid] = sz_list[tskid] + length
                            block = [address, length, name]
                            block_list[tskid].append(block)

                # Check if we are still in valid region
                if (length > 0) and \
                        (address + length < heap_address + heap_size):
                    index = index + length // Arch.addr_per_word
                    total = total + length
                else:
                    index = index + 1
            except ValueError:
                break

        return sz_list, sz_unknown, block_list

    @abc.abstractmethod
    def _lookup_debuginfo(self):
        """Queries debuginfo for information.

        We will use this information to get the heap memory usage.
        """

    @abc.abstractmethod
    def ret_get_watermarks(self):
        """Displays the memory overview.

        Returns:
            tuple: The heap usage.
        """

    @abc.abstractmethod
    def _get_heap_property(self, heap_number):
        """Internal function used to get information about a specific heap.

        Args:
            heap_number (int): The heap number specifies the heap from
                which information is asked.

        Returns:
            tuple: Tuple containing information about heap.

                (available, heap_size, heap_start, heap_end, heap_free_start)

                available - True, if the heap is present in the build.
                heap_size - Size in octets.
                heap_start - Start address.
                heap_end - The last valid address.
                heap_free_start - The address of the first available block.
        """

    @abc.abstractmethod
    def _supports_discontinuous_heap(self):
        """Chips that support INSTALL_SWITCHABLE_CACHE can have P0's PM
        heap split in two nodes, rather than being one continuous node.
        This is the case of ROM builds, that have a direct cache.
        
        Returns:
            True if P0's PM heap is discontinuous, False otherwise.
        """

    @abc.abstractmethod
    def _get_heap_config(self, processor_number, heap_number):
        """Read information about heap and return it.

        In dual core configuration information about the heap can be read
        for the other processor too.

        Args:
            processor_number (int): The processor where the heap lives.
            heap_number (int): The heap number specifies the heap from
                which information is asked.

        Returns:
            tuple: Containing information about heap.

                (available, heap_size, heap_start, heap_end)

                available - True, if the heap is present in the build.
                heap_size - Size in octets.
                heap_start - Start address.
                heap_end - The last valid address.
        """

    @abc.abstractmethod
    def _get_heap(self, heap_address, heap_size):
        """Get content of heap.

            Args:
                heap_address
                heap_size

            Returns:
                Heap Data.
        """

    @abc.abstractmethod
    def _get_magic_offset(self):
        """Get magic offset.

            Args:
                memory_type

            Returns:
                The distance between two 32-bit words.
        """

    @abc.abstractmethod
    def _get_node_length(self, address):
        """Return the number of 32 bits words contained by the node
           at the specified address .

            Args:
                address

            Returns:
                The length of the payload in uint32.
        """

    @abc.abstractmethod
    def _get_node_next(self, address):
        """Return the address to the node pointed to by the node
           at the specified address .

            Args:
                address

            Returns:
                A address to a node.
        """

    @abc.abstractmethod
    def _get_node_magic(self, address):
        """Return the magic value of a PM node.

            Args:
                address

            Returns:
                The node magic value.
        """

    @abc.abstractmethod
    def _get_node_file_address(self, address):
        """Return the address of a string containing the file
           that made the allocation

            Args:
                address

            Returns:
                Either the address or None.
        """

    @abc.abstractmethod
    def _get_node_line(self, address):
        """Return the line number of the file
           that made the allocation

            Args:
                address

            Returns:
                Either the line number or None.
        """


