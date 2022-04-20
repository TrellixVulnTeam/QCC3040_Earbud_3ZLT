############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
@file
Pmalloc Firmware Component file.

@section Description
Implements Pmalloc class used for all pmalloc work.

@section Usage
Currently provides the full set of functionality required to see the state of the pmalloc structures
"""
import sys
from collections import Counter
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.env.env_helpers import _Variable, _Pointer, var_member_list, \
    InvalidDereference
from csr.dev.model import interface
from csr.dev.model.interface import Warning as Warn
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dwarf.read_dwarf import  DW_TAG
from csr.dev.framework.meta.elf_firmware_info import DwarfVariableInfo, BadPC
from csr.dev.fw.call import Call
from csr.dev.fw.structs import StructWrapper

class Pmalloc(FirmwareComponent):
    """
    Pmalloc object implementation for hydra generic. This is meant to facilitate
    access to all pmalloc related tools.
    """
    # Set static class variable pmalloc_as_smalloc to 1 to have outputs as smalloc ones.
    pmalloc_as_smalloc = 0

    # Derived classes must implement these
    _dm_pmalloc = None
    _size_field_name = None
    _blocks_field_name = None
    _max_allocated_field_name = None
    _max_ideal_size_field_name = None

    def __init__(self, fw_env, core, parent=None):

        FirmwareComponent.__init__(self, fw_env, core, parent=parent)

        self._elf_code = self.env.build_info.elf_code.program_space

    # FirmwareComponent interface
    #----------------------------

    def _generate_report_body_elements(self):

        elements = []
        elements.append(self.info(True, report=True))

        try:
            debug = self.debug(report=True)
            if debug is not None:
                elements.append(debug)
        except KeyError:
            #It's perfectly valid to not have all the debug options this requires
            #enabled in the firmware, so don't complain
            pass
        return elements

    def _on_reset(self):
        pass

    def _num_blocks(self, pools, pool_idx):
        '''
        Return the number of blocks in the given pool_idx from the pool
        control block list 'pools' (as returned by state).
        Index starts at 0, and can go up to pmalloc_num_pools-1
        Returns None if the index is out of bounds.
        '''
        try:
            return pools[pool_idx][0][self._blocks_field_name].value
        except IndexError:
            return None

    def state(self, check_free_lists=False):
        '''
        Return the pmalloc state as a list of pools and free lists. Each pool
        is a dictionary of fields and the free_list is a list of memory
        block locations.
        Param check_free_lists when TRUE follows the linked list of free
        blocks checking that they are all within the correct memory range
        for the pool. When FALSE an empty free_list will be returned for each
        pool. Note that it takes 5 seconds or so to produce this list on the
        Application subsystem.
        '''
        table_address= self.env.globalvars["pmalloc_pools"].value
        num_pools = self.env.globalvars["pmalloc_num_pools"].value
        min_addr = self.env.globalvars["pmalloc_blocks"].value
        pool_type = self.env.types["pmalloc_pool"]
        bytes_per_addr = self.env._layout_info.addr_unit_bits // 8
        pool_vars = []
        for i in range(num_pools):
            pool = _Variable.create_from_type(pool_type,
                     table_address + i * pool_type["byte_size"] // bytes_per_addr,
                     self.env._data, self.env._layout_info)
            free_list = []
            pool_vars.append((pool, free_list))

        if check_free_lists:
            for idx, (pool, free_list) in enumerate(pool_vars):
                pool_vars[idx] = (pool_vars[idx][0], self._follow_list(pool["free"], min_addr,
                                                   pool["pool_end"].value,
                                                   self._num_blocks(pool_vars, idx)))
        return pool_vars

    def memory_block_size(self, address):
        '''
        Given an address of a pmalloc block this method returns the length
        of that block. Returns None if the address is not within the memory
        pools.
        '''
        pools = self.state()
        for idx, (pool,dummy) in enumerate(pools):
            if address < pool["pool_end"].value and address > (pool["pool_end"].value -
                                             pool["size"].value * self._num_blocks(pools, idx)):
                return pool["size"].value

    def memory_block_mem(self, address):
        size = self.memory_block_size(address)
        if size:
            return self._core.dm[address: address+size]


    def info(self, check_free_lists=False, report=False):
        '''
        Print (or return a report of) the pmalloc state. This is just a
        text formatting wrapper round the data from self.state().
        Param check_free_lists when TRUE follows the linked list of free
        blocks checking that they are all within the correct memory range
        for the pool and showing the first 3.
        Param report when TRUE causes the data to be returned as a report.
        When at the default of FALSE this function just outputs the report
        as text to the console.
        '''
        output = interface.Group("pmalloc")
        output_table = interface.Table()
        pool_vars = self.state(check_free_lists=check_free_lists)
        have_old_stats = False

        if self._max_allocated_field_name in pool_vars[0][0].members:
            # We have 'old' PMALLOC_STATS (max_allocated and overflows)
            have_old_stats = True

        # See if 'new' PMALLOC_STATS is enabled
        if self._max_ideal_size_field_name in pool_vars[0][0].members:
            # If it isn't enabled, set the max_ideal_size_field_name to None
            # If it is, then we also need to know the length of the pmalloc_length array
            try:
                pmalloc_length = len(self.env.gv["pmalloc_length"])
            except KeyError:
                pmalloc_length = 0
        else:
            self._max_ideal_size_field_name = None

        # Build the table header
        output_table_tabs = ["size (%s)" % self._word_size_message, "size (bytes)", "num", "out", "p_free", "p_end"];
        
        if self._max_ideal_size_field_name:
            # With new-style PMALLOC_STATS
            output_table_tabs.insert(4, "ideal")
        if have_old_stats:
            # With old-style PMALLOC_STATS
            output_table_tabs.insert(4, "max_alloc")
            output_table_tabs.insert(5, "overflows")

        if check_free_lists:
            output_table_tabs.append ("free list")

        output_table = interface.Table(output_table_tabs)

        pool_cnt = 0
        for idx, (pool, free_list) in enumerate(pool_vars):
            if free_list:
                free_list_str = "[" + ", ".join(
                       ["0x%4x" % x for x in free_list[0:min(3,len(free_list))]])
                if len(free_list) > 3:
                    free_list_str += "...] (len %d)" % len(free_list)
                else:
                    free_list_str += "]"
                if len(free_list) != (self._num_blocks(pool_vars, idx) -
                                                pool[self._allocated_field_name].value):
                    free_list_str += " Error! Some blocks are missing"
            else:
                free_list_str = ""

            # Build the output data row
            output_table_row = ["%4d" % (pool[self._size_field_name].value // self._storage_unit_to_words_divider),
                                "%4d" % (pool[self._size_field_name].value * self._storage_unit_to_bytes_multiplier),
                                "%4d" % self._num_blocks(pool_vars, idx),
                                "%4d" % pool[self._allocated_field_name].value,
                                "0x%04x" % pool[self._free_list_field_name].value,
                                "0x%04x" % pool["pool_end"].value]

            # With old-style PMALLOC_STATS
            if have_old_stats:
                output_table_row.insert(4, "%4d" % pool[self._max_allocated_field_name].value)
                output_table_row.insert(5, "%4d" % pool[self._overflows_field_name].value)
            # With new-style PMALLOC_STATS
            if self._max_ideal_size_field_name:
                pool_cnt = pool_cnt + self._num_blocks(pool_vars, idx)
                output_table_row.insert(4,"%4s" % (pool[self._max_ideal_size_field_name].value if pool_cnt <= pmalloc_length else "Untracked"))

            if check_free_lists:
                output_table_row.append("%s" % free_list_str)

            output_table.add_row(output_table_row)

            min_addr = pool["pool_end"].value

        output.append(output_table)
        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)

    def _follow_list(self, ptr_obj, min_ptr, max_ptr, max_count):
        '''
        Given a pointer variable (ptr_obj) that is of type "void *", this
        functions follows it as a linked list and checks that each element
        it finds is within the range of min_ptr, max_ptr. A parameter
        max_count limits how far along the linked list it will go.
        Returns a list of the memory addresses that are in the linked list.
        '''
        next = ptr_obj
        # We have to construct a _Pointer object of type "void *" at the
        # address of the next pointer in order to follow the list. This is
        # sort of the equivalent of a cast. We need it because dereferencing
        # a "void *" just gives us a "void" type which we can't do anything
        # with.
        ptr_type = { "type_tag":DW_TAG["pointer_type"],
                        "type_name":"void *",
                        "byte_size":ptr_obj._info.byte_size }
        chain = []
        while next.value != 0:
            if next.value < min_ptr or next.value > max_ptr:
                iprint("Pointer 0x%x[%d] out of range (= 0x%x)" % (
                                          ptr_obj.value, len(chain), next.value))
                break
            if len(chain) >= max_count:
                iprint("Pointer 0x%x depth of %d exceeded" % (ptr_obj.value, 
                                                             len(chain)))
                break
            chain.append(next.value)
            next_info = DwarfVariableInfo(ptr_type, next.value, self.env._layout_info)
            next = _Pointer(next_info, self.env._data)

        return chain

    def likely_owner(self, pc):
        """
        Returns the most likely function name which matches a PC value passed
        We chop off the lowest N bits of the PCs in the FW logging code (to get it to fit in 16 bits)
        so we can only make a "very educated" guess at the function name.
        By losing N bits of precision we have to search +(1<<N) and -(1<<N) from the PC we recorded
        """

        try:
            shift_amount = self.env.enum.PC_TRACE_DETAILS.PC_TRACE_SHIFT
        except AttributeError:
            # A firmware build that doesn't do shifting
            shift_amount = 0

        search_range = 1 << shift_amount

        pcs = list(range(pc, pc+search_range))
        candidates = []
        for cur_pc in pcs:
            try:
                candidates.append(self.env.functions.get_function_of_pc(cur_pc)[1])
            except BadPC:
                pass
        if len(candidates) == 0:
            return "Unknown. Bad PC"

        x = Counter(candidates)
        function_name = x.most_common()[0][0]
        try:
            function_name, _ = function_name
        except ValueError:
            pass
        return function_name

    def debug(self, report=False):
            """
            This function iterates through all allocated blocks and
             * As a bare minimum this function returns the ID/Address of each
             * With PMALLOC_TRACE_OWNER_PC_ONLY returns function names and files
             * With PMALLOC_RECORD_LENGTHS returns sizes requested and wastage
            Returns the names of the functions which allocated memory and the lengths requested.
            Can return a report of the data too
            """
            try:
                pmalloc_owner = self.env.gv["pmalloc_owner"]
            except KeyError:
                pmalloc_owner = None

            try:
                pmalloc_length = self.env.gv["pmalloc_length"]
            except KeyError:
                pmalloc_length = None

            # See if 'new' PMALLOC_STATS is enabled. That affects the pmalloc_length packing.
            if self._max_ideal_size_field_name in self.env.vars["pmalloc_pools"][0].members:
                have_octet_pmalloc_length = True
            else:
                have_octet_pmalloc_length = False

            output = interface.Group("pmalloc debug")

            # Now build up a table header of the correct shape
            # At most we will have the following columns
            # ID/Address  | Size | Size Req | Total Wastage | Fn Name | Fn File
            table_header = []

            if Pmalloc.pmalloc_as_smalloc:
                table_header.append("Address")
            else:
                table_header.append("Address")
                table_header.append("ID")

            table_header.append("Size")

            # Size Req and Total Waste require PMALLOC_RECORD_LENGTHS
            if pmalloc_length is not None:
                table_header.append("Size Req")
                table_header.append("Wastage")
                block_sizes = []
                block_size_granularity = 256
                for i in pmalloc_length:
                    if have_octet_pmalloc_length:
                        block_sizes.append(i.value)
                    else:
                        low_size = i.value % block_size_granularity
                        high_size = (i.value >> 8) % block_size_granularity
                        block_sizes.append(high_size)
                        block_sizes.append(low_size)

            # Fn Name and Fn File require PMALLOC_TRACE_OWNER_PC_ONLY
            if pmalloc_owner is not None:
                table_header.append("Fn Name")
                table_header.append("Fn File")

            output_table = interface.Table(table_header)

            #Now loop through each pool size (4, 8, 16, ...) and record the following
            sizes = []
            blocks = []
            pool_end = []
            free_blocks = []
            pools = self.state(check_free_lists = True)
            for idx, (pool,free_list) in enumerate(pools):
                # Size in bytes of each block in this pool
                sizes.append(pool["size"].value)
                # Number of blocks of this pool size
                blocks.append(self._num_blocks(pools, idx))
                # End address in memory of this pool
                pool_end.append(pool["pool_end"].value)
                # Addresses of all blocks that are free
                free_blocks += free_list

            total_num_blocks = sum(blocks)

            #Now add one row per block to the table
            cur_size_group = 0
            blocks_in_cur_pool = blocks[cur_size_group]
            group_base_address = self.env.globalvars["pmalloc_blocks"].value
            group_base_ctr = 0
            total_wastage = 0
            for cur_block_id in range(0, total_num_blocks):

                if cur_block_id >= blocks_in_cur_pool:
                    group_base_address = address = pool_end[cur_size_group]
                    group_base_ctr = cur_block_id
                    cur_size_group += 1
                    blocks_in_cur_pool += blocks[cur_size_group]

                # Skip over unallocated blocks.
                address = group_base_address + ((cur_block_id - group_base_ctr) * sizes[cur_size_group])
                if address in free_blocks:
                    continue

                if Pmalloc.pmalloc_as_smalloc != 0:
                    current_row = ["0x%4x" % address, sizes[cur_size_group]]
                else:
                    current_row = ["0x%4x" % address, cur_block_id, sizes[cur_size_group]]

                # Track sizes
                if pmalloc_length is not None:
                    if cur_block_id < len(block_sizes):
                        pool_size = sizes[cur_size_group]
                        req_size = block_sizes[cur_block_id]
                        this_wastage = pool_size - req_size
                        #If we get a negative wastage then something bad has happened
                        assert this_wastage >= 0, " Negative wasted calculated!"
                        if req_size + block_size_granularity > pool_size:
                            # Only one solution, adding even one lot of
                            # granularity will make this request too big for
                            # this pool.
                            total_wastage += this_wastage
                            current_row.extend([req_size, total_wastage])
                        else:
                            # Multiple solutions, assume the wastage is minimal
                            this_wastage %= block_size_granularity
                            total_wastage += this_wastage
                            current_row.extend(
                                ["%d + N*%d" % (req_size,
                                                block_size_granularity),
                                 total_wastage])
                    else:
                        current_row.extend(["Untracked", "Untracked"])

                # Track owners
                if pmalloc_owner is not None:
                    if cur_block_id < len(pmalloc_owner):
                        logged_pc = pmalloc_owner[cur_block_id]
                        try:
                            shift_amount = self.env.enum.PC_TRACE_DETAILS.PC_TRACE_SHIFT
                        except AttributeError:
                            # A firmware build that doesn't do shifting
                            shift_amount = 0
                        true_pc = logged_pc.value << shift_amount
                        if true_pc  == Call.MALLOC_TRACE_OWNER:
                             name = "[python]"
                        else:
                            likely_pc_list_or_str = self.likely_owner(true_pc)
                            
                        if isinstance(likely_pc_list_or_str, str):
                            name = likely_pc_list_or_str
                            likely_pc_list = []
                        else:
                            likely_pc_list = likely_pc_list_or_str
                            name = "/".join(sorted(set("%s @0x%x" % 
                                                       (self.env.functions[likely_pc],likely_pc) 
                                                       for likely_pc,_ in likely_pc_list))) # hopefully just one

                        if name == "Unknown. Bad PC" or name == "[python]":
                            src_file = "?"
                        elif likely_pc_list:
                            # likely_owners was smart enough to give be able to
                            # give us the likely specific PC at which the call
                            # to a malloc-like function was made (very occasionally
                            # this is ambiguous so we get a list in general)
                            _,_,fn_api = self.env.functions.get_function_of_pc(likely_pc_list[0][0])
                            try:
                                src_filename, line_no = fn_api.get_srcfile_and_lineno(true_pc)
                                src_file = "{}:{}".format(src_filename, line_no)
                            except AttributeError:
                                src_file = "?"
                            
                            for likely_pc,_ in likely_pc_list[1:]:
                                _,_,fn_api = self.env.functions.get_function_of_pc(likely_pc)
                                try:
                                    src_filename, line_no = fn_api.get_srcfile_and_lineno(likely_pc)
                                    src_file += "/{}:{}".format(src_filename, line_no)
                                except AttributeError:
                                    src_file += "/?"
                        else:
                            # likely_owners wasn't smart enough to give us a specifc
                            # PC back so we're stuck with "true_pc" which is a
                            # misnomer since we've just arbitarily set the
                            # truncated bits to 0.
                            _,_,fn_api = self.env.functions.get_function_of_pc(true_pc)
                            try:
                                src_filename, line_no = fn_api.get_srcfile_and_lineno(true_pc)
                                src_file = "{}:{}".format(src_filename, line_no)
                            except AttributeError:
                                src_file = "?"
                        current_row.extend([name, src_file])
                    else:
                        current_row.extend(["Untracked", "Untracked"])

                output_table.add_row(current_row)

            output.append(output_table)

            #Show total number of pmalloc blocks vs what we are tracking
            total_msg = "Total number of Pmalloc blocks      : %d" % total_num_blocks
            if pmalloc_owner is not None:
                total_msg += "\nTotal number of PC trace blocks     : %d" %  len(pmalloc_owner)
            if pmalloc_length is not None:
                total_msg += "\nTotal number of length trace blocks : %d" % len(block_sizes)

            output.append(interface.Code(total_msg))

            if report is True:
                return output
            TextAdaptor(output, gstrm.iout)

    def requests(self, report=False):
        """
        Shows the total number of request for each block size.
        Requires PMALLOC_RECORD_REQUEST to be enabled in the firmware
        """
        try:
            foo = self.env.gv["pmalloc_requests"]
        except KeyError:
            iprint("Enable the PMALLOC_RECORD_REQUEST debug option in your firmware build")
            return

        output = interface.Group("pmalloc requests")
        output_table = interface.Table()
        output_table.add_row(["Count", "Size"])

        vals = []
        for ctr, i in enumerate(self.env.gv["pmalloc_requests"]):
            if i.value != 0:
                foo = (ctr,i.value)
                vals.append(foo)

        foo = sorted(vals, key=lambda x:x[1])

        for i in reversed(foo):
            output_table.add_row(["%4d" % i[1], "%4d" % i[0]])

        output.append(output_table)
        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)


    @property
    def unused(self):
        """
        Returns the number of bytes of RAM in the pools which have never been used
        """
        total_unused = 0
        total_used = 0
        x = self.state()
        if self._max_allocated_field_name in x[0][0].members:
            # We have 'old' PMALLOC_STATS (max_allocated and overflows)
            for idx, (pool, free_list) in enumerate(x):
                size = pool[self._size_field_name].value
                unused_of_size = self._num_blocks(x, idx) - pool[self._max_allocated_field_name ].value
                total_unused += (size * unused_of_size)
        else:
            # We have new stats, so we don't know the max allocated, we have to
            # work out how much we ideally used and subtract that from the total pool ram
            # For an untracked pool, we need to assume it's fully used.
            pool_cnt = 0
            try:
                tracked_blocks = len(self.env.gv["pmalloc_length"])
            except KeyError:
                tracked_blocks = 0
            for idx, (pool, free_list) in enumerate(x):
                size = pool[self._size_field_name].value
                pool_cnt += self._num_blocks(x, idx)
                if pool_cnt <= tracked_blocks:
                    total_used += (pool[self._max_ideal_size_field_name].value * size)
                else:
                    total_used += (self._num_blocks(x, idx) * size)
            total_unused = self.total_pool_ram - total_used

        return total_unused

    @property
    def total_pool_ram(self):
        """
        Returns the number of bytes of RAM we have carved up into the pools.
        In other words, the amount of RAM we use for pmalloc (ignoring pool control blocks)
        """
        total_in_pools = 0
        x = self.state()
        for idx, (pool, free_list) in enumerate(x):
            size = pool[self._size_field_name].value
            num = self._num_blocks(x, idx)
            total_in_pools += (size * num)
        return total_in_pools

    def _generate_memory_report_component(self):
        dm_pmalloc = self._dm_pmalloc

        if self._core.nicknames[0] == "apps1":
            wastage_msg = "unallocated_ram"
            wastage_comment = ""
        else:
            wastage_msg = "pmalloc_block_wastage"
            wastage_comment = "Ideally this should be zero"

        #We can lose up to 3 bytes in alignment here
        pmalloc_alignment_wastage = {"name":"pmalloc_alignment_wastage",
                                     "start": dm_pmalloc["start"],
                                     "end": self.env.gbl.pmalloc_pools.value,
                                     "size": self.env.gbl.pmalloc_pools.value - \
                                             dm_pmalloc["start"],
                                     "comment":"Ideally this should be zero"}

        pmalloc_pool_control_blocks = {"name":"pmalloc_control_blocks",
                                       "start": self.env.gbl.pmalloc_pools.value,
                                       "end": self.env.gbl.pmalloc_blocks.value,
                                       "size": self.env.gbl.pmalloc_blocks.value - \
                                               self.env.gbl.pmalloc_pools.value}

        pmalloc_pools = {"name":"pmalloc_blocks",
                         "start": self.env.gbl.pmalloc_blocks.value,
                         "end": self._core.fw.pmalloc.state()[-1][0]["pool_end"].value,
                         "size": self._core.fw.pmalloc.state()[-1][0]["pool_end"].value - \
                         self.env.gbl.pmalloc_blocks.value}

        unused = self.unused
        total_pools = self.total_pool_ram
        used = total_pools - unused
        pmalloc_pools["used"] = used
        pmalloc_pools["unused"] = self.unused
        pmalloc_pools["percent_used"] =  (float(used) / self.total_pool_ram) * 100

        pmalloc_block_wastage = {"name":wastage_msg,
                                 "start": self._core.fw.pmalloc.state()[-1][0]["pool_end"].value,
                                 "end":dm_pmalloc["end"],
                                 "size":dm_pmalloc["end"] - \
                                 self._core.fw.pmalloc.state()[-1][0]["pool_end"].value,
                                 "comment":wastage_comment}

        return [dm_pmalloc, [pmalloc_alignment_wastage, pmalloc_pool_control_blocks, pmalloc_pools, pmalloc_block_wastage]]

class AudioPmalloc(Pmalloc):

    def __init__(self, fw_env, core):
        FirmwareComponent.__init__(self, fw_env, core)

        # Potential extension:: This is P0 specific. When we start to become interested in P1 we want
        # "P1MemPool0 ... P1Mem2Pool2"
        self._pool_ram = ['P0MemPool0', 'P0MemPool1', 'P0MemPool2', 'P0MemPool3', 'P0MemPool4',
                          'P0Mem2Pool0', 'P0Mem2Pool1','P0Mem2Pool2',]

    def state(self, check_free_lists=False):
        '''
        Return the pmalloc state as a list of pools and free lists. Each pool
        is a dictionary of fields and the free_list is a list of memory
        block locations.
        Param check_free_lists when TRUE follows the linked list of free
        blocks checking that they are all within the correct memory range
        for the pool. When FALSE an empty free_list will be returned for each
        pool. Note that it takes 5 seconds or so to produce this list on the
        Application subsystem.
        '''
        table_address= self.localvars['aMemoryPoolControl'].address
        num_pools = self.localvars['aMemoryPoolControl'].num_elements
        pool_type = self.env.types["tPlMemoryPoolControlStruct"]
        bytes_per_addr = self.env._layout_info.addr_unit_bits // 8
        pool_vars = []

        for i in range(num_pools):
            pool = _Variable.create_from_type(pool_type,
                     table_address + i * pool_type["byte_size"] // bytes_per_addr,
                     self.env._data, self.env._layout_info)

            free_list = []

            pool_vars.append((pool, free_list))
        return pool_vars

    def info(self, check_free_lists=False, report=False):
        '''
        Print (or return a report of) the pmalloc state. This is just a
        text formatting wrapper round the data from self.state().
        Param check_free_lists when TRUE follows the linked list of free
        blocks checking that they are all within the correct memory range
        for the pool and showing the first 3.
        Param report when TRUE causes the data to be returned as a report.
        When at the default of FALSE this function just outputs the report
        as text to the console.
        '''

        output = interface.Group("pmalloc")
        output_table = interface.Table()
        pool_vars = self.state(check_free_lists=check_free_lists)

        output_table_tabs = ["size (32bit)", "size (byte)", "num", "out", "max_alloc", "p_free", "p_addr"]

        if check_free_lists:
            output_table_tabs.append ("free list")

        output_table = interface.Table(output_table_tabs)

        for i in range(0,len(self._pool_ram)):
            pool_address = self.localvars[self._pool_ram[i]].address
            pool_length = self.localvars[self._pool_ram[i]].size
            pool_size = self.localvars['aMemoryPoolControl'][i].blockSizeWords.value
            free_addr = self.localvars['aMemoryPoolControl'][i].pFirstFreeBlock.value

            # Now we need to look in the constant initialisers to work out how many pools we started with
            temp_addr = self.localvars["aMemoryPoolControl"][i].numBlocksFree.address
            temp_addr += self.env.abs["MEM_MAP_DM2_INITC_ROM_ADDR"]
            pool_n_elements_temp = self._core.dm[temp_addr:temp_addr+2]
            pool_n_elements = (pool_n_elements_temp[1]<<8)|(pool_n_elements_temp[0])

            #'minBlocksFree', 'numBlocksFree'
            pool_minBlocksFree = self.localvars['aMemoryPoolControl'][i].minBlocksFree.value
            pool_numBlocksFree = self.localvars['aMemoryPoolControl'][i].numBlocksFree.value

            bytes_per_word = 4

            output_table_row = ["%4d" % pool_size,
                                "%4d" % (pool_size * bytes_per_word),
                                "%4d" % pool_n_elements,
                                "%4d" % (pool_n_elements - pool_numBlocksFree),
                                "%4d" % (pool_n_elements - pool_minBlocksFree),
                                "0x%04x" % free_addr,
                                "0x%04x" % pool_address]

            output_table.add_row(output_table_row)

        output.append(output_table)
        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)

    @property
    def localvars(self):
        return self.env.cu.pl_malloc.localvars

    @property
    def total_pool_ram(self):
        """
        Returns the number of bytes of RAM we have carved up into the pools.
        In other words, the amount of RAM we use for pmalloc (ignoring pool control blocks)
        """
        total_in_pools_low = 0
        total_in_pools_high = 0
        for i in range(0,len(self._pool_ram)):
            pool_size = self.localvars['aMemoryPoolControl'][i].blockSizeWords.value
            pool_length = self.localvars[self._pool_ram[i]].size
            pool_size = self.localvars['aMemoryPoolControl'][i].blockSizeWords.value
            temp_addr = self.localvars["aMemoryPoolControl"][i].numBlocksFree.address
            temp_addr += self.env.abs["MEM_MAP_DM2_INITC_ROM_ADDR"]
            pool_n_elements_temp = self._core.dm[temp_addr:temp_addr+2]
            pool_n_elements = (pool_n_elements_temp[1]<<8)|(pool_n_elements_temp[0])

            # To avoid unaligned accesses, pmalloc returns blocks which are 4 byte minimum
            bytes_per_block = 4

            # Audio have one extra 32bit word in each pool block so we need a +1 here
            pool_size += 1

            # Audio have a few areas of memory used for Pmalloc. The 2nd set have "Mem2" in the name
            if "Mem2" in self._pool_ram[i]:
                total_in_pools_high += bytes_per_block * (pool_size * pool_n_elements)
            else:
                total_in_pools_low += bytes_per_block * (pool_size * pool_n_elements)
        return [total_in_pools_low, total_in_pools_high]

    @property
    def unused(self):
        """
        Returns the number of bytes of RAM in the pools which have never been used
        """
        total_unused_low = 0
        total_unused_high = 0
        for i in range(0,len(self._pool_ram)):
            pool_size = self.localvars['aMemoryPoolControl'][i].blockSizeWords.value
            pool_minBlocksFree = self.localvars['aMemoryPoolControl'][i].minBlocksFree.value

            # To avoid unaligned accesses, pmalloc returns blocks which are 4 byte minimum
            bytes_per_block = 4

            if "Mem2" in self._pool_ram[i]:
                total_unused_high += bytes_per_block * (pool_size * pool_minBlocksFree)
            else:
                total_unused_low += bytes_per_block * (pool_size * pool_minBlocksFree)
        return [total_unused_low, total_unused_high]

class AudioP0Pmalloc(AudioPmalloc):
    def _generate_memory_report_component(self):
        dm1_p0_pool = self._core.sym_get_range("MEM_MAP_DM1_P0_POOL")
        dm1_p0_pool_total = self.total_pool_ram[0]
        dm1_p0_pool_unused = self.unused[0]
        dm1_p0_pool["used"] = dm1_p0_pool_total - dm1_p0_pool_unused
        dm1_p0_pool["unused"] = dm1_p0_pool_unused
        dm1_p0_pool["percent_used"] =  (float(dm1_p0_pool_total - dm1_p0_pool_unused) / dm1_p0_pool_total) * 100

        dm2_p0_pool  = self._core.sym_get_range("MEM_MAP_DM2_P0_POOL")
        dm2_p0_pool_total = self.total_pool_ram[1]
        dm2_p0_pool_unused = self.unused[1]
        dm2_p0_pool["percent_used"] =  (float(dm2_p0_pool_total - dm2_p0_pool_unused) / dm2_p0_pool_total) * 100
        dm2_p0_pool["used"] = dm2_p0_pool_total - dm2_p0_pool_unused
        dm2_p0_pool["unused"] = dm2_p0_pool_unused

        return [dm1_p0_pool, dm2_p0_pool]

class AudioP1Pmalloc(AudioPmalloc):
    def _generate_memory_report_component(self):
        dm1_p1_pool = self._core.sym_get_range("MEM_MAP_DM1_P1_POOL")
        #if self._core.fw._p1_is_running:
        if self._core.is_running:
            dm1_p1_pool["comment"] = "We don't have code to calculate this"
        else:
            dm1_p1_pool["used"] = 0
            dm1_p1_pool["unused"] = dm1_p1_pool["size"]
            dm1_p1_pool["percent_used"] =  0

        dm2_p1_pool = self._core.sym_get_range("MEM_MAP_DM2_P1_POOL")
        #if self._core.fw._p1_is_running:
        if self._core.is_running:
            dm2_p1_pool["comment"] = "We don't have code to calculate this"
        else:
            dm2_p1_pool["used"] = 0
            dm2_p1_pool["unused"] = dm2_p1_pool["size"]
            dm2_p1_pool["percent_used"] =  0

        return [dm1_p1_pool, dm2_p1_pool]

class AppsPmalloc(Pmalloc):
    _size_field_name = "size"
    _blocks_field_name = "blocks"
    _max_allocated_field_name = "max_allocated"
    _max_ideal_size_field_name = "max_ideal_size"
    _allocated_field_name = "allocated"
    _free_list_field_name = "free"
    _overflows_field_name = "overflows"
    _storage_unit_to_bytes_multiplier = 1
    _storage_unit_to_words_divider = 4
    _word_size_message = "32bit"

    def __init__(self, fw_env, core):
        Pmalloc.__init__(self, fw_env, core)
        self._dm_pmalloc = self._core.sym_get_range('MEM_MAP_PMALLOC')
        self._pool_block_cache = []

    def _call_instructions(self, pcs):
        """
        Determine which of the given list of PCs look like a call to a pmalloc
        PC tracing function
        Returns a caller,callee list, plus a flag to indicate that we hit the
        end of program space.
        """
        candidates = []
        for pc in pcs:
            # Check it's a valid PC
            if pc not in self._elf_code:
                return candidates, True

            # Check it's a minim instruction
            if self.env.functions.convert_to_call_address(pc) != pc+1:
                continue # there could theoretically be minim instructions in
                         # the supplied list
            if not self._core.is_prefix(pc-2):
                called_address = self._core.is_call_to_fixed_address(pc)
                if called_address:
                    candidates.append((pc,
                                      self.env.functions[called_address]))
                else:
                    # Could be a call through a function pointer.  Obviously
                    # the callee is unknown in this case.
                    if self._core.is_call_through_ptr(pc):
                        candidates.append((pc,
                                           None))

        return candidates, False

    def _num_blocks(self, pools, pool_idx):
        '''
        Return the number of blocks in the given pool_idx from the pool
        control block list 'pools' (as returned by state).
        Index starts at 0, and can go up to pmalloc_num_pools-1
        Returns None if the index is out of bounds.
        '''
        if "cblocks" in pools[0][0].members:
            # We have cumulative blocks. Calculate and cache the values
            try:
                return self._pool_block_cache[pool_idx]
            except IndexError:
                # Create the cache
                cblocks = 0
                for idx, (pool,dummy) in enumerate(pools):
                    self._pool_block_cache.append(pool["cblocks"].value - cblocks)
                    cblocks = int(pool["cblocks"].value)
                self._pool_block_cache.append((self.env.globalvars["pmalloc_total_blocks"].value - cblocks))
                del self._pool_block_cache[0]
                return self._pool_block_cache[pool_idx]
        else:
            try:
                return pools[pool_idx][0]["blocks"].value
            except IndexError:
                return None
        return None

    def likely_owner(self, pc):
        """
        A more sophisticated "likely owner" algorithm than the one in the base
        class that looks at all the potential addresses and finds those that
        are call instructions.  If there is more than one, we try to refine the
        list by looking for called functions containing the substring "alloc".
        If that doesn't give us a unique match we return all the matched
        function names in a string.
        """

        try:
            shift_amount = self.env.enum.PC_TRACE_DETAILS.PC_TRACE_SHIFT
        except AttributeError:
            # A firmware build that doesn't do shifting
            shift_amount = 0

        search_range = 1 << shift_amount

        pcs = list(range(pc-search_range, pc+search_range, 2))

        candidate_funcs, out_of_range = self._call_instructions(pcs)

        # but we may have lost significants upper bits too since we only saved
        # 16 bits.
        aliasing_factor = 0
        while not out_of_range:
            aliasing_factor += 1 << (shift_amount + 16)
            funcs, out_of_range = self._call_instructions([pc + aliasing_factor for pc in pcs])
            candidate_funcs += funcs

        if len(candidate_funcs) == 0:
            return "Unknown. Bad PC"
        elif len(candidate_funcs) > 1:
            # More than one function call in range: do any of them look like
            # some kind of malloc/pmalloc/realloc/PanicUnlessMalloc etc?
            filtered_candidate_funcs = [(caller,callee)
                                        for (caller,callee) in candidate_funcs
                                    if callee is not None and "alloc" in callee]
            # If not, just return what we've got
            if len(filtered_candidate_funcs) > 0:
                candidate_funcs = filtered_candidate_funcs
            else:
                iprint("WARNING: no calls that look like allocations found "
                       "matching traced PC 0x%x" % pc)
        return [caller for caller in candidate_funcs]
        
