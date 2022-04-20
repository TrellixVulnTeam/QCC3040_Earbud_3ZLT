#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""aanclogger doc plugin finder"""

import os


def get_doc():
    """Gives documentation information."""
    current_directory = os.path.dirname(os.path.realpath(__file__))
    doc_index = os.path.join(current_directory, 'docs', 'index.html')
    return dict(
        title="aanclogger documentation",
        location=doc_index,
        summary="AANC Logging Tool Documentation",
        rank=12
    )
