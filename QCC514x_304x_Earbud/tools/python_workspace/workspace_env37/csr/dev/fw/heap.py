############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
'''
Class to model the Audio FW heap
'''

from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import bytes_to_dwords
from csr.dev.model import interface
from csr.dev.fw.firmware_component import FirmwareComponent


class AudioHeap(FirmwareComponent):

    def __init__(self, fw_env, core):
        FirmwareComponent.__init__(self, fw_env, core)

        # Processor specific heap
        self._pheap_info = self.env.cu.heap_alloc.localvars["pheap_info"].deref

        self._dm1_heap = self._pheap_info.heap[0]
        self._dm2_heap = self._pheap_info.heap[1]
        self._shared_heap = self._pheap_info.heap[2]
        self._dm1_fast_heap = self._pheap_info.heap[3]

        self.heaps = ["MEM_MAP_DM1_HEAP",
                      "MEM_MAP_DM2_HEAP",
                      "MEM_MAP_DM2_SHARED_HEAP",
                      "MEM_MAP_DM1_FAST_HEAP"]

    def get_heap_sizes(self):
        sizes = {}
        for ctr, heap in enumerate(self.heaps):
            sizes[heap] = self._pheap_info.heap[ctr]["heap_size"].value
        return sizes

    def get_heap_end(self):
        """
        Return the end of the Audio P0 heaps, and so the beginning of the P1 heaps
        """
        ends = {}
        for ctr, heap in enumerate(self.heaps):
            ends[heap] = self._pheap_info.heap[ctr]["heap_end"]
        return ends

    def get_heap_free(self):
        """
        Returns the free space on the various heaps.
        It is worth noting that the "nodes" here consume 8 bytes each. So even a completely unused
        heap will have 8 bytes consumed for the node which points to the "full_heap_size - 8" block
        """
        free = []

        for ctr, heap in enumerate(self.heaps):
            pointer = self.env.cu.heap_alloc.localvars["freelist"][ctr]
            free_info = []
            address_history = set()
            address = pointer.value
            total_free = 0
            while address != 0:
                if not address in address_history:
                    address_history.add(address)
                else:
                    iprint("Repeating nodes with address 0x%x. Probably memory corruption" % address)

                node = pointer.deref
                length = node["length"]
                total_free += length.value
                pointer = node.u.next
                address = pointer.value
            free.append(total_free)

        all_free = {}
        for ctr, heap in enumerate(self.heaps):
            all_free[heap] = free[ctr]

        return all_free

    def _generate_memory_report_component(self):
        # This currently returns the heap information for both P0 and P1.
        # Because P0 often doesn't run, this makes our life easier.

        #P0
        dm1_heap_start = self._core.sym_get_value("MEM_MAP_DM1_HEAP_START")
        dm1_heap_size = self._core.sym_get_value("DM1_HEAP_SIZE")
        dm1_heap_p0_size = self.get_heap_sizes()["MEM_MAP_DM1_HEAP"]
        dm1_heap_p0 = {"name":"MEM_MAP_DM1_HEAP(P0)",
                    "start":dm1_heap_start,
                    "end":dm1_heap_start + dm1_heap_p0_size,
                    "size":dm1_heap_p0_size}
        dm1_heap_p0_free = self.get_heap_free()["MEM_MAP_DM1_HEAP"]
        dm1_heap_p0["used"] = dm1_heap_p0_size - dm1_heap_p0_free
        dm1_heap_p0["unused"] = dm1_heap_p0_free
        dm1_heap_p0["percent_used"] =  (float(dm1_heap_p0_size - dm1_heap_p0_free) / dm1_heap_p0_size) * 100

        #P1
        dm1_heap_p1_size = dm1_heap_size - dm1_heap_p0_size
        dm1_heap_p1_start = dm1_heap_start + dm1_heap_p0_size
        dm1_heap_p1 = {"name":"MEM_MAP_DM1_HEAP(P1)",
                    "start":dm1_heap_p1_start,
                    "end":dm1_heap_p1_start + dm1_heap_p1_size,
                    "size":dm1_heap_p1_size}

        # This is nasty because we are reaching from P0 into P1
        if self._core.fw._core.subsystem.cores[1].is_running:
            dm1_heap_p1["comment"] = "We don't have code to calculate this"
        else:
            dm1_heap_p1["used"] = 0
            dm1_heap_p1["unused"] = dm1_heap_p1_size
            dm1_heap_p1["percent_used"] =  0

        try:
            dm1_fast_heap_start = self._core.sym_get_value("MEM_MAP_DM1_EXT_HEAP_START")
            dm1_fast_heap_end = self._core.sym_get_value("MEM_MAP_DM1_EXT_HEAP_END")
            dm1_fast_heap_size = dm1_fast_heap_end - dm1_fast_heap_start
            dm1_fast_heap_p0_size = self.get_heap_sizes()["MEM_MAP_DM1_FAST_HEAP"]
            dm1_fast_heap_p0 = {"name":"MEM_MAP_DM1_FAST_HEAP(P0)",
                        "start":dm1_fast_heap_start,
                        "end":dm1_fast_heap_start + dm1_fast_heap_p0_size,
                        "size":dm1_fast_heap_p0_size}
            dm1_fast_heap_p0_free = self.get_heap_free()["MEM_MAP_DM1_FAST_HEAP"]
            dm1_fast_heap_p0["used"] = dm1_fast_heap_p0_size - dm1_fast_heap_p0_free
            dm1_fast_heap_p0["unused"] = dm1_fast_heap_p0_free
            dm1_fast_heap_p0["percent_used"] =  (float(dm1_fast_heap_p0_size - dm1_fast_heap_p0_free) / dm1_fast_heap_p0_size) * 100

            dm1_fast_heap_p1_size = dm1_fast_heap_size - dm1_fast_heap_p0_size
            dm1_fast_heap_p1_start = dm1_fast_heap_start + dm1_fast_heap_p0_size
            dm1_fast_heap_p1 = {"name":"MEM_MAP_DM1_FAST_HEAP(P1)",
                        "start":dm1_fast_heap_p1_start,
                        "end":dm1_fast_heap_p1_start + dm1_fast_heap_p1_size,
                        "size":dm1_fast_heap_p1_size}

            # This is nasty because we are reaching from P0 into P1
            if self._core.fw._core.subsystem.cores[1].is_running:
                dm1_fast_heap_p1["comment"] = "We don't have code to calculate this"
            else:
                dm1_fast_heap_p1["used"] = 0
                dm1_fast_heap_p1["unused"] = dm1_fast_heap_p1_size
                dm1_fast_heap_p1["percent_used"] =  0
        except KeyError:
            # Some chips don't have this area of RAM
            dm1_fast_heap_p0 = []
            dm1_fast_heap_p1 = []

        #P0
        dm2_shared_heap_start = self._core.sym_get_value("MEM_MAP_DM2_SHARED_HEAP_START")
        dm2_shared_heap_size = self._core.sym_get_value("DM2_SHARED_HEAP_SIZE")
        dm2_shared_heap_end = dm2_shared_heap_start + dm2_shared_heap_size
        dm2_shared_heap_p0_size = self.get_heap_sizes()["MEM_MAP_DM2_SHARED_HEAP"]
        dm2_shared_heap_p0_free = self.get_heap_free()["MEM_MAP_DM2_SHARED_HEAP"]
        dm2_shared_heap_p0 = {"name":"MEM_MAP_DM2_SHARED_HEAP(P0)",
                              "start":dm2_shared_heap_start,
                              "end":dm2_shared_heap_start + dm2_shared_heap_p0_size,
                             "size":dm2_shared_heap_p0_size}
        dm2_shared_heap_p0["used"] = dm2_shared_heap_p0_size - dm2_shared_heap_p0_free
        dm2_shared_heap_p0["unused"] = dm2_shared_heap_p0_free
        dm2_shared_heap_p0["percent_used"] =  (float(dm2_shared_heap_p0_size - dm2_shared_heap_p0_free) / dm2_shared_heap_p0_size) * 100
        dm2_shared_heap_p0["comment"] = "Heap data structures cost 8 bytes each"

        #P1
        dm2_shared_heap_p1_start = dm2_shared_heap_start + dm2_shared_heap_p0_size
        dm2_shared_heap_p1_size = dm2_shared_heap_size - dm2_shared_heap_p0_size
        dm2_shared_heap_p1 = {"name":"MEM_MAP_DM2_SHARED_HEAP(P1)",
                              "start":dm2_shared_heap_p1_start,
                              "end":dm2_shared_heap_p1_start + dm2_shared_heap_p1_size,
                             "size":dm2_shared_heap_p1_size}

        # This is nasty because we are reaching from P0 into P1
        if self._core.fw._core.subsystem.cores[1].is_running:
            dm2_shared_heap_p1["comment"] = "We don't have code to calculate this"
        else:
            dm2_shared_heap_p1["used"] = 0
            dm2_shared_heap_p1["unused"] = dm2_shared_heap_p1_size
            dm2_shared_heap_p1["percent_used"] =  0

        #P0
        dm2_heap_start = self._core.sym_get_value("MEM_MAP_DM2_HEAP_START")
        dm2_heap_size = self._core.sym_get_value("DM2_RAM_HEAP_SIZE")
        dm2_heap_p0_size = self.get_heap_sizes()["MEM_MAP_DM2_HEAP"]
        dm2_heap_p0_free = self.get_heap_free()["MEM_MAP_DM2_HEAP"]
        dm2_heap_p0 = {"name":"MEM_MAP_DM2_HEAP(P0)",
                       "start":dm2_heap_start,
                       "end":dm2_heap_start + dm2_heap_p0_size,
                       "size":dm2_heap_p0_size}
        dm2_heap_p0["used"] = dm2_heap_p0_size - dm2_heap_p0_free
        dm2_heap_p0["unused"] = dm2_heap_p0_free
        dm2_heap_p0["percent_used"] =  (float(dm2_heap_p0_size - dm2_heap_p0_free) / dm2_heap_p0_size) * 100

        #P1
        dm2_heap_p1_start = dm2_heap_start + dm2_heap_p0_size
        dm2_heap_p1_size = dm2_heap_size - dm2_heap_p0_size
        dm2_heap_p1 = {"name":"MEM_MAP_DM2_HEAP(P1)",
                       "start":dm2_heap_p1_start,
                       "end":dm2_heap_p1_start + dm2_heap_p1_size,
                       "size":dm2_heap_p1_size}

        # This is nasty because we are reaching from P0 into P1
        if self._core.fw._core.subsystem.cores[1].is_running:
            dm2_heap_p1["comment"] = "We don't have code to calculate this"
        else:
            dm2_heap_p1["used"] = 0
            dm2_heap_p1["unused"] = dm2_heap_p1_size
            dm2_heap_p1["percent_used"] =  0

        return [dm1_heap_p0, dm1_heap_p1, dm1_fast_heap_p0, dm1_fast_heap_p1,
                dm2_shared_heap_p0, dm2_shared_heap_p1,
                dm2_heap_p0, dm2_heap_p1]
