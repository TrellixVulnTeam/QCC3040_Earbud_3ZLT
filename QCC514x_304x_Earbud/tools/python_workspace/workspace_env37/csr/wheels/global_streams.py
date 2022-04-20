from __future__ import print_function
import sys
import logging
from abc import ABCMeta, abstractmethod
from os import devnull

"""
This module supports dynamic global stream re-assignment both for direct use of 
streams and implicit use via print.  The basic idea is that this module provides
proxies for sys.stdout and print which add a simple notion of logging level
which resembles the levels available in the standard Python logging module.
These proxies are dynamically switchable, but they are also global.  
"""

# Py2/3-compliant use of metaclass
IStreamWrapperBase = ABCMeta(
    'IStreamWrapperBase', (object,), {'__slots__': ()})

class IStreamWrapper(IStreamWrapperBase):
    """
    Any class that holds a reference to an underlying stream should be an
    IStreamWrapper.  This specifies an interface that supports manually replacing
    the underlying stream with another, possibly recursively.
    """
    @abstractmethod
    def replace_stream(self, prev, new):
        """
        Replace any references to the stream "prev" amongst streams this
        StreamWrapper wraps with reference to "new".  At the same time any
        wrapped "streams" that are themselves IStreamWrappers should have
        replace_stream called on them with identical parameters. 
        """
        return NotImplemented
        

dout = iout = wout = eout = sys.stdout

def _create_level_print_fn(name, public_stream_name):
    
    def _printfn(msg):
        """
        print to the {} stream
        """.format(public_stream_name)

        print(msg, file=globals()[public_stream_name])
    
    return _printfn

_default_dprint = _create_level_print_fn("dprint", "dout")
_default_iprint = _create_level_print_fn("iprint", "iout")
_default_wprint = _create_level_print_fn("wprint", "wout")
_default_eprint = _create_level_print_fn("eprint", "eout")

_dprint = _default_dprint
_iprint = _default_iprint
_wprint = _default_wprint
_eprint = _default_eprint

def dprint(*msgs, **kwargs):
    return _dprint(kwargs.get("sep", " ").join(str(m) for m in msgs))
def iprint(*msgs, **kwargs):
    return _iprint(kwargs.get("sep", " ").join(str(m) for m in msgs))
def wprint(*msgs, **kwargs):
    return _wprint(kwargs.get("sep", " ").join(str(m) for m in msgs))
def eprint(*msgs, **kwargs):
    return _eprint(kwargs.get("sep", " ").join(str(m) for m in msgs))


def set_streams(stream=None, dstream=None, istream=None, wstream=None, estream=None,
                logger=None):
    """
    Set the streams that the various print functions will write to.
    
    :param stream: If supplied, all streams not set explicitly will be set to
     this stream
    :param dstream: If supplied, the debug stream will be set to this
    :param istream: If supplied, the info stream will be set to this
    :param wstream: If supplied, the warning stream will be set to this
    :param estream: If supplied, the error stream will be set to this
     
    :param logger: If supplied, dprint, iprint, wprint and eprint will be set to
     logger.debug, logger.info, logger.warning and logger.error respectively.
     Note: this does not affect the stream settings.
    """
    global _dprint, _iprint, _wprint, _eprint, dout, iout, wout, eout
    
    if stream is not None:
        
        dout = iout = wout = eout = stream

    """ 
    If False is supplied to any stream value,
    direct it to a null stream
    """
    if dstream is not None:
        dout = dstream
        if dstream is False:
            discard_stream(dout)
    if istream is not None:
        iout = istream
        if istream is False:
            discard_stream(iout)
    if wstream is not None:
        wout = wstream
        if wstream is False:
            discard_stream(wout)
    if estream is not None:
        eout = estream

    if logger is not None:
        
        if logger == False:
            _dprint = _default_dprint
            _iprint = _default_iprint
            _wprint = _default_wprint
            _eprint = _default_eprint
        
        else:
            _dprint = logger.debug
            _iprint = logger.info
            _wprint = logger.warning
            _eprint = logger.error


def discard_stream(stream):
    """Discard the stream to a null stream"""
    try:
        null_stream = open(devnull, "w")
    except OSError as e:
        raise OSError("Failed to open a null stream. Error: ", e)
    replace_stream(stream, null_stream)


def replace_stream(prev, new):
    """
    If client code decides to replace an underlying stream, e.g. redirect stdout,
    global_streams has to be manually updated, or else it will continue to use
    its reference to the previous stream.
    """
    global dout, iout, wout, eout
    
    if dout is prev:
        dout = new
    elif isinstance(dout, IStreamWrapper):
        dout.replace_stream(prev, new)

    if iout is prev:
        iout = new
    elif isinstance(iout, IStreamWrapper):
        iout.replace_stream(prev, new)

    if wout is prev:
        wout = new
    elif isinstance(wout, IStreamWrapper):
        wout.replace_stream(prev, new)

    if eout is prev:
        eout = new
    elif isinstance(eout, IStreamWrapper):
        eout.replace_stream(prev, new)


class GlobalStreamProxy(object):
    """
    Simple proxy for the global streams which forces retrieval of the stream
    object on every attribute access, meaning that whatever object is using
    this stream always uses the current instance, not some historical reference.
    """
    
    def __init__(self, wrapped_stream_name):
        self._wrapped = wrapped_stream_name
        
    def __getattr__(self, attr):
        
        strm = globals()[self._wrapped]
        return getattr(strm, attr)
    

class FilterBetween(object):
    """
    Filter for use with logging objects to emit only records in a certain
    range.
    """
    def __init__(self, lower, upper):
        self._lower = lower
        self._upper = upper
        
    def filter(self, record):
        
        if self._lower is not None and self._upper is not None:
            return self._lower <= record.levelno < self._upper
        if self._upper is not None:
            return record.levelno < self._upper
        if self._lower is not None:
            return self._lower <= record.levelno
        return True
    
def configure_logger_for_global_streams(logger, formatter=None):
    """
    Set up the given logger object with four filtered handlers corresponding
    to global_streams' four logging levels, such that everything logged at a 
    level lower than logging.INFO goes to dout, everything from logging.INFO and
    lower than logging.WARNING goes to iout, etc.   Optionally set a common
    formatter in all the handlers.
    """
    dout_handler = logging.StreamHandler(GlobalStreamProxy("dout"))
    dout_handler.addFilter(FilterBetween(lower=None,upper=logging.INFO))
    if formatter is not None:
        dout_handler.setFormatter(formatter)
    
    iout_handler = logging.StreamHandler(GlobalStreamProxy("iout"))
    iout_handler.addFilter(FilterBetween(lower=logging.INFO, upper=logging.WARNING))
    if formatter is not None:
        iout_handler.setFormatter(formatter)

    wout_handler = logging.StreamHandler(GlobalStreamProxy("wout"))
    wout_handler.addFilter(FilterBetween(lower=logging.WARNING, upper=logging.ERROR))
    if formatter is not None:
        wout_handler.setFormatter(formatter)
    
    eout_handler = logging.StreamHandler(GlobalStreamProxy("eout"))
    eout_handler.addFilter(FilterBetween(lower=logging.ERROR, upper=None))
    if formatter is not None:
        eout_handler.setFormatter(formatter)

    logger.handlers += [dout_handler, iout_handler, wout_handler, eout_handler]
    