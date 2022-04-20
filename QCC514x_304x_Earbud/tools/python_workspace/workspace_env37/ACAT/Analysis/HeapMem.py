############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2020 Qualcomm Technologies, Inc. and/or its
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
    OutdatedFwAnalysisError, DebugInfoNoVariableError,
    InvalidDebuginfoEnumError, VariableMemberMissing,
    InvalidDebuginfoTypeError, HeapNotSupported
)


# 'heap_config':() is empty because members are not necessarily accessed,
# 'mem_node' also has members 'line' and 'file' missing since they are in try
VARIABLE_DEPENDENCIES = {
    'strict': (
        'L_processor_heap_info_list', 'L_pheap_info'
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
        "Extra heap",
        "DM for PM heap",
        "External heap"
    ]

    # These are the names of the entries in the enum heap_names
    # in the build.
    heap_names_enum = [
        "HEAP_MAIN",
        "HEAP_SHARED",
        "HEAP_SLOW",
        "HEAP_EXTRA",
        "HEAP_NVRAM",
        "HEAP_EXT"
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
        self.heap_info_list = None
        self.pheap_info = None
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
        heap_info = self.chipdata.cast(self.pheap_info, "heap_config")
        free_heap = heap_info['heap_debug_free'].value
        min_free_heap = heap_info['heap_debug_min_free'].value

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

        heap_info = self.chipdata.cast(self.pheap_info, "heap_config")
        free_heap = heap_info['heap_debug_free'].value
        # Wash down the watermark (min available =  current available)
        self.chipdata.set_data(
            heap_info['heap_debug_min_free'].address,
            [free_heap]
        )

    ##################################################
    # Private methods
    ##################################################

    def _get_heap_id(self, heap_number):
        """ Checks if the supplied heap is supported by the build and
        returns the real ID from the build. If the heap is not
        supported, an exception is raised.

        Args:
            heap_number (int): The heap number specifies the heap from
                which information is asked.

        Returns:
            The real heap ID from the enum in the build.

        Raises:
            HeapNotSupported if the build does not support the heap.
        """

        heap_name = self.heap_names_enum[heap_number]

        heap_names_debug = self.debuginfo.get_enum("heap_names")
        heap_invalid_number = heap_names_debug["HEAP_INVALID"]

        try:
            heap_real_number = heap_names_debug[heap_name]
        except (DebugInfoNoVariableError, KeyError):
            raise HeapNotSupported()

        # It is possible that the heap will be defined even for
        # platforms that do not support, but the enum value
        # would be after HEAP_INVALID to signal that it is not
        # a valid heap, it is merely there to simplify the code.
        if heap_real_number < heap_invalid_number:
            return heap_real_number
        else:
            raise HeapNotSupported()

    def _check_kymera_version(self):
        """Checks if the Kymera version is compatible with this analysis.

        Raises:
            OutdatedFwAnalysisError: For an outdated Kymera.
        """
        try:
            heap_info = self.chipdata.get_var_strict("L_processor_heap_info_list")
            heap_info[0]['heap_debug_free']
        except (DebugInfoNoVariableError, VariableMemberMissing):
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

        # Check if COMMON_SHARED_HEAP is defined
        common_shared_heap_defined = True
        try:
            # This is for Aura and Crescendo that don't support common
            # shared heap.
            endpoint_shadow_state = \
                self.debuginfo.get_type_members("endpoint_shadow_state")
            # This member of the structure was removed for
            # COMMON_SHARED_HEAP.
            if endpoint_shadow_state[2] == 'meta_channel_id':
                common_shared_heap_defined = False
        except InvalidDebuginfoTypeError:
            # This is for Kalsim builds
            common_shared_heap_defined = False

        if heap_name == "Shared heap" and common_shared_heap_defined:
            # If COMMON_SHARED_HEAP is defined, only P0's copy of the heap
            # information is updated (heap_free, freelist, heap_debug_free,
            # heap_debug_min_free). The heap configuration (heap_start,
            # heap_end and heap_size) is correct for P1 as well because it is
            # set once at boot up (with P0's values) and it does not change.
            processor_number = 0
        else:
            processor_number = self.chipdata.processor
        heap_free_start = 0

        # when offloading is enabled the private heap property of the
        # second core is not populated. Use the common heap config to
        # decide if the heap is enabled.
        available, heap_size, heap_start, heap_end = \
            self._get_heap_config(processor_number, heap_number)

        # DM for PM heap usage is displayed as part of the HeapPmMem analysis.
        # In this analysis, only the configuration is displayed.
        if heap_name == "DM for PM heap":
            available = False

        if available is True:
            # Adjust the heap number
            # This function was called before, and it didn't throw the
            # exception (because available is True), therefore it won't
            # throw it this time either.
            heap_number = self._get_heap_id(heap_number)

            heap_info = self.heap_info_list[processor_number]
            heap_free_start = heap_info['freelist'][heap_number].value

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
            heap_number = self._get_heap_id(heap_number)
        except HeapNotSupported:
            return False, 0, 0, 0

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

        # Once we reach here, the heap is available

        heap_info = self.heap_info_list[processor_number]
        heap = heap_info['heap'][heap_number]

        heap_size = heap['heap_size'].value
        heap_start = heap['heap_start'].value
        heap_end = heap['heap_end'].value - 1

        # If the heap size is zero, it's not available.
        available = heap_size != 0

        # The configuration for the extra heap is available, but if
        # the guard is 0, then the heap is not being used (the DM banks
        # are powered off).
        if heap_name == "Extra heap":
            if heap['heap_guard'].value == 0:
                available = False

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
            patch_end = self.debuginfo.get_var_strict("$PATCH_RESERVED_DM_END").address
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

        self.pheap_info = self.chipdata.get_var_strict("L_pheap_info").value

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

    def _generate_heap_configs(self, active_cores=1):
        shared_heap_added = False
        for proc_num in range(active_cores):
            for heap_num in range(self.max_num_heaps):
                _, heap_size, heap_start, heap_end =\
                    self._get_heap_config(proc_num, heap_num)
                is_shared_heap = self.heap_names[heap_num] == "Shared heap"
                if heap_size == 0 or (is_shared_heap and shared_heap_added):
                    continue

                yield proc_num, self.heap_names[heap_num], heap_size,\
                    heap_start, heap_end
                if is_shared_heap:
                    shared_heap_added = True

    def display_memory_map(self):
        """Displays DM memory map."""

        num_cores_active = 1
        try:
            num_cores_active = self.chipdata.get_var_strict(
                '$_proc_number_present_cores'
            ).value
        except DebugInfoNoVariableError:
            pass

        # Look up the debug information.
        self._lookup_debuginfo()

        # sort the heaps in decreasing order of heap_end addresses
        heap_configs =\
            sorted(list(self._generate_heap_configs(num_cores_active)),\
            key=lambda x: x[4], reverse=True)

        self.formatter.section_start('DM Memory Map')

        # Now that all values are read, create the memory map
        output_str = ""

        # display the map
        for config in heap_configs:
            if num_cores_active == 2 and config[1] == "Shared heap":
                output_str += (
                    "==========================\n" +
                    "|                        |<-0x{:08X}\n".format(config[4]) +
                    "|  P0 & P1 {:12}  |\n".format(config[1]) +
                    "|       {:7} Bytes    |\n".format(config[2]) +
                    "|                        |<-0x{:08X}\n".format(config[3])
                )
            else:
                output_str += (
                    "==========================\n" +
                    "|                        |<-0x{:08X}\n".format(config[4]) +
                    "|  P{} {:16}   |\n".format(config[0], config[1]) +
                    "|       {:7} Bytes    |\n".format(config[2]) +
                    "|                        |<-0x{:08X}\n".format(config[3])
                )

        output_str += (
            "==========================\n" +
            "|                        |<-0x{:08X}\n".\
            format(heap_configs[-1][3] - 1) +
            "|  Debug buffers,        |\n" +
            "|  private, static       |\n" +
            "|  and global variables  |\n" +
            "|       {:7} Bytes    |<-0x00000000\n".\
            format(heap_configs[-1][3]) +
            "=========================="
        )

        self.formatter.output_raw(output_str)
        self.formatter.section_end()
