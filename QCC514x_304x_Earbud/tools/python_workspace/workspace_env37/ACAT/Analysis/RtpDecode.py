############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""RTP Decode Operators Analysis.

Module to analyse RTP decode operators.
"""
from ACAT.Analysis import Opmgr
from ACAT.Core import CoreUtils as cu


class RtpDecode(Opmgr.Operator):
    """
    Class representing a RTP decode operator.
    """
    @staticmethod
    def get_data_octet_based(buffers, buffer_p, usable_octets):
        """Converts a list with 32 bit data to a list containing 8bit data.
        In other words, this funciton transform a list of words to a list of bytes.

        Args:
            buffers - The Buffer analysis which is used to get the content of the
                buffer.
            buffer_p - Pointer to the buffer.
            usable_octets - How many octets are used in a words.
        Returns:
            List containing the octets values.
        """
        byte_list = []
        for i in buffers.get_content(buffer_p, "read"):
            if usable_octets == 4:
                byte_list.append((i >> 24) & 0xff)
                byte_list.append((i >> 16) & 0xff)
            byte_list.append((i >> 8) & 0xff)
            byte_list.append(i & 0xff)

        byte_list.reverse()
        return byte_list
    @staticmethod
    def display_rtp_op_buffer(buffer_name, buffer_holder_variable, buffer_analysis):
        """Returns the string representation of an RTP operator buffer.

        Args:
            buffer_name - The name of the buffer.
            buffer_holder_variable - The variable holding the buffer details.
            buffer_analysis - The Buffer analysis.
        Returns:
            String representation of the RTP buffer.

        Note:
        - Buffer analysis must be provided to represent the buffer.
        - Buffer name should be a valid path in the extra operator data.
        """
        op_str = ""
        try:
            op_buffer = buffer_holder_variable[buffer_name].value
            op_str += "\n " + buffer_name + ":\n"
            buff_var = buffer_analysis.inspect_cbuffer(op_buffer, True)
            op_str += Opmgr.insert_spaces(str(buff_var))
            op_str += cu.mem_dict_to_string(buffer_analysis._get_content(buff_var))
        except AttributeError:
            pass
        return op_str

    def _desc_str(self):
        """
        Descriptor string function for the RTP capability.
        This will be the to string (__str__) function of a RTP operator.

        Args:
            self - Pointer to the RTP operator object.
        Returns:
            String representation of the RTP operator.
        """
        op_str = Opmgr.Operator._desc_str(self)
        buffers = self.helper.interpreter.get_analysis(
            "buffers", self.helper.chipdata.processor
        )
        # Display all internal buffers. The input and output should already
        # be displayed in stream analysis.
        op_str += self.display_rtp_op_buffer(
            "clone_frame_buffer", self.extra_op_data, buffers
        )
        op_str += self.display_rtp_op_buffer(
            "frame_buffer", self.extra_op_data, buffers
        )
        op_str += self.display_rtp_op_buffer(
            "internal_buffer", self.extra_op_data, buffers
        )
        return op_str
