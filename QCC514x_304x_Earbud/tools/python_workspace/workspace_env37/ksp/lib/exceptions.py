#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KSP exception classes."""


class ArgumentNotFound(AttributeError):
    """When an invalid input argument is requested.

    This happens when an invalid argument is requested from an instance of
    KSPArgument.
    """


class FirmwareError(RuntimeError):
    """The communication with the firmware is failed."""


class DownloadableError(FirmwareError):
    """If downloadable fails to do its job."""


class OperatorError(FirmwareError):
    """If the operator failed to do its job."""


class CommandError(RuntimeError):
    """Given command is invalid."""


class StreamDataError(RuntimeError):
    """When unexpected circumstances is detected."""


class ConfigurationError(RuntimeError):
    """When something goes wrong during a configuration process."""


class InvalidTransformID(ValueError):
    """When a given ID isn't a transform ID."""


class InvalidTransformIDList(ValueError):
    """When a value does not represent a valid transform ID list."""
