############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2019 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

# This file is only to be imported by Py2.X implementations. Raise ImportError is otherwise

import sys
if sys.version_info >= (3,):
    raise ImportError("Should only be importing this module if running "
                      "as a Py2.X implementation")

def _exec_cmd(cmd, globals_dict, locals_dict=None):
    # Wrapper function for exec which uses the Py2 statement syntax.
    # Required to avoid syntax error when using exec in a function that has a subfunction.
    if locals_dict is not None:
        exec cmd in globals_dict, locals_dict
    else:
        exec cmd in globals_dict
    