############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2013 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
from .i_io_map_info import BaseIOMapInfo

from ...register_field.meta.noddy_register_field_info import NoddyRegisterFieldInfo

class NoddyIOMapInfo (BaseIOMapInfo):
    """\
    Implementation of IIOMapInfo using rather simplistic db of field tuples.
    """
    RegisterFieldInfoType = NoddyRegisterFieldInfo
    
    def __init__(self, field_records, misc_values, layout_info):
        """\
        Params:-
        - field_records: Dictionary of register field symbol records.            
        
        symbol => (
                start_word,
                start_bit,
                num_bits,
                writeable,
                desciption,
                reset_value
        )
        
        - misc_values:  Dictionary of misc symbols values.
        Includes field enums.
        ExcludesField lsb, msb & mask
        
        symbol => value        
        """
        
        BaseIOMapInfo.__init__(self, misc_values, layout_info)
        self._field_records = field_records
