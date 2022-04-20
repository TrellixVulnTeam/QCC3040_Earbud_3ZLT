#
# Copyright (c) 2020 Qualcomm Technologies, Inc. and/or its subsidiaries.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Entry points functions."""
import os

# Needed for the package version reporting, `ksp.__version__`.
from ._version import __version__, version_info

CURRENT_DIRECTORY = os.path.dirname(os.path.realpath(__file__))


def get_documents():
    """Gives documentation information."""
    doc_index = os.path.join(CURRENT_DIRECTORY, 'docs', 'index.html')
    return dict(
        title="KSP {} Documentation".format(__version__),
        location=doc_index,
        summary="Kymera Stream Probe User Guide.",
        rank=12
    )
