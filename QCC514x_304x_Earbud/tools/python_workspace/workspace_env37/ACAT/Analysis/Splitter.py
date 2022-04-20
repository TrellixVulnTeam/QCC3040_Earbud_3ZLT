############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2020 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Splitter Operators Analysis.

Module to analyse Splitter operators.
"""
from ACAT.Analysis import Opmgr
from ACAT.Core import CoreUtils as cu
from ACAT.Core.exceptions import InvalidDmAddressError
from ACAT.Core.CoreTypes import ChipVarHelper as ch


class Splitter(Opmgr.Operator):
    """
    Class representing a Splitter operator.
    """

    @staticmethod
    def str_splitter_metadata(extra_op_data, helper, buffer_analysis):
        """Function which returns the string representation of a
        splitter operator internal metadata.

        Args:
            extra_op_data - Pointer to the operator's extra information.
            helper - Helper object used to access the chipdata.
            buffer_analysis - The Buffer analysis.
        Returns:
            String representation of the splitter operator metadata.
        """
        op_str = ""
        # check the metadata first
        internal_metadata = extra_op_data['internal_metadata']
        if internal_metadata is None:
            raise AttributeError()
        internal_metadata_buffer_p = internal_metadata['buffer'].value
        if internal_metadata_buffer_p != 0:
            metadata_read_index_output = internal_metadata['prev_rd_indexes']
            # get the proper variable
            internal_metadata_buffer = helper.chipdata.cast(
                internal_metadata_buffer_p,
                "tCbuffer"
            )
            # don't handle errors here as this buffer must have metadata!
            metadata = internal_metadata_buffer['metadata'].value
            metadata = helper.chipdata.cast(metadata, "metadata_list")
            metadata_size = metadata['buffer_size'].value
            metadata_write_index = metadata['prev_wr_index'].value
            # ok now display the buffer usage.
            for output_index in range(2):
                data = metadata_write_index - metadata_read_index_output[output_index].value
                if data < 0:
                    data += metadata_size
                percent = (float(data) * 100) / metadata_size
                op_str += "Channel %d : usage %3.2f%% data %d size %d\n" % (
                    output_index, percent, data, metadata_size
                )
            op_str += "Internal Metadata buffer"
            op_str += Opmgr.insert_spaces(str(internal_metadata_buffer))
            metadata_buffer = buffer_analysis.inspect_cbuffer(internal_metadata_buffer_p, True)
            op_str += Opmgr.insert_spaces(str(metadata_buffer.metadata_list))
        return op_str


    @staticmethod
    def str_splitter_chan_buff(buffer_p, helper, buffer_analysis, display_content):
        """Function which returns the string representation of a splitter buffer.

        Args:
            buffer_p - Pointer to the buffer.
            helper - helper object used to access the chipdata.
            buffer_analysis - The Buffer analysis.
            display_content - If true the buffer content is displayed.
        Returns:
            String representation of the splitter operator buffer.
        """
        op_str = ""
        if buffer_p != 0:
            cbuffer_var = helper.chipdata.cast(buffer_p, "tCbuffer")
            op_str += Opmgr.insert_spaces(str(cbuffer_var))
            buff_var = buffer_analysis.inspect_cbuffer(buffer_p, True)
            op_str += Opmgr.insert_spaces(str(buff_var))
            if display_content:
                base_addr = cbuffer_var['base_addr'].value
                op_str += Opmgr.insert_spaces(
                    "valid address range 0x%08x = 0x%08x" % (
                        base_addr,
                        base_addr + cbuffer_var['size'].value - 1
                    )
                )
                # Display the content of the buffer.
                try:
                    op_str += Opmgr.insert_spaces(
                        cu.mem_dict_to_string(
                            buffer_analysis._get_content(buff_var)
                        )
                    )
                except InvalidDmAddressError:
                    op_str += "\n" + Opmgr.insert_spaces(
                        "Cannot access buffer content. Possibly in SRAM.\n"
                    )
        else:
            op_str += Opmgr.insert_spaces("NULL")
        return op_str


    @staticmethod
    def str_splitter_channels(extra_op_data, helper, buffer_analysis):
        """Function which returns the string representation of a
        splitter operator channels.
        Args:
            extra_op_data - Pointer to the operator's extra information.
            helper - Helper object used to access the chipdata.
            buffer_analysis - The Buffer analysis.
        Returns:
            String representation of the splitter operator channel.
        """
        op_str = ""
        channels = extra_op_data['channel_list']
        if channels is None:
            raise AttributeError()
        for channel in ch.parse_linked_list(channels, "next"):
            input_buff = channel['input_buffer'].value
            channel_id = channel['id'].value
            op_str += "Channel %d info \n" % channel_id
            op_str += str(channel)
            # Display the input channel
            op_str += "\nchannel %d input:\n" % channel_id
            op_str += Splitter.str_splitter_chan_buff(
                input_buff, helper, buffer_analysis, False
            )

            # The internal buffer content could be very intresting
            internal_buff = channel['internal']['buffer'].value
            op_str += "channel %d internal buffer:\n" % channel_id
            op_str += Splitter.str_splitter_chan_buff(
                internal_buff, helper, buffer_analysis, True
            )

            # Display the output channels.
            output_buffers = channel['output_buffer']
            for index, output_buffer in enumerate(output_buffers):
                output_buff = output_buffer.value
                op_str += "channel %d output buffer[%d]:\n" % (channel_id, index)
                op_str += Splitter.str_splitter_chan_buff(
                    output_buff, helper, buffer_analysis, False
                )
        return op_str


    def _desc_str(self):
        """Descriptor string function for the splitter capability.
        This will be the to string (__str__) function of a splitter operator.

        Args:
            self - Pointer to the splitter operator object.
        Returns:
            String representation of the splitter operator.
        """
        # get the standard to string.
        final_op_str = Opmgr.Operator._desc_str(self)
        try:

            buffers = self.helper.interpreter.get_analysis(
                "buffers", self.helper.chipdata.processor
            )
            op_str = ""
            op_str += self.str_splitter_metadata(
                self.extra_op_data, self.helper, buffers
            )
            op_str += self.str_splitter_channels(
                self.extra_op_data, self.helper, buffers
            )

            final_op_str = final_op_str + "Additional Info:\n" + Opmgr.insert_spaces(op_str)
        except AttributeError:  # Any error? just fall back to the standard display.
            pass

        return final_op_str
