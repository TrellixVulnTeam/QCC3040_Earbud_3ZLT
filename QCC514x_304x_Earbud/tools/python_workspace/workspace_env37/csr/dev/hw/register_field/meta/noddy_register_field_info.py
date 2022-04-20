############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .i_register_field_info import BaseRegisterFieldInfo
 
class NoddyRegisterFieldInfo (BaseRegisterFieldInfo):
    """\
    Implementation of IRegisterFieldInfo by reference to pre-processed field
    meta-data held in dictionary of python tuples. (see databases in ../hw/io)
    
    Note: in this context "word" means "data word", not "addressable unit".
    """
    

    def __init__(self, field_sym, field_records, misc_values, layout_info):
        """\
        Construct _RegisterFieldInfo.
        
        Params:-
        - field_sym: Field symbol => tuple (see NoddyRegisterMapInfo)
        - misc_values: Misc symbol => value map
        """
        BaseRegisterFieldInfo.__init__(self, field_sym, misc_values, layout_info)
        self._record = field_records[field_sym]        
        
    # IRegisterFieldInfo compliance
    
    @property
    def description(self):
        
        # Lookup
        return self._record[self._Index.DESCRIPTION]

    @property
    def parent(self):
        
        raise NotImplementedError("Field Parent Not implemented in DB")
        
    @property
    def children(self):
        
        return False
        
    @property
    def start_addr(self):
        
        # Lookup
        return self._record[self._Index.START_WORD]
    
    @property
    def start_bit(self):
        
        # Lookup
        return self._record[self._Index.START_BIT]
    
    @property
    def num_bits(self):
        
        # Lookup
        return self._record[self._Index.NUM_BITS]
    
    @property
    def is_writeable(self):
        
        # Lookup
        return self._record[self._Index.IS_WRITEABLE]
        
    @property
    def reset_value(self):
        
        # Lookup
        return self._record[self._Index.RESET_VALUE]

    @property
    def enum_infos(self):
        
        raise NotImplementedError("Enums Not implemented in DB")

    # Private
        
    class _Index:
        """\
        Register Field Record Field Index Enumeration
        """
        START_WORD = 0
        START_BIT = 1
        NUM_BITS = 2
        IS_WRITEABLE = 3
        DESCRIPTION = 4
        RESET_VALUE = 5
    
