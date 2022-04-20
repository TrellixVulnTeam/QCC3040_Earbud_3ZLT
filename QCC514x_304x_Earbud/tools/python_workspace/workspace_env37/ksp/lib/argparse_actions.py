#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Custom argument parsers."""

import argparse
from argparse import ArgumentError

from ksp.lib.exceptions import ConfigurationError
from ksp.lib.namespace import STREAM_DATA_TYPES
from ksp.lib.types import Stream


# The argparse.Action derivative interface does not specify any public
# methods, it is only constructed and called.
# pylint: disable=too-few-public-methods
class StreamArgParser(argparse.Action):
    """Stream Argument Parser.

    Parse a string value given for the --stream, -s
    option. The value is a string of the form
    <stream_id>:<data_format>:<key[=value]>:<key[=value]>
    with the following keys:

    tr=<transform list> (default for format TTR is 'all',
    other formats require this key),

    samples=<number> (no default),

    fs=<sampling rate> (no default),

    bs=<buffer size> (default 0)

    md|metadata[=y|n] (default 'n'),

    td|timed_data=<endpoint id> (no default),

    p[roc]=[0|1] (default 0).

    The values have the same formats as in the interactive mode,
    except transform IDs are separated with commas without spaces.

    Arguments are as required by argparse.Action:

    - option_strings -- A list of command-line option strings which
    should be associated with this action.

    - dest -- The name of the attribute to hold the created object(s)

    - nargs -- StreamArgParser supports only '*'.

    - const -- Not used in StreamArgParser.

    - default -- Should be an empty dictionary for StreamArgParser.

    - type -- StreamArgParser expects the type str

    - choices -- Not supported by StreamArgParser.

    - required -- Should be False.

    - help -- The help string describing the argument.

    - metavar -- Should be specified because the default value
    is not suitable as a metavar.
    """

    STREAM_KEYS = {
        'tr': 'transform_list',
        'samples': 'samples',
        'fs': 'fs',
        'bs': 'bs',
        'md': 'metadata',
        'metadata': 'metadata',
        'td': 'timed_data',
        'timed_data': 'timed_data',
        'p': 'proc',
        'proc': 'proc'
    }

    # argparse.Action requires custom actions to have keyword arguments called
    # 'type' and 'help'
    # pylint: disable=redefined-builtin
    # argparse.Action specifies this signature
    # pylint: disable=too-many-arguments
    def __init__(self, option_strings, dest, nargs='*',
                 const=None, default=None, type=str,
                 choices=None, required=False,
                 help=None, metavar=None):
        if nargs != '*':
            raise ArgumentError(self,
                                "StreamArgParser only supports nargs='*'")
        if const:
            raise ArgumentError(self,
                                "StreamArgParser does not support const")
        if default:
            raise ArgumentError(self,
                                "The default argument to "
                                "StreamArgParser should be None")
        if type != str:
            raise ArgumentError(self,
                                "StreamArgParser only supports type=str")
        if choices:
            raise ArgumentError(self,
                                "StreamArgParser does not support choices")
        if not metavar:
            raise ArgumentError(self,
                                "A metavar should be specified for "
                                "StreamArgParser")

        super(StreamArgParser, self).__init__(
            option_strings, dest, nargs=nargs, const=const,
            default=default, type=type, choices=choices, required=required,
            help=help, metavar=metavar)

    def _parse_stream_number(self, stream_number_str):
        if not stream_number_str.strip():
            raise ArgumentError(
                self,
                "Valid stream numbers are 0 or 1: %s" %
                stream_number_str)
        try:
            stream_number = int(stream_number_str)
        except ValueError as error:
            raise ArgumentError(
                self,
                "Valid stream numbers are 0 or 1: %s: %s" %
                (stream_number_str, error))

        if stream_number not in Stream.SUPPORTED_STREAMS:
            raise ArgumentError(
                self,
                "Valid stream numbers are 0 or 1: %s" %
                stream_number_str)

        return stream_number

    def _parse_key_values(self, parts):
        # collect key/value pairs
        key_values = dict()
        for part in parts:
            eq_sides = part.split('=')
            key = eq_sides[0]
            normalized_key = self.STREAM_KEYS.get(key.lower(), None)
            if not normalized_key:
                raise ArgumentError(
                    self,
                    "Valid stream config keys are %s: %s" %
                    (', '.join(sorted(self.STREAM_KEYS.keys())), key))
            if normalized_key in key_values:
                raise ArgumentError(
                    self,
                    "Stream config key specified more than once: %s" % key)
            if len(eq_sides) < 2:
                # Boolean specified without value means true/yes
                value = 'y'
            elif len(eq_sides) > 2:
                raise ArgumentError(
                    self,
                    "Invalid value for stream config key %s: %s" %
                    (key, '='.join(eq_sides[1:])))
            else:
                value = eq_sides[1]
            key_values[normalized_key] = value
        return key_values

    def _parse_kw_opts_non_ttr(self, option_string, key_values, kw_opts):
        if 'transform_list' not in key_values:
            raise ArgumentError(
                self,
                "%s: tr=<transform ID list> is required" %
                option_string)

        try:
            kw_opts['sample_rate'] = int(key_values.get('fs', '0'), 0)
        except ValueError as error:
            raise ArgumentError(
                self,
                "%s: invalid value format for fs: %s: %s" %
                (option_string, key_values['fs'], error))

        if 'metadata' in key_values:
            raise ArgumentError(
                self,
                "%s: metadata is only valid with data format TTR" %
                option_string)

        if 'timed_data' in key_values:
            raise ArgumentError(
                self,
                "%s: timed data is only valid with data format TTR" %
                option_string)

    def _parse_kw_opts_ttr(self, option_string, key_values, kw_opts):
        if 'transform_list' not in key_values:
            key_values['transform_list'] = 'all'

        if 'fs' in key_values:
            raise ArgumentError(
                self,
                "%s: fs is not valid with data format TTR" %
                option_string)

        kw_opts['metadata'] = key_values.get('metadata', 'n')[0] in "YyTt"

        kw_opts['timed_data'] = key_values.get('timed_data', '')

    def _parse_kw_opts(self, option_string, data_type, key_values, kw_opts):
        # Some parsing or pre-validation differs between TTR and other formats
        try:
            kw_opts['buffer_size'] = int(key_values.get('bs', 0))
        except ValueError as error:
            raise ArgumentError(
                self,
                '%s: Invalid value format for the Buffer Size: %s: %s' %
                (option_string, key_values['samples'], error)
            )
        if data_type == 'TTR':
            self._parse_kw_opts_ttr(option_string, key_values, kw_opts)
        else:
            self._parse_kw_opts_non_ttr(option_string, key_values, kw_opts)

        try:
            kw_opts['samples'] = int(key_values.get('samples', 0))
        except ValueError as error:
            # to fail here, key_values['samples'] had to exist
            raise ArgumentError(
                self,
                "%s: Invalid value format for samples: %s: %s" %
                (option_string, key_values['samples'], error)
            )

        try:
            processor = int(key_values.get('proc', 0))
            if processor not in (0, 1):
                raise ValueError("Processor value is not 0 or 1.")
            kw_opts['processor'] = processor

        except ValueError:
            raise ArgumentError(
                self,
                "Processor number must be 0 or 1: {}".format(option_string)
            )


    def __call__(self, parser, namespace, values, option_string=None):
        """
        Implement custom parsing of option values
        """
        stream_config = getattr(namespace, self.dest, None)
        if stream_config is None:
            stream_config = {}

        parts = values[0].split(':')
        if len(parts) < 2:
            raise ArgumentError(
                self,
                "%s requires at least a stream number and a data format." %
                option_string)

        stream_number = self._parse_stream_number(parts[0])
        if stream_number in stream_config:
            raise ArgumentError(
                self,
                "%s: stream %d configured more than once" %
                (option_string, stream_number))

        data_type = parts[1].upper()
        if data_type not in STREAM_DATA_TYPES:
            raise ArgumentError(
                self,
                "%s: %s is not a valid data type" %
                (option_string, data_type))

        key_values = self._parse_key_values(parts[2:])

        kw_opts = {
            'sample_rate': 0,
            'samples': 0,
            'buffer_size': 0,
            'metadata': False,
            'timed_data': '',
            'processor': 0
        }
        self._parse_kw_opts(option_string, data_type, key_values, kw_opts)
        transform_ids = key_values['transform_list'].split(',')

        try:
            stream = Stream(stream_number, data_type, transform_ids,
                            **kw_opts)
        except ConfigurationError as error:
            raise ArgumentError(
                self,
                "%s: invalid stream configuration: %s" %
                (option_string, error))

        stream_config[stream_number] = stream
        setattr(namespace, self.dest, stream_config)
