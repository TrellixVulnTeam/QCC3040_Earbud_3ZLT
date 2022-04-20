############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from __future__ import division
from .base_chip import BaseChip
from ..address_space import AddressSlavePort, AddressMasterPort
from ..port_connection import NoAccess
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint, wprint
from csr.wheels.bitsandbobs import PureVirtualError, NameSpace
from csr.dev.hw.address_space import AddressMap, AddressConnection
from csr.dev.hw.debug_bus_mux import TrbSubsystemMux, TcMemWindowed, TcMemRegBased
from csr.dev.model.interface import Group, Table
from csr.dev.adaptor.text_adaptor import TextAdaptor
from time import sleep, time
from collections import namedtuple

DeepSleepData = namedtuple("DeepSleepData",
                           ["timestamp_diff",
                            "time_asleep",
                            "percent_time_asleep",
                            "ds_entry_count",
                            "ds_exit_count"])


class FixedSubsystemNumber(object): #pylint:disable=too-few-public-methods
    """
    SubsystemNumber is the numbering as seen by the host and used in filenames
    do not confuse with the TBUS SSID subsystem numbering which is different
    and can vary, e.g. on partial emulators.
    """
    # This table must match the values in filename.xml.
    # Considered too heavyweight to parse the xml to generate this enum.
    CURATOR = 0
    BT = 1
    WLAN = 2
    AUDIO = 3
    GNSS = 4
    NFC = 5
    JANITOR = 6
    APPS = 7
    BLE = 8

class HydraSpiMux (object):
    """\
    Model Hydra chip-level SPI to subsystem MUX.

    Responsible for routing subsystem-relative SPI access 
    requests to correct subsystem.
     
    Exposes 1 top level SPI slave/input port & an array of master 
    output ports - one for each subsystem indexed by ssid.
    """
    def __init__(self, chip_level_spi_space, subsystems, 
                 access_cache_type):

        self._reg_space = chip_level_spi_space
        
        self.input = self.InPort(self, "CHIP_SPI_MUX", access_cache_type)        
        self.outputs = [None] * (max(subsystems)+1)
        for ssid in subsystems:
            self.outputs[ssid] = self.SubsystemFacingPort(self, ssid)
                        
    # Extensions
    
    @property
    def selected_output(self):
        """\
        Currently selected subsystem-facing port.        
        """
        # Infer from hardware
        return self.outputs[self._get_selected_ssid()]
    
    def select_output(self, output):
        """\
        Change currently selected subsystem-facing port.    
        """
        return self._set_selected_ssid(output.ssid)
    
    def _set_selected_ssid(self, ssid):
        """\
        Select specified subsystem's SPI space.
        """
        # Future: Optim: only need to hit register if value needs changing.
        # - waiting for cache to be implemented.
        self._reg_space[HydraChip.SPI.GBL_SPI_SUB_SYS_SEL] = ssid
        
    def _get_selected_ssid(self):
        """\
        Get currently selected subsystem id
        """
        return self._reg_space[HydraChip.SPI.GBL_SPI_SUB_SYS_SEL]


    class InPort (AddressSlavePort):
        """\
        Model the host-facing port of a HydraSpiMux.
        """        
        def __init__(self, mux, *args, **kwargs):
            
            AddressSlavePort.__init__(self, *args, **kwargs)
            self.mux = mux
        
        # AddressSlavePort compliance

        def _extend_access_path(self, path):
            # Extend path directly through each output
            for output in self.mux.outputs:
                path.create_simple_fork(output)
        
        def resolve_access_request(self, access_request):
            # Pass request, unmodified, out of currently selected subsystem
            # facing port.
            current_subsystem_port = self.mux.selected_output 
            current_subsystem_port.resolve_access_request(access_request)


    class SubsystemFacingPort (AddressMasterPort):
        """\
        Model a subsystem-facing port of a HydraSpiMux.
        """
        def __init__(self, mux, ssid):
            
            AddressMasterPort.__init__(self)
            self.mux = mux
            self.ssid = ssid
            
        # AddressMasterPort compliance
        
        def execute_outwards(self, access_request):
            # Select this port as the MUX output and pass the unaltered request
            # on outwards.
            self.mux.select_output(self)
            self.mux.input.execute_outwards(access_request)


