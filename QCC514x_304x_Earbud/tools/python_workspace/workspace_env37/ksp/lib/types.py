#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KSP Concepts."""
import logging
from enum import Enum

import ksp.lib.namespace as ns
from ksp.lib.data_types import KspDataType
from ksp.lib.exceptions import (
    InvalidTransformID, InvalidTransformIDList, ConfigurationError
)

logger = logging.getLogger(__name__)


class TransformID(object):
    """Transform ID Integer type."""

    TRANSFORM_COOKIE = 0x1C1E
    INT_ID_MIN = 1
    INT_ID_MAX = 255

    def __new__(cls, value):
        # The value can be in the form of integer or hex string.
        if isinstance(value, str):
            try:
                value = int(value, 16)
            except ValueError as error:
                raise InvalidTransformID(error)

        cls._validate(value)
        return value

    @staticmethod
    def get_int_id(tr_ext_id):
        """Gets the internal transform ID from an external one.

        Given an external transform ID or zero, returns the equivalent internal
        transform ID. get_int_id preserves the validity of transform IDs.

        Args:
            tr_ext_id (int): A valid external transform ID, zero, or an integer
                which is neither zero nor a valid external transform ID.

        Returns:
            Internal transform ID if the argument is a valid external
                transform ID; 0 if the argument is 0. If the argument is not a
                valid external transform ID, the value returned is an integer
                which is not a valid internal transform ID.
        """
        # Modeled after STREAM_TRANSFORM_GET_INT_ID in
        # components/stream/stream_transform.h.
        if tr_ext_id:
            return tr_ext_id ^ TransformID.TRANSFORM_COOKIE
        return 0

    @staticmethod
    def is_valid_int_id(tr_int_id):
        """Returns True if the argument is a valid internal transform ID.

        Args:
            tr_int_id (int): A valid internal transform ID.

        Returns:
            True if tr_int_id is a valid internal transform ID.
        """
        return TransformID.INT_ID_MIN <= tr_int_id <= TransformID.INT_ID_MAX

    @staticmethod
    def is_valid_ext_id(tr_ext_id):
        """Returns True if the argument is a valid internal transform ID.

        Args:
            tr_ext_id: an integer

        Returns:
            True if tr_ext_id is a valid external transform ID.
        """
        return TransformID.is_valid_int_id(TransformID.get_int_id(tr_ext_id))

    @staticmethod
    def _validate(value):
        """Makes sure the given value is in line with the expected format."""
        if not TransformID.is_valid_ext_id(value):
            raise InvalidTransformID(
                "{} is not a valid transform ID.".format(hex(value))
            )


