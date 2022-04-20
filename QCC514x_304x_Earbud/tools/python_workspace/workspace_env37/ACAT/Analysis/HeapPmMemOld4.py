############################################################################
# Copyright (c) 2016 - 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
############################################################################
"""
Module which analyses the heap PM ram for QCC515x ROM
"""
from ACAT.Analysis.Heap import Heap
from ACAT.Core.CoreUtils import global_options
from ACAT.Core import Arch
from ACAT.Core.exceptions import (
    DebugInfoNoVariableError,
    InvalidDebuginfoEnumError, OutdatedFwAnalysisError,
    InvalidDebuginfoTypeError
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
                  "PM slow heap P0", "PM slow heap P1",
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
        self.magic_offset = None

        self.heaps = []

        self._check_kymera_version()

    def _get_correct_chip_name(self):
        """Checks if the build id is compatible with this analysis and
        and returns chip name (qcc515x, qcc516x are the only 2 supported).

        Returns:
            chip name for valid analysis, None if it is not valid.
        """
        int_addr = self.debuginfo.get_var_strict(
            '$_build_identifier_integer'
        ).address
        build_id_int = self.debuginfo.get_dm_const(int_addr, 0)
        for i in Arch.ROM_BUILD_INFO:
            if build_id_int == Arch.ROM_BUILD_INFO[i]['build_id']:
                return i
        return None

    def _get_pm_mem_node_from_address(self, pm_addr):
        """Creates and populates freeblock, as if it was pm_mem_node which
        may not be accessible and needs to be populated manually.

        Args:
            addr: PM address where pm_mem_node is

        Returns:
            Populated dictionary with pm_mem_node shape.
        """
        pm_mem_node = {}
        pm_mem_node['u'] = {}
        pm_mem_node['length_32'] = self.chipdata.get_data_pm(pm_addr)
        pm_mem_node['u']['next'] = self.chipdata.get_data_pm(pm_addr + 4)
        pm_mem_node['u']['magic'] = pm_mem_node['u']['next']
        return pm_mem_node

    def _check_kymera_version(self):
        """Checks if the Kymera version is compatible with this analysis.

        Raises:
            OutdatedFwAnalysisError: For outdated Kymera.
        """
        if self._get_correct_chip_name() is None:
            raise OutdatedFwAnalysisError()
        pass

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
        (_, addnl_p1_size, addnl_p1_start, addnl_p1_end, _) = \
            self._get_heap_property(2)

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
                "==============================<-{:8x}\n"
                "|   Additional {} PM Heap    |\n"
                "|        {:6d} Bytes        |\n"
                "==============================<-{:8x}\n"
                "|         P1 Cache           |\n".format(
                    addnl_p1_end,
                    "P1" if p1_size != 0 else "P0",
                    addnl_p1_size,
                    addnl_p1_start,
                )
            )
        elif p1_cache_present:
            output_str += ("==============================\n"
                           "|         P1 Cache           |\n"
                          )

        if p1_size != 0:
            output_str += (
                "==============================<-{:8x}\n".format(p1_end) +
                "|        P1 PM Heap          |\n" +
                "|       {:7d} Bytes        |\n".format(p1_size) +
                "==============================<-{:8x}\n".format(p1_start)
            )

        if p0_size != 0:
            if p1_size == 0 and addnl_p1_size == 0:
                # No P1
                output_str += (
                    "==============================<-{:8x}\n".format(p0_end)
                )
            output_str += (
                "|        P0 PM Heap          |\n" +
                "|       {:7d} Bytes        |\n".format(p0_size) +
                "==============================<-{:8x}\n".format(p0_start)
            )

        if patch_size != 0:
            output_str += (
                "|        Patch Code          |\n"
                "|         {:5d} Bytes        |\n".format(patch_size)
            )

        if code_start != 0 and code_end != 0:
            if patch_size != 0:
                output_str += (
                    "==============================<-{:8x}\n".format(code_end)
                )
            output_str += (
                "|                            |\n"
                "|       PM RAM Code          |\n"
                "|                            |\n"
                "==============================<-{:8x}\n".format(code_start)
            )

        output_str += (
            "==============================\n"
            "|         P0 Cache           |\n"
            "=============================="
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
        if internal_name not in self.pm_block_type:
            return False, 0, 0, 0, 0

        available = False
        heap_size = 0
        heap_start = 0
        heap_end = 0
        heap_free_start = 0

        chip_name = self._get_correct_chip_name()
        pm_heap_block = Arch.ROM_BUILD_INFO[chip_name]['pm_heap_block']
        pm_heap_block = pm_heap_block[heap_number]
        heap_start = self.chipdata.get_data(pm_heap_block['start_addr'])
        heap_end = self.chipdata.get_data(pm_heap_block['end_addr'])

        freelist_pm = Arch.ROM_BUILD_INFO[chip_name]['freelist_pm']
        
        if (internal_name == "PM_BLOCK_ADDL"):
            heap_number = 1
        heap_free_start = self.chipdata.get_data(freelist_pm[heap_number])
        heap_size = heap_end - heap_start
        heap_end = heap_end - 1
        # heap_start can be different than 0 for disabled heaps
        available = heap_size != 0

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
            chip_name = self._get_correct_chip_name()
            if chip_name is not None:
                self.patch_size = Arch.ROM_BUILD_INFO[chip_name]['L_pm_reserved_size']
                self.pm_block_type = Arch.ROM_BUILD_INFO[chip_name]['pm_block_type']
                self.magic_offset = Arch.ROM_BUILD_INFO[chip_name]['magic_offset']
            else:
                self.patch_size = self.debuginfo.get_var_strict(
                    'L_pm_reserved_size'
                ).address
                self.pm_block_type = self.debuginfo.get_enum("PM_BLOCK")
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

        pm_address = address
        pm_address -= Arch.dRegions['PMRAM'][0]
        pm_address += Arch.pRegions['PMRAM'][0]
        try:
            freeblock = self.chipdata.cast(
                pm_address,
                'pm_mem_node',
                False,
                'PM'
            )
            return freeblock['length_32'].value * Arch.addr_per_word
        except InvalidDebuginfoTypeError:
            freeblock = self._get_pm_mem_node_from_address(pm_address)
            return freeblock['length_32'] * Arch.addr_per_word

    def _get_node_next(self, address):
        """Return the address to the node pointed to by the node
           at the specified address .

            Args:
                address

            Returns:
                A address to a node.
        """
        pm_address = address
        pm_address -= Arch.dRegions['PMRAM'][0]
        pm_address += Arch.pRegions['PMRAM'][0]
        try:
            freeblock = self.chipdata.cast(
                pm_address,
                'pm_mem_node',
                False,
                'PM'
            )
            return freeblock['u']['next'].value
        except InvalidDebuginfoTypeError:
            freeblock = self._get_pm_mem_node_from_address(pm_address)
            return freeblock['u']['next']

    def _get_node_magic(self, address):
        """Return the magic value of a PM node.

            Args:
                address

            Returns:
                The node magic value.
        """

        # Because the PM Windows is not necessary enabled at this point,
        # reading must be done in PM space.
        pm_address = address
        pm_address -= Arch.dRegions['PMRAM'][0]
        pm_address += Arch.pRegions['PMRAM'][0]
        testblock = self.chipdata.cast(
            pm_address, 'pm_mem_node', False, 'PM'
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
        pm_address = heap_address
        pm_address -= Arch.dRegions['PMRAM'][0]
        pm_address += Arch.pRegions['PMRAM'][0]
        heap_data = self.chipdata.get_data_pm(pm_address, heap_size)
        return heap_data

    def _get_magic_offset(self):
        """Get heap and magic offset.

            Returns:
                The distance between two 32-bit words.
        """
        return self.magic_offset

