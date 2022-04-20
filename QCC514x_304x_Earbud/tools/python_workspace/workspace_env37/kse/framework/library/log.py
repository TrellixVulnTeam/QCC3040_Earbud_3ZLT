#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Logging extensions"""

import inspect
import logging
import platform
import sys
import time
from logging.config import dictConfig  # pylint:disable=ungrouped-imports

import wrapt

import kats
from ._log import format_arguments, format_instance_params, format_value
from .config import DEBUG_SESSION
from .util import check_package_editable


def patch_log_formatter():
    """Monkey patch python logging standard module so that the default logging.Formatter format
    is HH:MM:SS.mmm instead of YYYY-MM-DD HH:MM:SS,mmm
    """

    def formatTime(self, record, datefmt=None):  # pylint: disable=invalid-name
        """Return the creation time of the specified LogRecord as formatted text.

        This method should be called from format() by a formatter which
        wants to make use of a formatted time. This method can be overridden
        in formatters to provide for any specific requirement, but the
        basic behaviour is as follows: if datefmt (a string) is specified,
        it is used with time.strftime() to format the creation time of the
        record. Otherwise, the ISO8601 format is used. The resulting
        string is returned. This function uses a user-configurable function
        to convert the creation time to a tuple. By default, time.localtime()
        is used; to change this for a particular formatter instance, set the
        'converter' attribute to a function with the same signature as
        time.localtime() or time.gmtime(). To change it for all formatters,
        for example if you want all logging times to be shown in GMT,
        set the 'converter' attribute in the Formatter class.
        """
        ct = self.converter(record.created)  # pylint: disable=invalid-name
        if datefmt:
            s = time.strftime(datefmt, ct)  # pylint: disable=invalid-name
        else:
            t = time.strftime("%H:%M:%S", ct)  # pylint: disable=invalid-name
            s = "%s.%03d" % (t, record.msecs)  # pylint: disable=invalid-name
        return s

    logging.Formatter.formatTime = formatTime


def log_level_to_int(level):
    """Convert a log level in integer or str format to integer format

    Args:
        level (str/int): Log level

    Returns:
        int: Log level
    """
    try:
        level = int(level)
    except ValueError:
        level = logging.getLevelName(level)
    return level


def setup_logging(config, level):
    """Configure logging module

    Args:
        config (dict or None): Configuration
        level (int): Log level
    """

    patch_log_formatter()
    if config:
        dictConfig(config)
    for logger in logging.Logger.manager.loggerDict:
        for handler in logging.getLogger(logger).handlers:
            handler.setLevel(level)
    log = logging.getLogger()
    log.info('Python %s (%s, %s) [%s] on %s',
             platform.python_version(), platform.python_build()[0], platform.python_build()[1],
             platform.python_compiler(), sys.platform)
    log.info('kats:%s release:%s', kats.__version__, not check_package_editable(kats.__name__))
    log.info('cmdline %s', ' '.join(sys.argv))


def dump(data, pattern='%02x', elem_line=None):
    """Get the string representation of an array dump.

    This dumps an array as

    .. code-block:: python

        a = [1, 2, 127, 32]
        dump(a)
        '01 02 7f 20'

        dump(a, '0x%02x')
        '0x01 0x02 0x7f 0x20'

        a = [121, 312, 127, 232, 323, 234, 121256, 32334, 2321, 2312, 12, 23, 121, 121, 12344]
        print(dump(a, '0x%04x', 8))
        0x0079 0x0138 0x007f 0x00e8 0x0143 0x00ea 0x1d9a8 0x7e4e
        0x0911 0x0908 0x000c 0x0017 0x0079 0x0079 0x3038

    Args:
        data (list[int]): Data list
        pattern (str): Per element conversion from value to string representation
        elem_line (int): If None then ommit any line-feeds in the output, otherwise limit the
            elements per line to this value
    """

    cad = ''
    for ind, entry in enumerate(data):
        if elem_line and ind % elem_line == 0:
            cad += '\n'
        cad += pattern % (entry)
        if ind < len(data) - 1:
            cad += ' '
    return cad


def log_docstring(level=logging.DEBUG):  # @DontTrace
    """Decorator to emit a log message in the logger self._log with the docstring of the method

    Args:
        level (int): Log level of the message
    """

    @wrapt.decorator(enabled=not DEBUG_SESSION)  # @DontTrace
    def wrapper(wrapped, instance, args, kwargs):  # @DontTrace
        """Wrapped function"""
        # pylint: disable=protected-access
        instance._log.log(level, '%s. %s', wrapped.__name__, inspect.getdoc(wrapped))
        return wrapped(*args, **kwargs)

    return wrapper


