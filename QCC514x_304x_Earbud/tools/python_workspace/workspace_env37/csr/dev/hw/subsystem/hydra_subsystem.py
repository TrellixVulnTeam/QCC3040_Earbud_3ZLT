############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
import sys
import time
from collections import namedtuple
from contextlib import contextmanager
from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.wheels.bitsandbobs import PureVirtualError, flatten_list
from csr.dev.model.base_component import BaseComponent
from csr.dev.model.interface import Group, Table, Text
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.fw.meta.i_firmware_build_info import IFirmwareBuildInfo
from csr.dev.hw.address_space import AddressMap

class HydraSubsystem (BaseComponent):
    """\
    Hydra Subsystem Proxy (Base)
    
    N.B. Includes Host subsystem.
    """
    def __init__(self, chip, ss_id, access_cache_type):
        
        self._chip = chip
        self._id = ss_id
        self._access_cache_type = access_cache_type
        from .curator_subsystem import CuratorSubsystem
        if not isinstance(self, CuratorSubsystem):
            try:
                self._curator = chip.curator_subsystem
            except AttributeError:
                # Partial emulator chips don't have a Curator - we have to wait
                # for an explicit connection
                self._curator = None
        else:
            self._curator = self

    # BaseComponent compliance
    
    @property
    def title(self):
        return self.name + ' subsystem'

    @property
    def subcomponents(self):
        return {}
    
    @property
    def number(self):
        """
        The subsystem number (not to be confused with the SSID)
        as defined by csr.dev.hw.chip.hydra_chip.FixedSubsystemNumber.
        """
        return NotImplemented
    
    @property
    def firmware_type(self):
        '''
        Type of firmware that this subsystem should have running on its core.
        Cores can override this type if they wish but by default they fall back
        on this function.
        '''
        raise PureVirtualError
    
    @property
    def default_firmware_type(self):
        '''
        Type of firmware object, if any, that should be instantiated if there 
        is no proper firmware support available to pydbg.  Should inherit from
        DefaultFirmware (csr.dev.fw.firmware)
        '''
        return None

    @property
    def patch_type(self):
        """
        Type of patch object, that this subsystem firmware needs.
        """
        return PureVirtualError

    @property
    def firmware_build_info_type(self):
        '''
        Type of firmware_build_info that this subsystem needs
        '''
        raise PureVirtualError

    @property
    def patch_build_info_type(self):
        """
        Type of patch firmware build info that this subsystem needs
        """
        raise PureVirtualError

    @property
    def has_per_core_firmware(self):
        """
        Predicate indicating that the subsystem runs different firmware on 
        each core (this is obviously not very interesting if there's only one
        core)
        """
        return True

    # Extensions

    @property
    def chip(self):
        """Which chip is this subsystem on?"""
        return self._chip

    @property
    def curator(self):
        """
        Reference to the Curator subsystem on the chip (or companion chip in
        the case of the partial emulator)
        """
        return self._curator
        
    @property
    def id(self):
        """Chip-relative subsystem id/bus number"""
        return self._id
    
    @property
    def name(self):
        """Friendly name"""
        raise PureVirtualError(self)

