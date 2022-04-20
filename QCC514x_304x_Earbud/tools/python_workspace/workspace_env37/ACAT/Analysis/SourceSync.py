############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Source Sync Operators Analysis.

Module to analyse Source Sync operators.
"""
from ACAT.Analysis import Opmgr
from ACAT.Core import CoreUtils as cu
from ACAT.Core.CoreTypes import ChipVarHelper as ch


class SourceSync(Opmgr.Operator):
    """
    Class representing a Source Sync operator.
    """
    def __init__(self, op_entry, helper, cap_data):
        super(SourceSync, self).__init__(op_entry, helper, cap_data)

        if self.extra_op_data is None:
            return

        formatter_dict = {
            "sinks":ch.deref,
            "sources":ch.deref,
            "sink_groups":ch.deref_with_formatter_dict(
                {
                    "common":ch.linked_list_with_formatter_dict(
                        next_name="next",
                        formatter_dict={
                                "terminals": ch.linked_list("next"),
                                "sample_rate": ch.value_format(cu.u32_to_s32),
                            }
                    )
                }
            ),
            "source_groups":ch.deref_with_formatter_dict(
                {
                    "common":ch.linked_list_with_formatter_dict(
                        next_name="next",
                        formatter_dict={
                                "terminals": ch.linked_list("next"),
                                "sample_rate": ch.value_format(cu.u32_to_s32),
                            }
                    )
                }
            ),
            "cur_params":{
                "OFFSET_SS_PERIOD":
                    ch.value_format(cu.qformat_factory(1, 31)),
                "OFFSET_SS_MAX_PERIOD":
                    ch.value_format(cu.qformat_factory(6, 26)),
                "OFFSET_SS_MAX_LATENCY":
                    ch.value_format(cu.qformat_factory(6, 26)),
                "OFFSET_SS_OUTPUT_SPACE":
                    ch.value_format(cu.qformat_factory(6, 26)),
                "OFFSET_SS_P3":
                    ch.value_format(cu.qformat_factory(6, 26)),
                "OFFSET_SS_STALL_RECOVERY_DEFAULT_FILL":
                    ch.value_format(cu.qformat_factory(6, 26)),
                "OFFSET_SS_STALL_RECOVERY_CATCHUP_LIMIT":
                    ch.value_format(cu.qformat_factory(1, 31, multiplier=1000))
            }


        }
        self.extra_op_data.set_formatter_dict(formatter_dict)
