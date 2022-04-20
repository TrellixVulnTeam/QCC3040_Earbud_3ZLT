"""
SCons Tool to invoke the Type Definition Generator.
"""

#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#

import python
import os
import SCons.Builder

class RuleGenWarning(SCons.Warnings.Warning):
    pass

class RuleGenNotFound(RuleGenWarning):
    pass

SCons.Warnings.enableWarningClass(RuleGenWarning)

def _detect(env):
    """Try to detect the presence of the rulegen tool."""
    try:
        return env['rulegen']
    except KeyError:
        pass

    rulegen = env.WhereIs('rulegen', env['ADK_TOOLS'] + '/packages/rulegen',
                          pathext='.py')
    if rulegen:
        return rulegen

    raise SCons.Errors.StopError(
        RuleGenNotFound,
        "Could not find Rules Table Generator (rulegen.py)")

# Builder for Rules Table Generator
def _RulesEmitter(target, source, env):
    target_base = os.path.splitext(str(target[0]))[0]
    target = [env.File(target_base + '_rules_table.c'),
              env.File(target_base + '_rules_table.h')]
    return target, source

_RulesBuilder = SCons.Builder.Builder(
        action=[r'$python $rulegen \\?\${SOURCE.abspath} --rule_table_header > \\?\${TARGET.base}.h',
                r'$python $rulegen \\?\${SOURCE.abspath} --rule_table_source > \\?\$TARGET'],
        suffix='.c',
        src_suffix='.rules',
        emitter=_RulesEmitter)

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    python.generate(env)
    env['rulegen'] = _detect(env)
    env['BUILDERS']['RulesObject'] = _RulesBuilder

def exists(env):
    return _detect(env)

