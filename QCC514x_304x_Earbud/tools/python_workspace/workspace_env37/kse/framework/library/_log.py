#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Logging helper functions"""

import inspect

PARAM_MAX_SIZE = 200
ELLIPSIS = '...'


def format_name(name, value):
    """Get representation of an object name

    Args:
        name (any): Any object name
        value (any): Any object value

    Returns:
        str: Object name representation
    """
    if isinstance(value, (list, tuple)):
        return '%s[%s]' % (name, len(value))
    return str(name)


def format_value(value, max_length=PARAM_MAX_SIZE, formatter='%s'):
    """Get representation of an object value

    Obtain the string representation of an object trimming max length

    Args:
        value (any): Any object value
        max_length (int): Maximum representation string length
        formatter (str): Format specifier for the given `param`.
            If the param is a tuple or a list this format specifier will be applied to all of
            their members. When the format specifier fails to apply, e.g. trying to allow
            decimal point for a string, the whole param will be converted to the string
            regardless of the given format specifier.

    Returns:
        str: Object value representation
    """
    try:
        if isinstance(value, (list, tuple)):
            # apply format to all the members of the given list/tuple.
            ret = '[' + ', '.join([formatter % item for item in value]) + ']'

        else:
            ret = formatter % value

    except TypeError:
        ret = str(value)

    if len(ret) > max_length:
        ret = ret[:max_length] + ELLIPSIS
    return ret


def format_instance_params(instance):
    """Get representation of all parameters of an instance
    Those are stored as a dictionary _log_param as instance property

    Args:
        instance (any): Any object

    Returns:
        str: Instance parameter representation
    """
    ret = ''
    # pylint: disable=protected-access
    if hasattr(instance, '_log_param') and instance._log_param:
        ret += '['
        for arg_name in instance._log_param:
            arg_val = instance._log_param[arg_name]
            ret += format_name(arg_name, arg_val) + ':' + format_value(arg_val) + ' '
        ret = ret[:-1] + '] '
    return ret


def format_arguments(method, args, kwargs, **kw):
    """Get string representation of all parameters invoked in a method call

    Args:
        args (tuple[any]): Positional arguments supplied when the method was called.
        kwargs (dict): Keyword arguments supplied when method was called.
        formatters (dict): Specific log format for every  parameter.
            The parameter name is the key and its format specifier should be its value.
            For parameters in the method as *args used args key
        ignore (list): Arguments and keyword arguments which should be ignored in the log.
        include_default (bool): Include positional parameters and keyword arguments not included
            in args/kwargs

    Returns:
        str: Method call parameter representation
    """
    formatters = kw.pop('formatters', {})
    ignore = kw.pop('ignore', [])
    include_default = kw.pop('include_default', False)

    args_spec = inspect.getfullargspec(method)  # pylint: disable=no-member,deprecated-method
    arg_names = args_spec.args[1:]  # skip self

    arg_vals = {}
    for ind, entry in enumerate(arg_names):
        if entry in kwargs:
            arg_vals[entry] = kwargs[entry]
        else:
            if ind < len(args):
                arg_vals[entry] = args[ind]
            # positional parameter not passed but with default value
            elif include_default and args_spec.defaults:
                if len(arg_names) - ind <= len(args_spec.defaults):
                    arg_vals[entry] = args_spec.defaults[
                        len(args_spec.defaults) - len(arg_names) + ind]

    ret = ' '.join(
        '{k}:{v}'.format(
            k=format_name(k, v),
            v=format_value(v, formatter=formatters.get(k, '%s'))
        )
        for k, v in arg_vals.items()
        if k not in ignore
    )

    # positional parameters passed as *args in signature
    if len(args) > len(arg_vals) and 'args' not in ignore:
        ret += ' args:' + format_value(
            args[len(arg_vals):], formatter=formatters.get('args', '%s'))

    kwarg_vals = {}
    for key in kwargs:
        kwarg_vals[key] = kwargs[key]

    # keyword arguments not passed but exist in signature (and hence with default values)
    if include_default and args_spec.kwonlydefaults:
        for key in args_spec.kwonlydefaults:
            kwarg_vals.setdefault(key, args_spec.kwonlydefaults[key])

    ret += ' ' + ' '.join(
        '{k}:{v}'.format(
            k=format_name(k, v),
            v=format_value(v, formatter=formatters.get(k, '%s'))
        )
        for k, v in kwarg_vals.items()
        if k not in arg_vals and k not in ignore
    )

    return ret
