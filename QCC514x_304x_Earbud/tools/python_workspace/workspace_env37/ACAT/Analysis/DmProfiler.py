############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""DM Profiler Analysis.

Module used for DM profiling.
"""
from ACAT.Analysis import Analysis
from ACAT.Core import Arch
from ACAT.Core.exceptions import BundleMissingError
from ACAT.Core.CoreTypes import ChipVarHelper as ch
from ACAT.Core.exceptions import (
    OutdatedFwAnalysisError, InvalidDebuginfoEnumError,
    DebugInfoNoVariableError, VariableMemberMissing)

DM_PROFILING_PATCH_MAGIC = 0xfab01005


class DmProfiler(Analysis.Analysis):
    """
    Encapsulates an analysis for DM memory: find the owner of individual
    blocks, in heap and pools. Then for each owner report the sum of owned
    blocks, and a list of allocated blocks for (a list of) owner(s).

    Args:
        **kwargs: Arbitrary keyword arguments.
    """

    def __init__(self, **kwargs):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwargs)

        self.heap = self.interpreter.get_analysis(
            "heapmem", self.chipdata.processor
        )
        self.pools = self.interpreter.get_analysis(
            "poolinfo", self.chipdata.processor
        )
        self.stream = self.interpreter.get_analysis(
            "stream", self.chipdata.processor
        )

        self._transforms = []
        self._transforms_accounted = []
        self._check_kymera_version()

    def run_all(self):
        """Analyse DM memory used by operator tasks.
        """
        self.analyse_dm_memory(False, False)

    def profile_dm_memory(self, every=False):
        """Analyse DM memory used by tasks and return a list
           with profile information. Each list entry is a list
           itself with N entries:

           0. Task ID,
           1. Task description,
           2. Sum size of allocations (in octets),
           3. Sum size of all heap allocations (in octets),
           4. Sum size of all pool allocations (in octets),
           5. List of the allocated blocks for this entry's task ID, each
              of which is a list by itself, e.g.:

            ::

              [[address1, length1, name1],
               [address2, length2, name2],
               ...
              ]

        Args:
            every (bool): If True, analyse only operator tasks,
                 if False, analyse all tasks.
        """
        task_generator = DMProfilerTaskIDs(self)
        t_list, tstr_list = task_generator.get_tasks(every)
        transform_generator = DMProfilerTransformIDs(self)
        tf_list = transform_generator.get_dm_transforms()
        poolptr_generator = DMProfilerPools(self)
        p_list = poolptr_generator.get_dm_pools()
        fileptr_generator = DMProfilerFiles(self)
        f_list = fileptr_generator.get_dm_files()
        hsz_list, hblk_list, hsz_unknown = self.heap.get_dmprofiling_heaps_blocks(
            t_list, tf_list, p_list, f_list)
        psz_list, pblk_list, psz_unknown = self.pools.get_dmprofiling_pool_blocks(
            t_list, tf_list, f_list)

        sz_list = [
            [
                item,
                tstr_list[i],
                hsz_list[i] + psz_list[i],
                hsz_list[i],
                psz_list[i],
                hblk_list[i] + pblk_list[i]
            ]
            for i, item in enumerate(t_list)
        ]

        entry = [
            256,
            "Unknown (not in task list)",
            hsz_unknown + psz_unknown,
            hsz_unknown,
            psz_unknown
        ]
        sz_list.append(entry)

        if not self.heap.dmprofiling_enabled:
            self.formatter.output(
                "\nDM Memory Profiling appears not to be supported or "
                "enabled in the firmware.\nInformation displayed may be "
                "incorrect."
            )

        return sz_list, tf_list, p_list, f_list

    def analyse_dm_memory(self, every=False, verbose=False):
        """Analyse DM memory used by tasks and display it.

        Args:
            every (bool): If True, analyse only operator tasks, if False,
                analyse all tasks.
            verbose (bool): if True, show all heap and pool blocks used by
                a task/bgint if False, don't show any heap and pool block
                information
        """
        sz_list, tf_list, p_list, f_list = self.profile_dm_memory(every)

        self._display_dm_memory(sz_list, tf_list, p_list, f_list, every)

        if verbose:
            self._display_verbose_dm_memory(sz_list, tf_list, p_list, f_list, every)

    def _check_patch_level_dm_profiling(self):
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

    def _check_patch_dm_profiling(self):
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

        # Examine the release patch version number for AuraPlus
        if self._check_patch_level_dm_profiling():
            return True

        # Look through patch global data if dm profiling magic number is set
        # (debug patch)
        try:
            patch_start = self.debuginfo.get_var_strict(
                "$PATCH_RESERVED_DM_START").address
        except DebugInfoNoVariableError:
            return False
        try:
            patch_end = self.debuginfo.get_var_strict(
                "$PATCH_RESERVED_DM_END").address
        except DebugInfoNoVariableError:
            return False

        buffer_content = self.chipdata.get_data(
            patch_start, patch_end - patch_start
        )

        for offset, value in enumerate(buffer_content):
            if value == DM_PROFILING_PATCH_MAGIC:
                return True

        return False

    def _check_kymera_version(self):
        """Checks if the Kymera version is compatible with this analysis.

        Raises:
            OutdatedFwAnalysisError: For an outdated Kymera.
        """
        try:
            self.debuginfo.get_enum("DMPROFILING_OWNER")
        except InvalidDebuginfoEnumError:
            if not self._check_patch_dm_profiling():
                # fallback to the old implementation
                raise OutdatedFwAnalysisError()

    def _display_dm_memory(self, sz_list, tf_list, p_list, f_list, every):
        """Display DM memory used by tasks as specified in 'sz_list'.

        Args:
            sz_list: list as produced by 'profile_dm_memory'
            every:   if True, only operator tasks in 'sz_list',
                     if False, all tasks in 'sz_list'.
        """

        if every:
            descr = "Analyse task/bgint dm memory ({0} tasks/bgints)".format(
                len(sz_list) - 1
            )

        else:
            descr = "Analyse operator dm memory ({0} operators)".format(
                len(sz_list) - 1
            )

        self.formatter.section_start(descr)
        for i in range(len(sz_list) - 1):
            self.formatter.output(
                "ID %3d size %6d (heap %6d, pools %4d) %s" % (
                    sz_list[i][0],
                    sz_list[i][2],
                    sz_list[i][3],
                    sz_list[i][4],
                    sz_list[i][1]
                )
            )

        self._display_files_info(f_list)
        self._display_transform_info(tf_list)
        self._display_pools_info(p_list)

        i = len(sz_list) - 1
        self.formatter.output(
            "Other  size %6d (heap %6d, pools %4d) %s" % (
                sz_list[i][2],
                sz_list[i][3],
                sz_list[i][4],
                sz_list[i][1]
            )
        )
        self.formatter.section_end()

    def _display_heap_block(self, block_list):
        """Display the entries of a list of allocated DM memory blocks
           used by a particular task ID as specified in 'block_list'.

        Args:
            block_list (lst): List of allocated memory blocks, each
                allocated memory block is a list itself, e.g.:

                        [[address1, length1, name1],
                         [address2, length2, name2],
                         ...
                        ]
        """
        for block in block_list:
            if len(block) > 2:
                self.formatter.output(
                    "   address 0x%08x size %6d:  %s" % (
                        block[0],
                        block[1],
                        block[2]
                    )
                )

    def _display_verbose_dm_memory(self, sz_list, tf_list, p_list, f_list, every):
        """Display DM memory used by tasks as specified in 'sz_list',
           and also the heap and pool blocks.

        Args:
            sz_list (list): List as produced by 'profile_dm_memory'
            every (bool): If True, only operator tasks in 'sz_list', if
                False, all tasks in 'sz_list'.
        """

        if every:
            descr = (
                        "Heap/pool blocks for task/bgint dm memory "
                        "(%d tasks/bgints)"
                    ) % (len(sz_list) - 1)

        else:
            descr = (
                        "Heap/pool blocks for operator dm memory "
                        "(%d operators)"
                    ) % (len(sz_list) - 1)

        self.formatter.section_start(descr)

        for i in range(len(sz_list) - 1):
            self.formatter.output(
                "ID %3d size %6d (heap %6d, pools %4d) %s" % (
                    sz_list[i][0],
                    sz_list[i][2],
                    sz_list[i][3],
                    sz_list[i][4],
                    sz_list[i][1]
                )
            )
            self._display_heap_block(sz_list[i][5])

        self._display_files_info(f_list)
        self._display_verbose_files(f_list)
        self._display_transform_info(tf_list)
        self._display_verbose_transforms(tf_list)
        self._display_verbose_pools(p_list)

        i = len(sz_list) - 1
        self.formatter.output(
            "Other  size %6d (heap %6d, pools %4d) %s" % (
                sz_list[i][2],
                sz_list[i][3],
                sz_list[i][4],
                sz_list[i][1]
            )
        )

        self.formatter.section_end()

    def _display_transform_info(self, tf_list):
        """Display DM memory used by transforms as specified in 'tf_list'
        Args:
            tf_list (list): List as produced by 'profile_dm_memory'
        """

        counted_list = []
        sum_heaps = 0
        sum_pools = 0

        for i in range(len(tf_list[0])):
            if not tf_list[1][i] in counted_list:
                counted_list.append(tf_list[1][i])
                sum_heaps = sum_heaps + tf_list[5][i]
                sum_pools = sum_pools + tf_list[8][i]
            if not tf_list[2][i] in counted_list:
                counted_list.append(tf_list[2][i])
                sum_heaps = sum_heaps + tf_list[6][i]
                sum_pools = sum_pools + tf_list[9][i]
            if not tf_list[3][i] in counted_list:
                counted_list.append(tf_list[3][i])
                sum_heaps = sum_heaps + tf_list[7][i]
                sum_pools = sum_pools + tf_list[10][i]

        sum_all = sum_heaps + sum_pools
        self.formatter.output(
            "TFs    size %6d (heap %6d, pools %4d) %d Transforms" % (
                sum_all,
                sum_heaps,
                sum_pools,
                len(tf_list[0])
            )
        )

    def _display_pools_info(self, p_list):
        """Display DM memory used by pools as specified in 'p_list'
        Args:
            p_list (list): Pools pointers list as produced by 'profile_dm_memory'
        """

        sum_heaps = 0
        sum_pools = 0
        pblocks = 0

        for i in range(len(p_list[0])):
            sum_heaps = sum_heaps + p_list[2][i]
            if not p_list[2][i] == 0:
                pblocks = pblocks + 1

        sum_all = sum_heaps + sum_pools
        self.formatter.output(
            "Pools  size %6d (heap %6d            ) %d Pool blocks" % (
                sum_all,
                sum_heaps,
                pblocks
            )
        )

    def _display_files_info(self, f_list):
        """Display DM memory used by files as specified in 'f_list'
        Args:
            f_list (list): List as produced by 'profile_dm_memory'
        """

        counted_list = []
        sum_heaps = 0
        sum_pools = 0
        count = 0

        for i in range(len(f_list[0])):
            if not f_list[1][i] == 0:
                if not f_list[1][i] in counted_list:
                    counted_list.append(f_list[1][i])
                    sum_heaps = sum_heaps + f_list[5][i]
                    sum_pools = sum_pools + f_list[8][i]
                if not f_list[2][i] in counted_list:
                    counted_list.append(f_list[2][i])
                    sum_heaps = sum_heaps + f_list[6][i]
                    sum_pools = sum_pools + f_list[9][i]
                if not f_list[3][i] in counted_list:
                    counted_list.append(f_list[3][i])
                    sum_heaps = sum_heaps + f_list[7][i]
                    sum_pools = sum_pools + f_list[10][i]
                count = count + 1

        sum_all = sum_heaps + sum_pools
        self.formatter.output(
            "Files  size %6d (heap %6d, pools %4d) %d Files" % (
                sum_all,
                sum_heaps,
                sum_pools,
                count
            )
        )

    def _display_verbose_pools(self, p_list):
        """Display DM memory used by pools as specified in 'p_list'
        Args:
            p_list (list): Pools pointers list as produced by 'profile_dm_memory'
        """

        sum_heaps = 0
        sum_pools = 0
        pblocks = 0

        for i in range(len(p_list[0])):
            sum_heaps = sum_heaps + p_list[2][i]
            if not p_list[2][i] == 0:
                pblocks = pblocks + 1

        sum_all = sum_heaps + sum_pools
        self.formatter.output(
            "Pools  size %6d (heap %6d            ) %d Pool blocks" % (
                sum_all,
                sum_heaps,
                pblocks
            )
        )

        for i in range(len(p_list[0])):
            if not p_list[2][i] == 0:
                self.formatter.output(
                    "   address 0x%08x size %6d:  %s" % (
                        p_list[1][i],
                        p_list[2][i],
                        p_list[3][i]
                    )
                )

    def _display_transform_dm_block(self, i, block, descr, tf_list):
        """Display the entries of a list of allocated DM memory blocks
           used by a particular task ID as specified in 'block_list'.

        Args:
            block index in 'tf_list' to display
            tf_list (list): List as produced by 'profile_dm_memory'
        """
        self.formatter.output(
            "   address 0x%08x size %6d:  %-12s %s" % (
                tf_list[i][block],
                tf_list[i + 4][block] + tf_list[i + 7][block],
                tf_list[i + 10][block],
                descr
            )
        )

    def _display_verbose_transforms(self, tf_list):
        """Display DM memory used by transforms as specified in 'tf_list',
           and also the heap and pool blocks.

        Args:
            tf_list (list): List as produced by 'profile_dm_memory'
        """

        counted_list = []
        for i in range(len(tf_list[0])):
            if not tf_list[3][i] in counted_list:
                counted_list.append(tf_list[3][i])
                descr = "Shared data buffer between"
                num = 0
                last = i
                # Find all transforms using this data buffer
                for j in range(len(tf_list[0])):
                    if tf_list[3][j] == tf_list[3][i]:
                        self._display_transform_dm_block(1, j, "T struct: " +
                                                         tf_list[4][j], tf_list)
                        self._display_transform_dm_block(2, j, "B struct: " +
                                                         tf_list[4][j], tf_list)
                        descr = descr + " " + hex(tf_list[0][j])
                        num = num + 1
                        last = j

                if num == 1:
                    self._display_transform_dm_block(3, i,
                                                     "Data Buf: " + tf_list[4][
                                                         i], tf_list)
                else:
                    self._display_transform_dm_block(3, last,
                                                     "Data Buf: " + descr,
                                                     tf_list)
            else:
                # Already handled/printed. Ignore.
                pass

    def _display_files_dm_block(self, i, block, descr, f_list):
        """Display the entries of a list of allocated DM memory blocks
           used by a particular task ID as specified in 'block_list'.

        Args:
            i            : index of f_list entry
            block        : index of f_list[i] list
            descr        : descriptive test for this f_list entry
            f_list (list): List as produced by 'profile_dm_memory'
        """
        if f_list[17][block] < 0:
            self.formatter.output(
                "   address 0x%08x size %6d:  %-12s (Unknown owner)      %s" % (
                    f_list[i][block],
                    f_list[i + 4][block] + f_list[i + 7][block],
                    f_list[i + 10][block],
                    descr
                )
            )
        else:
            self.formatter.output(
                "   address 0x%08x size %6d:  %-12s (In Use, Task ID %02d)  %s" % (
                    f_list[i][block],
                    f_list[i + 4][block] + f_list[i + 7][block],
                    f_list[i + 10][block],
                    f_list[17][block],
                    descr
                )
            )

    def _display_verbose_files(self, f_list):
        """Display DM memory used by files as specified in 'f_list',
           and also the heap and pool blocks.

        Args:
            f_list (list): List as produced by 'profile_dm_memory'
        """

        for i in range(len(f_list[0])):
            if not f_list[1][i] == 0:
                descr = "DATA_FILE " + str(i) + " struct" + f_list[4][i]
                self._display_files_dm_block(1, i, descr, f_list)
                descr = "DATA_FILE " + str(i) + " tCbuffer struct" + f_list[4][i]
                self._display_files_dm_block(2, i, descr, f_list)
                descr = "DATA_FILE " + str(i) + " tCbuffer data buffer" + f_list[4][i]
                self._display_files_dm_block(3, i, descr, f_list)


class DMProfilerTransformIDs:
    """An object to generate transforms and their info.
    Args:
        dmprofiler_analysis: An instance of DmProfiler.
    """

    def __init__(self, dmprofiler_analysis):
        self._analysis = dmprofiler_analysis
        self._transforms_list = []
        self._populate_transforms_list()

    def _populate_transforms_list(self):
        # Get the list of transforms.
        """Profile the transforms."""
        t0 = []
        t1 = []
        t2 = []
        t3 = []
        t4 = []
        t5 = []
        t6 = []
        t7 = []
        t8 = []
        t9 = []
        t10 = []
        t11 = []
        t12 = []
        t13 = []
        t14 = []
        t15 = []
        t16 = []
        self._analysis.stream._read_all_transforms()
        for t in self._analysis.stream.transforms:
            t0.append(t.id)
            t1.append(t.address)
            t2.append(t.buffer)
            buffer_pointer = t.buffer
            buffer_var = self._analysis.chipdata.cast(buffer_pointer,
                                                      'tCbuffer')
            t3.append(buffer_var['base_addr'].value)
            t4.append(t.title_str)
            t5.append(0)
            t6.append(0)
            t7.append(0)
            t8.append(0)
            t9.append(0)
            t10.append(0)
            t11.append("")
            t12.append("")
            t13.append("")
            t14.append(t.address)
            t15.append(t.buffer)
            t16.append(buffer_var['base_addr'].value)

        self._transforms_list = [
            t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10,
            t11, t12, t13, t14, t15, t16
        ]

    def get_dm_transforms(self):
        """Returns a tuple, list of transforms and their info.
        Returns:
            tuple: (transforms list, transforms info)
        """
        self._transforms_list = []
        self._populate_transforms_list()

        return self._transforms_list


class DMProfilerPools:
    """An object to generate pool heap pointers.
    Args:
        dmprofiler_analysis: An instance of DmProfiler.
    """

    def __init__(self, dmprofiler_analysis):
        self._analysis = dmprofiler_analysis
        self._pools_list = []
        self._populate_pools_list()

    def _populate_pools_list(self):
        # Get the list of pool pointers.
        """Profile the pools.
        """
        t0 = []
        t1 = []
        t2 = []
        t3 = []
        for pool in range(len(self._analysis.pools.p_pool)):
            t0.append(self._analysis.pools.p_pool[pool] & 0xFFFFF)
            t1.append(self._analysis.pools.p_pool[pool])
            t2.append(0)
            t3.append("")

        self._pools_list = [t0, t1, t2, t3]

    def get_dm_pools(self):
        """Returns a list of pool pointers.
        Returns:
            list pointers to main pool blocks
        """
        self._analysis.pools._lookup_debuginfo()
        self._pools_list = []
        self._populate_pools_list()

        return self._pools_list


class DMProfilerFiles:
    """An object to generate file heap/pool pointers.
    Args:
        dmprofiler_analysis: An instance of DmProfiler.
    """

    def __init__(self, dmprofiler_analysis):
        self._analysis = dmprofiler_analysis
        self._files_list = []
        self._populate_files_list()

    def _find_patch_file_info(self, t17, file_count):
        # Does the patch have file owner info?
        """Examine the patch for file owner info.
        """

        try:
            patch_data_ptr = self._analysis.chipdata.get_var_strict("$_patch_data")
            patch_data = self._analysis.chipdata.cast(patch_data_ptr.value, "patch_data_struct")
            try:
                file_owners_ptr = patch_data.stored_file_owners
                for idx in range(file_count):
                    file_owner = self._analysis.chipdata.cast(file_owners_ptr.value+idx*2, "uint16")
                    t17.append(file_owner.value)
                return True
            except VariableMemberMissing:
                return False
        except DebugInfoNoVariableError:
            return False

    def _find_rom_file_info(self, t17, file_count):
        # Does the rom have file owner info?
        """Examine the rom for file owner info.
        """

        try:
            file_owners_ptr = self._analysis.chipdata.get_var_strict("L_stored_file_owners")
            for idx in range(file_count):
                file_owner = self._analysis.chipdata.cast(file_owners_ptr.value+idx*2, "uint16")
                t17.append(file_owner.value)
            return True
        except DebugInfoNoVariableError:
            return False

    def _populate_files_list(self):
        # Get the list of file pointers.
        """Profile the file list.
        """
        t0 = []
        t1 = []
        t2 = []
        t3 = []
        t4 = []
        t5 = []
        t6 = []
        t7 = []
        t8 = []
        t9 = []
        t10 = []
        t11 = []
        t12 = []
        t13 = []
        t14 = []
        t15 = []
        t16 = []
        t17 = []
        stored_ptr = self._analysis.chipdata.get_var_strict("L_stored_files")
        try:
            file_count = self._analysis.chipdata.get_var_strict("L_file_mgr_file_count").value
        except DebugInfoNoVariableError:
            file_count = 2
        for idx in range(file_count):
            try:
                file_ptr = self._analysis.chipdata.cast(stored_ptr.value+idx*4, "uintptr_t")
            except TypeError:
                file_ptr = stored_ptr[idx]
            file_entry = self._analysis.chipdata.cast(file_ptr.value, "DATA_FILE")
            t0.append(idx)
            t1.append(file_ptr.value)
            t2.append(file_entry.u.file_data.value)
            file_buf = self._analysis.chipdata.cast(file_entry.u.file_data.value, "tCbuffer")
            t3.append(file_buf.base_addr.value)
            t4.append("")
            
            t5.append(0)
            t6.append(0)
            t7.append(0)
            t8.append(0)
            t9.append(0)
            t10.append(0)
            
            t11.append("")
            t12.append("")
            t13.append("")
            
            t14.append(file_ptr.value)
            t15.append(file_entry.u.file_data.value)
            t16.append(file_buf.base_addr.value)

        if not self._find_patch_file_info(t17, file_count):
            if not self._find_rom_file_info(t17, file_count):
                for idx in range(file_count):
                    t17.append(-1)

        self._files_list = [
            t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10,
            t11, t12, t13, t14, t15, t16, t17
        ]

    def get_dm_files(self):
        """Returns a list of file pointers.
        Returns:
            list pointers to file heap/pool blocks
        """
        self._files_list = []
        self._populate_files_list()

        return self._files_list


class DMProfilerTaskIDs:
    """An object to generate tasks and their info.

    Args:
        dmprofiler_analysis: An instance of DmProfiler.
    """

    def __init__(self, dmprofiler_analysis):
        self._analysis = dmprofiler_analysis

        self._tasks_list = []
        self._tasks_info = []

        self._all_tasks_list = []
        self._all_tasks_info = []

        self._populate_task_qeues()

        self._allbgints_list = []
        self._allbgints_info = []

        self._populate_bigints_in_priority()

    def _populate_task_qeues(self):
        # Get the list of task queues.
        task_queues = self._analysis.chipdata.get_var_strict(
            '$_tasks_in_priority'
        )

        for queue in task_queues:
            if queue['first'].value == 0:
                continue
            first_task = queue['first'].deref
            all_tasks_in_q = [
                t for t in ch.parse_linked_list(first_task, 'next')
            ]

            for task in all_tasks_in_q:
                self._all_tasks_list.append(task['id'].value & 0xFF)
                handler = task['handler'].value
                if handler != 0:
                    try:
                        module_name = self._analysis.debuginfo.get_source_info(
                            handler
                        ).module_name
                    except BundleMissingError:
                        module_name = (
                                "No source information." + "Bundle is missing."
                        )
                    self._all_tasks_info.append(module_name)
                else:
                    self._all_tasks_info.append("")

    def _populate_bigints_in_priority(self):
        bg_ints_in_priority = self._analysis.chipdata.get_var_strict(
            '$_bg_ints_in_priority'
        )
        for bg_int_g in bg_ints_in_priority:
            if bg_int_g['first'].value == 0:
                continue

            first_bg_int = self._analysis.chipdata.cast(
                bg_int_g['first'].value, 'BGINT'
            )

            bg_int_queue = ch.parse_linked_list(
                first_bg_int, 'next'
            )
            for bg_int in bg_int_queue:
                self._allbgints_list.append(
                    bg_int['id'].value & 0xFF
                )
                handler = bg_int['handler'].value
                if handler != 0:
                    try:
                        module_name = self._analysis.debuginfo.get_source_info(
                            handler
                        ).module_name

                    except BundleMissingError:
                        module_name = (
                                "No source information." + "Bundle is missing."
                        )

                    self._allbgints_info.append(module_name)

                    if module_name == "opmgr_operator_bgint_handler":
                        try:
                            # Get the operator id
                            opmgr_analysis = self._analysis.interpreter.get_analysis(
                                "opmgr", self._analysis.chipdata.processor
                            )
                            oppointer = self._analysis.chipdata.get_data(
                                bg_int['ppriv'].value
                            )
                            opdata = self._analysis.chipdata.cast(
                                oppointer, "OPERATOR_DATA"
                            )
                            opid = opdata['id'].value
                            operator = opmgr_analysis.get_operator(opid)
                            desc_str = (
                                ' Operator {} {}'.format(
                                    hex(operator.op_ep_id),
                                    operator.cap_data.name
                                )
                            )
                        except BaseException as error:
                            desc_str = 'Operator not found %s' % str(error)

                        self._tasks_list.append(
                            bg_int['id'].value & 0xFF
                        )
                        self._tasks_info.append(desc_str)

                        self._allbgints_info.pop()
                        self._allbgints_info.append(desc_str)
                else:
                    self._allbgints_info.append("")

    def _list_all(self):
        # If we have a full list, e.g. both tasks and bgints, we need to
        # process the list to throw out duplicates.  Description strings
        # for the same owner id are concatenated.  The actual task ids or
        # bgint ids are larger than 1 octet, as used in the pool and heap
        # block descriptors, so there are going to be multiple occurances
        # of owner ids.  When only looking for an operator task id list,
        # this is not necessary, as every operator id is unique and the
        # task id and bgint id are the same (for the ls octet).
        self._tasks_list = []
        self._tasks_info = []

        # Read the alltasks list and add those entries that
        # are not already present in it to tasks list
        for index, task in enumerate(self._all_tasks_list):
            tid = task & 0xFF
            descr = self._all_tasks_info[index]
            try:
                idx = self._tasks_list.index(tid)
                self._tasks_info[idx] = ', '.join(
                    (self._tasks_info[idx], descr)
                )
            except ValueError:
                self._tasks_list.append(tid)
                self._tasks_info.append(descr)

        # Read the allbgints list and add those entries that
        # are not already present in it to tasks list
        for index, bigint in enumerate(self._allbgints_list):
            tid = bigint & 0xFF
            descr = self._allbgints_info[index]
            try:
                idx = self._tasks_list.index(tid)
                self._tasks_info[idx] = ', '.join(
                    (self._tasks_info[idx], descr)
                )
            except ValueError:
                self._tasks_list.append(tid)
                self._tasks_info.append(descr)

        # Finally, add the 'no task' (0xFF). Heap/pool blocks with
        # such a tags are typically allocated before 'sched()' init.
        tid = 0xFF
        descr = "No task"
        try:
            idx = self._tasks_list.index(tid)
            self._tasks_info[idx] = self._tasks_info[idx] + ", " + descr
        except ValueError:
            self._tasks_list.append(tid)
            self._tasks_info.append(descr)

    def _remove_aliases(self):
        # When only looking for an operator task id list, operators may
        # have identical IDs (for example, with different priorities).
        # Find identical ones and compress to a single entry
        xtasks_list = self._tasks_list
        xtasks_info = self._tasks_info
        self._tasks_list = []
        self._tasks_info = []

        # Read the xtasks list and add those entries that
        # are not already present in it to tasks list
        for index, task in enumerate(xtasks_list):
            tid = xtasks_list[index]
            descr = xtasks_info[index]
            try:
                idx = self._tasks_list.index(tid)
                self._tasks_info[idx] = ','.join(
                    (xtasks_info[idx], descr)
                )
            except ValueError:
                self._tasks_list.append(tid)
                self._tasks_info.append(descr)

        # Read the allbgints list and mark those entries
        # in the tasks list that have an identical ID
        others = [False] * len(self._tasks_list)
        for index, bigint in enumerate(self._allbgints_list):
            tid = bigint & 0xFF
            try:
                idx = self._tasks_list.index(tid)
                others[idx] = True
            except ValueError:
                # We're not actually interested in the exception
                # Just set 'others[idx] = True' if there's a double entry
                pass
        for index, bigint in enumerate(self._tasks_info):
            if others[index] == False:
                self._tasks_info[index] = self._tasks_info[
                                              index] + ', others (multiple operators/entities with this ID)'

    def get_tasks(self, every=False):
        """Returns a tuple, list of tasks and their info.

        Args:
            every (bool): If `True` it lists all tasks and big-ints, it
                lists operators otherwise.

        Returns:
            tuple: (tasks list, tasks info)
        """
        self._tasks_list = []
        self._tasks_info = []

        self._all_tasks_list = []
        self._all_tasks_info = []
        self._populate_task_qeues()

        self._allbgints_list = []
        self._allbgints_info = []
        self._populate_bigints_in_priority()

        if every:
            self._list_all()
        else:
            self._remove_aliases()

        return self._tasks_list, self._tasks_info