class TransformIDList(object):
    """Transform ID List.

    A TransformIDList implements representation of a list of
    transform IDs with special conventions for no or all possible
    transforms.
    The internal representation of 'all transforms' is a list with
    a single element '0xffff'.
    For other values, the internal representation is a list of
    hex strings of external transform IDs.
    """
    ALL = 0xffff
    ALL_HEX = '0xffff'
    ALL_STR = 'all'
    NONE_STR = 'none'
    ALL_LEN = 255

    def __new__(cls, value):
        """Generates the new format.

        Convert an external string representation or an internal representation
        of a transform ID list into an internal representation.

        Args:
            value: May be a string or a list of hex strings.
                   The string 'none', an empty string, or an empty list stand
                   for a list containing no transform IDs.
                   The string 'all' means a list containing all valid
                   transform IDs. A list of hex strings, or a string
                   containing hex strings separated by spaces, all of which
                   represent valid external transform IDs, is converted to the
                   equivalent internal representation.

        Returns:
            Internal representation of a transform ID list.
        """
        if isinstance(value, str):
            value = value.lower().split()

        if isinstance(value, list):
            if len(value) == 1:
                if isinstance(value[0], str):
                    if value[0] == cls.ALL_STR or \
                        value[0] == cls.ALL_HEX:
                        return [cls.ALL_HEX]
                    if value[0] == cls.NONE_STR:
                        return []
                elif isinstance(value[0], int):
                    if value[0] == cls.ALL:
                        return [cls.ALL_HEX]
            try:
                return list(hex(TransformID(w)) for w in value)
            except InvalidTransformID as error:
                raise InvalidTransformIDList(error)
        else:
            raise InvalidTransformIDList(
                "TransformIDList initializer must "
                "be a string or a list, got a {}".format(
                    type(value).__name__
                )
            )

    @staticmethod
    def _validate(value):
        _ = TransformIDList(value)

    @staticmethod
    def is_all(value):
        """Check if the given value represents the ``all transforms``.

        Returns:
            bool: True if the argument is the internal representation of
                ``all transforms``.
        """
        if isinstance(value, list) and len(value) == 1:
            if isinstance(value[0], str):
                if value[0].lower() in (hex(TransformIDList.ALL),
                                        TransformIDList.ALL_STR):
                    return True
        return False

    @staticmethod
    def is_none(value):
        """Check if the given value represents the `no transforms`.

        Returns:
            bool: True if the argument is the internal representation of
                ``no transforms``.
        """
        return isinstance(value, list) and len(value) == 0

    @staticmethod
    def as_str(value):
        """Return the external string form of an internal representation."""
        if TransformIDList.is_all(value):
            return TransformIDList.ALL_STR
        if TransformIDList.is_none(value):
            return TransformIDList.NONE_STR

        return ' '.join(value)

    @staticmethod
    def as_int_list(value):
        """Return the list of integers of transform IDs."""
        if TransformIDList.is_all(value):
            raise TypeError("Cannot convert `all` to a list of integers.")
        if TransformIDList.is_none(value):
            raise TypeError("Cannot convert `none` to a list of integers.")

        return [int(transform_id, 16) for transform_id in value]

    @staticmethod
    def len(value):
        """Return the number of transforms IDs represented by the argument.

        Args:
            value: an internal representation of a TransformIDList.

        Returns:
            int: The number of transform IDs represented by the value. For a
                value representing all transforms, the result is 255.
        """
        if TransformIDList.is_all(value):
            return TransformIDList.ALL_LEN
        return len(value)


