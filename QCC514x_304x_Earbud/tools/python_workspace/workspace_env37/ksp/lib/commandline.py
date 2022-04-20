#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Core KSP Command Line Interface."""
import logging
import os

from cmd import Cmd

import ksp.lib.namespace as ns
from ksp.lib import configurations
from ksp.lib.common import get_input
from ksp.lib.exceptions import ConfigurationError
from ksp.lib.logger import method_logger
from ksp.lib.pretty import PrettyDictionaryPrinter
from ksp.lib.run import KymeraStreamProbeRun
from ksp.lib.types import Stream

logger = logging.getLogger(__name__)

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))


class InteractiveBaseCLI(Cmd, object):
    """CLI Base"""
    @method_logger(logger)
    def __init__(self):
        super(InteractiveBaseCLI, self).__init__(
            completekey='tab',
            stdin=None,
            stdout=None
        )


class KymeraStreamProbeCLI(KymeraStreamProbeRun, InteractiveBaseCLI):
    """Kymera Stream Probe CLI.

    Args:
        device (object): A device instance from pydbg tool.
    """
    prompt = 'KSP> '
    intro = (
        "Welcome to Kymera Stream Probe (KSP) CLI!\n"
        "To get the list of commands typel `help`. For command helps\n"
        "use `help [COMMAND]`. Use `exit` command to quit."
    )

    STREAM_DATA_TYPES = ['PCM16', 'PCM24', 'PCM32', 'DATA16', 'DATA32', 'TTR']

    @method_logger(logger)
    def __init__(self, device):
        super(KymeraStreamProbeCLI, self).__init__(device)
        self._init_configurations()
        self._wait_input = True

    @method_logger(logger)
    def postcmd(self, stop, line):
        """Executes after each input command."""
        print('')
        return super(KymeraStreamProbeCLI, self).postcmd(stop, line)

    @method_logger(logger)
    def do_remove_stream(self, stream_number):
        """Remove the given stream number from streams."""
        try:
            stream_number = int(stream_number)
            if stream_number in self._config[ns.STREAMS].keys():
                del self._config[ns.STREAMS][stream_number]
                return

            logger.error("Stream number not found.")
            return

        except ValueError:
            logger.error("Stream number should be an integer.")
            return

    # BUG: B-299459 "Refactor Stream object"
    # pylint: disable=too-many-locals,too-many-branches,too-many-statements
    # pylint: disable=too-many-return-statements
    @method_logger(logger)
    def do_config_stream(self, stream_number):
        """Configure a stream.

        The command asks stream parameters from the user and populate the
        CLI's configuration based on them. CLI uses these parameters to start
        KSP and perform appropriate extractions.

        Args:
            stream_number (int): The stream number.
        """
        if not stream_number.strip():
            logger.error("Please enter a valid stream number.")
            return
        try:
            stream_number = int(stream_number)
            if stream_number not in Stream.SUPPORTED_STREAMS:
                logger.error("Entered stream number is not supported.")
                return

        except ValueError:
            logger.error("Stream number should be an integer.")
            return

        # If the stream is already configured, read the current values and
        # use its properties as defaults.
        if stream_number in self._config[ns.STREAMS]:
            stream = self._config[ns.STREAMS][stream_number]
            default_data_type = stream[ns.STREAMS_DATA_TYPE]
            default_transform_ids = stream[ns.STREAMS_TRANSFORM_IDS]
            # Optional attributes.
            default_samples = stream.get(ns.STREAMS_SAMPLES, 0)
            default_sample_rate = stream.get(ns.STREAMS_SAMPLE_RATE, 0)
            default_buffer_size = stream.get(ns.STREAMS_BUFFER_SIZE, 0)
            default_processor = stream.get(ns.STREAMS_PROCESSOR, 0)
            default_metadata = "n"
            default_timed_data = ""
            if default_data_type == "TTR":
                default_metadata_bool = stream.get(ns.STREAMS_METADATA, False)
                default_timed_data = stream.get(ns.STREAMS_TIMED_DATA, "")
                if default_metadata_bool:
                    default_metadata = "y"
        else:
            default_data_type = None
            default_transform_ids = None
            default_samples = 0
            default_sample_rate = 0
            default_buffer_size = 0
            default_processor = 0
            default_metadata = "n"
            default_timed_data = ""

        kw_opts = {}

        processor = get_input(ns.STREAMS_PROCESSOR, default_processor)
        try:
            kw_opts['processor'] = int(processor)
        except ValueError:
            logger.error("%s should be either 0 or 1.", ns.STREAMS_PROCESSOR)
            return


        data_type = get_input(ns.STREAMS_DATA_TYPE, default_data_type)
        if data_type.upper() == 'TTR':
            transform_help = "Space separated IDs or 'all' or 'none'"
        else:
            transform_help = "Space separated IDs"
        transform_ids = get_input(
            '{field} ({help})'.format(
                field=ns.STREAMS_TRANSFORM_IDS,
                help=transform_help
            ),
            default_transform_ids
        ).split()

        # Optional samples for stream.
        try:
            kw_opts['samples'] = int(
                get_input(ns.STREAMS_SAMPLES, default_samples)
            )
        except ValueError:
            logger.error("Samples must be in integer.")
            return

        # Optional metadata enable for TTR streams
        if data_type.upper() == 'TTR':
            metadata_str = get_input(
                '{field} (y/n)'.format(
                    field=ns.STREAMS_METADATA
                ),
                default_metadata
            )
            kw_opts['metadata'] = metadata_str[0] in "YyTt"

        else:
            kw_opts['metadata'] = False

        if data_type.upper() == 'TTR':
            kw_opts['timed_data'] = get_input(
                '{field} (source endpoint ID, 0 to disable)'.format(
                    field=ns.STREAMS_TIMED_DATA
                ),
                default_timed_data
            )

        else:
            kw_opts['timed_data'] = ""

        # Optional sample rate for the stream.
        if data_type.upper() == 'TTR':
            kw_opts['sample_rate'] = 0
        else:
            try:
                kw_opts['sample_rate'] = int(
                    get_input(ns.STREAMS_SAMPLE_RATE, default_sample_rate)
                )
            except ValueError:
                logger.error("Sample Rate must be in integer.")
                return

        # Optional internal buffer size for the stream.
        try:
            kw_opts['buffer_size'] = int(
                get_input(ns.STREAMS_BUFFER_SIZE, default_buffer_size)
            )
        except ValueError:
            logger.error("Buffer size must be in integer.")
            return

        try:
            stream = Stream(stream_number, data_type, transform_ids, **kw_opts)
            # Remove the stream number from the dictionary as ``self._config``
            # already knows about the ``stream_number``.
            # BUG: B-299459 "Refactor Stream object"
            # pylint: disable=unsupported-delete-operation
            del stream[ns.STREAMS_STREAM]
            # pylint: enable=unsupported-delete-operation

            self._config[ns.STREAMS][stream_number] = stream
        except ConfigurationError as error:
            logger.error(error)

    # BUG: B-299459 "Refactor Stream object"
    # pylint: enable=too-many-locals,too-many-branches,too-many-statements

    @method_logger(logger)
    def do_config(self, _):
        """Display the current configurations."""
        printer = PrettyDictionaryPrinter(
            4,
            self._config,
            title="Current Configurations:"
        )
        printer.pprint()

    @method_logger(logger)
    def do_use_builtin_cap(self, arg):
        """Use the built-in KSP capability.

        Some of the chips have the built-in KSP capability and don't need a
        downloadable. Set this configuration only if this is the case.

        The arguments can be `true` and `false`. The value is case insensitive.
        """
        if arg.lower() == 'true':
            self._config[ns.USE_BUILTIN_CAP] = True
        elif arg.lower() == 'false':
            self._config[ns.USE_BUILTIN_CAP] = False
        else:
            logger.error(
                "Entered value is not recognised. "
                "Accepted values are `true` and `false`."
            )

    @method_logger(logger)
    def do_set_output_filename(self, arg):
        """Set the output filename, where the data is going to be saved."""
        directory = os.path.dirname(arg)
        if directory and os.path.exists(directory) is False:
            os.makedirs(directory)

        if self._check_output_file_permission(arg):
            self._config[ns.OUTPUT_FILENAME] = arg

    @method_logger(logger)
    def do_add_datetime_to_filename(self, arg):
        """Adds date-time template to the output filename.

        The argument can be one of the strings `true` and `false`. The value
        is case insensitive.
        """
        if arg.lower() == 'true':
            self._config[ns.ADD_DATETIME_TO_OUTPUT_FILE] = True
        elif arg.lower() == 'false':
            self._config[ns.ADD_DATETIME_TO_OUTPUT_FILE] = False
        else:
            logger.error(
                "Entered value is not recognised. "
                "Accepted values are `true` and `false`."
            )

    @method_logger(logger)
    def do_start(self, _):
        """Start the KSP with the current configurations.

        If the operator is running, it stops it first and then
        the application will reconfigure it before starting it again.
        """
        if not self._config[ns.OUTPUT_FILENAME].strip():
            logger.error("You must specify the output filename.")
            return
        if not self._config[ns.STREAMS]:
            logger.error("You must configure at least one stream.")
            return

        success = self.run_configured()

        if success:
            # Operation was successful, save the configuration.
            configurations.save(**self._config)

    def do_start_op(self, _):
        """Setup and configure the KSP operator in Kymera.

        This function is only setting up the KSP operator in the connected
        chip from the KSP downloadable.
        """
        if not self._config[ns.STREAMS]:
            logger.error("You must configure at least one stream.")
            return

        self.start_op()

    @method_logger(logger)
    def do_stop_op(self, _):
        """Stop the running KSP operator."""
        self.stop_op()

    @method_logger(logger)
    def do_start_reader(self, _):
        """Start the Reader and save the incoming data to the output filename"""
        self.start_reader()

    # This method can not be static as it's integral to how the command
    # prompt works.
    # pylint: disable=no-self-use
    @method_logger(logger)
    def do_exit(self, _):
        """Exits the KSP app."""
        print("Thanks for using Kymera Stream Probe (KSP). Goodbye!")
        return True
    # pylint: enable=no-self-use

    def _init_configurations(self):
        # Check if there is any saved configurations. If nothing found, this
        # method populates the ``self._config`` with default values.
        config = configurations.retrieve()
        streams = config.get(ns.STREAMS)
        if streams:
            self._config[ns.STREAMS] = {}
            for num, stream_dict in streams.items():
                self._config[ns.STREAMS][int(num)] = stream_dict
        else:
            self._config[ns.STREAMS] = {}

        self._config[ns.OUTPUT_FILENAME] = config.get(ns.OUTPUT_FILENAME, '')
        self._config[ns.ADD_DATETIME_TO_OUTPUT_FILE] = config.get(
            ns.ADD_DATETIME_TO_OUTPUT_FILE, True
        )
        self._config[ns.USE_BUILTIN_CAP] = config.get(ns.USE_BUILTIN_CAP, False)
