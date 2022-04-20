############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2018 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
#
############################################################################
"""Constants.

Constant values in ACAT.
"""
import sys
from collections import OrderedDict


COREDUMP_EXTENSION = '.xcd'


# The map between an analysis name and its class name in the Analysis
# package. Add new analysis modules here. The order is important. It will
# be used as a running order in automatic mode.
ANALYSES = OrderedDict((
    # Internal streams (graph) and state debug
    ('stream', 'Stream'),
    ('opmgr', 'Opmgr'),
    ('sanitycheck', 'SanityCheck'),
    ('debuglog', 'DebugLog'),
    ('stackinfo', 'StackInfo'),
    ('profiler', 'Profiler'),
    # memory related
    ('poolinfo', 'PoolInfo'),
    ('heapmem', 'HeapMem'),
    ('sharedmem', 'SharedMem'),
    ('scratchmem', 'ScratchMem'),
    ('heappmmem', 'HeapPmMem'),
    ('dmprofiler', 'DmProfiler'),
    # Specific Kymera modules
    ('sched', 'Sched'),
    ('adaptor', 'Adaptor'),
    ('audio', 'Audio'),
    ('fault', 'Fault'),
    ('patches', 'Patches'),
    ('dorm', 'Dorm'),
    ('sco', 'Sco'),
    ('ipc', 'IPC'),
    # helper analyses:
    ('cbops', 'Cbops'),
    ('buffers', 'Buffers'),
    ('fats', 'Fats'),
    ('aanc', 'AANC')
))

