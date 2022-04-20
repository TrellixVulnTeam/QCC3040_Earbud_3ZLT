#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Wrapper to call the LrwStreamExtractor."""
import logging
import os

import ksp.lib.namespace as ns
from ksp.lib.extractor import LrwStreamExtractor
from ksp.lib.logger import function_logger
from ksp.lib.pretty import PrettyDictionaryPrinter
from ksp.lib.types import Stream

logger = logging.getLogger(__name__)


@function_logger(logger)
def extract(output_file_name, configured_streams):
    """Extracts streams from a raw file.

    Extract sample rates from Streams configuration objects
    and call the LrwStreamExtractor.

    Args:
        output_file_name (str) Output file name.
        configured_streams: Dictionary of Streams config objects
            by stream number.
    """
    if os.path.isfile(output_file_name) is False:
        print("WARNING: Output file does not exist!")
        return

    if os.path.getsize(output_file_name) == 0:
        print("WARNING: Output file is empty!")
        return

    # Get all configured sample rates to the extractor.
    sample_rates = {}

    for stream_id in Stream.SUPPORTED_STREAMS:
        if stream_id in configured_streams:
            stream = configured_streams[stream_id]
            sample_rates[stream_id] = stream.get(
                ns.STREAMS_SAMPLE_RATE, 0
            )

    extractor = LrwStreamExtractor(
        output_file_name,
        sample_rates=sample_rates
    )
    result = extractor.extract()

    printer = PrettyDictionaryPrinter(4, result, title="\nStreams:")
    printer.pprint()
