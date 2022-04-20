#!/bin/python
'''
%%fullcopyright(2015)
%%version

Utility functions to support logging.

This is only for the convenience of applications that use
pylib and should not be used by modules in the library.

Logging configuration should be under the control of the application
and library modules should not be changing logging levels or adding
handlers apart from the NullHandler.
'''
import logging.config
import inspect

from . import global_streams as gstrm

def config_logging(version=1,
                   disable_existing_loggers=False,
                   handlers={
                       'console': {
                           'class': 'logging.StreamHandler',
                           'level': 'INFO',
                           'stream': 'ext://gstrm.iout'
                       }
                   },
                   root={
                       'level': 'NOTSET',
                       'handlers': ['console']
                   },
                   **kwargs):
    '''
    Configure logging using the dictionary passed in.

    Default configuration is to send log messages at
    INFO level and above to standard output as
    specified by the named argument defaults.

    The version key is mandatory, but all the other
    ones are optional.
    '''

    # add the default arguments to kwargs
    # so we can pass in a single dictionary
    # to dictConfig
    frame = inspect.currentframe()
    args, _, _, values = inspect.getargvalues(frame)

    for arg in args:
        if arg == 'root' and 'console' in root.get('handlers', []):
            try:
                handlers['console']
            except KeyError:
                # Handlers being overridden without
                # overriding root logger. Omit root
                # from logger list.
                continue

        kwargs.update({arg: values[arg]})

    logging.config.dictConfig(kwargs)
