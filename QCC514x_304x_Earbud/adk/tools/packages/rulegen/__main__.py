"""
Copyright (c) 2021 Qualcomm Technologies International, Ltd.
Wrapper script for running rule generation module
"""

from . import rulegen

if rulegen.main() is True:
    exit(0)
else:
    exit(1)
