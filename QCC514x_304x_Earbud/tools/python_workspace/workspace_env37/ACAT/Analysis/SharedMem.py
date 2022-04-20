############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2014 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Shared Memory analysis.

Module to analyse the Shared Memory.
"""
from . import Analysis
from ACAT.Core.CoreTypes import ChipVarHelper as ch

VARIABLE_DEPENDENCIES = {'strict': ('L_shared_memory_list',)}
TYPE_DEPENDENCIES = {'SHARED_MEMORY_TABLE': ('next',)}


class SharedMem(Analysis.Analysis):
    """Encapsulates an analysis for usage of shared memory.

    Args:
        **kwarg: Arbitrary keyword arguments.
    """
    def run_all(self):
        """Performs analysis and spew the output to the formatter.

        Displays the contents of the shared memory blocks.
        """
        self.formatter.section_start('Shared Memory Info')
        self._shared_memory_blocks()
        self.formatter.section_end()

    ##################################################
    # Private methods
    ##################################################

    def _shared_memory_blocks(self):
        shared_memory_list = self.chipdata.get_var_strict(
            'L_shared_memory_list'
        )

        for sh_table in ch.parse_linked_list(shared_memory_list, 'next'):
            self.formatter.output(str(sh_table))
