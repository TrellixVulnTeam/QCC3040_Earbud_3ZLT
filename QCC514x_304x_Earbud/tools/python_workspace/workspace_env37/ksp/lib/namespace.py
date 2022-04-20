#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Holds information about namespaces."""
import os


LIB_DIRECTORY = os.path.dirname(os.path.realpath(__file__))
CONFIGURATION_FILE = os.path.join(
    LIB_DIRECTORY,
    '..',
    'configurations.json'
)

STREAMS = "Streams"
STREAMS_STREAM = "Stream"
STREAMS_DATA_TYPE = "Data Type"
STREAMS_TRANSFORM_IDS = "Transform IDs"
STREAMS_SAMPLES = "Samples"
STREAMS_SAMPLE_RATE = "Sample Rate"
STREAMS_METADATA = "Enable Metadata"
STREAMS_TIMED_DATA = "Timed Data"
STREAMS_BUFFER_SIZE = "Buffer Size"
STREAMS_PROCESSOR = "Processor"

STREAM_DATA_TYPES = {
    'DATA16': 0,   # 16-bit DATA, will be stored big endian.
    'PCM16': 1,    # 16-bit PCM, will be stored little endian.
    'PCM24': 2,    # 24-bit PCM, will be stored little endian.
    'PCM32': 3,    # 32-bit PCM, will be stored little endian.
    'DATA32': 4,   # 32-bit DATA, will be stored big endian.
    'TTR': 5       # 32-bit DATA, little endian
}

OUTPUT_FILENAME = "Output Filename"
ADD_DATETIME_TO_OUTPUT_FILE = "Add Date-Time to the Output Filename"
USE_BUILTIN_CAP = "Use the built-in capability"