# BUG: B-299459
# pylint: disable=too-many-arguments,too-few-public-methods
class Stream(object):
    """Stream Dictionary type."""
    SUPPORTED_STREAMS = (0, 1)

    def __new__(cls, stream, data_type, transform_ids, **kw):
        """Validates the configuration of a stream and return a dictionary.

        Args:
            stream (int): Stream number.
            data_type(str): Data type string. It's case-insensitive but
                should be a valid type.
            transform_ids (list): List of transform IDs.
            samples (int): number of samples in each stream packet, up to 256,
                if 0, the audio FW will decide.
            metadata (bool): enable tracing metadata (only with stream
                type TTR).
            timed_data (str): hex source endpoint or terminal ID (only
                with stream type TTR).
            buffer_size (int): The size of the internal buffer in number of
                words.
            processor (int): The processor number.

        Returns:
            dict: A dictionary representing a Stream.
        """
        stream = int(stream)
        data_type = data_type.upper()
        samples = kw.pop('samples', 0)
        sample_rate = kw.pop('sample_rate', 0)
        metadata = kw.pop('metadata', False)
        timed_data = kw.pop('timed_data', 0)
        buffer_size = kw.pop('buffer_size', 0)
        processor = kw.pop('processor', 0)

        cls._validate(
            stream,
            data_type,
            transform_ids,
            samples,
            sample_rate,
            metadata,
            timed_data,
            buffer_size,
            processor
        )
        return cls._asdict(
            stream,
            data_type,
            TransformIDList(transform_ids),
            samples,
            sample_rate,
            metadata,
            timed_data,
            buffer_size,
            processor
        )

    @classmethod
    # BUG: B-299459
    # pylint: disable=too-many-branches
    def _validate(cls, stream, data_type, transform_ids, samples,
                  sample_rate, metadata, timed_data, buffer_size,
                  processor):
        is_valid = True
        if stream not in cls.SUPPORTED_STREAMS:
            is_valid = False
            logger.error(
                "Supported streams are %s, %s is given",
                cls.SUPPORTED_STREAMS, stream
            )

        if data_type not in ns.STREAM_DATA_TYPES.keys():
            is_valid = False
            logger.error(
                "Supported data types are %s, %s is given",
                ns.STREAM_DATA_TYPES.keys(), data_type
            )

        try:
            TransformIDList(transform_ids)
        except ValueError as error:
            is_valid = False
            logger.error(error)

        if samples < 0 or samples > 256:
            is_valid = False
            logger.error(
                "Samples should be between 0 and 256 (inclusive). %s is given",
                samples
            )

            if sample_rate != 0:
                if "PCM" not in data_type:
                    is_valid = False
                    logger.error("Samples rate is only valid for PCM streams")
                elif sample_rate < 8000 or sample_rate > 192000:
                    is_valid = False
                    logger.error(
                        "Samples rate should be between 8 and 192KHz. % given",
                        sample_rate
                    )

        if metadata and data_type != "TTR":
            is_valid = False
            logger.error("Metadata tracing is only valid for TTR streams")

        if timed_data:
            if data_type != "TTR":
                is_valid = False
                logger.error("Timed data is only valid for TTR streams")
            else:
                try:
                    i = int(timed_data, 16)
                except ValueError as error:
                    is_valid = False
                    logger.error(error)
                if i != 0 and ((i & 0xe000) not in (0x6000, 0xa000)):
                    is_valid = False

        if buffer_size < 0 or buffer_size > 16384:
            is_valid = False
            logger.error("The Buffer Size should be between 0 and 16384.")

        if processor not in (0, 1):
            is_valid = False
            logger.error("The Processor must be either 0 or 1.")

        is_valid = is_valid and cls._validate_data_type(
            data_type, transform_ids
        )

        if is_valid is False:
            raise ConfigurationError("Stream configuration is invalid.")

    @staticmethod
    def _validate_data_type(data_type, transform_ids):
        # BUG: B-299459: The sanity check should happen within the stream
        #      object. This method is too long. Also, Review the all cases of
        #      the data type and transform_ids. i.e. the data type is audio but
        #      still none/all is acceptable!
        is_valid = True
        nr_channels = len(TransformIDList(transform_ids))
        data_type = KspDataType[data_type]
        if data_type == KspDataType.TTR:
            min_channels = 0
            max_channels = TransformIDList.ALL_LEN
        else:
            min_channels = 1
            if data_type.is_audio_type():
                max_channels = 4
            else:
                max_channels = 1

        if nr_channels < min_channels:
            is_valid = False
            error = 'Configure at least {} transform ID for stream type {}' \
                .format(min_channels, str(data_type))
            logger.error(error)

        if nr_channels > max_channels:
            is_valid = False
            error = (
                "KSP Config failed. {} channels requested, "
                "max {} channels supported for stream "
                "type {}" .format(nr_channels, max_channels, str(data_type))
            )
            logger.error(error)

        return is_valid

    @classmethod
    def _asdict(cls, stream, data_type, transform_ids, samples,
                sample_rate, metadata, timed_data, buffer_size,
                processor):
        """Converts the stream instance into a dictionary."""
        asdict_value = {
            ns.STREAMS_STREAM: stream,
            ns.STREAMS_DATA_TYPE: data_type,
            ns.STREAMS_TRANSFORM_IDS: TransformIDList.as_str(transform_ids),
            ns.STREAMS_BUFFER_SIZE: buffer_size,
            ns.STREAMS_PROCESSOR: processor
        }

        if samples:
            asdict_value[ns.STREAMS_SAMPLES] = samples
        if sample_rate != 0:
            asdict_value[ns.STREAMS_SAMPLE_RATE] = sample_rate
        if data_type == "TTR":
            asdict_value[ns.STREAMS_METADATA] = metadata
            asdict_value[ns.STREAMS_TIMED_DATA] = ''
            if timed_data:
                timed_data_int = int(timed_data, 16)
                if timed_data_int:
                    timed_data = '0x%04x' % timed_data_int
                    asdict_value[ns.STREAMS_TIMED_DATA] = timed_data

        return asdict_value


# pylint: disable=too-few-public-methods
class RunMode(Enum):
    """Definition of the program's modes of operation."""
    INTERACTIVE = 0
    NON_INTERACTIVE = 1

    def __str__(self):
        return self.name
