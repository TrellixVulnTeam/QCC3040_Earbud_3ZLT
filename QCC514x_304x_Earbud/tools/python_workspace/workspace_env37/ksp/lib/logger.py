#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Logger related functions and classes.

Configuring the logger and provide decorators for logging methods and
functions.

Decorators for functions and methods require `logger` as parameter,
this helps the user to relate log lines with their corresponding
packages/logger. Individual packages need to define their own loggers and
pass it to the suitable decorator.

See the example below for hypothetical package called `ksp.lib.foo.py`:

    # foo.py
    import logging

    from ksp.lib.logger import function_logger

    logger = logging.getLogger(__name__)


    @function_logger(logger)
    def say_hi(name):
        return 'Hi %s' % name

Above code will generate log lines for `name` parameter and record the result
for `ksp.lib.foo.py` (or whatever the full package name is). The usage for
`method_logger` decorator is exactly the same but should be used against
methods only, as they have extra `self` parameter.

Function and method loggers also track inner calls and show the inheritance
of those calls. For instance, if function `foo` calls function `bar`,
the logger shows something similar to "foo -> bar CALLED WITH (...)".
"""
import functools
import json
import numbers
import os
import traceback

from logging import Logger
from logging.config import dictConfig

from ksp.lib.exceptions import ConfigurationError

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
LOG_LOCATION = os.path.join(
    CURRENT_DIRECTORY, '..', 'logs'
)
LOG_CONFIGURATIONS = os.path.join(
    CURRENT_DIRECTORY, '..', 'config', 'logging.json'
)
LOG_CONFIGURATIONS_VERBOSE = os.path.join(
    CURRENT_DIRECTORY, '..', 'config', 'logging_verbose.json'
)


def config_logger(verbose=False):
    """Configure the logger using the configuration json file.

    Args:
        verbose (bool, optional): If logger should run in verbose mode,
            the verbose configuration file will be used. This means log
            lines are going to be more expressive and more lines on the
            console.
    """
    if verbose:
        log_configuration = LOG_CONFIGURATIONS_VERBOSE
        # Sanity checks
        if not os.path.exists(LOG_LOCATION):
            os.makedirs(LOG_LOCATION)

    else:
        log_configuration = LOG_CONFIGURATIONS

    if not os.path.exists(log_configuration):
        raise ConfigurationError(
            "Log Configuration file <{}> doesn't exist".format(
                log_configuration
            )
        )

    # Start the configuration
    with open(log_configuration) as handler:
        try:
            config = json.load(handler)

        except ValueError as error:
            raise ConfigurationError(
                "Invalid JSON file. Error: {}".format(error)
            )

    # Make the logging file names absolute
    for handler_name, handler_attrs in config.get('handlers', {}).items():
        if handler_attrs.get('filename') is not None:
            abs_path = os.path.join(
                LOG_LOCATION,
                handler_attrs.get('filename')
            )
            config['handlers'][handler_name]['filename'] = abs_path

    dictConfig(config)


def function_logger(logger):
    """A decorator wraps around functions and record parameters and results.

    If the function raises exception, this will be record it as well
    and re-raise the exception.

    Args:
        logger: A logger instance.

    Returns:
        A wrapped function.
    """
    def decorator(func):
        """Decorates a function."""
        # The wrapper function expects all sorts of arguments and keyword
        # arguments. Suppress the Pylint to complain about this.
        # pylint: disable=bad-option-value
        @functools.wraps(func)
        def wrapper(*args, **kwargs):
            """Wraps around the called function with the exact parameters."""
            call_format_str = _get_call_format()
            logger.debug(call_format_str.format(
                _gen_call_repr(func, *args, **kwargs)
            ))

            # Call the requested function with the same arguments.
            result = func(*args, **kwargs)
            logger.debug(call_format_str.format(
                '{} RETURNED: {}'.format(
                    func.__name__, str(_convert_to_hex(result))
                ),
            ))
            return result
        return wrapper
        # pylint: enable=bad-option-value

    return decorator


def method_logger(logger):
    """A Decorator wraps around methods and record parameters and results.

    If the method raises exception, it will record it as well and re-raise
    the exception. Also, it skips the first argument of all the wrapped
    methods, since that argument is reserved for `cls` and `self`.

    Args:
        logger: A logger instance.

    Returns:
        Wrapped method.
    """
    def decorator(method):
        """Decorates a method."""
        # The wrapper function expects all sorts of arguments and keyword
        # arguments. Suppress the Pylint to complain about this.
        # pylint: disable=bad-option-value
        @functools.wraps(method)
        def wrapper(*args, **kwargs):
            """Wraps around the called method with the exact parameters."""
            call_format_str = _get_call_format()
            logger.debug(call_format_str.format(
                _gen_call_repr(method, *args[1:], **kwargs)
            ))

            # Call the requested method with the same arguments.
            result = method(*args, **kwargs)
            logger.debug(call_format_str.format(
                '{} RETURNED: {}'.format(
                    method.__name__, str(_convert_to_hex(result))
                ),
            ))
            return result
        return wrapper
        # pylint: enable=bad-option-value

    return decorator


def _get_call_format():
    """Generates the call hierarchy format template."""
    current_calls = []
    invalid_calls = ('wrapper', 'main', '<module>', '_get_call_format')
    for call in [item[2] for item in traceback.extract_stack()]:
        if call not in invalid_calls:
            current_calls.append(call)

    if not current_calls:
        call_format_str = '%s{}' % ' > '.join(current_calls)

    else:
        call_format_str = '%s > {}' % ' > '.join(current_calls)

    return call_format_str


def _convert_to_hex(value):
    """Converts the value to hex.

    Args:
        value (any): The value that needs to be converted into hex.

    Returns:
        Any: The converted value to hex can be in in forms of None, a tuple,
            a list or a dictionary.
    """
    if isinstance(value, (list, tuple)):
        output = []
        for item in value:
            output.append(_convert_to_hex(item))

        return output

    if isinstance(value, dict):
        output = {}
        for key, val in value.items():
            output[key] = _convert_to_hex(val)

        return output

    if isinstance(value, bool):
        return value

    if isinstance(value, numbers.Integral):
        return hex(value)

    return value


def _gen_call_repr(wrapped, *args, **kwargs):
    """Generate a representative message on how a method/function is called.

    Args:
        wrapped: The function/method which is wrapped by decorators.
        arags: Arbitrary arguments.
        kwargs: Arbitrary keyword arguments.

    Returns:
        str: Method name with its arguments and keyword arguments.
    """
    args = _convert_to_hex(args)
    kwargs = _convert_to_hex(kwargs)

    kwargs_lst = sorted([('{}={}'.format(k, v)) for k, v in kwargs.items()])
    called_parameters = ', '.join(
        [str(param) for param in list(args) + kwargs_lst]
    )
    return '{} CALLED WITH: ({})'.format(wrapped.__name__, called_parameters)


class KSPLogger(Logger):
    """KSP logger handler.

    Args:
        name (str): Name of the logger.
    """

    def __init__(self, name):
        super(KSPLogger, self).__init__(name)
