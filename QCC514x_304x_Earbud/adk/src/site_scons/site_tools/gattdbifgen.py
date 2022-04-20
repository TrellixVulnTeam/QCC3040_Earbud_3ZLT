"""
SCons Tool to invoke the GATT DB Interface Generator.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import os
import SCons.Builder
import SCons.Scanner
from SCons.Defaults import Move

import kas
import python
import gattdbgen

class ToolGattDBIFGenWarning(SCons.Warnings.Warning):
    pass

class GattDBIFGenNotFound(ToolGattDBIFGenWarning):
    pass

SCons.Warnings.enableWarningClass(ToolGattDBIFGenWarning)

def _detect(env):
    """Try to detect the presence of the gattdbifgen tool."""
    try:
        return env['gattdbifgen']
    except KeyError:
        pass

    gattdbifgen = env.WhereIs('dbgen_interface_generator.py',
                              env['ADK_TOOLS'] + '/packages/gattdbifgen',
                              pathext='.py')
    if gattdbifgen:
        return gattdbifgen

    raise SCons.Errors.StopError(
        GattDBIFGenNotFound,
        "Could not find GATT DB Interface Generator "
        "(dbgen_interface_generator.py)")

# Builder for GATT DB Interface Generator
def _sortSources(target, source, env):
    """Where the source is a list, re-ordering can result in rebuilds
       even though the content hasn't changed. So sorting the sources
       list can prevent unnecessary rebuilding. Where the order isn't
       significant, this emitter can hide this from the tool user."""
    source.sort()
    return target, source

_dbxBuilder = SCons.Builder.Builder(
        action=['$cpredbif'],
        suffix='.dbx',
        src_suffix='.dbi',
        source_scanner=SCons.Scanner.C.CScanner())

_dbyBuilder = SCons.Builder.Builder(
        action=['$gattdbgen -i $SOURCE',
                Move('$TARGET', '${SOURCE.base}.h')],
        suffix='.dby',
        src_suffix='.dbx',
        source_scanner=SCons.Scanner.C.CScanner(),
        src_builder=_dbxBuilder)

_gattdbifgenHBuilder = SCons.Builder.Builder(
        action=[r'$python $gattdbifgen --header \\?\$TARGET $( ${_concat("\\\\?\\", SOURCES, "", __env__)} $)'],
        suffix='_if.h',
        src_suffix='.dby',
        emitter=_sortSources,
        src_builder=_dbyBuilder)

_gattdbifgenBuilder = SCons.Builder.Builder(
        action=[r'$python $gattdbifgen --source \\?\$TARGET \\?\$SOURCE'],
        suffix='_if.c',
        src_suffix='.h')

def generate(env):
    """Add Builders and construction variables to the Environment.
    """
    kas.generate(env)
    python.generate(env)
    gattdbgen.generate(env)

    env['gattdbifgen'] = _detect(env)
    env['cpredbif'] = '$cpre $CPPFLAGS -DGATT_DBI_LIB $_CPPDEFFLAGS $_CPPINCFLAGS $SOURCE -o ${TARGET.base}.dbx'
    env['BUILDERS']['DbxObject'] = _dbxBuilder
    env['BUILDERS']['DbyObject'] = _dbyBuilder
    env['BUILDERS']['DbIfHObject'] = _gattdbifgenHBuilder
    env['BUILDERS']['DbIfObject'] = _gattdbifgenBuilder

def exists(env):
    return _detect(env)
