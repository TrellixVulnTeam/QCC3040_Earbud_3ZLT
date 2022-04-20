############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Heap Memory Analysis.

Module responsible to analyse the heap memory in Kymera.
"""
from ACAT.Analysis import DebugLog
from ACAT.Analysis.Heap import Heap
from ACAT.Core import Arch
from ACAT.Core.CoreUtils import global_options
from ACAT.Core.exceptions import (
    OutdatedFwAnalysisError, DebugInfoNoVariableError, InvalidDebuginfoEnumError
)


# 'heap_config':() is empty because members are not necessarily accessed,
# 'mem_node' also has members 'line' and 'file' missing since they are in try
VARIABLE_DEPENDENCIES = {
    'strict': (
        'L_processor_heap_info_list', 'L_pheap_info', '$_heap_debug_free',
        '$_heap_debug_min_free'
    )
}
TYPE_DEPENDENCIES = {
    'heap_config': (),
    'heap_info': ()
}

SRAM_START_ADDRESS = 0xfa000000
SRAM_SIZE = 128 * 1024

DM_PROFILING_PATCH_MAGIC = 0xfab01005

class HeapMem(Heap):
    """Encapsulates an analysis for heap data memory usage.

    Args:
        **kwarg: Arbitrary keyword arguments.
    """
    # heap names
    heap_names = [
        "Main heap",
        "Shared heap",
        "Slow heap",
        "External heap",
    ]
    # maximum number of heaps per processor.
    max_num_heaps = len(heap_names)

    memory_type = "dm"

    def __init__(self, **kwarg):
        Heap.__init__(self, **kwarg)
        # Look up the debuginfo once. Don't do it here though; we don't want
        # to throw an exception from the constructor if something goes
        # wrong.
        self._do_debuginfo_lookup = True
        self.pmalloc_debug_enabled = None
        self.heap_info = None
        self.heap_info_list = None
        self.freelist = None
        self._check_kymera_version()

    def display_configuration(self):
        """Prints out the heap configuration for both processors."""
        # Look up the debug information.
        self._lookup_debuginfo()

        self.formatter.section_start('Heap Configuration')
        num_heap_processors = len(self.heap_info_list)

        for pnum in range(num_heap_processors):
            self.formatter.section_start('Processor %d' % pnum)
            self.formatter.output(
                self._get_heap_config_str(pnum)
            )
            self.formatter.section_end()
        self.formatter.section_end()

    @DebugLog.suspend_log_decorator(0)
    def ret_get_watermarks(self):
        """Same as get_watermarks, but it will return values.

        Returns:
            tuple: The heap usage.
        """
        # Look up the debug information.
        self._lookup_debuginfo()

        total_heap = 0
        free_heap = self.chipdata.get_var_strict("$_heap_debug_free").value
        min_free_heap = self.chipdata.get_var_strict(
            "$_heap_debug_min_free"
        ).value
        
        # Check if COMMON_SHARED_HEAP is enabled for this platform.
        try:
            shared_heap_debug_free = self.chipdata.get_var_strict(
                                     "$_shared_heap_debug_free").value
            shared_heap_min_debug_free = self.chipdata.get_var_strict(
                                    "$_heap_debug_min_free").value
        except DebugInfoNoVariableError:
            shared_heap_debug_free = 0
            shared_heap_min_debug_free = 0
        
        # The common shared heap is reported as part of P0's stats
        if self.chipdata.processor == 0:
            free_heap += shared_heap_debug_free
            # This is not correct, but this is what the firmware does. Unless
            # the firmware is patched (only AuraPlus 1.2 ROM supports the
            # COMMON_SHARED_HEAP in this shape), this should stay like this.
            min_free_heap += shared_heap_min_debug_free

        for heap_num in range(self.max_num_heaps):
            (available, heap_size, _, _, _) = \
                self._get_heap_property(heap_num)
            if available:
                total_heap += heap_size

        return total_heap, free_heap, min_free_heap

    @DebugLog.suspend_log_decorator(0)
    def clear_watermarks(self):
        """Clears the minimum available memory watermark.

        It's doing it by equating it with the current available memory.
        """
        # Look up the debug information.
        self._lookup_debuginfo()

        free_heap = self.chipdata.get_var_strict("$_heap_debug_free").value
        # Wash down the watermark (min available =  current available)
        self.chipdata.set_data(
            self.chipdata.get_var_strict("$_heap_debug_min_free").address,
            [free_heap]
        )

        # Do the same for the COMMON_SHARED_HEAP debug variables
        try:
            shared_free_heap = self.chipdata.get_var_strict(
                                     "$_shared_heap_debug_free").value
            self.chipdata.set_data(
                self.chipdata.get_var_strict(
                    "$_shared_heap_debug_min_free").address, [shared_free_heap])
        except DebugInfoNoVariableError:
            # The build doesn't support COMMON_SHARED_HEAP, nothing to do here
            pass

    ##################################################
    # Private methods
    ##################################################

    def _check_kymera_version(self):
        """Checks if the Kymera version is compatible with this analysis.

        Raises:
            OutdatedFwAnalysisError: For an outdated Kymera.
        """
        try:
            self.debuginfo.get_var_strict("L_heap_single_mode")
        except DebugInfoNoVariableError:
            # fallback to the old implementation
            raise OutdatedFwAnalysisError()

    def _get_heap_property(self, heap_number):
        """Internal function used to get information about a specific heap.

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
        heap_name = self.heap_names[heap_number]
        processor_number = self.chipdata.processor
        heap_free_start = 0

        # when offloading is enabled the private heap property of the
        # second core is not populated. Use the common heap config to
        # decide if the heap is enabled.
        available, heap_size, heap_start, heap_end = self._get_heap_config(
            processor_number, heap_number)

        # Adjust the heap number
        if heap_name == "External heap" and available is True:
            try:
                self.chipdata.get_var_strict("L_slow_heap").value
            except DebugInfoNoVariableError:
                heap_number = heap_number - 1

        if available is True:
            if heap_name == "Shared heap":
                try:
                    heap_free_start = self.chipdata.get_var_strict('L_freelist_shared').value
                except DebugInfoNoVariableError:
                    heap_free_start = self.freelist[heap_number].value
            else:
                heap_free_start = self.freelist[heap_number].value
                
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

            available - True, if the heap is present and the size is non-zero.
            heap_num  - The actual heap number
            heap_size - Size in octets.
            heap_start - Start address.
            heap_end - The last valid address.
        """
        heap_name = self.heap_names[heap_number]
        heap_start = 0
        heap_end = 0
        heap_size = 0
        extmem_cntrl = 0

        try:
            self.chipdata.get_var_strict("L_slow_heap").value
        except DebugInfoNoVariableError:
            if heap_name == "Slow heap":
                return False, 0, 0, 0
            elif heap_name == "External heap":
                heap_number = heap_number - 1

        try:
            extmem_cntrl = self.chipdata.get_var_strict("$_extmem_cntrl").value
        except DebugInfoNoVariableError:
            pass

        if heap_name == "External heap":
            sram_enabled = False
            if extmem_cntrl != 0:
                extmem_blk = self.chipdata.cast(
                    extmem_cntrl, "EXTMEM_CNTRL_BLOCK")
                clk = extmem_blk['cur_clk'].value
                # If clk is greater than EXT_CLK_OFF , then sram is available
                if clk > 1:
                    sram_enabled = True

            if sram_enabled is False:
                return False, 0, 0, 0

        # Once we reach here, the heap is avialable

        heap_info = self.heap_info_list[processor_number]
        proc_config = heap_info['heap']
        heap = proc_config[heap_number]

        heap_size = heap['heap_size'].value
        heap_start = heap['heap_start'].value
        heap_end = heap['heap_end'].value - 1

        # If the heap size is zero, it's not available.
        available = heap_size != 0

        return available, heap_size, heap_start, heap_end

    def _patch_level_dm_profiling(self):
        """Examine the release patch version number for AuraPlus.
           This tells whether DM Profiling is enabled in the patch.

        Returns:
            bool: Whether DM Profiling is enabled in the patch or not.
        """
        try:
            patch_analysis = self.interpreter.get_analysis(
                "patches",
                self.chipdata.processor
            )
            # AuraPlus 1.1: introduced in Audio Package ID 556 of 21/05/2020
            if self.chipdata.get_firmware_id() == 7120:
                if patch_analysis.get_patch_level() >= 10340:
                    return True
                else:
                    return False
            # AuraPlus 1.2: has DM Profiler support in rom
            elif self.chipdata.get_firmware_id() == 11639:
                return True
            # AuraPlus ?.?: ?
            else:
                return False
        except KeyError:
            return False
        return False

    def _patch_dm_profiling(self):
        """Queries the global patch variables for the presence of a patch
           global variable initialised to DM_PROFILING_PATCH_MAGIC. This
           indicates that DM Profiling is enabled in the patch.

        Returns:
            bool: Whether DM Profiling is enabled in the patch or not.
        """

        # If not Hydra or not Kalimba 4, no DM Profiling
        if (Arch.kal_arch != 4) or (Arch.chip_arch != "Hydra"):
            return False
        # Aura and earlier chips have no DM Profiling
        if Arch.chip_id <= 0x4A:
            return False
        # Mora and later chips have DM Profiling
        if Arch.chip_id > 0x4C:
            return True
        # AuraPlus 1.2: has DM Profiler support in rom
        if self.chipdata.get_firmware_id() == 11639:
            return True

        # Look through patch global data if dm profiling magic number is set (debug patch)
        try:
            patch_start = self.debuginfo.get_var_strict("$PATCH_RESERVED_DM_START").address
        except DebugInfoNoVariableError:
            return self._patch_level_dm_profiling()
        try:
            patch_end   = self.debuginfo.get_var_strict("$PATCH_RESERVED_DM_END").address
        except DebugInfoNoVariableError:
            return self._patch_level_dm_profiling()

        buffer_content = self.chipdata.get_data(
            patch_start, patch_end - patch_start
        )

        for offset, value in enumerate(buffer_content):
            if value == DM_PROFILING_PATCH_MAGIC:
                return True

        return False

    def _lookup_debuginfo(self):
        """Queries debuginfo for information.

        The information is needed to get the heap memory usage.
        """
        if not self._do_debuginfo_lookup and not global_options.live:
            # It's a coredump and no need to re-read the variable.
            return

        self._do_debuginfo_lookup = False

        # Freelist
        self.freelist = self.chipdata.get_var_strict('L_freelist')

        # Check for PMALLOC_DEBUG
        # If mem_node has a member called "line" then PMALLOC_DEBUG is enabled
        try:
            _ = self.chipdata.cast(0, "mem_node")['line'].value
            self.pmalloc_debug_enabled = True
        except AttributeError:
            self.pmalloc_debug_enabled = False

        # Check for INSTALL_DM_MEMORY_PROFILING
        # If DMPROFILING_OWNER exists then INSTALL_DM_MEMORY_PROFILING is enabled
        try:
            self.debuginfo.get_enum('DMPROFILING_OWNER')
            self.dmprofiling_enabled = True

        except InvalidDebuginfoEnumError:
            self.dmprofiling_enabled = self._patch_dm_profiling()

        pheap_info = self.chipdata.get_var_strict("L_pheap_info").value
        heap_info = self.chipdata.cast(pheap_info, "heap_config")
        self.heap_info = heap_info['heap']

        # Processor_heap_info_list should be always different than NULL!
        self.heap_info_list = self.chipdata.get_var_strict(
            "L_processor_heap_info_list"
        )

    def _get_heap(self, heap_address, heap_size):
        """Get heap and magic offset.

            Args:
                heap_address
                heap_size

            Returns:
                Heap Data.

            Raises:
                FatalAnalysisError: Memory type not recognized.
        """
        # Get the address we will be working with from the start of heap_pm
        address = heap_address
        return self.chipdata.get_data(
            heap_address, heap_size
        )

    def _get_magic_offset(self):
        """Get heap and magic offset.

            Returns:
                The distance between two 32-bit words.

            Raises:
                FatalAnalysisError: Memory type not recognized.
        """
        testblock = self.chipdata.cast(0, 'mem_node')
        testblock_magic = testblock['u']['magic']

        testblock_address = testblock_magic.address
        # magic_offset here shows the distance between two 32-bit words, first
        # being start of the test block and second being magic value
        magic_offset = testblock_address // Arch.addr_per_word
        return magic_offset

    def _get_node_length(self, address):
        try:
            freeblock = self.chipdata.cast(address, 'mem_node')
            freeblock_size = freeblock['length'].value
        except InvalidDmAddressError:
            self.formatter.error(
                "Address 0x%x in %s cannot be access. "
                "Heap cannot be analysed." %
                (address, str(Arch.get_dm_region(address, False)))
            )
            freeblock_size = 0
        return freeblock_size

    def _get_node_next(self, address):
        freeblock = self.chipdata.cast(address, 'mem_node')
        return freeblock['u']['next'].value

    def _get_node_magic(self, address):
        testblock = self.chipdata.cast(address, 'mem_node')
        return testblock['u']['magic'].value

    def _get_node_file_address(self, address):
        testblock = self.chipdata.cast(address, 'mem_node')
        if self.pmalloc_debug_enabled:
            file_address = testblock['file'].value
        else:
            file_address = None
        return file_address 

    def _get_node_line(self, address):
        testblock = self.chipdata.cast(address, 'mem_node')
        if self.pmalloc_debug_enabled:
            line = testblock['line'].value
        else:
            line = None
        return line

