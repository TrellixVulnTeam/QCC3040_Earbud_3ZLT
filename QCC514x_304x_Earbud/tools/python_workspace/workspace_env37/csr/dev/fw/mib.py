############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
@file
Mib Firmware Component file.

@section Description
Implements Mib class used for all MIB work.
See http://wiki/HydraMIBImplementation for details on the MIB data structures.

@section Usage
Call dump() to dump all MIB contents on the screen.
Access container_intidtoname, container_psidtoname, container_strings,
container_activerecords and container_defaultrecords to access the data
directly. Please not that accessing container_strings, container_activerecords
and container_defaultrecords causes fresh fetches of data from firmware so make
sure you create local copies if performance is an issue. 
Call mib_set() and mib_get() to set or get an integer MIB key.
"""

from csr.wheels import gstrm
from csr.wheels.global_streams import iprint
from csr.dev.fw.firmware_component import FirmwareComponent
from csr.interface import mibdb
from csr.interface.lib_util import vlint_to_decimal, decimal_to_vlint, \
                                   unit_to_l8, l8_to_unit, vldata_to_decimal,\
                                   bmsg_unpack, bmsg_get_data
from csr.wheels.bitsandbobs import create_reverse_lookup, StaticNameSpaceDict
from csr.dev.model import interface
from csr.dev.adaptor.text_adaptor import TextAdaptor
from csr.dev.hw.address_space import NullAccessCache
import sys

try:
    long
except NameError:
    long = int

class Mib(FirmwareComponent, StaticNameSpaceDict):
    """
    MIB object implementation for hydra generic. This is meant to facilitate
    access to all MIB related tools. 
    """
    class MibError(RuntimeError):
        pass

    class MibKey(object):
        """
        MIB key object. This is used to hold the methods and properties of mib
        keys which are members of the Mib class.
        """
        def __init__(self, name, mib):
            self.name = name
            self.psid = mib.container_nametopsid[self.name]
            self.is_int = mib.container_nameisint[self.name]
            self._set = mib.set
            self._get = mib.get

        def set(self, value = None):
            self._set(self.psid, value)

        def get(self, type = None):
            return self._get(self.psid, type)

    def __init__(self, fw_env, core, parent=None):
        FirmwareComponent.__init__(self, fw_env, core, parent)
        self._mibdb = fw_env.build_info.mibdb
        try:
            self.env.cus["mib.c"]
        except KeyError:
            raise FirmwareComponent.NotDetected
        self._expand_keys()

    def _expand_keys(self):
        mibkeys = dict()
        for key_name in self.container_nametopsid.keys():
            mibkeys[key_name] = Mib.MibKey(key_name, self)
        StaticNameSpaceDict.__init__(self, mibkeys)

    def _generate_report_body_elements(self):
        return [self.dump(report=True)]

    def _on_reset(self):
        pass

    def _get_intidtoname(self, psidtoname):
        """
        @brief Returns a dictionary mapping the internal ID to a tuple
        containing the key name and a 0 for int keys or a 1 for string keys.
        
        @param psidtoname
        PSID to name mapping. This can be self.container_psidtoname.
        
        Parse the MIB keys data structure in the ROM to work out what keys
        this firmware build knows about, their type, and how to map from their
        internal MIB ID in this build to the name in the MIB XML defines.
        """
        def _key_id(encoded_num):
            """
            @brief Returns the encoded MIB ID.
            
            @param encoded_num
            Number which encodes the ID. Example: 
            0x0501,    /* string mibid: 2 ## STUBSTR | len 1 */
            - external_id = (0x05 >> 1) = 0x02
            
            Helper function to get the MIB key ID.
            See http://wiki/HydraMIBImplementation#ROM_contents
            """
            return (encoded_num >> 1)
        def _key_is_string(encoded_num):
            """
            @brief Returns the encoded flag which says if the key is a string.
            
            @param encoded_num
            Number which encodes the type. Example: 
            0x0501,    /* string mibid: 2 ## STUBSTR | len 1 */
            - is_a_string = (0x05 & 0x1) = 0x01
            
            Helper function to check if a key is a string or not.
            See http://wiki/HydraMIBImplementation#ROM_contents
            """
            return (encoded_num & 1)
        container_intidtoname = dict()
        #First build up an octet stream container
        start_addr = \
              (self.env.cus["mibrom.c"].localvars["mibkeydb"].address)
        stop_addr = start_addr + \
                 (self.env.cus["mibrom.c"].localvars["mibkeydb"].size)

        mibrom_octets = unit_to_l8(self._core.info.layout_info.addr_unit_bits,
                                         self._core.data[start_addr:stop_addr])
        #Now iterate through it
        i = 0
        while (mibrom_octets[i]):
            length = mibrom_octets[i]
            i = i + 1
            initial_offset = i
            octet = mibrom_octets[i]
            if (octet & 0x80):
                if(octet == 0x81):
                    vlint_value = mibrom_octets[i + 1]
                elif(octet == 0x82):
                    vlint_value = ((mibrom_octets[i + 1] << 8) | 
                                                        (mibrom_octets[i + 2]))
            else:
                vlint_value = mibrom_octets[i]
            id = _key_id(vlint_value)
            is_a_string = _key_is_string(vlint_value)
            i = i + length
            try:
                name = psidtoname[id]
            except KeyError:
                # This can happen if a patch removes the definition of a MIB
                # key. The firmware still has it compiled in so we find it in
                # our search but it's no longer named. We could just skip it
                # entirely but a deprecation hint might be useful as a customer
                # may be setting it and we want to warn them that it's
                # having no effect.
                name = "<deprecated_key_psid_%d>" % id
            container_intidtoname[initial_offset] = (name, is_a_string)
        return container_intidtoname

    def _get_strings(self):
        """
        @brief Returns a dictionary mapping the string ID to a list containing
        the unpacked string as 8 bit integers.
        
        Decodes and dumps the string store.
        """
        container_strings = dict()
        #MIB RAM stuff
        string_store = (self.env.cus["mibram.c"].localvars["string_store"])
        if string_store.value == 0 or string_store.deref["blksiz"].value == 0:
            return dict()
        #Get the bmsg which holds the strings bmsg addresses
        bmsg_dict = bmsg_unpack(string_store.deref["bmsg"].deref)
        ptrlen = self._core.info.layout_info.data_word_bits // 8
        num_strings = bmsg_dict["index"] // ptrlen
        string_addrs = bmsg_get_data(bmsg_dict)
        if len(string_addrs) == 0 or len(string_addrs) != ptrlen * num_strings:
            return None
        for i in range(num_strings):
            string_addr_l8 = string_addrs[i*ptrlen : (i+1)*ptrlen]
            # Figure out how the serialized address should look. Every element 
            # must be one addresable unit in size.
            string_addr_to_deserialise = l8_to_unit(
                    self._core.info.layout_info.addr_unit_bits, string_addr_l8)
            if not string_addr_to_deserialise:
                return None
            string_addr = self._core.info.layout_info.deserialise(
                                                    string_addr_to_deserialise)
            if string_addr == 0:
                continue

            string_bmsg_dict = bmsg_unpack(
                                   self.env.cast(string_addr, "BMSG"))
            string = bmsg_get_data(string_bmsg_dict)
            container_strings[i] = string
        return container_strings

    def _get_records(self, store_name):
        """
        @brief Returns a dictionary mapping the internal ID to an integer 
        representing the actual value for int keys and the string ID for octet
        string keys.
        
        @param store_name
        String holding the name of the store: "active_store" or "default_store"
        
        Traverse the store passed (active or default) and dump the contents.
        """
        def _decode_id_val(rec):
            """
            @brief Returns the encoded ID, value and the unprocessed part of the
            list.
                    
            @param rec
            Stores record as an 8 bit integer list. This usually contains at least
            two vlints, one for the ID and one for the value. It can also cope with
            incomplete records which can happen if firmware runs out of RAM when
            setting a key.
            
            Helper function to decode the ID and value of the RAM contents.
            This is designed to cope with incomplete record entries.
            """
            ret = []
            ret_l = []
            for i in range(2):
                length = 1
                if len(rec) != 0:
                    if rec[0] & 0x80:
                        #Add the VLData length
                        length = length + (rec[0] & 0x1f)
                    if len(rec) >= length:
                        ret.append(vldata_to_decimal(rec[:length]))
                        ret_l = rec[length:]
                    else:
                        ret.append(None)
                else:
                    ret.append(None)
                rec = ret_l
            return ret[0],ret[1],ret_l
        container_records = dict()
        #MIB RAM stuff
        store = self.env.cus["mibram.c"].localvars[store_name]
        if store["recls"].value == 0:
            return dict()
        cur_recl = store["recls"]
        recs = []
        while cur_recl.value != 0:
            vararr = cur_recl.deref["vararr"]
            if vararr.deref["blksiz"].value == 0:
                return None
            bmsg_dict = bmsg_unpack(vararr.deref["bmsg"].deref)
            rec = bmsg_get_data(bmsg_dict)
            recs.append(rec)
            cur_recl = cur_recl.deref["next"]
        for rec in recs:
            while rec:
                id, val, rec = _decode_id_val(rec)
                if id >= 0:
                    container_records[id] = val
        return container_records

    @property
    def container_psidtoname(self):
        try:
            self._container_psidtoname
        except AttributeError:
            self._container_psidtoname = {v.psid():k for k, v in self._mibdb.mib_dict.items()}
        return self._container_psidtoname

    @property
    def container_nametopsid(self):
        try:
            self._container_nametopsid
        except AttributeError:
            self._container_nametopsid = create_reverse_lookup(self.container_psidtoname)
        return self._container_nametopsid

    @property
    def container_nameisint(self):
        try:
            self._container_nameisint
        except AttributeError:
            self._container_nameisint = {k:v.is_integer() for k, v in self._mibdb.mib_dict.items()}
        return self._container_nameisint

    @property
    def container_intidtoname(self):
        try:
            self._container_intidtoname
        except AttributeError:
             # build self._container_intidtoname
            self._container_intidtoname = self._get_intidtoname(
                                                     self.container_psidtoname)
        return self._container_intidtoname

    @property
    def container_strings(self):
        # always build self._container_strings since keys can be set at any 
        # time
        return self._get_strings()

    @property
    def container_activerecords(self):
        # always build self._container_activerecords since keys can be set at
        # any time
        return self._get_records("active_store")

    @property
    def container_defaultrecords(self):
        # always build container_defaultrecords since mibinitialized() can be 
        # called at any time
        return self._get_records("default_store")

    def _report_mib_not_in_ram(self):
        return (False,[])

    def dump(self, report=False):
        """
        @brief Dumps the firmware MIB contents on screen.

        Reads all the MIB keys stored by firmware and their default values.
        Please note that the default store holds the keys set before calling 
        mibinitialised(). The XML defaults are held in ROM and are not dumped
        here (yet).
        
        For curator, this function also tries to report the mibs which
        are modified while are not stored in RAM. The mibs that do not
        have set/get functions or cannot report a default value are not dumped.
        If any new MIBs are added or the default value of the existing MIBs
        are modified, this function may need to be changed.
        """
        def _print_record(record, intidtoname, strings):
            """
            @brief Helper function which returns a table containing record data as
            key names and values.
            
            @param record 
            Record to be printed. This is usually self.container_activerecord or
            self.container_defaultrecord.
            
            @param intidtoname
            Internal ID to name mapping. This can be self.container_intidtoname.
            
            @param strings 
            String ID to string mapping. This is usually self.container_strings.
            
            This function parses the record and either returns the integer value
            or looks for the octet string and then displays it. I also returns 
            "None" for incomplete key entries. 
            """
            table = interface.Table(["Name", "Value"])
            for id, val in record.items():
                if id in intidtoname:
                    name, is_str = intidtoname[id]
                    if val >= 0:
                        if(is_str):
                            val = strings[val]
                    else:
                        val = None
                    table.add_row([name, val])
            return table

        group = interface.Group("MIB Dump")
        # Check for string store problems
        strings = self.container_strings
        if strings == None:
            group.append(interface.Code("ERROR: string store is corrupted"))
        elif strings == dict():
            group.append(interface.Code("String store is empty"))
        
        group.append(interface.Code("Dumping active store:")) 
        activerecords = self.container_activerecords 
        if activerecords == None:
            group.append(interface.Code("ERROR: active store is corrupted"))
        elif activerecords == dict():
            group.append(interface.Code("Active store is empty"))
        else:
            group.append(_print_record(activerecords, 
                                       self.container_intidtoname, strings))

        group.append(interface.Code("Dumping default store:")) 
        defaultrecords = self.container_defaultrecords 
        if defaultrecords == None:
            group.append(interface.Code("ERROR: default store is corrupted"))
        elif defaultrecords == dict():
            group.append(interface.Code("Default store is empty"))
        else:
            group.append(_print_record(defaultrecords, 
                                       self.container_intidtoname, strings))

        # Only dump the mibs that has been modified and not stored in RAM
        # on a live chip. For coredump, we can not do it.
        if issubclass(self._core.data.cache_type, NullAccessCache):
            #Dumping MIBs that have been modified and not stored in RAM
            found, mib_not_in_ram = self._report_mib_not_in_ram()
            if found:
                group.append(interface.Code("\nDumping MIBs that have been modified while "
                                            "not stored in RAM:"))
                group.append(mib_not_in_ram)
                group.append(interface.Code("Note: There are some mibs which might "\
                                            "be modified but are not dumped here.\n" \
                                            "It could be due to no get function, " \
                                            "or the default value are unavailable."))

        if report is True:
            return group
        TextAdaptor(group, gstrm.iout)

    def _get_mib_info(self, mib):
        if isinstance(mib, str):
            psid = self.container_nametopsid.setdefault(mib, None)
        else:
            psid = mib
        name = self.container_psidtoname.setdefault(psid, None)
        is_int = self.container_nameisint.setdefault(name, None)
        if psid == None:
            raise self.MibError("Could not find a PSID for MIB key %s" % mib)
        return psid, name, is_int

    def _validate_set_value(self, value, mib_is_int):
        if mib_is_int == True:
            if ((value != None) and
                (not isinstance(value, (int, long)))):
                raise self.MibError("Attempt to set non-integer value to integer key")
        elif mib_is_int == False:
            if isinstance(value, (int, long)):
                raise self.MibError("Attempt to set integer value to octet string key")

    def _validate_get_type(self, type, mib_is_int):
        if mib_is_int == True:
            if type == str:
                raise self.MibError("Cannot read integer key as string")
            else:
                type = int
        elif mib_is_int == False:
            if type == int:
                raise self.MibError("Cannot read octet string key as int")
        return type

    def set(self, mib, value = None):
        """
        @brief Sets a MIB key in firmware.
        
        @param mib
        Key name or PSID.
        
        @param value
        Value to be set as integer, octet string or string.
        If this is None the MIB key will be cleared.
        """
        psid, name, is_int = self._get_mib_info(mib)
        self._validate_set_value(value, is_int)
        self._setter(psid, value)

    def get(self, mib, type = None):
        """
        @brief Returns a MIB key from firmware.
        
        @param mib
        Key name or PSID.
        
        @param type
        Type of data. This can be int, str or None. Normally None should be
        used but for getting actual strings use str.
        If the key is not set than this function will return None.
        """
        psid, name, is_int = self._get_mib_info(mib)
        type = self._validate_get_type(type, is_int)
        return self._getter(psid, type)

    def _setter(self, psid, value):
        raise NotImplementedError("MIB setter not implemented")

    def _getter(self, psid, type):
        raise NotImplementedError("MIB getter not implemented")

class MibApps(Mib):
    """
    MIB object implementation for Apps. This is meant to facilitate access to
    all MIB related tools. 
    """
    def __init__(self, fw_env, core, parent=None):
        Mib.__init__(self, fw_env, core, parent=parent)
        self._appcmd = self._core.fw.appcmd

    @property
    def _is_mibcmd_ok(self):
        try:
            self.__is_mibcmd_ok
        except:
            try:
                self._core.subsystem.mibcmd
                self.__is_mibcmd_ok = True
            except:
                iprint ("Mibcmd trasnsport failed, reverting to appcmd")
                self.__is_mibcmd_ok = False
        return self.__is_mibcmd_ok

    def _setter(self, psid, value):
        if self._is_mibcmd_ok:
            self._core.subsystem.mibcmd.mib_set(psid, value)
        else:
            self._appcmd_setter_wrapper(psid, value)

    def _getter(self, psid, type):
        if self._is_mibcmd_ok:
            value = self._core.subsystem.mibcmd.mib_get(psid, type)
        else:
            value = self._appcmd_getter_wrapper(psid, type)
        return value

    def _appcmd_setter_wrapper(self, psid, value):
        if value == None:
            raise NotImplementedError("Appcmd does not implement MIB clearing")
        elif isinstance(value, (int, long)):
            vlint = decimal_to_vlint(value)
        else:
            if isinstance(value, str):
                value = map(ord, value)
        if isinstance(value, (int, long)):
            self._appcmd.mib_set_id(psid, vlint)
        else:
            self._appcmd.mib_octet_set_v(self.container_psidtoname[psid], value)

    def _appcmd_getter_wrapper(self, psid, type):
        if type == int:
            value = vlint_to_decimal(self._appcmd.mib_get_id(psid))
        else:
            value = self._appcmd.mib_octet_get_v(self.container_psidtoname[psid])
            if type == str:
                value = "".join(map(chr, value))
        return value
