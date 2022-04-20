############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""
Madule which check for patches.
"""
from ACAT.Core import CoreUtils as cu
from ACAT.Core.exceptions import OutdatedFwAnalysisError
from . import Analysis

VARIABLE_DEPENDENCIES = {
    'not_strict': ('$_patched_fw_version',),
    'old_style': ('$_patch_level',)
}


class Patches(Analysis.Analysis):
    """Encapsulates an analysis for patches.

    Args:
        **kwarg: Arbitrary keyword arguments.
    """

    def __init__(self, **kwarg):
        Analysis.Analysis.__init__(self, **kwarg)
        self._check_kymera_version()

    def _check_build_id_version(self):
        """Checks if the build id is compatible with this analysis,

        Returns:
            True if build id can be analysed, False if it cannot.
        """
        qcc515x_build_id = 11639
        int_addr = self.debuginfo.get_var_strict(
            '$_build_identifier_integer'
        ).address
        build_id_int = self.debuginfo.get_dm_const(int_addr, 0)
        
        return build_id_int == qcc515x_build_id

    def _check_kymera_version(self):
        """Checks if the Kymera version is compatible with this analysis.

        Raises:
            OutdatedFwAnalysisError: For outdated Kymera.
        """
        if not self._check_build_id_version():
            raise OutdatedFwAnalysisError()
        pass

    def run_all(self):
        """Performs analysis and spew the output to the formatter.

        It outputs the patch level of the firmware.
        """
        self.formatter.section_start('Patches')
        self.analyse_patch_level()
        if self.get_patch_level() > 0:
            self.analyse_patch_size()
        self.formatter.section_end()

    def get_patch_level(self):
        """Returns the patch level of the firmware."""
        return self.chipdata.get_patch_id()

    #######################################################################
    # Analysis methods - public since we may want to call them individually
    #######################################################################

    def analyse_patch_level(self):
        """Prints the patch level."""
        self.formatter.output('Patch Level: ' + str(self.get_patch_level()))

    def get_patch_size(self):
        """Display the memory used by patches."""
        return self.chipdata.get_data(0x00004b60)

    def analyse_patch_size(self):
        """Display the memory used by patches."""
        patch_address_start = self.debuginfo.get_constant_strict(
            '$PM_RAM_P0_CODE_START'
        ).value

        self.formatter.output(
            "Patch size : " +
            cu.mem_size_to_string(self.get_patch_size(), "o") +
            " at address: 0x{0:0>8x}".format(patch_address_start)
        )