@wrapt.decorator
def log_exception(wrapped, instance, args, kwargs):  # @DontTrace
    """Logs an exception if any exception is raised in the decorated function

    Args:
        wrapped (function): Wrapped function which needs to be called by your wrapper function.
        instance (any): Object to which the wrapped function was bound when it was called.
        args (tuple[any]): Positional arguments supplied when the decorated function was called.
        kwargs (dict): Keyword arguments supplied when the decorated function was called.
    """
    try:
        return wrapped(*args, **kwargs)
    except Exception as exc:
        # pylint: disable=protected-access
        instance._log.error('error executing %s (%s)', wrapped.__name__, str(exc))
        raise


def log_input(level=logging.DEBUG, formatters=None, ignore=None):  # @DontTrace
    """Decorator to emit a log message in the logger self._log when the function is invoked

    Args:
        level (int): Log level of the message
        formatters (dict): Specific log format for the given parameter.
            The parameter name is the key and its format specifier should be its value.
        ignore (list): Arguments and Keyword Arguments which should be ignored in the log.
    """
    formatters = {} if not formatters else formatters
    ignore = [] if not ignore else ignore

    @wrapt.decorator(enabled=not DEBUG_SESSION)  # @DontTrace
    def wrapper(wrapped, instance, args, kwargs):  # @DontTrace
        """
        Args:
            wrapped (function): Wrapped function which needs to be called by your wrapper function.
            instance (any): Object to which the wrapped function was bound when it was called.
            args (tuple[any]): Positional arguments supplied when the decorated function was called.
            kwargs (dict): Keyword arguments supplied when the decorated function was called.
        """
        arguments = format_arguments(wrapped, args, kwargs, formatters=formatters, ignore=ignore)
        param_str = format_instance_params(instance)

        # pylint: disable=protected-access
        instance._log.log(level, '%s%s %s', param_str, wrapped.__name__, arguments)
        return wrapped(*args, **kwargs)

    return wrapper


def log_output(level=logging.DEBUG, formatters=None, ignore=None):  # @DontTrace
    """Decorator to emit a log message in the logger self._log when the function returns

    Args:
        level (int): Log level of the message
        formatters (dict): Specific log format for the given parameter. The parameter name is the
            key and its format specifier should be its value. To format the return
            value, use `return` as the key.
        ignore (list): Arguments and Keyword Arguments which should be ignored in the log.
    """
    formatters = {} if not formatters else formatters
    ignore = [] if not ignore else ignore

    @wrapt.decorator(enabled=not DEBUG_SESSION)  # @DontTrace
    def wrapper(wrapped, instance, args, kwargs):  # @DontTrace
        """
        Args:
            wrapped (function): Wrapped function which needs to be called by your wrapper function.
            instance (any): Object to which the wrapped function was bound when it was called.
            args (tuple[any]): Positional arguments supplied when the decorated function was called.
            kwargs (dict): Keyword arguments supplied when the decorated function was called.
        """
        ret = wrapped(*args, **kwargs)

        param_str = format_instance_params(instance)
        arg_str = format_arguments(wrapped, args, kwargs, formatters=formatters, ignore=ignore)

        # pylint: disable=protected-access
        instance._log.log(level, '%s%s %s ret:%s', param_str, wrapped.__name__, arg_str,
                          '%s' % (format_value(ret, formatter=formatters.get('return', '%s')))
                          if ret is not None else '')
        return ret

    return wrapper


def log_input_output(in_level=logging.DEBUG, out_level=logging.DEBUG,
                     formatters=None, ignore=None):  # @DontTrace
    """Decorator to emit a log message in the logger self._log when the function is invoked
    and another one when the function returns

    Args:
        in_level (int): Log level of the message when invoked
        out_level (int): Log level of the message when returns
        formatters (dict): Specific log format for the given parameter. The parameter name is the
            key and its format specifier should be its value. To format the return
            value, use `return` as the key.
        ignore (list): Arguments and Keyword Arguments which should be ignored in the log.
    """
    formatters = {} if not formatters else formatters
    ignore = [] if not ignore else ignore

    @wrapt.decorator(enabled=not DEBUG_SESSION)  # @DontTrace
    def wrapper(wrapped, instance, args, kwargs):  # @DontTrace
        """
        Args:
            wrapped (function): Wrapped function which needs to be called by your wrapper function.
            instance (any): Object to which the wrapped function was bound when it was called.
            args (tuple[any]): Positional arguments supplied when the decorated function was called.
            kwargs (dict): Keyword arguments supplied when the decorated function was called.
        """
        param_str = format_instance_params(instance)
        arg_str = format_arguments(wrapped, args, kwargs, formatters=formatters, ignore=ignore)

        # pylint: disable=protected-access
        instance._log.log(in_level, '%s%s %s', param_str, wrapped.__name__, arg_str)

        ret = wrapped(*args, **kwargs)

        param_str = format_instance_params(instance)  # may have changed
        instance._log.log(out_level, '%s%s %s ret:%s', param_str, wrapped.__name__, arg_str,
                          '%s' % (format_value(ret, formatter=formatters.get('return', '%s')))
                          if ret is not None else '')
        return ret

    return wrapper
