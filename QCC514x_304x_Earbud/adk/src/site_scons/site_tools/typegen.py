"""
SCons Tool to invoke the Type Definition Generator.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import python
import os
import SCons.Builder

class ToolTypeGenWarning(SCons.Warnings.Warning):
    pass

class TypeGenNotFound(ToolTypeGenWarning):
    pass

SCons.Warnings.enableWarningClass(ToolTypeGenWarning)

def _detect(env):
    """Try to detect the presence of the typegen tool."""
    try:
        return env['typegen']
    except KeyError:
        pass

    typegen = env.WhereIs('typegen', env['ADK_TOOLS'] + '/packages/typegen',
                          pathext='.py')
    if typegen:
        return typegen

    raise SCons.Errors.StopError(
        TypeGenNotFound,
        "Could not find Type Definition Generator (typegen.py)")

# Builder for Type Definition Generator
def _TypeDefEmitter(target, source, env):
    target_base = os.path.splitext(str(target[0]))[0]
    target = [env.File(target_base + '_marshal_typedef.c'),
              env.File(target_base + '_marshal_typedef.h'),
              env.File(target_base + '_typedef.h')]
    return target, source

_TypeDefBuilder = SCons.Builder.Builder(
        action=[r'$python $typegen \\?\${SOURCE.abspath} --marshal_header > \\?\${TARGET.base}.h',
                r'$python $typegen \\?\${SOURCE.abspath} --marshal_source > \\?\$TARGET',
                r'$python $typegen \\?\${SOURCE.abspath} --typedef_header > \\?\${TARGET.dir}\${SOURCE.filebase}_typedef.h'],
        suffix='.c',
        src_suffix='.typedef',
        emitter=_TypeDefEmitter)

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    python.generate(env)
    env['typegen'] = _detect(env)
    env['BUILDERS']['TypeDefObject'] = _TypeDefBuilder

def exists(env):
    return _detect(env)