#    @property
#    def chip_version(self):        
#        """\
#        The chip hardware version is mapped to a register in every subsystem.
#        Probably only ever ask the curator.
#        """
#        raise PureVirtualError(self)

    @property
    def spi_in(self):
        """\
        This subsystem's SPI AddressSlavePort.
        Used to wire up the chip's memory access model.
        
        It is not usually addressed directly but is needed
        to model the spi access route.
        
        """
        return self.spi_data_map.port

       
    @property
    def spi_data_map(self):
        """
        Lazily construct a SPI data map for the subsystem
        """
        try:
            self.__spi_data_map
        except AttributeError:
            self.__spi_data_map = self._create_spi_data_map()
        return self.__spi_data_map

    @property
    def trb_in(self):
        """\
        This subsystem's TRB AddressSlavePort.
        Used to wire up the chip's memory access model.
        
        It is not usually addressed directly but is needed
        to model the trb access route.
        
        """
        return self.trb_map.port

       
    @property
    def trb_map(self):
        """
        Lazily construct a TRB data map for the subsystem
        """
        try:
            self.__trb_map
        except AttributeError:
            self.__trb_map = self._create_trb_map()
        return self.__trb_map

    @property
    def tc_in(self):
        """
        This subsystem's toolcmd AddressSlavePort.  Used to wire up the chip's
        memory access model.  This defaults to the trb map if the core doesn't
        provide a dedicated memory map for toolcmd-based accesses, because in 
        that case (non-XAP codes) toolcmd will directly generate bus transactions
        """
        try:
            self._tc_map
        except AttributeError:
            try:
                primary_core = self.cores[0]
                self._tc_map = primary_core.create_tc_map()
            except (IndexError, AttributeError):
                # If there's no dedicated ToolCmd map, access the subsystem via
                # the register-based view
                self._tc_map = self.trb_map
        return self._tc_map.port
      
    @property
    def cores(self):
        """\
        List of CPU Cores in this subsystem (if any)
        """
        return []


    def load_register_defaults(self):
        """
        Load register defaults on all the cores
        """
        for core in self.cores:
            core.load_register_defaults()
        self.has_data_source = True

    @property
    def is_up(self):
        """\
        Is this subsystem powered and stable?
        """
        # Delegate to Curator Core as it contains the register for 
        # answering this.
        #
        return self.curator.core.is_subsystem_up(self)

    def power_cycle(self):
        """
        Power-cycle a subsystem
        """
        self.set_power(on_not_off=False)
        self.set_power()
        
    def set_power(self, on_not_off = True): 
        """\
        Enable power to this subsystem.        
        Can take a while to stabilise. See is_power_stable()
        """
        # Delegate to Curator Core as it contains the register for 
        # controlling this.
        #
        self.curator.core.set_subsystem_power(self, on_not_off)

    @contextmanager
    def ensure_powered(self):
        """
        As a context manager, can be used 'with' a block of code that
        needs to ensure subsystem is powered for an operation
        and wants to restore it to original state afterwards.
        """
        raise PureVirtualError(self, 'ensure_powered')

    @contextmanager
    def ensure_clocked(self):
        """
        As a context manager, can be used 'with' a block of code that
        needs to ensure subsystem is clocked for an operation
        and wants to restore it to original state afterwards.
        Executes in a context with ensure_powered().
        """
        raise PureVirtualError(self, 'ensure_clocked')

    # Provides
    
    @property
    def _connection(self):
        """\
        Device Access/connection interface.
        """
        return self.chip._connection
    
    def _create_spi_data_map(self):
        """
        The SPI data map looks different on different subsystems so let them
        create it themselves
        """
        raise PureVirtualError()
    
    def _create_trb_map(self):
        '''
        The TRB data map looks different on different subsystems
        '''
        raise PureVirtualError()
    
    @property
    def hif(self):
        """
        Subsystem's view of the host subsystem
        """
        try:
            self.__hif
        except AttributeError:
            self.__hif = self._create_host_subsystem_view()
        return self.__hif

    def bulk_erase(self,bank=0):
        """\
        Most basic way to completely erase a SQIF. 
        
        ONlY Uses register peeks and pokes so does need to have have had 
        firmware specified.
        
        SHOULD be able to erase a SQIF regardless of the system state.
        """
        iprint("No-one has taught me how to erase the SQIF for this subsystem") 

    def _flatten(self, my_thing):
       flat_list = []
       for i in my_thing:
           if i:
               if isinstance(i, dict):
                   flat_list.append(i)
               else:
                   flat_list.extend(self._flatten(i))
       return flat_list

    def _adjust_memory_report(self, report):
        """\
        By default we don't modify the report
        """
        return report

    MemUsageEntry = namedtuple(
        "MemUsageEntry", 
        "nesting name full_name size start end used unused percent_used comment")

    def _get_memory_usage_data(self):
        """\
        Returns a table describing the subsystem's RAM usage.
        Dynamic RAM such as stack, malloc and heap have their % utilisation shown
        """
        raw_report = self._gather_memory_report()
        flat_report = self._flatten(raw_report)
        flat_report = self._adjust_memory_report(flat_report)
        flat_report.sort(key=lambda tup:(tup["start"], 0xFFFFFFFF - tup["end"]), reverse=False)

        initial_indent = 1
        try:
            addr_unit_len = self.addr_unit_bits
        except AttributeError:
            addr_unit_len = self.core.info.layout_info.addr_unit_bits

        bytes_per_word = addr_unit_len // 8

        proc_entries = []

        last_nested_name = {0:""}
        def get_full_name(name, nest_level):
            def get_ancestry_name(nest_level):
                if nest_level > 0:
                    parent_of_parent = get_ancestry_name(nest_level-1)
                else:
                    parent_of_parent = ""
                try:
                    parent = last_nested_name[nest_level]
                except KeyError:
                    parent = ""
                return ":".join([parent_of_parent, parent]).strip(":")
            return ":".join([get_ancestry_name(nest_level-1), name]).lstrip(":")

        for count, entry in enumerate(flat_report):

            nesting = 1
            # Loop through previous entries and look for ranges this current entry is within
            for previous_counter in range(0, count):
                if flat_report[previous_counter]["start"] <= entry["start"] and \
                   flat_report[previous_counter]["end"] >= entry["end"]:
                       nesting += 1
            last_nested_name[nesting] = entry["name"]

            try:
                used = entry["used"] * bytes_per_word
            except KeyError:
                used = None
            try:
                unused = entry["unused"] * bytes_per_word
            except KeyError:
                unused = None

            last_nested_name[nesting] = entry["name"]

            proc_entries.append(self.MemUsageEntry(
                nesting=nesting,
                name=entry["name"],
                full_name=get_full_name(entry["name"], nesting),
                size=entry["size"]*bytes_per_word,
                start=entry["start"], end=entry["end"], 
                used=used, 
                unused=unused, 
                percent_used=entry.get("percent_used"),
                comment=entry.get("comment")))
            
        return proc_entries

    def memory_usage(self, report = False):

        group_report = Group("Memory usage")

        try:
            self.core
        except AttributeError:
            # No core so no RAM (for example, host subsystem)
            group_report.append(Text("No RAM in this subsystem"))

            if report is True:
                return group_report
            TextAdaptor(group_report, gstrm.iout)

            return

        try:
            memory_usage_table = self._get_memory_usage_data()
        except IFirmwareBuildInfo.FirmwareSetupException as exc:
            group_report.append(
                Text("Unable to load firmware symbols when gathering memory "
                     "usage data ('%s')" % exc))

            if report is True:
                return group_report
            TextAdaptor(group_report, gstrm.iout)

            return
        
        combined_report = Table(["Label", "Address range", "Size(Bytes)",
                                 "Used", "Unused", "% Used",
                                 "Comments"])

        # We currently track the RAM
        #   At the outermost "indent" level which we know is the subsystems total RAM
        #   At the second "indent" level which is the first decomposition of the RAM into blocks
        total_ram = [0,0]
        
        def fmt(val, fmt, suffix="", default="-"):
            return ((fmt % val)+suffix) if val is not None else default

        for entry in memory_usage_table:

            if entry.nesting == 1:
                total_ram[0] += entry.size
            elif entry.nesting == 2:
                total_ram[1] += entry.size

            combined_report.add_row([" "*entry.nesting + entry.name,
                                 "0x%08x - 0x%08x" % (entry.start, entry.end),
                                 "%8d" % entry.size,
                                 fmt(entry.used, "%8d"),
                                 fmt(entry.unused, "%8d"),
                                 fmt(entry.percent_used, "%2.2f", suffix="%"),
                                 fmt(entry.comment, "%s")])

        combined_report.add_row([" TOTAL_RAM_USED",
                                 "-",
                                 "%8d" % total_ram[0],
                                 "%8d" % total_ram[1],
                                 "%8d" % (total_ram[0] -total_ram[1]),
                                 "-",
                                 "RAM 'unused' here needs investigation"])

        group_report.append(combined_report)
        if report is True:
            return group_report
        TextAdaptor(group_report, gstrm.iout)

    def memory_usage_data(self, dynamic_only=True):
        
        return [entry for entry in self._get_memory_usage_data() 
                                                if not dynamic_only or entry.unused is not None]
                
    def _generate_report_body_elements(self):
        elements = []
        elements.append(self.memory_usage(report=True))
        return elements

    @property
    def has_data_source(self):
        """
        Has at least one of the cores been connected to a source of data of some kind?
        """
        if self.cores:
            return any(c.has_data_source for c in self.cores)
        else:
            try:
                return self._has_data_source
            except AttributeError:
                return False
        
    @has_data_source.setter
    def has_data_source(self, has):
        """
        Indicate that all the cores have been connected to a data source
        """
        if self.cores:
            for c in self.cores:
                c.has_data_source = has
        else:
            self._has_data_source = True
            
    @property
    def emulator_build(self):
        return self._chip.emulator_build
    
