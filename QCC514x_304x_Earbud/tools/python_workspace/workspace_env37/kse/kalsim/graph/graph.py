#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Graph class"""

import argparse
import copy
import logging
import time
from functools import partial

from kats.core.stream_base import CALLBACK_EOF
from kats.framework.library.log import log_input, log_exception
from kats.framework.library.schema import DefaultValidatingDraft4Validator
from kats.library.graph.graph import Graph as GGraph, STREAM, ENDPOINT, OPERATOR, KOPERATOR, \
    STREAM_TYPE_SOURCE, STREAM_TYPE_SINK, GRAPH_FROM, GRAPH_TO
from kats.library.registry import get_instance

CALLBACK_CONSUME = 'callback_consume'
CALLBACK_EOF = 'callback_eof'

ARGS = 'args'
KWARGS = 'kwargs'

COMMAND = 'command'
START = 'start'
START_GROUP_OPERATOR = 'group_operator'
START_MIN_TIME = 'min_time'
START_AUTOSTOP = 'autostop'
START_AUTOSTOP_DELAY = 'autostop_delay'
STOP = 'stop'
STOP_GROUP_OPERATOR = 'group_operator'
DESTROY = 'destroy'
DESTROY_GROUP_OPERATOR = 'group_operator'

COMMAND_SCHEMA = {
    'type': 'object',
    'properties': {
        START: {
            'type': 'object',
            'properties': {
                ARGS: {
                    "type": "array",
                    "default": [],
                },
                KWARGS: {
                    "type": "object",
                    "default": {},
                    "properties": {
                        START_GROUP_OPERATOR: {'type': 'boolean', 'default': True},
                        START_MIN_TIME: {'type': 'number', 'minimum': 0, 'default': 0},
                        START_AUTOSTOP: {'type': 'boolean', 'default': False},
                        START_AUTOSTOP_DELAY: {'type': 'number', 'minimum': 0, 'default': 0},
                    }
                },
            },
            "default": {},
        },
        STOP: {
            'type': 'object',
            'properties': {
                ARGS: {
                    "type": "array",
                    "default": [],
                },
                KWARGS: {
                    "type": "object",
                    "default": {},
                    "properties": {
                        STOP_GROUP_OPERATOR: {'type': 'boolean', 'default': True},
                    }
                },
            },
            "default": {},
        },
        DESTROY: {
            'type': 'object',
            'properties': {
                ARGS: {
                    "type": "array",
                    "default": [],
                },
                KWARGS: {
                    "type": "object",
                    "default": {},
                    "properties": {
                        DESTROY_GROUP_OPERATOR: {'type': 'boolean', 'default': False},
                    }
                },
            },
            "default": {},
        },
    }
}


