############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
General purpose python utilities you thought you wouldn't have to re-invent but 
were wrong about! In no way CSR specific.
"""

# Import main objects so can be accessed as wheels.XXX
#
from .bitsandbobs import *
# Alias for the global streams
from . import global_streams as gstrm
from .global_streams import dprint, iprint, wprint, eprint
from .index_or_slice import *
