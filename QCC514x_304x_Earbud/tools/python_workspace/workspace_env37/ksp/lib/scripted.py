############################################################################
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
############################################################################
"""KSP scripted front end."""
import logging
import os

import ksp.lib.namespace as ns
from ksp.lib.exceptions import ConfigurationError
from ksp.lib.logger import method_logger
from ksp.lib.pretty import PrettyDictionaryPrinter
from ksp.lib.run import KymeraStreamProbeRun

logger = logging.getLogger(__name__)

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))


class KymeraStreamProbeScripted(KymeraStreamProbeRun):
    """Kymera Stream Probe scripted front end.

    Args:
        device (object): A device instance from pydbg.
    """

    @method_logger(logger)
    def __init__(self, device):
        super(KymeraStreamProbeScripted, self).__init__(device)

    @method_logger(logger)
    def _fetch_args(self, arguments):
        """Constructs the configuration from the arguments.

        Reassemble the command line arguments into a configuration
        dictionary as expected by the run method.
        """
        if not getattr(arguments, 'stream_config', None):
            raise ConfigurationError(
                "At least one stream must be configured "
                "(-s or --stream_config).")
        self._wait_input = getattr(arguments, 'wait_input', False)
        self._duration = getattr(arguments, 'duration', 0)
        if not self._wait_input and not self._duration:
            raise ConfigurationError(
                "One of duration (-d, --duration) or wait-for-input "
                "(-w, --wait-input) must be specified.")
        if self._wait_input and self._duration:
            raise ConfigurationError(
                "Only one of duration (-d, --duration) or wait-for-input "
                "(-w, --wait-input) may be specified.")

        output_file_name = getattr(arguments, 'output_file_name', None)
        if not output_file_name:
            raise ConfigurationError(
                "An output file name (-o, --output) must be specified.")

        self._config[ns.OUTPUT_FILENAME] = output_file_name
        self._config[ns.STREAMS] = arguments.stream_config
        self._config[ns.USE_BUILTIN_CAP] = arguments.use_builtin_capability

        # By default, KSP adds datetime to the output filename. However, in
        # batch mode user can choose not to have this feature by passing the
        # right argument.
        add_datetime = not arguments.remove_datetime
        self._config[ns.ADD_DATETIME_TO_OUTPUT_FILE] = add_datetime

    @method_logger(logger)
    def _make_output_dir(self):
        """Ensure directory for output file exists."""
        directory = os.path.split(self._config[ns.OUTPUT_FILENAME])[0]
        if directory and (os.path.exists(directory) is False):
            os.makedirs(directory)

    @method_logger(logger)
    def run(self, arguments):
        """Run the probe.

        Args:
            arguments: The namespace returned by argparse.

        Return:
            int: An exit code: 0 for success, 1 for failure.
        """
        try:
            self._fetch_args(arguments)
        except ConfigurationError as error:
            raise RuntimeError('In non-interactive mode: %s' % error)

        printer = PrettyDictionaryPrinter(
            4,
            self._config,
            title="Configuration:"
        )
        printer.pprint()

        self._make_output_dir()

        success = self.run_configured()

        if not success:
            return 1
        return 0
