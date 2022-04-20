############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from collections import UserDict
from .structs import IAdkStructHandler, DeviceFlags, DeviceProfiles

class Device(FirmwareComponent, UserDict):
    ''' This class decodes the key_value_list_t type used to store a single
        device's key-value pairs. It can be used in a similar way to a standard
        python dict. It adds attributes named after the key for each of the key
        value pairs found for the device.

        There are three types of key_value_list in existence:
        1. The current version stores keys in one array and values in size
           specific arrays.
        2. The previous version stores key/values in either linked list
           implementation or dynamic reallocated list.
        3. The legacy type supported only a linked list.

        This decoder supports decoding the three variants by inspecting the
        structure members.
    '''
    def __init__(self, env, core, key_value_list, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        UserDict.__init__(self)

        # Determine the type of key value list and call the relevant decoder
        with key_value_list.footprint_prefetched():
            if hasattr(key_value_list, 'len_keys'):
                kv_dict = self._decode_key_values_multi_array_key_value_list(key_value_list)
            else:
                kv_dict = self._decode_key_values_data_legacy_key_value_list(key_value_list)

        # Add the decoded key/values into the user dict
        for k, v in kv_dict.items():
            decoded_key, decoded_value = self._decode_key_and_value(k, v)
            self.data[decoded_key] = decoded_value

        # Add each key as a class property for simple access, e.g. Device.bdaddr
        for key, value in self.data.items():
            if hasattr(self, key):
                raise RuntimeError("Attribute {} was unexpectedly present in object".format(key))
            setattr(self, key, value)

    def _decode_key_values_multi_array_key_value_list(self, key_value_list):
        ''' Decoder for key_value_list which stores keys in one array and
            different sized values in seperate arrays '''

        # All keys are stored in a single array
        keys_len = key_value_list.len_keys.value
        keys = self.env.cast(key_value_list.keys, 'uint16', array_len=keys_len)

        # Values <=32 bits are stored in seperate size-specific arrays
        len8 = key_value_list.len8.value
        values8 = self.env.cast(key_value_list.values8, "uint8", array_len=len8)
        len16 = key_value_list.len16.value
        values16 = self.env.cast(key_value_list.values16, "uint16", array_len=len16)
        len32 = key_value_list.len32.value
        values32 = self.env.cast(key_value_list.values32, "uint32", array_len=len32)

        key_index = 0
        kv_dict = {}
        for v8 in values8:
            key = keys[key_index].value
            kv_dict[key] = v8
            key_index += 1
        for v16 in values16:
            key = keys[key_index].value
            kv_dict[key] = v16
            key_index += 1
        for v32 in values32:
            key = keys[key_index].value
            kv_dict[key] = v32
            key_index += 1

        # Large values >32 bits are stored in a linked link
        for ele in key_value_list.head:
            data = self.env.cast(ele.data, "uint8", array_len=ele.len.value)
            kv_dict[ele.key.value] = data

        return kv_dict

    def _decode_key_values_data_legacy_key_value_list(self, key_value_list):
        ''' Decoder for key_value_list which stores key-values pairs in a linked
            list or variable length array '''

        def _decode_kvp(key_value_pair_struct):
            ''' Decode and return the key and value from the structure.
                key_value_pair_struct is an instance of the struct
                key_value_pair_tag '''

            # Small keys are stored within the u32 member
            KEY_VALUE_TYPE_SMALL = 1
            # Large keys have a pointer to the another object that stores the value
            KEY_VALUE_TYPE_LARGE = 2

            flags = key_value_pair_struct.flags.value
            size = key_value_pair_struct.size.value
            key = key_value_pair_struct.key.value

            if flags == KEY_VALUE_TYPE_SMALL:
                u32 = key_value_pair_struct.value.u32
                if size == 4:
                    value = u32
                elif size == 2:
                    value = self.env.cast(u32, 'uint16')
                elif size == 1:
                    value = self.env.cast(u32, 'uint8')
                else:
                    raise ValueError("Failed to decode KEY_VALUE_TYPE_SMALL with size={}".format(size))

            elif flags == KEY_VALUE_TYPE_LARGE:
                value_pointer = key_value_pair_struct.value.ptr
                value = self.env.cast(value_pointer, 'uint8', array_len=size)

            return key, value

        # Determine the type of the key_value_list
        is_linked = False
        is_legacy = False
        try:
            is_linked = (key_value_list.is_linked.value == 1)
        except AttributeError:
            is_legacy = True

        kv_dict = {}

        # Iterate through the key/values for the device storing in the kv dict
        if is_legacy:
            for pair in key_value_list.head:
                key, value = _decode_kvp(pair)
                kv_dict[key] = value

        elif is_linked:
            for pair in key_value_list.list.head:
                key, value = _decode_kvp(pair)
                kv_dict[key] = value

        else:
            for index in range(key_value_list.list.dynamic.len.value):
                pair = key_value_list.list.dynamic.kvps[index]
                key, value = _decode_kvp(pair)
                kv_dict[key] = value
        return kv_dict

    @staticmethod
    def short_key_name(full_name):
        ''' Strip header from key names to make them shorter and easier to access '''
        prefix = "device_property_"
        return full_name.split(prefix, 1)[1] if full_name.startswith(prefix) else full_name

    def _decode_key_and_value(self, key, value):
        ''' Decode and return the key and its value. '''
        def _decode_generic(value):
            return value.value

        def _decode_bdaddr(value):
            bdaddr = self.env.cast(value, 'bdaddr')
            return IAdkStructHandler.handler_factory("bdaddr")(self._core, bdaddr)

        def _decode_type(value):
            return self.env.cast(value, 'deviceType')

        def _decode_flags(value):
            flags = self.env.cast(value, 'uint16')
            return DeviceFlags(self._core, flags)

        def _decode_av_instance(value):
            return self.env.cast(value.value, "avInstanceTaskData")

        def _decode_hfp_instance(value):
            return self.env.cast(value.value, "hfpInstanceTaskData")

        def _decode_profiles(value):
            return DeviceProfiles(self._core, value)

        def _decode_link_mode(value):
            return self.env.cast(value, 'deviceLinkMode')

        def _decode_voice_source(value):
            return self.env.cast(value, 'voice_source_t')

        def _decode_audio_source(value):
            return self.env.cast(value, 'audio_source_t')

        def _decode_handset_service_config(value):
            return self.env.cast(value, 'handset_service_config_t')

        def _decode_utc(value):
            return self.env.cast(value, 'uint8')

        def _decode_ui_user_config_gesture_table(value):
            output_as_string = ""
            for i in range(len(value)):
                if (i%4 == 0):
                    output = self.env.cast(value[i:], 'ui_user_gesture_table_content_t')
                    output_as_string += "%s" % output
            return output_as_string

        # Map short key names to custom decoders.
        key_map = {
            "bdaddr" : _decode_bdaddr,
            "type"   : _decode_type,
            "flags"  : _decode_flags,
            "av_instance" : _decode_av_instance,
            "hfp_instance" : _decode_hfp_instance,
            "connected_profiles" : _decode_profiles,
            "last_connected_profiles" : _decode_profiles,
            "supported_profiles" : _decode_profiles,
            "link_mode" : _decode_link_mode,
            "audio_source" : _decode_audio_source,
            "voice_source" : _decode_voice_source,
            "headset_service_config" : _decode_handset_service_config,
            "ui_user_gesture_table" : _decode_ui_user_config_gesture_table,
            "upgrade_transport_connected" : _decode_utc,
        }

        key_names = self.env.enums["earbud_device_property_t"]
        key_short = self.short_key_name(key_names[key])
        try:
            value_decoder = key_map[key_short]
        except KeyError:
            value_decoder = _decode_generic
        value_decoded = value_decoder(value)
        return key_short, value_decoded

    def _generate_report_body_elements(self):
        ''' Report the device's list of key value pairs in a table '''
        grp = interface.Group("Device")
        tbl = interface.Table(["Key", "Value"])
        for k, v in sorted(self.data.items()):
            tbl.add_row([k, v])
        grp.append(tbl)
        return [grp]


class DeviceList(FirmwareComponent):
    ''' Class storing the list of devices from ADK application device
        list. The list of devices can be accessed through the "devices" property.
    '''

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

    def _read_device_list(self):
        ''' Read the list of devices from the chip '''
        try:
            device_list_pointer = self.env.vars["device_list"]
            maxlen = self.env.vars['trusted_device_list'].value
        except KeyError:
            raise self.NotDetected

        device_list = []

        if device_list_pointer != 0:
            # The device list is an array of pointers to device_t objects. Some
            # of the pointers may be NULL.
            raw_device_list = self.env.cast(device_list_pointer, 'device_t', array_len = maxlen)

            for device in raw_device_list:
                if device.value != 0:
                    # Find the key_value_list_t in the device_t
                    device_obj = device.deref
                    if hasattr(device_obj, "properties"):
                        key_value_list = device_obj.properties.deref
                    else:
                        key_value_list = device_obj
                    device = Device(self.env, self._core, key_value_list, parent=self)
                    device_list.append(device)

        return device_list


    def _generate_report_body_elements(self):
        ''' Generates a summary report for each device.
            Call report() on an individual device for a full list of properties.
        '''
        grp = interface.Group("Summary")

        keys_in_summary = ['bdaddr', 'type', 'mru', 'flags', 'connected_profiles']
        tbl = interface.Table(keys_in_summary)

        for device in self.devices:
            values_in_summary = []
            for k in keys_in_summary:
                # Handle absent keys
                try:
                    value = device[k]
                except KeyError:
                    value = None
                values_in_summary.append(value)
            tbl.add_row(values_in_summary)

        grp.append(tbl)
        return [grp]

    @property
    def devices(self):
        ''' Returns a list of all devices '''
        return self._read_device_list()

    @property
    def earbuds(self):
        ''' Returns a filtered list of earbud devices '''
        earbud_types = [self.env.econst.DEVICE_TYPE_EARBUD, self.env.econst.DEVICE_TYPE_SELF]
        earbud_filter = lambda device : (device.type.value in earbud_types)
        return list(filter(earbud_filter, self.devices))

    @property
    def handsets(self):
        ''' Returns a filtered list of handset devices '''
        handset_filter = lambda device: (device.type.value == self.env.econst.DEVICE_TYPE_HANDSET)
        return list(filter(handset_filter, self.devices))

