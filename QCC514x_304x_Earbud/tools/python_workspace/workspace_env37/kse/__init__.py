#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""KATS main package file"""

import os

from ._version import __version__, version_info

BIN_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), 'bin'))
try:
    DIRS = os.listdir(BIN_DIR)
except Exception:  # pylint:disable=broad-except
    DIRS = []

for directory in DIRS:
    directory = os.path.join(BIN_DIR, directory)
    if os.path.isdir(directory):
        os.environ.setdefault('PATH', '')
        os.environ['PATH'] += os.pathsep + directory
