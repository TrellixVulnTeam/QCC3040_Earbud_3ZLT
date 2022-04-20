"""
SCons Tool to invoke the Button Parser.
"""

#
# Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
#

import python
import os
import SCons.Builder

class ToolButtonXmlWarning(SCons.Warnings.Warning):
    pass

class ButtonXmlNotFound(ToolButtonXmlWarning):
    pass

SCons.Warnings.enableWarningClass(ToolButtonXmlWarning)

def _detect(env):
    """Try to detect the presence of the buttonparsexml tool."""
    try:
        return env['buttonparsexml']
    except KeyError:
        pass

    buttonparsexml = env.WhereIs('buttonparsexml',
                                 env['ADK_TOOLS'] + '/packages/buttonparsexml',
                                 pathext='.py')
    if buttonparsexml:
        return buttonparsexml

    raise SCons.Errors.StopError(
        ButtonXmlNotFound,
        "Could not find Button Parser (buttonparsexml.py)")

# Builder for Button Parser
def _ButtonXmlEmitter(target, source, env):
    target_base = os.path.splitext(str(target[0]))[0]
    target.append(target_base + '.h')
    return target, source

_ButtonXmlBuilder = SCons.Builder.Builder(
        action=[r'$python $buttonparsexml --header > \\?\${TARGET.base}.h --xsd $xmlSchemaFile --msg_xml \\?\${SOURCES[0]} --pio_xml \\?\${SOURCES[1].abspath}',
                r'$python $buttonparsexml --source --pio_bank $NUM_PIO_BANKS > \\?\$TARGET --xsd $xmlSchemaFile --msg_xml \\?\${SOURCES[0]} --pio_xml \\?\${SOURCES[1].abspath}'],
        suffix='.c',
        src_suffix='.buttonxml',
        emitter=_ButtonXmlEmitter)

def generate(env):
    """Add Builders and construction variables to the Environment.
    """

    python.generate(env)
    env['buttonparsexml'] = _detect(env)
    env['xmlSchemaFile']  = env['buttonparsexml'].replace('.py', '.xsd')
    env['BUILDERS']['ButtonXmlObject'] = _ButtonXmlBuilder

def exists(env):
    return _detect(env)