class HydraChip (BaseChip):
    """\
    Hydra chip (abstract base)
    """
    class SPI:
        """\
        Misc chip-level SPI register definitions that makes no sense at 
        subsystem/core level.
        
        Future:- work out how to import this value from digits (its not core
        relative)
        """
        GBL_SPI_SUB_SYS_SEL = 0xfe82 

    class _SSID(object):
        def __init__(self, bus_address_offset, num_subsystems):
            self._offset = bus_address_offset
            self._num_subsystems = num_subsystems
            
        @property
        def CURATOR(self):
            return self._offset
        
        @property
        def HOST(self):
            return 1 + self._offset
        
    @property
    def SSID(self):
        """
        Subsystem IDs (enum)
        
        Curator & Host block have consistent ids on all hydra chips but maybe
        offset if accessing the chip through the TrBridge of another one
        
        Future:-
        - Consider replacing all uses of these with references to
        concrete subsystem _classes_ (which will know their chip-relative id)! 
        (typesafe and much more explicit/direct).
        """
        return self._ssid
    
    def __init__(self, access_cache_type, bus_address_offset = 0):
        """\
        Params:-
        - access_cache_type    Memory Access Cache type/policy.
        """
        self._access_cache_type = access_cache_type
        self._ssid = self._SSID(bus_address_offset, self.num_subsystems)
        
    def set_device(self, device):
        self.device = device
        
    # BaseChip compliance
    
    @property
    def subcomponents(self):
        return {"curator_subsystem" : "_curator_subsystem",
                "host_subsystem" : "_host_subsystem"}

    # Extensions
    
    def populate(self):
        """\
        Construct Chip sub-components and wire them together.
        
        This should be called after a concrete HydraChip object has 
        been fully constructed and before any other use is made of it. I.e.:-
        
            chip=AmberD00Chip()
            chip.populate()
        
        Or, more fluently:-
        
            chip=AmberD00Chip().populate()
            
        This can not be done during object construction because details depend
        on specialisations in the derived Chip class. E.g. the number and exact
        type of subsystems present.
        """ 
        self._components = NameSpace()
        comps = self._components # shorthand
        
        # Chip level SPI space.
        #
        # The space is _almost_ empty but we can't elide it as it contains the
        # vital SPI subsystem Mux register.
        #
        comps.spi_map = AddressMap("CHIP_SPI", self._access_cache_type)
        
        # Special case: The spi memory must provide an identity check for use
        # by the FPGAControl mux port to which it will be connected.
        #
        spi_mem = comps.spi_map.port
        spi_mem.set_identity_checker(self.check_identity)
        
        # Main SPI to subsystem MUX
        #
        comps.spi_mux = HydraSpiMux(spi_mem, self.subsystems, 
                                    self._access_cache_type) 

        # Add mappings to chip level spi space.
        #
        # All SPI accesses, _except 0xfe82_, map via the main CHIP SPI MUX to
        # the currently selected subsystem.
        #
        # We must not inadvertently map 0xfe82 lest requests to set the CHIP
        # MUX register get resolved to a subsystem address space by the SPI MUX
        # model and then get routed out via the MUX along the spi access path -
        # triggering another request to set the SPI MUX ...ad infinitum (been
        # there) - did you follow that?.
        # 
        comps.spi_map.add_mapping(
            0x0000, self.SPI.GBL_SPI_SUB_SYS_SEL, 
            comps.spi_mux.input, to_start=0x0000
        )
        # 1 word gap here for GBL_SPI_SUB_SYS_SEL
        comps.spi_map.add_mapping(
            self.SPI.GBL_SPI_SUB_SYS_SEL + 1, 0x10000, 
            comps.spi_mux.input, to_start=self.SPI.GBL_SPI_SUB_SYS_SEL +1
        )

        # Internal access routing connections
        #
        self._connections = set()
        cons = self._connections # shorthand
        
        try:
            # Connect SPI MUX outputs to subsystem SPI inputs.
            for ssid, ss in self.subsystems.items():
                cons.add(AddressConnection(comps.spi_mux.outputs[ssid], 
                                           ss.spi_in))
        except NotImplementedError:
            # Not all chips have SPI anymore
            pass
        
        # TRB mux is connected up within the TrbSubsystemMux object init.
        
        return self 
    
    @property
    def spi_port(self):
        """\
        Chip level SPI port (model)
        
        This port is mapped to a specific subsystem (via GBL_SPI_SUB_SYS_SEL
        FE82).
        
        Not normally addressed directly by applications. This is exposed to 
        support the construction of explicit SPI network model for routing 
        purposes.
        """
        return self._components.spi_map.port
    
    @property
    def is_emulation(self):
        """\
        Is this an emulated chip?
        
        Potential extension:: Promote but not to Chip as that includes LPCs - which are always
        FPGAs! Need intermediate class ...or something.
        """        
        return False
    
    @property
    def subsystems(self):
        """\
        Dictionary of Subsystems indexed by native ID.
        
        Published for generic code that wishes to iterate over all subsystems
        present on chip.
        
        Subtypes should override and add extra subsystems to this dictionary.
        
        Q: Is SSID index actually needed? Is there any code that needs to
        lookup Subsystems by index? - Yes - some external data refers to SSIDs
        (of course we could scan and build a map - but leaving it with index
        for now)
        """
        return {
            self.SSID.CURATOR: self.curator_subsystem,
            self.SSID.HOST: self.host_subsystem,
        }

    @property
    def curator_subsystem(self):
        """\
        Curator Subsystem.
        """ 
        # Construct/Create on demand (COD)
        #
        # Potential extension:: steal/invent @on_demand_property decorator for this pattern.
        #
        try:
            self._curator_subsystem
        except AttributeError:
            self._curator_subsystem = self._create_curator_subsystem()
            
        return self._curator_subsystem
                 
    @property
    def host_subsystem(self):
        """\
        Host Subsystem.
        """ 
        # COD
        try:
            self._host_subsystem
        except AttributeError:
            self._host_subsystem = self._create_host_subsystem()
            self._host_subsystem.populate()
            
        return self._host_subsystem
                
    @property
    def lpc_pin_mux(self):
        """\
        LPC Pin MUX model.
        
        It is debatable whether this MUX model should be contained in the Chip
        or in the Curator Subsystem (where the control register is).
        """
        # Lazy
        try:
            self._lpc_pin_mux
        except AttributeError:
            from csr.dev.hw.hydra_lpc_pin_mux import HydraLpcPinMux
            self._lpc_pin_mux = HydraLpcPinMux(self.curator_subsystem.core.field_refs)

        return self._lpc_pin_mux
    
    @property
    def lpc_link_mux(self):
        """\
        LPC Link MUX (Proxy)
        
        It is debatable whether this MUX model should be contained in the Chip
        or in the Curator Subsystem (where the control register is).
        """
        # Lazy
        try:
            self._lpc_link_mux
        except AttributeError:
            self._lpc_link_mux = self._create_lpc_link_mux()
            
        return self._lpc_link_mux
    
    def check_identity(self):
        """\
        Quick check that the accessible memory looks plausible for this chip.
        
        Known Uses:
            To check that the chip core has been correctly selected via
            SPI FPGAControl.
        """
        # Default implementation just checks version is a hydra chip of some
        # sort.
        #
        return self.version.is_hydra_family

    def load_register_defaults(self):
        """
        Load register defaults on all the subsystems
        """
        for ss in self.subsystems.values():
            ss.load_register_defaults()
             
    def halt(self):
        """\
        Bring the chip hardware to a grinding halt (all cpus & active 
        subsystems).
        
        Use violence, do not negotiate with firmware.
        """
        # Delegate to curator core as it contains all the relevant 
        # registers.
        #
        self.curator_subsystem.core.halt_chip()

    def reset(self):
        """\
        Reset the chip as thoroughly as possible (using debug reset register).
        
        Use violence, do not negotiate with firmware.
                
        N.B. Prefer reset_and_wait() unless you know what you are doing...
        
        It may take some time (mS) for the chip's core to become
        accessible after a reset (and even longer for curator to boot).
        """
        # Delegate to curator core as it contains all the relevant 
        # registers.
        #
        self.curator_subsystem.core.reset_chip()

    def reset_and_wait(self):
        """\
        Reset the chip as thoroughly as possible (using debug reset register).        
        And block till enough time has passed for the chip to have 
        re-initialised.
        """
        CHIP_INIT_LIMIT_SECS = 0.250 # empirical x 10
        self.reset()
        sleep(CHIP_INIT_LIMIT_SECS)

    @property
    def trb_in(self):
        '''
        Chip's Tbridge adapter slave port
        '''
        return self._trb.port

    @property
    def _trb(self):
        try:
            self.__trb
        except AttributeError:
            self.__trb = self._create_trb_adapter()
        return self.__trb


    @property
    def tc_mem_win_port(self):
        """
        Chip toolcmd windowed memory access port.  Provides toolcmd-based
        access to XAP subsystems
        """
        try:
            self._lcd_win
        except AttributeError:
            # Windowed access for subsystems that have a dedicated toolcmd
            # memory access map
            self._lcd_win = TcMemWindowed([ss for ss in self.subsystems.values() 
                                                 if ss.tc_in is not ss.trb_in])
            iprint("%d windowed masters" % (len(self._lcd_win.masters)))
        return self._lcd_win.port

    @property
    def tc_mem_reg_port(self):
        """
        Chip toolcmd reg-based memory access port.  Provides toolcmd-based
        access to non-XAP subsystems
        """
        try:
            self._lcd_reg
        except AttributeError:
            # Reg-based access for subsystems for which toolcmd access
            # falls back to raw TRB access
            self._lcd_reg = TcMemRegBased([ss for ss in self.subsystems.values() 
                                             if ss.tc_in is ss.trb_in])
        return self._lcd_reg.port



