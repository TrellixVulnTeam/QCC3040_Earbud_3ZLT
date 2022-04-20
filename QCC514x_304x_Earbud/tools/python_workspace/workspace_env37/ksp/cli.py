#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Entry point for KSP CLI.

The script instantiates Pydbg external library and pass it on to the Command
Line controller.
"""
import logging

from csr.front_end.pydbg_front_end import PydbgFrontEnd
from ksp.lib.commandline import KymeraStreamProbeCLI
from ksp.lib.common import get_input
from ksp.lib.logger import function_logger
from ksp.lib.scripted import KymeraStreamProbeScripted
from ksp.lib.types import RunMode

logger = logging.getLogger(__name__)


@function_logger(logger)
def _get_device(arguments):
    pydbg_config = {
        'device_url': arguments.device_url,
    }
    if arguments.firmware_build:
        pydbg_config['firmware_builds'] = 'apps1:{}'.format(
            arguments.firmware_build
        )

    try:
        device, _ = PydbgFrontEnd.attach(pydbg_config)

    except NotImplementedError as error:
        raise RuntimeError("Pydbg error: %s" % error)

    return device


@function_logger(logger)
def _run_standalone(device, arguments):
    scripted = KymeraStreamProbeScripted(device)
    result = scripted.run(arguments)
    return result


@function_logger(logger)
def _run_interactive(device):
    try:
        device.chip.apps_subsystem.p1.fw.call.get_signature('OperatorCreate')

    except (UnboundLocalError, RuntimeError) as error:
        raise RuntimeError(
            "Given firmware doesn't match the chip. Detail: {}".format(
                error
            )
        )

    # Get interactive mode object.
    try:
        cli = KymeraStreamProbeCLI(device)

    except NotImplementedError as error:
        raise RuntimeError(error)

    cli.cmdloop()


def _adjust_trb_clock_speed(device):
    """Check and adjust the TRB clock speed.

    If the speed is too low, i.e. less than 40 MHz, it gives the option to
    user to increase it.
    """
    speed = int(device.transport.get_freq_mhz())

    if speed >= 40:
        return

    logger.warning(
        (
            "The TRB Clock speed is %s MHz and it is too low. The KSP may not "
            "be able to work properly. The acceptable speed is 40 MHz or "
            "higher. "
        ),
        speed
    )

    new_speed = int(get_input("Enter the new speed in MHz", default=40))
    device.transport.set_freq_mhz(new_speed)
    logger.info(
        "The TRB Clock speed is changed to %s for this session.",
        new_speed
    )


@function_logger(logger)
def main(arguments):
    """Start the TRB and the Interactive Console.

    Args:
        arguments (namespace): A namespace instance of argparse.

    Returns:
        int: 1 if there is an error, 0 otherwise.
    """

    try:
        device = _get_device(arguments)
        _adjust_trb_clock_speed(device)

        if arguments.run_mode == RunMode.NON_INTERACTIVE:
            _run_standalone(device, arguments)
        elif arguments.run_mode == RunMode.INTERACTIVE:
            _run_interactive(device)

    except RuntimeError as error:
        logger.error(error)
        return 1

    return 0
