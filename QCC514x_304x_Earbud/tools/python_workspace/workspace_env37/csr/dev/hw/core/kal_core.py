############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Provides class KalCore to represent Kalimba processor core.
"""
import sys
import time
import math
from collections import OrderedDict
from contextlib import contextmanager
from csr.wheels import gstrm
from csr.wheels.bitsandbobs import build_le, bytes_to_words
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.wheels.bitsandbobs import PureVirtualError
from .base_core import BaseCore
from .execution_state import KalimbaExecState
from .mixin.has_reg_based_breakpoints import HasRegBasedBreakpoints

if sys.version_info >= (3,):
    int_type = (int)
else:
    # Python 2
    int_type = (int, long)


class KalCore(BaseCore, HasRegBasedBreakpoints):
    #pylint: disable=too-many-public-methods
    """\
    Kalimba Core Proxy (Base / mixin?)

    CPU-centric collection of resources (hw + fw proxies) (Abstract Base)

    Cores are the focus of most attention - but do not represent all of
    a Chip's resources (e.g. shared memory, host blocks...)
    """

    def __init__(self):

        BaseCore.__init__(self)

    # BaseComponent compliance


    # Kalimba's conventional names are a bit different

    @property
    def dm(self):
        'data memory'
        return self.data

    @property
    def dmw(self):
        'Data-word-wide access to data memory'
        return self.dataw

    @property
    def pm(self):
        'program space memory'
        return self.program_space

    def run(self):
        """
        Run the processor, including from a breakpoint
        """
        self.step()
        self.fields.DEBUG.DEBUG_RUN = 1

    def step(self):
        """
        Step the processor, including from a breakpoint
        """
        self.pause()
        self.fields.DEBUG.DEBUG_STEP = 1
        self.fields.DEBUG.DEBUG_STEP = 0
        while self.fields.STATUS.STATUS_SINGSTEPCOMP == 0:
            pass

    def pause(self):
        'pause the processor'
        self.fields.DEBUG.DEBUG_RUN = 0

    @property
    def trace(self):
        'Accessor to the Kalimba hardware trace module'
        from csr.dev.hw.trace import TraceModule
        try:
            self._trace
        except AttributeError:
            self._trace = TraceModule(self)
        return self._trace

    @property
    def is_running(self):
        'return whether processor is running'
        return self.bitfields.STATUS_RUNNING == 1

    def flush_doloop_cache(self):
        ''' Disable then re-enable the DOLOOP cache to flush it.
        Can get problems with lower 32bit word of TCM not appearing
        properly in PM at 0x000000 without this (it appears in DM just fine).
        '''
        self.fields["DOLOOP_CACHE_CONFIG"] = 0
        self.fields["DOLOOP_CACHE_CONFIG"] = 3

    @property
    def pc(self):
        'Program counter'
        return self.core.fields["REGFILE_PC"]

    @property
    def sp(self):
        'stack pointer'
        return self.core.fields["STACK_POINTER"]

    @property
    def fp(self):
        'stack frame pointer'
        return self.core.fields["FRAME_POINTER"]

    @property
    def rmac(self):
        """
        rmac is a 72-bit value formed from two 32-bit registers and an 8-bit
        register
        """
        return build_le(
            [self.core.fields["REGFILE_RMAC%d" % i] for i in (0, 1, 2)],
            word_width=32)

    @property
    def rlink(self):
        'Accessor to the REGFILE_RLINK register'
        return self.core.fields["REGFILE_RLINK"]

    @property
    def core_regs_dict(self):
        """
        Returns the core registers as a dictionary. Keys are lower case and of
        the basic form. i.e. REGFILE_R0 would have key of "r0"
        """

        # Ordered dict here as .registers property leverages this and we want a
        # sensible order.
        ret_dict = OrderedDict()
        non_enumerated_regs = ['pc', 'sp', 'fp', 'rlink']
        for non_enumerated_reg in non_enumerated_regs:
            ret_dict[non_enumerated_reg] = getattr(self, non_enumerated_reg)

        for index in range(11):
            ret_dict["r{}".format(index)] = self.r[index]

        for index in range(8):
            ret_dict["i{}".format(index)] = self.i[index]

        for index in (0,1,2,24):
            ret_dict["rmac{}".format(index)] = self.core.fields[
                "REGFILE_RMAC{}".format(index)]

        return ret_dict

    class _RegBankList(object): #pylint: disable=too-few-public-methods
        """
        Simple class that gives list-like access to a register bank
        """

        def __init__(self, fields_lookup, reg_type, num_fields):
            self._fields = fields_lookup
            self._num_fields = num_fields
            self._reg_type = reg_type

        def __getitem__(self, index):

            if isinstance(index, int_type):
                regname = "REGFILE_{}{}".format(self._reg_type, index)
                return self._fields[regname]
            start = index.start if index.start is not None else 0
            stop = (index.stop if index.stop is not None
                    else self._num_fields)
            step = index.step if index.step is not None else 1
            return [self[i] for i in range(start, stop, step)]

        def __setitem__(self, index, value):
            if isinstance(index, int_type):
                regname = "REGFILE_{}{}".format(self._reg_type, index)
                self._fields[regname] = value
            else:
                start = index.start if index.start is not None else 0
                stop = (index.stop if index.stop is not None
                        else self._num_fields)
                step = index.step if index.step is not None else 1
                for i in range(start, stop, step):
                    self[i] = value[i]

    @property
    def r(self): #pylint: disable=invalid-name
        'Accessor to the Kalimba R register bank'
        return self._RegBankList(self.core.fields, 'R', 11)

    @property
    def i(self): #pylint: disable=invalid-name
        'Accessor to the Kalimba I register bank'
        return self._RegBankList(self.core.fields, 'I', 8)

    def counters(self, report=False):
        'Prints some Kalimba counters'
        try:
            return self._print_list_regs(
                "Kalimba Instruction Counters",
                [("Run Clocks", "NUM_RUN_CLKS", ",d"),
                 ("Instructions", "NUM_INSTRS", ",d"),
                 ("Stalls", "NUM_CORE_STALLS", ",d"),
                 ("Mem Stalls", "NUM_MEM_ACCESS_STALLS", ",d"),
                 ("Instr Stalls", "NUM_INSTR_EXPAND_STALLS", ",d")],
                report)
        except KeyError:
            return self._print_list_regs(
                "Kalimba Instruction Counters",
                [("Run Clocks", "NUM_RUN_CLKS", ",d"),
                 ("Instructions", "NUM_INSTRS", ",d"),
                 ("Stalls", "NUM_CORE_STALLS", ",d")],
                report)

    def prefetch_counters(self, report=False):
        'Prints some Kalimba prefetch counters'
        return self._print_list_regs(
            "Pre-Fetch Counters",
            [("DSP Requests", "PREFETCH_REQUEST_COUNT", ",d"),
             ("Memory Requests", "PREFETCH_PREFETCH_COUNT", ",d"),
             ("Waits", "PREFETCH_WAIT_OUT_COUNT", ",d")],
            report)

    def _set_cache_sel(self):
        """
        Set the cache select register for this processor so that we get the 
        right behaviour.  By default there's nothing to do, but for the Apps
        there's a register APPS_SYS_CACHE_SEL which needs to be set. 
        """
        return None

    @contextmanager
    def cache_counters_enabled(self):
        """
        Context manager that configures the Kalimba hardware to report various
        counts relating to cache activity
        """
        self._set_cache_sel()
        debug_counters = self.fields.DBG_COUNTERS_EN.read()
        kalimba_read_cache_control = self.fields.KALIMBA_READ_CACHE_CONTROL.read()
        prefetch_config_counters_en = self.bitfields.PREFETCH_CONFIG_COUNTERS_EN.read()
        doloop_config_counters_en = self.bitfields.DOLOOP_CACHE_CONFIG_COUNTERS_EN.read()

        #self.bitfields.KALIMBA_READ_CACHE_CONTROL_MODE = 1
        self.fields.DBG_COUNTERS_EN = 1
        self.bitfields.KALIMBA_READ_CACHE_CONTROL_ENABLE_PM_COUNTERS = 1
        self.bitfields.KALIMBA_READ_CACHE_CONTROL_ENABLE_MASTER_COUNTERS = 1
        self.fields.KALIMBA_READ_CACHE_DEBUG_EN = 1
        self.bitfields.PREFETCH_CONFIG_COUNTERS_EN = 1
        self.bitfields.DOLOOP_CACHE_CONFIG_COUNTERS_EN = 1
        yield
        self.fields.DBG_COUNTERS_EN = debug_counters
        self.fields.KALIMBA_READ_CACHE_CONTROL = kalimba_read_cache_control
        self.bitfields.PREFETCH_CONFIG_COUNTERS_EN = prefetch_config_counters_en
        self.bitfields.DOLOOP_CACHE_CONFIG_COUNTERS_EN = doloop_config_counters_en
        # In principle we should reset KALIMBA_READ_CACHE_DEBUG_EN, but for
        # reasons best known to the digits it is write-only, so we have no idea
        # what to set it back to.
        

    def clock_rate(self):
        """
        The CPU's input clock speed in MHz.  Implementation is deferred to subclasses
        as determining the current value varies between chips and subsystems.
        """
        raise PureVirtualError

    class KalimbaNotRunning(RuntimeError):
        """
        Raised when requesting performance stats on a processor that isn't
        running.
        """

    def core_perf_data(self, time_s=1):
        """
        Monitor the CPU performance over the given time interval by 
        temporarily enabling relevant hardware counters.
        """
        if self.fields.TIMER_TIME - self.fields.TIMER_TIME == 0:
            raise self.KalimbaNotRunning("Subsystem timer not ticking")
        if self.bitfields.DEBUG_RUN == 0:
            raise self.KalimbaNotRunning("Kalimba run bit not set")
        
        class CountReg(object):
            """
            Simple class to capture and store values from a particular register
            or bitfield on demand.
            """
            def __init__(self, reg):
                self._reg = reg
                self._values = {}
            def capture(self, key):
                self._values[key] = self._reg.read()
            def delta(self, from_key, to_key):
                # handle wrapping
                return (self._values[to_key] - 
                        self._values[from_key])&self._reg.info.mask
                
        class CacheStats(object):
            """
            Simple class that unifies information about the performance of a
            particular cache based on two of the three quantities
             * number of hits
             * number of misses
             * number of requests
            """
            def __init__(self, hits=None, misses=None, requests=None):
                assert sum(r is not None for r in (hits, misses, requests)) == 2
                
                if hits is None:
                    self._misses = misses
                    self._requests = requests
                    self._hits = self._requests - self._misses
                if misses is None:
                    self._hits = hits
                    self._requests = requests
                    self._misses = self._requests - self._hits
                if requests is None:
                    self._hits = hits
                    self._misses = misses
                    self._requests = self._hits + self._misses
            
            def __repr__(self):
                return "CacheStats(hits={} misses={} requests={} ratio={})".format(
                    self.hits, self.misses, self._requests, self.hit_ratio)
            
            @property
            def hits(self):
                return self._hits
            @property
            def misses(self):
                return self._misses
            @property
            def requests(self):
                return self._requests
            @property
            def hit_ratio(self):
                try:
                    return 1.0*self._hits / self._requests
                except ZeroDivisionError:
                    return 0
        
        regs = {"cycles"             : CountReg(self.fields.NUM_RUN_CLKS),
                "instructions"       : CountReg(self.fields.NUM_INSTRS),
                "prefetch_request"   : CountReg(self.fields.PREFETCH_REQUEST_COUNT),
                "prefetch_prefetch"  : CountReg(self.fields.PREFETCH_PREFETCH_COUNT),
                "doloop_hit"         : CountReg(self.fields.DOLOOP_CACHE_HIT_COUNT),
                "read_cache_pm_hit"  : CountReg(self.fields.KALIMBA_READ_CACHE_PM_HIT_COUNTER),
                "read_cache_pm_miss" : CountReg(self.fields.KALIMBA_READ_CACHE_PM_MISS_COUNTER),
                "prefetch_wait_out"  : CountReg(self.fields.PREFETCH_WAIT_OUT_COUNT),
                "core_stalls" : CountReg(self.fields.NUM_CORE_STALLS),
                "mem_access_stalls" : CountReg(self.fields.NUM_MEM_ACCESS_STALLS),
                "instr_expand_stalls" : CountReg(self.fields.NUM_INSTR_EXPAND_STALLS),
                }
            
        with self.cache_counters_enabled():
            # Grab all the cache counters before and after the requested delay
            
            for reg in regs.values():
                reg.capture("start")

            time.sleep(time_s)
            
            for reg in regs.values():
                reg.capture("end")

        prefetch =   CacheStats(requests=regs["prefetch_request"].delta("start","end"),
                                misses=  regs["prefetch_prefetch"].delta("start","end"))
        doloop   =   CacheStats(requests=regs["prefetch_prefetch"].delta("start","end"),
                                hits=    regs["doloop_hit"].delta("start","end"))
        read_cache = CacheStats(hits=    regs["read_cache_pm_hit"].delta("start","end"),
                                misses=  regs["read_cache_pm_miss"].delta("start","end"))
        
        # if fetches are "lost" between missing the doloop and looking up the read cache
        # there must be another layer in there.  I think in the Apps this is TCM.
        tcm = CacheStats(requests=doloop.misses, misses=read_cache.requests)
        

        instr = regs["instructions"].delta("start","end")
        cycles = regs["cycles"].delta("start","end")
        prefetch_stalls = regs["prefetch_wait_out"].delta("start","end")
        core_stalls = regs["core_stalls"].delta("start","end")
        mem_access_stalls = regs["mem_access_stalls"].delta("start","end")
        instr_expand_stalls = regs["instr_expand_stalls"].delta("start","end")

        mhz =  (cycles/1.0e6)/time_s
        mips = (instr/1.0e6)/time_s

        prefetch_instr = prefetch.requests
        # "absolute" hit rates are based on local hits and the total requests
        # (total instructions)
        # "relative" hit rates are based on local hits and local requests
        stats = {
            "cycles" : cycles,
            "instructions" : instr,
            "prefetch_requests" : prefetch_instr,
            "mhz" : mhz,
            "mips" : mips,
            "stalls" : {"core" : core_stalls,
                        "mem_access" : mem_access_stalls,
                        "instr_expand" : instr_expand_stalls,
                        "prefetch" : prefetch_stalls},
            "hits" : {"prefetch" : prefetch.hits,
                      "doloop" : doloop.hits,
                      "tcm" : tcm.hits,
                      "read_cache" : read_cache.hits},
            "rel" : {"prefetch" : 100.0*prefetch.hit_ratio,
                     "doloop" : 100.0*doloop.hit_ratio,
                      "tcm" : 100.0*tcm.hit_ratio,
                     "read_cache" : 100.0*read_cache.hit_ratio},
            "abs" : {"prefetch" : 100.0*prefetch.hits / prefetch_instr,
                     "doloop" : 100.0*doloop.hits/prefetch_instr,
                     "tcm" : 100.0*tcm.hits/prefetch_instr,
                     "read_cache" : 100.0*read_cache.hits/prefetch_instr}
            }
        
        read_cache_mode = self.bitfields.KALIMBA_READ_CACHE_CONTROL_MODE.read()
        metadata = {"mode" : {0 : "disabled",
                              1 : "two-way",
                              2 : "two-way half",
                              3 : "direct"}[read_cache_mode],
                    "word width" : self.bitfields.KALIMBA_READ_CACHE_PARAMS_WORD_WIDTH.read(),
                    "line length" : self.bitfields.KALIMBA_READ_CACHE_PARAMS_DIRECT_LINE_LENGTH.read()
                                      if read_cache_mode == 3 else 
                                       (self.bitfields.KALIMBA_READ_CACHE_PARAMS_2WAY_LINE_LENGTH.read()
                                         if read_cache_mode != 0 else "-")}

        return stats, metadata

    def core_perf(self, time_s=1.0, report=False, clock_MHz=None):
        """
        Report the performance of the Kalimba averaged over the given time interval.
        """
        try:
            stats, metadata = self.core_perf_data(time_s)
        except self.KalimbaNotRunning as exc:
            grp = interface.Group("Kalimba code fetch performance")
            grp.append(interface.Text("%s not running (%s)" % (self.nicknames[0], str(exc))))
            if report:
                return grp
            TextAdaptor(grp, gstrm.iout)
            return
         
        onboard_cache_hierarchy = ("prefetch", "doloop")
        parallel_ram_caches = ("tcm", "read_cache")
        
        exec_grp = interface.Group("Code execution")
        exec_tbl = interface.Table(["Active cycles", "Instructions", "Prefetch requests",  
                                    "Processing (active MHz)", "Processing (active MIPS)", "Processing (MIPS/MHz)"])
        exec_tbl.add_row([stats["cycles"], stats["instructions"], stats["prefetch_requests"],
                         "%0.2f" % stats["mhz"], "%0.2f" % stats["mips"], 
                         "%0.2f" % (stats["mips"]/stats["mhz"])])

        stalls_grp = interface.Group("Processor stalls")
        stall_tbl = interface.Table(["Type", "Number", "% of total", "Description"])
        stalls = stats["stalls"]
        total_stalls = sum(stalls.values())
        stalls["total"] = total_stalls
        stalls["non_stalls"] = stats["cycles"] - total_stalls
        
        def get_fmt(*values):
            max_values_digits = int(math.log10(max(values))) + 1
            return "%" + ("%d" % max_values_digits) + "d"
        stalls_fmt = get_fmt(total_stalls, stalls["non_stalls"])
        
        def add_stalls_row(stall_type, stall_key, description, no_percentage=False):
            stall_tbl.add_row([stall_type, stalls_fmt%stalls[stall_key], 
                               ("%0.2f"%(100.0*stalls[stall_key]/total_stalls) if not no_percentage else "-"), 
                                description])
        add_stalls_row("core", "core", "Stall cycles due to instruction "
                       "interlock, which prevents pipeline ordering hazards")
        add_stalls_row("memory access", "mem_access", "Cycles where a memory access "
                       "is pending, due to bus waits or memory instruction interlocks")
        add_stalls_row("instruction expand", "instr_expand", "Cycles where a dummy "
                       "or expand instruction starts")
        add_stalls_row("prefetch wait", "prefetch", "Number of wait cycles "
                       "sent by prefetch unit to the core")
        add_stalls_row("total", "total", "Stall cycles may overlap so the core will "
                       "see slightly fewer stalls than this")
        add_stalls_row("non-stalls", "non_stalls", "Stall cycles may overlap so the "
                       "core will see slightly more active cycles than this",
                       no_percentage=True)

        exec_grp.append(exec_tbl)
        stalls_grp.append(stall_tbl)
        exec_grp.append(stalls_grp)
        
        
        md_grp = interface.Group("Read cache configuration")
        md_tbl = interface.Table(["Mode", "Word size (bits)", "Line size (bytes)"])
        md_tbl.add_row([metadata["mode"], metadata["word width"], metadata["line length"]])
        md_grp.append(md_tbl)
        
        cache_grp = interface.Group("Cache performance")
        tbl = interface.Table(["Cache level", "Num hits", "Cumulative num hits", 
                               "Cumulative hit rate (%)", 
                               "Absolute hit rate (%)",
                               "Relative hit rate (%)"])
        tbl_key = interface.Text("    Cumulative hit rate = proportion of requests to prefetch that led to cache hits at any level up to and including this one\n"
                                 "    Absolute hit rate   = proportion of requests to prefetch that led to cache hits at this exact level\n"
                                 "    Relative hit rate   = proportion of instruction fetches reaching this level that led to cache hits at this level\n"
                                 )
        
        cum_hits = {}
        cum_hit_count = 0
        for level in onboard_cache_hierarchy:
            cum_hit_count += stats["hits"][level]
            cum_hits[level] = cum_hit_count
        onboard_cum_hits = cum_hit_count
            
        cum_hit_rate = {level : 100.0*hits / stats["instructions"] for (level, hits) in cum_hits.items()}

        hits_fmt = get_fmt(sum(stats["hits"].values()))

        # First add the layered onboard cache data        
        for level in onboard_cache_hierarchy:
            if stats["hits"][level] is not None:
                # If the hits number is None it means all entries for this level are None
                tbl.add_row([level, hits_fmt%stats["hits"][level], hits_fmt%cum_hits[level], 
                             "%0.2f" % cum_hit_rate[level],
                             "%0.2f" % stats["abs"][level],
                             "%0.2f" % stats["rel"][level]])
        
        # Now add a row for the combined hits to read_cache and tcm
        combined_ram_cache_name = " or ".join(parallel_ram_caches)
        hits = sum(stats["hits"][ram_mem] for ram_mem in parallel_ram_caches)
        combined_requests = stats["prefetch_requests"] - onboard_cum_hits
        cum_combined_hits = onboard_cum_hits + hits
        abs = "%0.2f" % (100.0*hits / stats["prefetch_requests"])
        cum = "%0.2f" % (100.0*cum_combined_hits / stats["prefetch_requests"])
        rel = "%0.2f" % (100.0*hits / combined_requests)
        tbl.add_row([combined_ram_cache_name, hits_fmt%hits, 
                     hits_fmt%cum_combined_hits, cum, abs, rel])
                
        # Now add separate rows for each one, with only the relevant information
        for ram_cache in parallel_ram_caches:
            tbl.add_row(["  (%s)"%ram_cache, hits_fmt%stats["hits"][ram_cache], "-", "-", 
                         "%0.2f" % stats["abs"][ram_cache], 
                         "%0.2f" % stats["rel"][ram_cache]])
                
        miss_tbl = interface.Table(["Num cache misses (fetches to NVM)", "Rate (%)"])
        miss_tbl.add_row([stats["prefetch_requests"]-cum_combined_hits, 
                          "%0.2f" % (100.0*(stats["prefetch_requests"]-
                                            cum_combined_hits)/stats["prefetch_requests"])])
        
        cache_grp.append(tbl)
        cache_grp.append(tbl_key)
        cache_grp.append(miss_tbl)
        
        grp = interface.Group("Kalimba core performance")
        
        grp.append(interface.Text("Sample duration: %0.4fs" % time_s))
        clock_rate = self.clock_rate() or clock_MHz 
        if not clock_rate:
            clock_rate = "unknown"
        else:
            clock_rate = "%d MHz" % clock_rate
        grp.append(interface.Text("Clock speed:     %s" % clock_rate))
        grp.append(exec_grp)
        grp.append(cache_grp)
        grp.append(md_grp)
        
        if report:
            return grp
        
        TextAdaptor(grp, gstrm.iout)


    def _print_list_regs(self, title, name_reg_fmt_list, report=False):
        '''
        Print a nicely formatted  table of the registers passed in a list
        using the friendly name in place of the register name.
        If the parameter report is True then the function will return a
        OutputText object. Otherwise the output is printed to the console.
        '''
        values = self._list_regs([reg for name, reg, fmt in name_reg_fmt_list])
        output = interface.Group(title)
        max_name_len = max([len(name) for name, reg, fmt in name_reg_fmt_list])

        def value_prefix(req_fmt):
            'return appropriate prefix for required format req_fmt'
            fmt_prefixes = [("x", "0x"), ("b", "b")]
            for fmt, prefix in fmt_prefixes:
                if fmt in req_fmt:
                    return prefix
            return ""

        for name, reg, fmt in name_reg_fmt_list:
            output.append(interface.Code("%s : %14s" %
                                         (format(name, "%ds" % max_name_len),
                                          value_prefix(fmt) +
                                          format(values[reg], fmt))))
        if report is True:
            return output
        TextAdaptor(output, gstrm.iout)


    def _list_regs(self, reg_list):
        'return a dictionary of register in iterable reg_list'
        reg_values = dict()
        for reg in reg_list:
            reg_values[reg] = self.fields[reg]
        return reg_values

    # Protected/Required

    @property
    def _info(self):
        """\
        Access Core Meta-data (CoreInfo).
        """
        raise PureVirtualError()

    # Protected/Overridable

    def _create_lpc_master(self):
        """\
        Create Data Address Space Proxy for this Core.
        """
        # So far all those encountered are the same...
        from ..lpc_master import LPCMaster
        return LPCMaster(self)

    def map_lpc_slave_regs_into_prog_space(self):

        raise NotImplementedError()

    def populate(self): #pylint: disable=no-self-use
        'populate the memory map for the core'
        # Kalimba cores all have different memory maps
        raise PureVirtualError

    #-------------------------------------------------------------------------
    # Kalimba-specific breakpoint implementation code.  This based on the Apps
    # memory map; I'm assuming the Audio subsystem looks the same.
    #-------------------------------------------------------------------------

    def _brk_enable(self, regid):
        """
        Set the enable bit for the given register
        """
        debug_reg = self.field_refs["DEBUG"]
        debug_reg_value = debug_reg.read()
        getattr(self.fields.DEBUG, "DEBUG_PM_BREAK%d"%regid).write(1)

    def _brk_disable(self, regid):
        """
        Clear the enable bit for the given breakpoint register
        """
        debug_reg = self.field_refs["DEBUG"]
        debug_reg_value = debug_reg.read()
        getattr(self.fields.DEBUG, "DEBUG_PM_BREAK%d"%regid).write(0)

    def brk_is_enabled(self, regid):
        """
        Check whether a given breakpoint is enabled
        """
        return (getattr(self.fields.DEBUG, "DEBUG_PM_BREAK%d"%regid).read()
                != 0)

    def brk_address(self, regid):
        """
        Return the address in the given breakpoint register
        """
        return self.fields["PM_BREAK%d_ADDR" % regid]

    def _brk_set_reg(self, regid, address, overwrite=True, enable=True):
        """
        Write the specified breakpoint register if it is currently free and
        enable the breakpoint.  If overwrite==True, write the register
        regardless of whether it's free
        """
        addr_reg = self.field_refs["PM_BREAK%d_ADDR" % regid]
        if overwrite or addr_reg.read() == self.brk_default_address_reg:
            # Set the address
            addr_reg.write(address)
            # Enable if necessary
            if enable:
                self.brk_enable(regid)
            return True
        return False

    @property
    def brk_default_address_reg(self):
        return 0

    def cpu_shallow_sleep_time(self, sample_ms=1000):
        """
        Returns the % of time the CPU spends in shallow sleep
        """
        # I could use either SHALLOW_SLEEP_STATUS or NUM_RUN_CLOCKS here
        #  * SHALLOW_SLEEP_STATUS only exists on CSRA68100 D01 onwards.
        #  * NUM_RUN_CLOCKS means I need to know the frequency the core is
        #    running at, which means I need to read some Curator registers.
        # Therefore I use SHALLOW_SLEEP_STATUS
        try:
            _ = self.fields.SHALLOW_SLEEP_STATUS.read()
        except AttributeError:
            # This chip doesn't support that register
            return 0

        asleep = 0
        awake = 0
        initial_time = time.time()
        final_time = initial_time + (sample_ms/1000.0)
        while time.time() < final_time:
            if self.fields.SHALLOW_SLEEP_STATUS.read():
                asleep += 1
            else:
                awake += 1
        total_samples = asleep + awake
        if total_samples < 10:
            self.logger.warning(
                'total samples is less than 10: increase sample_ms.')
        return (asleep * 100.0) / total_samples

    @property
    def execution_state(self):
        """
        Returns an object representing the execution state of the core.
        """
        try:
            return self._execution_state
        except AttributeError:
            self._execution_state = KalimbaExecState(core=self)
        return self._execution_state

    def _all_subcomponents(self):
        sub_dict = BaseCore._all_subcomponents(self)
        sub_dict.update({"execution_state": "_execution_state"})
        return sub_dict

    def is_call_to_fixed_address(self, pc, minim=True):
        """
        Check whether the instruction at the given address is a call to fixed
        address. If not, return False; if so, return the address.

        Note: only minim mode is supported but the flag is present for future
        extensibility/to make the minim-ness explicit.

        The supplied PC should not have the minim bit set (i.e. it should be on
        a 16-bit boundary)
        """
        if not minim:
            raise NotImplementedError(
                "Don't know how to decode MaxiM instructions")

        instruction = bytes_to_words(self.program_space[pc:pc+2])[0]

        if self.is_prefix(pc):
            # Prefixed instruction
            prefix = instruction & 0xfff
            instruction = bytes_to_words(self.program_space[pc+2:pc+4])[0]
            # In this case a call to constant has a fixed pattern in bits 15-12
            # and 7 - 5
            if instruction & 0xf0e0 == 0xe020:
                # and the relative call address is spread between bits 0-4, 8-11
                # and the 12-bit prefix as a 21-bit two's complement
                rel_addr = ((instruction & 0x1f) |
                            ((instruction & 0xf00) >> 3) |
                            (prefix << 9))
                if rel_addr >= (1<<20):
                    rel_addr -= (1<<21)
                return  pc +  rel_addr
        else:
            # In the non-prefixed case the upper 7 bits should be 0b0100111
            if instruction & 0xfe00 == 0x4e00:
                # The relative address is the remaining 9 bits as a 9-bit two's
                # complement
                rel_addr = instruction & 0x1ff
                if rel_addr >= (1<<8):
                    rel_addr -= 1<<9
                return pc + rel_addr*2 + 1 # it's a half-word address

        return False

    def is_call_through_ptr(self, pc, minim=True):
        """
        Check whether the instruction at the given address is a call to an
        address in a register. Return True if so, else False

        Note: only minim mode is supported but the flag is present for future
        extensibility/to make the minim-ness explicit.

        The supplied PC should not have the minim bit set (i.e. it should be on
        a 16-bit boundary)
        """
        if not minim:
            raise NotImplementedError(
                "Don't know how to decode MaxiM instructions")

        instruction = bytes_to_words(self.program_space[pc:pc+2])[0]
        # Bits 15-3 contain the opcode, which equals 0x4cd>>3
        return (instruction & 0xfff8) == 0x4cd0


    def is_prefix(self, pc, minim=True):
        """
        Is the instruction at the given pc a prefix instruction?
        """
        if not minim:
            raise NotImplementedError(
                "Don't know how to decode MaxiM instructions")

        instruction = bytes_to_words(self.program_space[pc:pc+2])[0]

        return (instruction & 0xf000) == 0xf000
