#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KATS capability base class"""

import argparse
import logging
from abc import ABC, abstractmethod, abstractproperty

from kats.framework.library.log import log_input

STATE_CREATED = 'kcapability_state_created'
STATE_STARTED = 'kcapability_state_started'
STATE_STOPPED = 'kcapability_state_stopped'

CALLBACK_CONSUME = 'callback_consume'
CALLBACK_EOF = 'callback_eof'


class CapabilityBase(ABC):
    """Basic kats capability base class

    A capability should have the following methods

    - consume(data). Invoked when new data is available for the capability (has been received)

    The capability should call

    - eof(). When EOF has been detected
    - produce(data). When data is ready to be sent out

    Args:
        stream_type (str): Type of stream 'source' or 'sink'
        stream_name (str): Stream name
        stream_rate (int): Stream frame rate in hertzs
        stream_data_width (int): Stream data width in hertzs
        stream_data (list[int]): Stream frame data
        callback_consume (list[func(data)]): Callback for End of File
        callback_eof (list[func()]): Callback for End of File
    """

    def __init__(self, *args, **kwargs):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self.__config = argparse.Namespace()
        self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME, None)
        self.__config.callback_eof = kwargs.pop(CALLBACK_EOF, None)
        self.__data = argparse.Namespace()
        self.__data.state = None

        if args:
            self._log.warning('unknown args:%s', str(args))

        if kwargs:
            self._log.warning('unknown kwargs:%s', str(kwargs))

    @abstractproperty
    def platform(self):
        """list[str]: Platform name"""

    @abstractproperty
    def interface(self):
        """str: Interface name"""

    @abstractproperty
    def input_num(self):
        """int: Number of input terminals"""

    @abstractproperty
    def output_num(self):
        """int: Number of output terminals"""

    @log_input(logging.INFO)
    def create(self):
        """Create operator"""
        if self.__data.state is not None:
            raise RuntimeError('operator already created')
        self.__data.state = STATE_CREATED

    @log_input(logging.INFO)
    def config(self, **kwargs):
        """Configure operator"""
        if CALLBACK_CONSUME in kwargs:
            self.__config.callback_consume = kwargs.pop(CALLBACK_CONSUME)
            if not isinstance(self.__config.callback_consume, list):
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))
            if len(self.__config.callback_consume) < self.output_num:
                raise RuntimeError('callback_consume:%s invalid' % (self.__config.callback_consume))

        if CALLBACK_EOF in kwargs:
            self.__config.callback_eof = kwargs.pop(CALLBACK_EOF)
            if not isinstance(self.__config.callback_eof, list):
                raise RuntimeError('callback_eof:%s invalid' % (self.__config.callback_eof))
            if len(self.__config.callback_eof) < self.output_num:
                raise RuntimeError('callback_eof:%s invalid' % (self.__config.callback_eof))

    @log_input(logging.INFO)
    def start(self):
        """Start operator"""
        if self.__data.state != STATE_CREATED and self.__data.state != STATE_STOPPED:
            raise RuntimeError('cannot start operator')

        self.__data.state = STATE_STARTED

    @log_input(logging.INFO)
    def stop(self):
        """Stop operator"""
        self.__data.state = STATE_STOPPED

    @log_input(logging.INFO)
    def destroy(self):
        """Destroy operator"""
        if self.__data.state is None:
            raise RuntimeError('operator not created')
        if self.__data.state == STATE_STARTED:
            self.stop()

        self.__data.state = None

    def get_state(self):
        """Get current state

        Returns:
            str: State, one of STATE_CREATED, STATE_STARTED, STATE_STOPPED
        """
        return self.__data.state

    @log_input(logging.DEBUG)
    @abstractmethod
    def consume(self, input_num, data):
        """Data received on operator

        This method has to be subclassed by derived classes and work appropriately, probably
        doing some processing and forwarding it to some output through rhe produce method

        Args:
            input_num (int): Input data arrives in
            data (list[int]): Data received
        """

    @log_input(logging.INFO)
    @abstractmethod
    def eof_detected(self, input_num):
        """End of File event received on input terminal

        This method has to be subclassed by derived classes and work appropriately, probably
        forwarding the eof to the applicable outputs through the dispatch_eof method

        Args:
            input_num (int): Input eof has been detected in
        """

    @log_input(logging.DEBUG)
    def produce(self, output_num, data):
        """Send data generated in operator"""
        if output_num >= self.output_num:
            raise RuntimeError('operator produce output:%s invalid' % (output_num))
        if self.__config.callback_consume:
            self.__config.callback_consume[output_num](data=data)

    @log_input(logging.INFO)
    def dispatch_eof(self, output_num):
        """Dispatch End of File event to operator output

        Args:
            output_num (int): Output to propagate EOF to
        """
        if output_num >= self.output_num:
            raise RuntimeError('operator eof output:%s invalid' % (output_num))
        if self.__config.callback_eof:
            self.__config.callback_eof[output_num]()