class Graph:
    """Kalimba graph handler

    This class handles all elements in a class (streams, endpoints, operators and koperators).
    It has the capability to create, configure, connect, start streaming, stop streaming,
    disconnect and destroy.

    Args:
        stream (kats.kalsim.stream.stream_factory.StreamFactory): Stream factory
        endpoint (kats.kymera.endpoint.endpoint_factory.EndpointFactory): Endpoint factory
        capability (kats.kymera.capability.capability_factory.CapabilityFactory): Capability factry
        kcapability (kats.kalsim.capability.capability_factory.CapabilityFactory): KCapability factr
        kymera (kats.kymera.kymera.kymera_base.KymeraBase): Kymera instance
        config (dict): Input data configuration
    """

    def __init__(self, stream, endpoint, capability, kcapability, kymera, config):
        self._log = logging.getLogger(__name__) if not hasattr(self, '_log') else self._log
        self._helper = argparse.Namespace()
        self._helper.stream = stream
        self._helper.endp = endpoint
        self._helper.cap = capability
        self._helper.kcap = kcapability
        self._helper.kymera = kymera
        self._helper.uut = get_instance('uut')

        self._inst = argparse.Namespace()
        self._inst.st = []
        self._inst.ep = []
        self._inst.op = []
        self._inst.kop = []
        self._inst.conn = []
        self._inst.ksp = None

        self._graph = None

        self._config = config
        self._cfg = argparse.Namespace()
        self._cfg.min_time = 0
        self._cfg.autostop = False
        self._cfg.autostop_delay = 0

        self._data = argparse.Namespace()
        self._data.stopped = True
        self._data.stopping = False
        self._data.start_time = None

        self.load(self._config)

    def _stop_stream(self, _id):
        _ = _id
        self.stop(include_all=False, include_stream=True)
        self._data.stopped = True
        self._data.stopping = False

    def _check_active(self):
        active = [
            False if stream.get_type() == STREAM_TYPE_SINK or not stream.check_active()
            else True
            for stream in self._inst.st]
        return any(active)

    def _eof(self, ind):
        _ = ind
        if not self._check_active():
            if self._cfg.autostop and not self._data.stopping:
                self._data.stopping = True
                remain = self._cfg.autostop_delay
                if self._cfg.min_time:
                    current = self._helper.uut.timer_get_time()
                    if (current - self._data.start_time) < self._cfg.min_time:
                        remain = max(
                            [self._data.start_time + self._cfg.min_time - current, remain])

                if remain <= 0:
                    self._stop_stream(None)
                else:
                    self._helper.uut.timer_add_relative(remain, self._stop_stream)

    @staticmethod
    def _parse_options(**kwargs):
        include = kwargs.get('include_all', True)
        return {
            STREAM: kwargs.get('include_stream', include) and not kwargs.get(
                'omit_stream', False),
            ENDPOINT: kwargs.get('include_endpoint', include) and not kwargs.get(
                'omit_endpoint', False),
            OPERATOR: kwargs.get('include_operator', include) and not kwargs.get(
                'omit_operator', False),
            KOPERATOR: kwargs.get('include_koperator', include) and not kwargs.get(
                'omit_koperator', False),
        }

    @log_input(logging.INFO)
    def load(self, config):
        """Reload configuration.

        This method is useful if configuration is not known when this helper is instantiated

        Args:
            config (dict): Input data configuration
        """
        self._config = copy.deepcopy(config if config else {})
        self._config.setdefault(COMMAND, {})
        DefaultValidatingDraft4Validator(COMMAND_SCHEMA).validate(self._config[COMMAND])

        self._inst.ksp = None
        try:
            from .ksp import Ksp
            if self._config.get('ksp', None):
                self._inst.ksp = Ksp(
                    self._helper.stream, self._helper.endp, self._helper.cap, self._helper.kymera,
                    self._config['ksp'], self)
        except ImportError:
            pass

    @log_input(logging.INFO)
    def play(self, config, sleep=0.5):
        """Play a graph.

        This will load and optional configuration data, and create, config, connect, start,
        wait for EOF, stop, disconnect and destroy a graph

        Args:
            config (dict): Input data configuration
            sleep (float): Sleep time between graph active state checks in seconds
        """
        self.load(config)
        self.create()
        self.config()
        self.connect()
        self.start()
        while self.check_active():
            time.sleep(sleep)
        self.stop()
        self.disconnect()
        self.destroy()

    @log_input(logging.INFO)
    @log_exception
    def create(self, **kwargs):
        """Builds the internal graph from the configuration entries in input_data
        stream, endpoint, operator and graph.

        Creates all the elements (streams, endpoints and operators)
        """
        opts = self._parse_options(**kwargs)
        self._graph = GGraph(self._config)

        if opts[STREAM]:
            self._inst.st = [None] * self._graph.get_stream_num(None)
            for ind in range(self._graph.get_stream_num()):
                interface, stream_type, args, kwargs = self._graph.get_stream(None, ind)
                if stream_type == STREAM_TYPE_SOURCE:
                    kwargs[CALLBACK_EOF] = partial(self._eof, ind)
                stream = self._helper.stream.get_instance(interface, stream_type, *args, **kwargs)
                self._log.info('creating stream%s interface:%s stream_type:%s', ind, interface,
                               stream_type)
                stream.create()
                self._inst.st[ind] = stream

        if opts[ENDPOINT]:
            self._inst.ep = [None] * self._graph.get_endpoint_num()
            for ind in range(self._graph.get_endpoint_num()):
                interface, endpoint_type, args, kwargs = self._graph.get_endpoint(None, ind)
                endpoint = self._helper.endp.get_instance(interface, endpoint_type, *args, **kwargs)
                self._log.info('creating endpoint%s interface:%s endpoint_type:%s', ind, interface,
                               endpoint_type)
                endpoint.create()
                self._inst.ep[ind] = endpoint

        if opts[OPERATOR]:
            self._inst.op = [None] * self._graph.get_operator_num()
            for ind in range(self._graph.get_operator_num()):
                interface, args, kwargs = self._graph.get_operator(ind)
                operator = self._helper.cap.get_instance(interface, *args, **kwargs)
                self._log.info('creating operator%s interface:%s', ind, interface)
                args, kwargs = self._graph.get_operator_method(ind, 'create')
                operator.create(*args, **kwargs)
                self._inst.op[ind] = operator

        if opts[KOPERATOR]:
            self._inst.kop = [None] * self._graph.get_koperator_num()
            for ind in range(self._graph.get_koperator_num()):
                interface, args, kwargs = self._graph.get_koperator(ind)
                koperator = self._helper.kcap.get_instance(interface, *args, **kwargs)
                self._log.info('creating koperator%s interface:%s', ind, interface)
                koperator.create()
                self._inst.kop[ind] = koperator

    @log_input(logging.INFO)
    @log_exception
    def config(self, **kwargs):
        """Configures all elements (streams, endpoints and operators) in the already built internal
        graph.
        Synchronises endpoints as required
        """
        opts = self._parse_options(**kwargs)

        if opts[STREAM]:
            for ind, stream in enumerate(self._inst.st):
                if stream is not None:
                    kwargs = {}
                    if stream.get_type() == STREAM_TYPE_SINK:
                        conns = self._graph.get_node_out_connections(STREAM, ind)
                        kwargs = {CALLBACK_CONSUME: [lambda *args, **kwargs: None] * 1}

                        for conn in conns:
                            source_terminal = 0
                            sink_type = conn[GRAPH_TO].get_type()
                            sink_number = conn[GRAPH_TO].get_index()
                            if sink_type == KOPERATOR:
                                sink_terminal = conn[GRAPH_TO].get_modifier(1)
                                consume = partial(self._inst.kop[sink_number].consume,
                                                  input_num=sink_terminal)
                                kwargs[CALLBACK_CONSUME][source_terminal] = consume

                    self._log.info('configuring stream%s', ind)
                    stream.config(**kwargs)

        if opts[ENDPOINT]:
            for ind, endpoint in enumerate(self._inst.ep):
                if endpoint is not None:
                    self._log.info('configuring endpoint%s', ind)
                    endpoint.config()

        if opts[OPERATOR]:
            for ind, operator in enumerate(self._inst.op):
                if operator is not None:
                    self._log.info('configuring operator%s', ind)
                    operator.config()

        if opts[KOPERATOR]:
            for ind, operator in enumerate(self._inst.kop):
                if operator is not None:
                    conns = self._graph.get_node_out_connections(KOPERATOR, ind)
                    kwargs = {
                        CALLBACK_CONSUME: [lambda *args, **kwargs: None] * operator.output_num,
                        CALLBACK_EOF: [lambda *args, **kwargs: None] * operator.output_num
                    }

                    for conn in conns:
                        source_terminal = conn[GRAPH_FROM].get_modifier(1)
                        sink_type = conn[GRAPH_TO].get_type()
                        sink_number = conn[GRAPH_TO].get_index()
                        if sink_type == STREAM:
                            sink_terminal = 0
                            consume = partial(self._inst.st[sink_number].consume,
                                              input_num=sink_terminal)
                            kwargs[CALLBACK_CONSUME][source_terminal] = consume
                            eof = partial(self._inst.st[sink_number].eof_detected,
                                          input_num=sink_terminal)
                            kwargs[CALLBACK_EOF][source_terminal] = eof
                        elif sink_type == KOPERATOR:
                            sink_terminal = conn[GRAPH_TO].get_modifier(1)
                            consume = partial(self._inst.kop[sink_number].consume,
                                              input_num=sink_terminal)
                            kwargs[CALLBACK_CONSUME][source_terminal] = consume
                            eof = partial(self._inst.kop[sink_number].eof_detected,
                                          input_num=sink_terminal)
                            kwargs[CALLBACK_EOF][source_terminal] = eof
                        else:
                            raise RuntimeError('koperator%s connected to invalid node' % (ind))
                    self._log.info('configuring koperator%s', ind)
                    operator.config(**kwargs)

        if opts[ENDPOINT]:
            for index in range(self._graph.get_sync_num()):
                syncs = self._graph.get_sync_connections(index)
                for ind, node in enumerate(syncs[:-1]):
                    self._log.info('syncing %s to %s',
                                   self._graph.get_sync(index)[ind],
                                   self._graph.get_sync(index)[ind + 1])
                    sid0 = self._inst.ep[node.get_index()].get_id()
                    sid1 = self._inst.ep[syncs[ind + 1].get_index()].get_id()
                    self._helper.kymera.stream_if_sync_sids(sid0, sid1)

    @log_input(logging.INFO)
    @log_exception
    def connect(self):
        """Connect all elements (endpoints and operators) in the already built internal graph"""
        self._inst.conn = []
        for entry in self._graph.get_graph_connections():
            if (entry[GRAPH_FROM].get_type() != KOPERATOR and
                    entry[GRAPH_TO].get_type() != KOPERATOR and
                    entry[GRAPH_FROM].get_type() != STREAM and
                    entry[GRAPH_TO].get_type() != STREAM):
                num = entry[GRAPH_FROM].get_index()
                if entry[GRAPH_FROM].get_type() == ENDPOINT:
                    id1 = self._inst.ep[num].get_id()
                else:
                    num2 = entry[GRAPH_FROM].get_modifier(1)
                    id1 = self._inst.op[num].get_source_endpoint(num2)

                num = entry[GRAPH_TO].get_index()
                if entry[GRAPH_TO].get_type() == ENDPOINT:
                    id2 = self._inst.ep[num].get_id()
                else:
                    num2 = entry[GRAPH_TO].get_modifier(1)
                    id2 = self._inst.op[num].get_sink_endpoint(num2)

                self._log.info('connecting %s to %s', str(entry[GRAPH_FROM]), str(entry[GRAPH_TO]))
                self._inst.conn.append(self._helper.kymera.stream_if_connect(id1, id2))

        # create/config/start ksp (stream, endpoint, capability) if it exists
        if self._inst.ksp:
            self._inst.ksp.create()
            self._inst.ksp.config()
            self._inst.ksp.connect()
            self._inst.ksp.start()

    @log_input(logging.INFO)
    @log_exception
    def start(self, **kwargs):
        """Starts streaming all source and sink streams in the already built internal
        graph

        Args:
            group_operator (bool): Start all operators in one go
            min_time (float): Minimum playing time
            autostop (bool): Stop streams when graph has played
            autostop_delay (float): Time to wait before stopping the streams after graph has played
        """
        group_operator = kwargs.pop(
            START_GROUP_OPERATOR, self._config[COMMAND][START][KWARGS][START_GROUP_OPERATOR])
        self._cfg.min_time = kwargs.pop(
            START_MIN_TIME, self._config[COMMAND][START][KWARGS][START_MIN_TIME])
        self._cfg.autostop = kwargs.pop(
            START_AUTOSTOP, self._config[COMMAND][START][KWARGS][START_AUTOSTOP])
        self._cfg.autostop_delay = kwargs.pop(
            START_AUTOSTOP_DELAY, self._config[COMMAND][START][KWARGS][START_AUTOSTOP_DELAY])
        opts = self._parse_options(**kwargs)

        # operators
        if opts[OPERATOR]:
            if not group_operator:
                for operator in self._inst.op:
                    if operator is not None:
                        operator.start()
            else:
                ops = [operator.get_id() for operator in self._inst.op if operator]
                if ops:
                    self._helper.kymera.opmgr_start_operators(ops)

        kalcmd = get_instance('kalcmd')
        with kalcmd.get_lock_object():
            if opts[STREAM]:
                # sink streams
                for ind, stream in enumerate(self._inst.st):
                    if stream is not None:
                        _, stream_type, _, _ = self._graph.get_stream(None, ind)
                        if stream_type == STREAM_TYPE_SINK:
                            stream.start()

                # source streams
                for ind, stream in enumerate(self._inst.st):
                    if stream is not None:
                        _, stream_type, _, _ = self._graph.get_stream(None, ind)
                        if stream_type == STREAM_TYPE_SOURCE:
                            stream.start()

            # koperators
            if opts[KOPERATOR]:
                for operator in self._inst.kop:
                    if operator is not None:
                        operator.start()

        self._data.start_time = self._helper.uut.timer_get_time()
        self._data.stopping = False
        self._data.stopped = False
        self._eof(None)  # trigger immediate EOF detection in case there are no source streams

    @log_exception
    def check_active(self):
        """Checks if all source streams have reached EOF in the already built internal graph"""
        current = self._helper.uut.timer_get_time()
        if (current - self._data.start_time) < self._cfg.min_time:
            return True
        if self._cfg.autostop:
            return not self._data.stopped
        return self._check_active()

    @log_input(logging.INFO)
    @log_exception
    def stop(self, **kwargs):
        """Stops all elements (streams and operators) in the already built internal graph

        Args:
            group_operator (bool): Stop all operators in one go
        """
        group_operator = kwargs.pop(
            STOP_GROUP_OPERATOR, self._config[COMMAND][STOP][KWARGS][STOP_GROUP_OPERATOR])
        opts = self._parse_options(**kwargs)

        # kats operators
        if opts[KOPERATOR]:
            for operator in self._inst.kop:
                if operator is not None:
                    operator.stop()

        # source streams
        if opts[STREAM]:
            for ind, stream in enumerate(self._inst.st):
                if stream is not None:
                    _, stream_type, _, _ = self._graph.get_stream(None, ind)
                    if stream_type == STREAM_TYPE_SOURCE:
                        stream.stop()

        # sink streams
        if opts[STREAM]:
            for ind, stream in enumerate(self._inst.st):
                if stream is not None:
                    _, stream_type, _, _ = self._graph.get_stream(None, ind)
                    if stream_type == STREAM_TYPE_SINK:
                        stream.stop()

        # operators
        if opts[OPERATOR]:
            if not group_operator:
                for operator in self._inst.op:
                    if operator is not None:
                        operator.stop()
            else:
                ops = [operator.get_id() for operator in self._inst.op if operator]
                if ops:
                    self._helper.kymera.opmgr_stop_operators(ops)

        self._data.stopped = True

    @log_input(logging.INFO)
    def disconnect(self):
        """Disconnects all elements (endpoints and operators) in the already built graph"""
        if self._inst.ksp:
            self._inst.ksp.stop()
            self._inst.ksp.disconnect()
            self._inst.ksp.destroy()

        self._helper.kymera.stream_if_transform_disconnect(self._inst.conn)

    @log_input(logging.INFO)
    def destroy(self, **kwargs):
        """Destroy all streams, endpoints and operators in the already built internal graph

        Args:
            group_operator (bool): Destroy all operators in one go
        """
        group_operator = kwargs.pop(
            DESTROY_GROUP_OPERATOR,
            self._config[COMMAND][DESTROY][KWARGS][DESTROY_GROUP_OPERATOR])
        opts = self._parse_options(**kwargs)

        if opts[OPERATOR]:
            if not group_operator:
                for operator in self._inst.op:
                    if operator is not None:
                        operator.destroy()
            else:
                ops = [operator.get_id() for operator in self._inst.op if operator]
                if ops:
                    self._helper.kymera.opmgr_destroy_operators(ops)
            for operator in self._inst.op:
                if operator is not None:
                    self._helper.cap.put_instance(operator)
            self._inst.op = []

        if opts[KOPERATOR]:
            for operator in self._inst.kop:
                if operator is not None:
                    operator.destroy()
                    self._helper.kcap.put_instance(operator)
            self._inst.kop = []

        if opts[ENDPOINT]:
            for endpoint in self._inst.ep:
                if endpoint is not None:
                    endpoint.destroy()
                    self._helper.endp.put_instance(endpoint)
            self._inst.ep = []

        if opts[STREAM]:
            for stream in self._inst.st:
                if stream is not None:
                    stream.destroy()
                    self._helper.stream.put_instance(stream)
            self._inst.st = []

    def get_stream_num(self):
        """Get number of stream instances available in the graph

        Returns:
            int: Number of stream instances
        """
        return len(self._inst.st)

    def get_stream(self, index):
        """Get stream instance

        Args:
            index (int): Zero based index of the stream

        Returns:
            kats.core.stream_base.StreamBase: Stream instance
        """
        return self._inst.st[index]

    def get_endpoint_num(self):
        """Get number of endpoint instances available in the graph

        Returns:
            int: Number of endpoint instances
        """
        return len(self._inst.ep)

    def get_endpoint(self, index):
        """Get endpoint instance

        Args:
            index (int): Zero based index of the endpoint

        Returns:
            kats.core.endpoint_base.EndpointBase: Endpoint instance
        """
        return self._inst.ep[index]

    def get_operator_num(self):
        """Get number of operator instances available in the graph

        Returns:
            int: Number of operator instances
        """
        return len(self._inst.op)

    def get_operator(self, index):
        """Get operator instance

        Args:
            index (int): Zero based index of the operator

        Returns:
            kats.kymera.capability.capability_base.CapabilityBase: Operator instance
        """
        return self._inst.op[index]

    def get_koperator_num(self):
        """Get number of kats operator instances available in the graph

        Returns:
            int: Number of kats operator instances
        """
        return len(self._inst.kop)

    def get_koperator(self, index):
        """Get kats operator instance

        Args:
            index (int): Zero based index of the kats operator

        Returns:
            kats.kalsim.operator.operator_base.OperatorBase: Operator instance
        """
        return self._inst.kop[index]

    def get_graph_connection(self, index):
        """Get transform id for a certain connection in the graph

        Args:
            index (int): Zero based index of the connection

        Returns:
            int: Transform id or None of the connection has not been created
        """
        num = 0
        for ind, entry in enumerate(self._graph.get_graph_connections()):
            # we skip those graph connections which do not actually create a transform
            if (entry[GRAPH_FROM].get_type() != KOPERATOR and
                    entry[GRAPH_TO].get_type() != KOPERATOR and
                    entry[GRAPH_FROM].get_type() != STREAM and
                    entry[GRAPH_TO].get_type() != STREAM):
                if index == ind:
                    return self._inst.conn[num]
                num += 1
        raise RuntimeError('unable to find connection for graph entry %s' % (index))
