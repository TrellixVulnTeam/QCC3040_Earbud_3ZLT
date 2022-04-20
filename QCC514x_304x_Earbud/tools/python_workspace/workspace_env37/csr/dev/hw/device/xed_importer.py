############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels.bitsandbobs import pack_unpack_data_le, build_le
from csr.dev.hw.chip_version import ChipVersion
from csr.dev.hw.device.device_factory import DeviceFactory
from csr.dev.hw.address_space import ExtremeAccessCache
from io import open
import sys


class EmptyDebugPartition(ValueError):
    pass


class XEDImporter(object):
    """
    Constructs virtual devices and loads some values based on information extracted 
    from debug partition xuv
    """
    
    def import_device(self, xed_path, emulator_build=None):
        """
        param: xed_path - A filepath to and xed file
        """       
        with open(xed_path, "rb") as f:
            if sys.version_info > (3,):
                xed_data = f.read()
            else:
                xed_data = [ord(c) for c in f.read()]

        debug_partition = DebugPartition(xed_data)

        events = debug_partition.debug_events
        if len(events) == 0:
            raise EmptyDebugPartition("No debug events found, please verify validity of .xed file")
        devices = []
        for event in events:
            chip_version = ChipVersion(event.header.chip_version)
            device = DeviceFactory.create(chip_version, None, ExtremeAccessCache,
                                          emulator_build=emulator_build)
            setattr(device, 'device_url', 'xed:' + xed_path)

            # Load the data from the event into the device
            self._load_into_device(device, event)
            devices.append(device)

        return devices
    
    def _load_into_device(self, device, event):
        """
        Load the event into the chip. Events consist of logs for some number of subsystems.
        Each of these logs contains the dumped memory from some range of memory in that subsystem, 
        in order to recreate it we simply populate Pydbgs resigter caches with this memory block for each of them.
        """

        chip = device.chips[0]

        subsystem_id_map = {0: chip.curator_subsystem,
                            1: chip.host_subsystem,
                            2: chip.bt_subsystem,
                            3: chip.audio_subsystem,
                            4: chip.apps_subsystem}
        
        # These events are unlikely to cover every subsystem so we load what we have
        for subsys in event.subsystem_logs:
            subsystem_id = subsys.header.subsystem_id
            
            subsystem = subsystem_id_map[subsystem_id]
            
            processor = subsys.header.processor
            if len(subsystem.cores) > 1:
                # Apps and audio can have multiple cores. Set the both through dm
                if subsystem_id in [3, 4]:
                    subsystem_mem = subsystem.cores[processor].dm
                else:
                    subsystem_mem = subsystem.trb_in
            else:
                subsystem_mem = subsystem.spi_in

            for sub_log in subsys.sub_logs:
                addr = sub_log.addr
                data = sub_log.data
                subsystem_mem[addr:addr + len(data)] = data
            subsystem.has_data_source = True
            subsystem.cores[processor].has_data_source = True
            # This should really be an attribute of the fw but this isn't constructed yet so save as attribute of core
            subsystem.cores[processor].dump_build_id = subsys.header.firmware_version
            subsystem.cores[processor].dump_build_string = None

class DebugEvent(object):

    def __init__(self, header, xuv_data):
        """
        A debug event contains information from at least 1 subsystem
        """

        self.header = header
        self.data = xuv_data

        self.subsystem_logs = []
        subsys_log_length_accum = 0

        while subsys_log_length_accum < self.header.event_length:
            header_start_addr = subsys_log_length_accum
            header_end_addr = header_start_addr + SubsystemLogHeader.header_length
            subsys_log_header = SubsystemLogHeader(self.data[header_start_addr:header_end_addr])

            start_addr = header_end_addr
            end_addr = start_addr + subsys_log_header.log_length

            subsys_log = SubsystemLog(subsys_log_header, self.data[start_addr:end_addr])
            self.subsystem_logs.append(subsys_log)
            subsys_log_length_accum += (subsys_log_header.log_length + subsys_log_header.header_length)



class DebugEventHeader(object):
    header_length = 8
    def __init__(self, data):
        self.layout_version = data[0]
        self.trigger_type = data[1]
        self.chip_version = build_le(data[2:4], 8)
        self.event_length = build_le(data[4:8], 8)

class SubsystemLog(object):

    def __init__(self, header, data):
        self.header = header
        self.data = data
        self.sub_logs = []

        sub_log_len = 0
        while sub_log_len < len(self.data):
            sub_log = SubsystemSubLog(self.data[sub_log_len:])
            self.sub_logs.append(sub_log)
            sub_log_len += (sub_log.length + sub_log.header_length)


class SubsystemLogHeader(object):
    header_length = 12
    def __init__(self, data):
        self.subsystem_id = data[0]
        self.processor = data[1]
        self.log_version = build_le(data[2:4], 8)
        self.firmware_version = build_le(data[4:8], 8)
        self.log_length = build_le(data[8:12], 8)


class SubsystemSubLog(object):
    header_length = 8
    def __init__(self, data):
        self.addr = build_le(data[0:4], 8)
        self.length = build_le(data[4:8], 8)
        self.data = data[8:8 + self.length]


class DebugPartition(object):

    def __init__(self, xed_data):
        """
        A debug partition comprises some number of debug events
        """
        self.debug_events = []

        event_start_addr = 0
        while True:
            header_end_addr = event_start_addr + DebugEventHeader.header_length
            # Break if we have consumed all data
            if event_start_addr > (len(xed_data) - 1):
                break
            debug_header = DebugEventHeader(xed_data[event_start_addr:header_end_addr])
            
            # Unwritten flash indicates end of events. System only supports flash which erases by setting bits
            # so we can make this assumption.
            if debug_header.layout_version == 2 ** 8 - 1:
                break
            self.debug_events.append(DebugEvent(debug_header, xed_data[header_end_addr:event_start_addr +
                                                                       debug_header.event_length +
                                                                       debug_header.header_length]))
            event_start_addr += (debug_header.event_length + debug_header.header_length)
