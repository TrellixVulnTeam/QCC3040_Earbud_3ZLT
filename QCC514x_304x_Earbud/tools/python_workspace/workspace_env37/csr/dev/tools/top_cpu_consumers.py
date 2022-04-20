############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

import time
import operator
from csr.wheels import gstrm
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.framework.meta.elf_firmware_info import BadPCLow,BadPCHigh
import sys

class TopCpuConsumers(object):
    """
    This class provides an interface to scrape the Program Counter over the debug interface,
    and then a second interface to decode that trace into a table of the functions called
    """

    def __init__(self, fw_env, core):
        self._core = core
        self._fw_env = fw_env
        self._pc_bucket = {}
        self._total_samples = 0
        # Make sure we run self._core.pc here, in case poll_pc() is the first thing to run on pydbg. 
        # If we don't execute this during __init__ then it will take too long (>2sec) during the 
        # while loop in poll_pc and take maximum 1 sample, which results in fake reports. 
        self.init_cur_pc = self._core.pc

    def poll_pc(self, sample_ms = 1000):
        """
        Collects PC samples for "sample_ms"
        """
        self._pc_bucket = {}
        num_samples = 0
        initial_time = time.time()
        final_time = time.time() + (sample_ms/1000.0)

        while time.time() < final_time:
            num_samples += 1
            cur_pc = self._core.pc
            try:
              self._pc_bucket[cur_pc] += 1
            except KeyError:
              self._pc_bucket[cur_pc] = 1

        self._total_samples = 0
        for pc,count in self._pc_bucket.items():
            self._total_samples += count

    def top(self, max_functions = 10, report=False):
        """
        Returns a table of the functions the processor spends most time in
        """
        #First sort the PC values by frequency
        sorted_pc_bucket = sorted(self._pc_bucket.items(), key=operator.itemgetter(1), reverse=True)

        #Now throw away the specific part (PC) of the function we were in and just tell me the function names
        function_call_count = {}
        for address,count in sorted_pc_bucket:
            try:
                function_name = self._fw_env.functions.get_function_of_pc(address)[1]
            except (BadPCLow, BadPCHigh):
                # Because the XAP PC reads are not atomic (we read the 24bit PC as two 16 bit reads,
                # XAP_PCH and XAP_PCL, we sometimes get a mismatched PCH and PCL.
                function_name = "<bad sample or dynamic code>"
            try:
                function_call_count[function_name] += count
            except KeyError:
                #First item
                function_call_count[function_name] = count

        #Now sort the function_call_count by frequency
        sorted_function_call_count = \
            sorted(function_call_count.items(), key=operator.itemgetter(1), reverse=True)

        output_table = interface.Table(["Function", "% CPU"])

        for i in range(0, max_functions):
            try:
                row = []
                row.append(sorted_function_call_count[i][0])
                row.append(sorted_function_call_count[i][1] * 100.0 / self._total_samples)
                output_table.add_row(row)
            except IndexError:
                #Not as many functions in the list as we were asked to display
                pass

        #No point in wiring this up to coredump, because it needs live sampling of the PC
        if report is True:
            return output_table
        TextAdaptor(output_table, gstrm.iout)
        return

