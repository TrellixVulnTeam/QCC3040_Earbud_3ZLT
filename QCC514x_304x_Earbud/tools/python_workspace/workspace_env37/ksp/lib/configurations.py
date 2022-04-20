#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Save and retrieve KSP configurations."""
import json
import logging
import os

from ksp.lib.logger import function_logger
from ksp.lib.namespace import CONFIGURATION_FILE

logger = logging.getLogger(__name__)


@function_logger(logger)
def retrieve():
    """Reads the saved configuration and return it.

    Returns:
        dict: If the configuration file does not exist, an empty
            dictionary will be returned.
    """
    if os.path.isfile(CONFIGURATION_FILE):
        with open(CONFIGURATION_FILE) as handler:
            config = json.load(handler, object_hook=_object_hook)

        return config

    return {}


@function_logger(logger)
def save(**kwargs):
    """Saves the configuration into the configuration json file

    Args:
        kwargs: Arbitrary key value parameters.
    """
    if os.path.isfile(CONFIGURATION_FILE):
        with open(CONFIGURATION_FILE) as handler:
            config = json.load(handler)
    else:
        config = {}

    for key, val in kwargs.items():
        config[key] = val

    with open(CONFIGURATION_FILE, 'w') as handler:
        json.dump(config, handler)


def _object_hook(data):
    if isinstance(data, dict):
        new_dict = {}
        for key, value in data.items():
            new_dict[key] = _object_hook(value)
        return new_dict

    if isinstance(data, (list, tuple)):
        new_list = []
        for item in data:
            new_list.append(_object_hook(item))

        return new_list

    # Otherwise.
    return data