class SimpleHydraSubsystem(HydraSubsystem):
    """
    Minimal placeholder to be used to avoid exposing unintended functionality.
    """
    @property
    def name(self):
        return "Generic"
    
    def _create_trb_map(self):
        trb_map = AddressMap("GENERIC_TRB_MAP", self._access_cache_type,
                             word_bits=8)
        # Use len in this awkward way to allow the length to be >=2**32 on a
        # 32-bit Python
        trb_map.add_mapping(0, self.core.data.__len__(), self.core.data)
        return trb_map
    
    
class SimpleHydraXAPSubsystem(SimpleHydraSubsystem):
    """
    Variant of SimpleHydraSubsystem for XAP-based subsystems.  This makes a
    hardcoded assumption that gw1 and possibly gw2 are pointing at program
    space.  We need this to allow SLTs to be read, and hence firmware versions
    to be retrieved.
    """
    def _create_trb_map(self):
        
        trb_map = AddressMap("GENERIC_TRB_MAP", self._access_cache_type,
                             word_bits=8,
                             max_access_width = 2)
        trb_map.add_mapping(0,0x100, self.core._data_space)
        trb_map.add_mapping(0x10000,0x20000, self.core._data_space)
        
        trb_map.add_mapping(0x20100, 0x24000, self.core._gw1)
        try:
            self.core._gw2
        except AttributeError:
            pass
        else:
            trb_map.add_mapping(0x24000, 0x28000, self.core._gw2)
        return trb_map
