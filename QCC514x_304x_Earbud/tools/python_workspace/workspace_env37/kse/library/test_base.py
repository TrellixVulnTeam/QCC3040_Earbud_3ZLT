#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
# All rights reserved.
# Qualcomm Technologies International, Ltd. Confidential and Proprietary.
#
"""Test base library"""

from kats.library.registry import register_instance, get_instance_num, get_instance

TEST_BASE = 'test_base'


def register_test_base_instance(instance):
    """Register test base instance

    This should be called before rest of functions in this library are usable

    Args:
        instance (any): Test base instance
    """
    register_instance(TEST_BASE, instance)


def add_output_file(filename):
    """Add file to test output files

    Args:
        filename (str): Path to filename
    """
    if get_instance_num(TEST_BASE):
        test_base = get_instance(TEST_BASE)
        test_base.result_add_output_file(filename)
