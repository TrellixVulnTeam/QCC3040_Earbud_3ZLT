#
# Copyright (c) 2019-2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries.  All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Common functions that can be used in multiple places."""
import logging

from ksp.lib.logger import function_logger

logger = logging.getLogger(__name__)


@function_logger(logger)
def get_input(question, default=None):
    """Asks a question to the user and return user's answer."""
    if default is not None:
        if isinstance(default, (tuple, list)):
            default = ' '.join(default)
        question = '{} [{}]: '.format(question, default)

    else:
        question = '{}: '.format(question)

    answer = input(question)
    if not answer.strip() and default is not None:
        answer = default

    return answer