# Overridable
    
    def _create_lpc_link_mux(self):
        """\
        Create LPC Link MUX Proxy.
        
        Derived classes should override to create a unconventional LPC link 
        MUX.
        (e.g. amber-asic)
        
        Called on first request for the Proxy.
        """
        from csr.dev.hw.hydra_lpc_link_mux import DefaultLpcLinkMux
        return DefaultLpcLinkMux(self)

    # Required
    
    def _create_curator_subsystem(self):
        """\
        Create CuratorSubsystem Proxy.
        
        Derived classes must override to create appropriate variant.
        
        Called on first request for the Proxy.
        """
        raise PureVirtualError();        

    def _create_host_subsystem(self):
        """\
        Create HostSubsystem Proxy.
        
        Derived classes must override to create appropriate variant.
        
        Called on first request for the Proxy.
        """
        raise PureVirtualError();        

    def _create_trb_adapter(self):
        '''
        Create the TBridge adapter.  This connects directly to the physical
        Tbridge so far as our model is concerned
        '''
        return TrbSubsystemMux(list(self.subsystems.values()))

    @property
    def trb_log(self):
        from csr.dev.framework.connection.trb_log import TrbLogger
        from csr.dev.framework.connection.trb import TrbTransConnection
        if not isinstance(self.device.transport, TrbTransConnection):
            raise AttributeError("trb_log unavailable: not connected over TRB")
        try:
            self._trb_log
        except AttributeError:
            self._trb_log = TrbLogger(self)
        return self._trb_log
            
    @property
    def emulator_build(self):
        """
        Root directory of the digital build that should be used to obtain
        register information
        """
        return None    
    
    @property
    def subsystems_dict(self):
        """
        Construct a dictionary mapping from subsystem attribute names to the
        objects.  This is just constructed on the fly by finding suitably
        named attributes pointing to objects in the (ID-keyed) subsystems dict 
        """
        return {attr:getattr(self,attr) for attr in dir(self) 
                   if attr.endswith("_subsystem") and not attr.startswith("_") and 
                       getattr(self,attr) in self.subsystems.values()}

    @property
    def deep_sleep(self):
        """
        Display the deep sleep statistics of the chip
        """
        if not self.device.transport:
            raise ValueError(
                "No trb device attached, you are probably calling "
                "this on coredump which will not work. "
                "Please attach a trb before calling the deep sleep method "
            )
        try:
            self._deep_sleep
        except AttributeError:
            self._deep_sleep = ShowDeepSleepStatistics(
                self.device.transport.trb)
        return self._deep_sleep


