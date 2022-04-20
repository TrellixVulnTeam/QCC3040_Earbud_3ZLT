"""
SCons Tool to invoke the Chain Generator.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import python
import os
import SCons.Builder

class ToolChainGenWarning(SCons.Warnings.Warning):
    pass

class ChainGenNotFound(ToolChainGenWarning):
    pass

SCons.Warnings.enableWarningClass(ToolChainGenWarning)

def _detect(env):
    """Try to detect the presence of the chaingen tool."""
    try:
        return env['chaingen']
    except KeyError:
        pass

    chaingen = env.WhereIs('chaingen', env['ADK_TOOLS'] + '/packages/chaingen',
                           pathext='.py')
    if chaingen:
        return chaingen

    raise SCons.Errors.StopError(
        ChainGenNotFound,
        "Could not find Chain Generator (chaingen.py)")

# Builder for Chain Generator
def _ChainEmitter(target, source, env):
    target_base = os.path.splitext(str(target[0]))[0]
    target.append(target_base + '.h')
    return target, source

_ChainBuilder = SCons.Builder.Builder(
        action=[r'$python $chaingen \\?\$SOURCE --header > \\?\${TARGET.base}.h',
                r'$python $chaingen \\?\$SOURCE --source > \\?\$TARGET'],
        suffix='.c',
        src_suffix='.chain',
        emitter=_ChainEmitter)

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    python.generate(env)
    env['chaingen'] = _detect(env)
    env['BUILDERS']['ChainObject'] = _ChainBuilder

def exists(env):
    return _detect(env)

