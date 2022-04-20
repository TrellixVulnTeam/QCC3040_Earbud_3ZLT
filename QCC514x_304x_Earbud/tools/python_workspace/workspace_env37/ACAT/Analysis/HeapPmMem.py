############################################################################
# Copyright (c) 2016 - 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
############################################################################
"""Heap PM Memory Analysis.

Module which analyses the heap PM ram.
"""
from ACAT.Analysis.Heap import Heap
from ACAT.Core.CoreUtils import global_options
from ACAT.Core import Arch
from ACAT.Core.exceptions import (
    DebugInfoNoVariableError,
    InvalidDebuginfoEnumError, OutdatedFwAnalysisError, 
    OutOfRangeError
)

CONSTANT_DEPENDENCIES = {
    'not_strict': (
        '$__pm_heap_start_addr', '$PM_RAM_P0_CODE_START'
    )
}
VARIABLE_DEPENDENCIES = {
    'strict': (
        'L_freelist_pm', '$_pm_heap_block',
        'L_pm_reserved_size'
    )
}
TYPE_DEPENDENCIES = {
    'pm_mem_node': (
        'u.magic', 'length_32',
        'u.next'
    )
}


class HeapPmMem(Heap):
    """Encapsulates an analysis for heap program memory usage.

    Args:
        **kwarg: Arbitrary keyword arguments.
    """
    # heap names
    possible_heaps = ["PM_BLOCK_P0", "PM_BLOCK_P1",
                      "PM_BLOCK_SLOW_P0", "PM_BLOCK_SLOW_P1",
                      "PM_BLOCK_ADDL"]
    heap_names = ["PM heap P0", "PM heap P1",
                  "PM in DM heap P0", "PM in DM heap P1",
                  "PM additional heap"]
    max_num_heaps = len(heap_names)
    memory_type = "pm"

    def __init__(self, **kwarg):
        Heap.__init__(self, **kwarg)

        # Look up the debuginfo once. Don't do it here though; we don't want
        # to throw an exception from the constructor if something goes
        # wrong.
        self._do_debuginfo_lookup = True
        self.patch_size = None
        self.pm_heap = None
        self.pm_heap_block = None
        self.freelist_pm = None
        self.magic_offset = None
        self._interpreter = kwarg.get("interpreter")
        self.supports_discontinuous_heap = False

        self.heaps = []

        self._check_kymera_version()

    def _check_kymera_version(self):
        """Checks if the Kymera version is compatible with this analysis.

        Raises:
            OutdatedFwAnalysisError: For outdated Kymera.
        """
        try:
            self.debuginfo.get_enum("PM_BLOCK")
        except InvalidDebuginfoEnumError:
            # fallback to the old implementation
            raise OutdatedFwAnalysisError()

    def run_all(self):
        """Perform all analysis and spew the output to the formatter.

        Displays the heap_pm memory usage.
        """
        if self.chipdata.processor != 0:
            self.formatter.section_start(
                'Heap %s Memory Info' % (self.memory_type.upper())
            )
            self.formatter.output(
                "Heap PM Memory Analysis is not available for processor %d. \n"
                "The control variables for Heap PM are only available in p0 "
                "domain." % self.chipdata.processor
            )
            self.formatter.section_end()
            return

        Heap.run_all(self)

    #######################################################################
    # Analysis methods - public since we may want to call them individually
    #######################################################################

    def display_memory_map(self):
        """Displays PM memory map based on current settings."""
        self._lookup_debuginfo()

        self.formatter.section_start('PM Memory Map')

        (_, p0_size, p0_start, p0_end, _) = \
            self._get_heap_property(0)
        (_, p1_size, p1_start, p1_end, _) = \
            self._get_heap_property(1)
        (_, p0_slow_size, p0_slow_start, p0_slow_end, _) = \
            self._get_heap_property(2)
        (_, p1_slow_size, p1_slow_start, p1_slow_end, _) = \
            self._get_heap_property(3)
        (_, addnl_p1_size, addnl_p1_start, addnl_p1_end, _) = \
            self._get_heap_property(4)

        if self.patch_size is not None:
            patch_size = self.chipdata.get_data(self.patch_size)

        code_start = self.debuginfo.get_constant_strict(
            '$PM_RAM_P0_CODE_START'
        ).value
        try:
            code_end = self.debuginfo.get_constant_strict(
                '$__pm_heap_start_addr'
            ).value
        except DebugInfoNoVariableError:
            code_end = p0_start - patch_size
            self.formatter.alert(
                'Constants for static code start and '
                'end address is not specified in build.'
            )

        # Mora 2.0 supports SQIF+ROM execution DSPSW-1244
        # This means that P0's heap is split, with the PM RAM
        # and the patch code in between.
        discontinuous_p0_heap_start = 0
        discontinuous_p0_heap_size = 0
        if p0_start < code_start and p0_size != 0:
            # discontinuous PM heap
            self.supports_discontinuous_heap = True
            discontinuous_p0_heap_start = p0_start
            discontinuous_p0_heap_size = code_start - p0_start
            p0_start += patch_size + (code_end - p0_start)
            p0_size -= (code_end - code_start) + patch_size
        else:
            self.supports_discontinuous_heap = False

        p1_cache_present = False
        try:
            offload_data = self.chipdata.get_var_strict(
                '$_offload_data'
            ).value
            if (offload_data != 0):
                p1_cache_present = True
        except DebugInfoNoVariableError:
            pass

        try:
            active_num_cores = self.chipdata.get_var_strict(
                '$_proc_number_present_cores'
            ).value
            if (active_num_cores == 2):
                p1_cache_present = True
        except DebugInfoNoVariableError:
            pass

        # Now that all values are read, create the memory map
        output_str = ""

        if addnl_p1_size != 0:
            output_str += (
                "==============================<-0x{:08X}\n"
                "|   Additional {} PM Heap    |\n"
                "|        {:6d} Bytes        |\n"
                "==============================<-0x{:08X}\n"
                "|         P1 Cache           |\n".format(
                    addnl_p1_end,
                    "P1" if p1_size != 0 else "P0",
                    addnl_p1_size,
                    addnl_p1_start,
                )
            )
        elif p1_cache_present:
            p1_cache_end, p1_cache_start = self._get_p1_cache_config()
            output_str += ("==============================<-0x{:08X}\n".format(p1_cache_end) +
                           "|         P1 Cache           |\n" +
                           "==============================<-0x{:08X}\n".format(p1_cache_start)
                          )

        if p1_size != 0:
            output_str += (
                "==============================<-0x{:08X}\n".format(p1_end) +
                "|        P1 PM Heap          |\n" +
                "|       {:7d} Bytes        |\n".format(p1_size) +
                "==============================<-0x{:08X}\n".format(p1_start)
            )

        if p0_size != 0:
            if p1_size == 0 and addnl_p1_size == 0:
                # No P1
                output_str += (
                    "==============================<-0x{:08X}\n".format(p0_end)
                )
            output_str += (
                "|        P0 PM Heap          |\n" +
                "|       {:7d} Bytes        |\n".format(p0_size - discontinuous_p0_heap_size) +
                "==============================<-0x{:08X}\n".format(p0_start)
            )

        if patch_size != 0:
            output_str += (
                "|        Patch Code          |\n"
                "|         {:5d} Bytes        |\n".format(patch_size)
            )

        if code_start != 0 and code_end != 0:
            if patch_size != 0:
                output_str += (
                    "==============================<-0x{:08X}\n".format(code_end)
                )
            if self.supports_discontinuous_heap == True:
                output_str += (
                    "|                            |\n"
                    "|       PM RAM Code          |\n"
                    "|                            |\n"
                    "==============================<-0x{:08X}\n".format(code_start) +
                    "|        P0 PM Heap          |\n" +
                    "|       {:7d} Bytes        |\n".format(discontinuous_p0_heap_size) +
                    "==============================<-0x{:08X}\n".format(discontinuous_p0_heap_start)
            )
            else:
                output_str += (
                    "|                            |\n"
                    "|       PM RAM Code          |\n"
                    "|                            |\n"
                    "==============================<-0x{:08X}\n".format(code_start)
                )

        output_str += (
            "==============================\n"
            "|         P0 Cache           |\n"
            "==============================\n"
        )

        p1_slow_heap = self._supports_slow_heap_core(1)
        if p1_slow_heap:
            if p1_slow_size != 0:
                output_str += (
                    "==============================<-0x{:08X}\n".format(p1_slow_end) +
                    "|       P1 PM in DM Heap     |\n" +
                    "|       {:7d} Bytes        |\n".format(p1_slow_size) +
                    "==============================<-0x{:08X}\n".format(p1_slow_start)
                )
            else:
                output_str += (
                    "==============================\n"
                    "|       P1 PM in DM Heap     |\n"
                    "|       (powered off)        |\n"
                    "==============================\n"
                )

        if self._supports_slow_heap_core(0):
            if p0_slow_size != 0:
                if p1_slow_size == 0:
                    output_str += (
                        "==============================<-0x{:08X}\n".format(p0_slow_end)
                        )
                output_str += (
                    "|       P0 PM in DM Heap     |\n" +
                    "|       {:7d} Bytes        |\n".format(p0_slow_size) +
                    "==============================<-0x{:08X}\n".format(p0_slow_start)
                )
            else:
                if not p1_slow_heap:
                    output_str += (
                        "==============================<\n"
                        )
                output_str += (
                    "|       P0 PM in DM Heap     |\n"
                    "|       (powered off)        |\n"
                    "==============================\n"
                )
        self.formatter.output_raw(output_str)
        self.formatter.section_end()

    @staticmethod
    def ret_get_watermarks():
        """heap_pm does not have watermark information
        the values returned are made 0, -1, -1 to escape
        _get_overview_str function's main purpose in heap.py
        as we dont have watermarks here.
        """
        return 0, -1, -1

    def _supports_slow_heap_core(self, core):
        """Determine if the core supports the PM in DM heap.
        
        Args:
            core (int): The core ID
            
        Returns:
            True if the heap is supported, False otherwise
        """
        if core == 0:
            slow_heap_enum = 'PM_BLOCK_SLOW_P0'
        else:
            slow_heap_enum = 'PM_BLOCK_SLOW_P1'

        for key in self.pm_block_type.keys():
            if key == slow_heap_enum:
                return True

        return False

    def _get_p1_cache_config(self):
        """Get P1's cache configuration.
        
        Returns:
            tuple: Containing information about P1's cache.
                (p1_cache_end, p1_cache_start)
                
                p1_cache_end - The upper limit of P1's cache
                p1_cache_start - The start address of P1's cache
        """
        # P1 cache is always at the end of the memory map
        p1_cache_end = Arch.get_pm_end_limit()
        try:
            # For platforms support cache switching, based on P1CacheMode 
            # (PMCacheMode for P0), the value returned by $PM_CACHE_SIZE_OCTETS
            # could be incorrect. Therefore, follow the firmware code, and set
            # the size of P1's cache depending on this variable. This variable is only
            # available with this feature.
            p1_pm_cache_2way = self.chipdata.get_var_strict("$_pm_cache_2way_p1").value
            if p1_pm_cache_2way:
                # Two way half cache is not used for Mora and AuraPlus, if the cache switching
                # feature is available, so it's safe to assume that the size would be equal to 
                # the two way cache one.
                p1_cache_size = self.debuginfo.get_constant_strict(
                                    '$TWO_WAY_CACHE_SIZE'
                                ).value
            else:
                p1_cache_size = self.debuginfo.get_constant_strict(
                                    '$DIRECT_CACHE_SIZE'
                                ).value
        except DebugInfoNoVariableError:
            # This symbol was introduced in Feb 2018, firmware older than this
            # should fall back on older analyses anyway
            p1_cache_size = self.debuginfo.get_constant_strict(
                                '$PM_CACHE_SIZE_OCTETS'
                            ).value
        p1_cache_start = p1_cache_end - p1_cache_size
        
        return p1_cache_end, p1_cache_start

    def _supports_discontinuous_heap(self):
        """Chips that support INSTALL_SWITCHABLE_CACHE can have P0's PM
        heap split in two nodes, rather than being one continuous node.
        This is the case of ROM builds, that have a direct cache. This
        function will only function correctly if the memory map is
        displayed first.
        
        Returns:
            True if P0's PM heap is discontinuous, False otherwise.
        """
        return self.supports_discontinuous_heap

    def _get_addr(self, addr, offset):
        """ Adds the offset to the PM address if requested. 
        Otherwise, it returns the address unchanged.

        Args:
            addr (int): The PM address.
            offset (int): The heap offset.

        Returns:
            The address unchanged if add_offset is False, or the
        address with the offset added, if add_offset is True.
        """
        if addr != 0:
            # Only 32-bit values are needed
            # The resulting value is negative, so the sign bits 
            # need to be masked
            addr = (addr - offset) & 0xFFFFFFFF
        
        return addr

    def _get_heap_property(self, heap_number):
        """Gets information about a specific heap.

        Args:
            heap_number (int): The heap number specifies the heap from
                which information is asked.

        Returns:
            tuple: Containing information about heap.

                (available, heap_size, heap_start, heap_end, heap_free_start)

                available - True, if the heap is present in the build.
                heap_size - Size in octets.
                heap_start - Start address.
                heap_end - The last valid address.
                heap_free_start - The address of the first available block.
        """
        internal_name = self.possible_heaps[heap_number]

        try:
            heap_real_number = self.pm_block_type[internal_name]
        except (KeyError, IndexError):
            return False, 0, 0, 0, 0

        number_pm_blocks_number = self.pm_block_type["NUMBER_PM_BLOCKS"]
        if heap_real_number >= number_pm_blocks_number:
            # Heap is not supported.
            return False, 0, 0, 0, 0

        pm_heap_block = self.chipdata.get_var_strict("$_pm_heap_block")
        pm_heap_block = pm_heap_block[heap_real_number]
        offset = pm_heap_block['offset'].value

        heap_start = self._get_addr(
                        pm_heap_block['start_addr'].value, 
                        offset
                    )
        heap_end = self._get_addr(
                        pm_heap_block['end_addr'].value, 
                        offset
                    )
        freelist_pm = self.chipdata.get_var_strict("L_freelist_pm")
        heap_free_start = self._get_addr(
                            freelist_pm[heap_real_number].value, 
                            offset
                        )
        heap_size = heap_end - heap_start
        heap_end = heap_end - 1
        # heap_start can be different than 0 for disabled heaps
        available = heap_size != 0

        if internal_name == "PM_BLOCK_P1" and available is True:
            # P1 heap start and end are configured even if P1
            # is not started for some platforms.
            if self._interpreter.processors['p1'].is_booted() is False:
                available = False
        
        return available, heap_size, heap_start, heap_end, heap_free_start

    def _get_heap_config(self, processor_number, heap_number):
        """Get heap configuration.

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
        if self.chipdata.processor == processor_number:
            # properties is (available, heap_size, heap_start, heap_end, _)
            properties = self._get_heap_property(heap_number)
            
            return properties[:-1]

        self.formatter.error(
            "_get_heap_config called for heap pm" +
            "processor_number = %d, heap_number = %d" % (
                processor_number, heap_number
            )
        )
        return False, 0, 0, 0

    def _lookup_debuginfo(self):
        """Queries debuginfo for information.

        We need this information to get the heap pm memory usage.
        """
        if not self._do_debuginfo_lookup and not global_options.live:
            # It's a coredump and no need to re-read the variable.
            return

        self._do_debuginfo_lookup = False

        try:
            self.patch_size = self.debuginfo.get_var_strict(
                'L_pm_reserved_size'
            ).address
            self.pm_block_type = self.debuginfo.get_enum("PM_BLOCK")
            self.pm_heap_block = self.chipdata.get_var_strict("$_pm_heap_block")
            self.freelist_pm = self.chipdata.get_var_strict("L_freelist_pm")

            testblock = self.chipdata.cast(0, 'pm_mem_node', False, 'DM')
            testblock_address = testblock['u']['magic'].address
            self.magic_offset = testblock_address // Arch.addr_per_word
        except DebugInfoNoVariableError:
            self.patch_size = 0

    def _get_node_length(self, address):
        """Return the number of 32 bits words contained by the node
           at the specified address .

            Args:
                address

            Returns:
                The length of the payload in uint32.
        """
        # Check if it's a DM_AS_PM address, checking this is 
        # needed only for coredumps, the PM casting would work
        # on a live chip.
        if Arch.get_pm_region(address) == "DM_AS_PM":
            # Get the address without offset
            address += Arch.dRegions['DM_AS_PM'][0]
            address -= Arch.pRegions['DM_AS_PM'][0]
            
            freeblock = self.chipdata.cast(
                            address,
                            'pm_mem_node'
                        )
        else:
            freeblock = self.chipdata.cast(
                address,
                'pm_mem_node',
                False,
                'PM'
            )

        return freeblock['length_32'].value * Arch.addr_per_word

    def _get_node_next(self, address):
        """Return the address to the node pointed to by the node
           at the specified address .

            Args:
                address

            Returns:
                A address to a node.
        """
        # Check if it's a DM_AS_PM address, checking this is 
        # needed only for coredumps, the PM casting would work
        # on a live chip.
        if Arch.get_pm_region(address) == "DM_AS_PM":
            # Get the address without offset
            address += Arch.dRegions['DM_AS_PM'][0]
            address -= Arch.pRegions['DM_AS_PM'][0]
            
            freeblock = self.chipdata.cast(
                            address,
                            'pm_mem_node'
                        )

            next_address = freeblock['u']['next'].value
            if next_address != 0:
                # This logic is doing what _get_addr() does,
                # adding the offset, since the offset is not available here.
                next_address -= Arch.dRegions['DM_AS_PM'][0]
                next_address += Arch.pRegions['DM_AS_PM'][0]
        else:
            freeblock = self.chipdata.cast(
                address,
                'pm_mem_node',
                False,
                'PM'
            )

            next_address = freeblock['u']['next'].value
            if next_address != 0:
                # This logic is doing what _get_addr() does,
                # adding the offset, since the offset is not available here.
                next_address -= Arch.dRegions['PMRAM'][0]
                next_address += Arch.pRegions['PMRAM'][0]

        return next_address

    def _get_node_magic(self, address):
        """Return the magic value of a PM node.

            Args:
                address

            Returns:
                The node magic value.
        """
        # Because the PM Windows is not necessary enabled at this point,
        # reading must be done in PM space.
        if Arch.get_pm_region(address) == "DM_AS_PM":
            # Get the address without offset
            address += Arch.dRegions['DM_AS_PM'][0]
            address -= Arch.pRegions['DM_AS_PM'][0]
            
            testblock = self.chipdata.cast(
                            address,
                            'pm_mem_node'
                        )
        else:
            testblock = self.chipdata.cast(
                address, 'pm_mem_node', False, 'PM'
            )

        magic = testblock['u']['magic'].value
        return magic

    def _get_node_file_address(self, address):
        """ Does nothing for PM
        """
        return None 

    def _get_node_line(self, address):
        """ Does nothing for PM
        """
        return None

    def _get_heap(self, heap_address, heap_size):
        """Get content of heap.

            Args:
                heap_address
                heap_size

            Returns:
                Heap Data.
        """
        # Because the PM Windows is not necessary enabled at this point,
        # reading must be done in PM space.
        if Arch.get_pm_region(heap_address) == "DM_AS_PM":
            # Get the address without offset
            heap_address += Arch.dRegions['DM_AS_PM'][0]
            heap_address -= Arch.pRegions['DM_AS_PM'][0]
            
            heap_data = self.chipdata.get_data(heap_address, heap_size)
        else:
            heap_data = self.chipdata.get_data_pm(heap_address, heap_size)

        return heap_data

    def _get_magic_offset(self):
        """Get heap and magic offset.

            Returns:
                The distance between two 32-bit words.
        """
        return self.magic_offset

