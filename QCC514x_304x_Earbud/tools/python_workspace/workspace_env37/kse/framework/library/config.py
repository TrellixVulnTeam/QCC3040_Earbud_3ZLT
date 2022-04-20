#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Configuration options"""

import ast


def check_debug_session():
    """Check if we are running in a pydev debug session

    Returns:
        bool: pydev session executing
    """
    try:
        # pylint: disable=unused-variable,import-error,unused-import
        import pydevd  # @UnresolvedImport @UnusedImport
        return True
    except ImportError:
        return False


#: constant indicating we are running inside a pydev debug session
DEBUG_SESSION = check_debug_session()

_LOCAL_CONFIG = {}


def set_config_param(param, value):
    """Set configuration parameter

    Args:
        param (str): Parameter name
        value (any): Parameter value
    """
    _LOCAL_CONFIG[param] = value


def get_config_param(param=None, default_value=None):
    """Get configuration parameter

    Args:
        param (str or None): Parameter name or None for all
        default_value (any): Default value if parameter does not exist

    Returns:
        any: Parameter value or all parameters value
    """
    if param:
        return _LOCAL_CONFIG.get(param, default_value)
    return _LOCAL_CONFIG


def parse_string_config_param(parameters):
    """Parse string with command line parameters in the form

    Example::

        >>> parse_string_config_param(["param1:1", "param2:True", "param3:value3"])
        {'param1': 1, 'param2': True, 'param3': 'value3'}

    Args:
        parameters (list[str]): Parameter string

    Returns:
        dict: Parsed parameters
    """
    config = {}
    for entry in parameters:
        params = entry.split(':', 1)
        if len(params) != 2:
            raise RuntimeError('param %s invalid' % (params))
        try:
            config[params[0]] = ast.literal_eval(params[1])
        except Exception:  # pylint:disable=broad-except
            config[params[0]] = params[1]
    return config