class ShowDeepSleepStatistics:
    """
    Calculate the amount of time a chip stays in deep sleep
    """
    def __init__(self, trb):
        self._trb = trb
        """ Monitoring flag to signal when monitoring phase has finished """
        self._monitoring_complete = False
        """ Set all the start/end variables to None """
        self._reset_start_data()
        self._reset_end_data()

    def _reset_start_data(self):
        self._start_timer = None
        self._start_ds_entry_count = None
        self._start_ds_exit_count = None
        self._begin_read_time = None
        self._remote_asleep_start = None

    def _reset_end_data(self):
        self._end_timer = None
        self._end_ds_entry_count = None
        self._end_ds_exit_count = None
        self._end_read_time = None
        self._remote_asleep_end = None
        self._monitoring_complete = False

    def start_deep_sleep_monitor(self):
        """ Monitor the first set of deep sleep info """

        if self._start_timer is not None and self._end_timer is None:
            wprint(
                "The start timer had already been called once without a "
                "corresponding end timer call. Ignoring the first call and"
                "resetting the start timer monitoring data..."
            )
            self._reset_start_data()
        elif self._end_timer is not None:
            """ 
            The end timer needs to be reset once the start timer is initiated
            """
            self._reset_end_data()
        """
        Calculate the time when the first deep sleep info is read.
        This is a fallback in case timestamp diff is zero.
        """
        self.begin_time = time()
        self._start_timer = self._trb.read_deep_sleep_info()
        self._start_ds_entry_count = self._start_timer.entry_count
        self._start_ds_exit_count = self._start_timer.exit_count
        self._remote_asleep_start = self._start_timer.timing_info.\
                                    remote_asleep_timer
        self._timestamp_constant = self._start_timer.timing_info.\
                                   timestamp_constant
        self._asleep_timer_constant = self._start_timer.timing_info.\
                                       asleep_timer_constant

        """
        Calculate time using times of first read
        (timestamp_full/timestamp_constant)
        """
        self._begin_read_time = (self._start_timer.timing_info.timestamp_full /
                                 self._timestamp_constant)

    def end_deep_sleep_monitor(self):
        """ Monitor the end set of deep sleep info"""
        if self._start_timer is None:
            raise DeepSleepMonitoringError(
                "End deep sleep monitor method called without calling the "
                "start deep sleep monitor method first!"
            )
        self._end_timer = self._trb.read_deep_sleep_info()
        self._end_ds_entry_count = self._end_timer.entry_count
        self._end_ds_exit_count = self._end_timer.exit_count
        self._remote_asleep_end = self._end_timer.timing_info.remote_asleep_timer

        """
        Calculate time using times of last read
        (timestamp_full/timestamp_constant)
        """
        self._end_read_time = (self._end_timer.timing_info.timestamp_full /
                               self._timestamp_constant)

        """
        Calculate the time when the last deep sleep info is read.
        This is a fallback in case timestamp diff is zero.
        """
        self.total_monitoring_time = (time() - self.begin_time)

        """ Monitoring complete. Set the flag to True """
        self._monitoring_complete = True

    def monitor_deep_sleep(self, time_interval):
        """
        time_interval: Block for the amount of time
        requested by the user if provided (in seconds)
        Call the report method to display
        """
        self.time_interval = time_interval
        if self.time_interval == 0:
            raise ValueError("Time interval cannot be zero. "
                             "Please provide a positive non zero value.")
        iprint("Resetting deep sleep data...")
        self._reset_start_data()
        self._reset_end_data()
        iprint("Start deep sleep information sampling")
        self.start_deep_sleep_monitor()
        iprint("Blocking for {} seconds".format(time_interval))
        sleep(time_interval)
        self.end_deep_sleep_monitor()
        iprint("Deep sleep sampling completed.")

    @property
    def time_period(self):
        """
        The difference between start and end read times.
        """
        if self._monitoring_complete:
            # Account for timestamp wrapping for the full time period.
            if self._end_read_time < self._begin_read_time:
                self._end_read_time += 1 << 32
            _timestamp_diff = self._end_read_time - self._begin_read_time
            """
            Check that timestamp diff is not zero.
            If it is, then return total_monitoring_time
            if time_interval not set, raise an error.
            """
            if _timestamp_diff == 0:
                # When timestamp full is garbage, then we don't need
                # to use the actual value of timestamp constant either
                # as that throws the calculations off for percent_time_asleep
                self._timestamp_constant = 1
                return self.total_monitoring_time * (10 ** 6)
            return _timestamp_diff * (10 ** 6)
        else:
            raise DeepSleepMonitoringError("Method cannot be called before "
                                           "monitoring is complete")

    @property
    def time_asleep(self):
        """
        Calculate the amount of time the chip spent in deep sleep
        This is done by subtracting the start and end values of the
        remote_asleep_timer and then diving it by the timestamp_constant
        """
        if self._monitoring_complete:
            # Account for timestamp wrapping for time asleep.
            if self._remote_asleep_end < self._remote_asleep_start:
                self._remote_asleep_end += 1 << 32
            if self._asleep_timer_constant == 0:
                raise ZeroDivisionError("Cannot calculate time asleep "
                                        "as the divisor is zero!")
            _time_asleep = (
                (self._remote_asleep_end / self._asleep_timer_constant) -
                (self._remote_asleep_start / self._asleep_timer_constant)
            )
            _time_asleep = _time_asleep * (10 ** 6)
            return _time_asleep
        else:
            raise DeepSleepMonitoringError("Method cannot be called before "
                                           "monitoring is complete")

    @property
    def percent_time_asleep(self):
        """
        Percentage of time the chip was asleep within a given time interval
        """
        if self._monitoring_complete:
            _percent_time_asleep = (self.time_asleep / self.time_period) * 100
            return _percent_time_asleep
        else:
            raise DeepSleepMonitoringError("Method cannot be called before "
                                           "monitoring is complete")

    @property
    def ds_entry_count(self):
        """
        Number of times the chip entered deep sleep
        """
        if self._monitoring_complete:
            _ds_entry_count = self._end_ds_entry_count - self._start_ds_entry_count
            return _ds_entry_count
        else:
            raise DeepSleepMonitoringError("Method cannot be called before "
                                           "monitoring is complete")

    @property
    def ds_exit_count(self):
        """
        Number of times the chip exited deep sleep
        """
        if self._monitoring_complete:
            _ds_exit_count = self._end_ds_exit_count - self._start_ds_exit_count
            return _ds_exit_count
        else:
            raise DeepSleepMonitoringError("Method cannot be called before "
                                           "monitoring is complete")

    def report_sleep_stats(self, report=False):
        """
        Report the deep sleep statistics using the interface classes
        """
        if self._monitoring_complete is False:
            raise DeepSleepMonitoringError(
                "Deep Sleep data has not been calculated! "
                "Please finish data sampling and calculations "
                "before calling this method.")

        _deep_sleep_data = DeepSleepData(
            timestamp_diff=self.time_period,
            time_asleep=self.time_asleep,
            percent_time_asleep=self.percent_time_asleep,
            ds_entry_count=self.ds_entry_count,
            ds_exit_count=self.ds_exit_count
        )

        output = Group("Deep Sleep Information")
        table = Table(headings=["Timestamp diff (microseconds)",
                                "Time asleep (microseconds)",
                                "Percentage time asleep",
                                "Number of DS entries",
                                "Number of DS exits"])

        table.add_row(_deep_sleep_data)
        output.append(table)
        if report:
            return output
        TextAdaptor(output, gstrm.iout)

    start_monitor = start_deep_sleep_monitor
    end_monitor = end_deep_sleep_monitor
    monitor = monitor_deep_sleep
    report = report_sleep_stats


class DeepSleepMonitoringError(RuntimeError):
    """ Error that could be raised during deep sleep monitoring """

