############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.wheels.bitsandbobs import unique_subclass
from csr.dev.model.base_component import BaseComponent
from csr.dev.hw.address_space import AddressSlavePort, NullAccessCache
from .meta.i_io_map_info import FieldValueDict, FieldRefDict, BitfieldValueDict,\
FieldArrayRefDict
from .debug_controller import BasicDebugController
from csr.dev.hw.core.base_core import MemW

class SimpleDataSpace(BaseComponent):

    def __init__(self, name, length=None, layout_info=None, data=None,
                 cache_type=NullAccessCache):
        self.data = (data if data is not None else  
                            AddressSlavePort(name.upper()+"_DATA", length=length, 
                                             cache_type=cache_type,
                                             layout_info=layout_info))
        self.dataw = MemW(self.data, layout_info) if layout_info is not None else None
        self.name = name

        self.mem_ap = self.data # The MEM-AP gives direct access to data space

class SimpleRegisterSpace(SimpleDataSpace):
    """
    Class representing a data space with no associated processor (e.g. a set of
    FPGA registers).  This is a bit like a stripped down BaseCore.  It just supports
    a single flat address space and can load a register map.
    
    At present this exposes a debug_controller and a MEM-AP but no TRB access port.
    """
    def __init__(self, name, length=None, info=None, data=None, cache_type=NullAccessCache):
        
        layout_info = info.layout_info if info is not None else None
        if data is not None:
            SimpleDataSpace.__init__(self, name, data=data, layout_info=layout_info,
                                     cache_type=cache_type)
        else:
            SimpleDataSpace.__init__(self, name, length=length, 
                                     layout_info=layout_info, cache_type=cache_type)
        
        self.info = info
        
    @property
    def fields(self):
        try:
            self._fields
        except AttributeError:
            self._fields = unique_subclass(FieldValueDict)(self.field_refs,
                                                            self.field_array_refs)
        return self._fields 

    @property
    def bitfields(self):
        try:
            self._bitfields
        except AttributeError:
            self._bitfields = unique_subclass(BitfieldValueDict)(self.field_refs,
                                                                  self.field_array_refs)
        return self._bitfields

    @property
    def field_refs(self):
        try:
            self._field_refs
        except AttributeError:
            self._field_refs = FieldRefDict(self.info.io_map_info, 
                                             self) 
        return self._field_refs
    
    @property
    def field_array_refs(self):
        try:
            self._field_array_refs
        except AttributeError:
            self._field_array_refs = FieldArrayRefDict(self.info.io_map_info,
                                                       self)
        return self._field_array_refs

    @property
    def debug_controller(self):
        'Accessor to the ARM DebugController object'
        try:
            self._debug_controller
        except AttributeError:
            self._debug_controller = BasicDebugController(self.data)
        return self._debug_controller.slave

    def load_register_defaults(self):
        if self.info.io_map_info.field_records:
            self.fields.set_defaults()

    @property
    def has_data_source(self):
        try:
            return self._has_data_source
        except AttributeError:
            return False
    
    @has_data_source.setter
    def has_data_source(self, has):
        self._has_data_source = has
