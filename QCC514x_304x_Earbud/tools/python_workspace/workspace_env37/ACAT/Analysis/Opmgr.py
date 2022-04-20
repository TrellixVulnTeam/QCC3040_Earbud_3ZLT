############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2019 Qualcomm Technologies, Inc. and/or its
# subsidiaries. All rights reserved.
#
############################################################################
"""Operator Manager Analysis.

Module to analyse the operator manager.
"""
import logging
import re
import time as import_time

try:
    # pylint: disable=redefined-builtin
    from future_builtins import hex
except ImportError:
    pass

from . import Analysis
from ..Core import Arch
from ACAT.Core.CoreTypes import ChipVarHelper as ch
from ACAT.Core.exceptions import (
    UsageError, AnalysisError, BundleMissingError,
    DebugInfoNoVariableError, InvalidDebuginfoTypeError,
    DebugInfoNoLabelError, InvalidDmLengthError
)

VARIABLE_DEPENDENCIES = {
    'strict': ('$_capability_data_table', '$_oplist_head')
}
ENUM_DEPENDENCIES = {'strict': ('STOP_KICK', 'OP_STATE', 'CAP_ID', 'OPCMD_ID')}
TYPE_DEPENDENCIES = {
    'OPERATOR_DATA': (
        'con_id', 'id', 'task_id', 'creator_client_id', 'cap_data', 'state',
        'stop_chain_kicks', 'kick_propagation_table', 'extra_op_data'
    ),
    'KP_TABLE': (
        'table_length', 'table', 'num_op_sources', 'num_op_sinks',
        'num_ep_sources', 'num_ep_sinks'
    ),
    'KP_ELEMENT': ('kt', 'kt.ep'),
    'BGINT': ('id',),
    'CAPABILITY_DATA': (
        'id', 'version_msw', 'version_lsw', 'max_sinks', 'max_sources',
        'handler_table', 'opmsg_handler_table', 'process_data',
        'instance_data_size'
    )
}

logger = logging.getLogger(__name__)


# Here's a quick sketch to aid digestion of the different terms below.
#
# +---------------------------+
# |          0x4040           |  <-- op_ep_id
# |                           |
# | +--------+    +--------+  |
# | | 0xe040 |    | 0x6040 |  |  <-- ep_ids
# | +--------+    +--------+  |
# |   Sink 0        Src 0     |  <-- port numbers
# +---------------------------+
#          operator 0            <-- opid


class Operator(object):
    """Encapsulates an operator.

    Args:
        op_entry: Operator Chipdata variable (OPERATOR_DATA structure).
        helper: A helper Analysis which in this case is Opmgr.
        cap_data: Capability Chipdata variable (CAPABILITY_DATA structure).
    """

    def __init__(self, op_entry, helper, cap_data):
        # Operator variables are initialised from an existing ct.Variable
        # extracted from the oplist. Also takes an Analysis object as a
        # 'helper' so that we can make use of analysis fundamentals like
        # cast().

        # All of our info will come from the op_entry structure.
        # This has been cast from malloc'd data so members are difficult
        # to get at.
        self.helper = helper
        self.op_entry = op_entry  # For posterity

        # Connection ID
        self.con_id = op_entry['con_id'].value

        # operator ID
        self.id = op_entry['id'].value

        # Task ID
        self.task_id = op_entry['task_id'].value

        # Creator client ID
        self.creator_client_id = op_entry['creator_client_id'].value

        # Flag indicating running / stopped etc. state
        self.state = op_entry['state'].value

        #  Field indicating the direction in which the kicks are ignored.
        self.stop_chain_kicks = helper.debuginfo.get_enum(
            'STOP_KICK', op_entry['stop_chain_kicks'].value
        )[0]

        # Pointer to a next operator in a list, e.g. kept my OpMgr
        # next used for storing the operator data

        try:
            # logging_enabled doesn't exist on really old builds
            self.logging_enabled = op_entry['logging_enabled'].value
        except AttributeError:
            self.logging_enabled = None

        try:
            # additional operator debug informations
            self.pio_num = op_entry['pio_num'].value
            self.pio_pattern = op_entry['pio_pattern'].value
        except AttributeError:
            self.pio_num = None
            self.pio_pattern = None

        # get the propagation table
        if op_entry['kick_propagation_table'].value != 0:
            self.kpt_table = helper.chipdata.cast(
                addr=op_entry['kick_propagation_table'].value,
                type_def="KP_TABLE",
                elf_id=helper.debuginfo.get_kymera_debuginfo().elf_id
            )

            amount = 0
            table_length = self.kpt_table['table_length'].value
            table_address = self.kpt_table['table'].address
            # the element size
            table_size = helper.debuginfo.get_type_info("KP_ELEMENT")[5]

            self.prop_table = []
            for i in range(0, table_length):
                self.prop_table.append(
                    helper.chipdata.
                    cast(table_address + i * table_size, "KP_ELEMENT")
                )

            num_op_sources = self.kpt_table['num_op_sources'].value
            self.kicked_op_sources = self.prop_table[amount:amount + num_op_sources]
            amount += num_op_sources

            num_op_sinks = self.kpt_table['num_op_sinks'].value
            self.kicked_op_sinks = self.prop_table[amount:amount + num_op_sinks]
            amount += num_op_sinks

            num_ep_sources = self.kpt_table['num_ep_sources'].value
            self.kicked_ep_sources = self.prop_table[amount:amount + num_ep_sources]
            amount += num_ep_sources

            num_ep_sinks = self.kpt_table['num_ep_sinks'].value
            self.kicked_ep_sinks = self.prop_table[amount:amount + num_ep_sinks]
            amount += num_ep_sinks

        # Some extra data needed by specific instance
        self.extra_op_data_address = op_entry['extra_op_data'].value
        self.extra_op_data_type = None

        # Additional information for an operator

        # Operator endpoint IDs
        self.op_ep_id = Opmgr.get_opidep_from_opid(self.id)

        # Pointer to static capability data
        self.cap_data = cap_data

        if not helper.debuginfo.is_elf_loaded(self.cap_data.elf_id):
            # bundle not loaded bail out early.
            return

        # try to get the extra operator data type from the debug information.
        # If this returns None then we are dealing with older FW.
        self.extra_op_data_type = helper.debuginfo.get_cap_data_type(
            self.cap_data.name,
            self.cap_data.elf_id
        )

        # search for casting in the data process function.
        if self.extra_op_data_type is None:
            self.extra_op_data_type = \
                helper.get_extra_op_data_type(
                    self.cap_data.process_data,
                    self.cap_data.elf_id
                )

        # if not type cast found continue with the operator message handlers
        if self.extra_op_data_type is None:
            for handler_entry in self.cap_data.opmsg_handler_table:
                self.extra_op_data_type = \
                    helper.get_extra_op_data_type(
                        handler_entry[1],
                        self.cap_data.elf_id
                    )
                if self.extra_op_data_type is not None:
                    break

        # Continue with the operator command handlers (it is last because
        # many of these function are just a reuse from the basic operator).
        if self.extra_op_data_type is None:
            for handler_entry in self.cap_data.handler_table:
                self.extra_op_data_type = \
                    helper.get_extra_op_data_type(
                        handler_entry[1],
                        self.cap_data.elf_id
                    )
                if self.extra_op_data_type is not None:
                    break

        # Could be that the operator is reusing everything from the main
        # (kymera)build. This means that the extra_op_data_type should be
        # in the main build.
        if self.extra_op_data_type is None:
            self.extra_op_data_type = \
                helper.get_extra_op_data_type(
                    self.cap_data.process_data,
                    helper.debuginfo.get_kymera_debuginfo().elf_id
                )

        self.extra_op_data = None
        if self.extra_op_data_type is None:
            pass
        else:
            try:
                self.extra_op_data = helper.chipdata.cast(
                    self.extra_op_data_address,
                    self.extra_op_data_type,
                    elf_id=self.cap_data.elf_id
                )
            except InvalidDebuginfoTypeError:
                # probably the operator was patched
                self.extra_op_data_type = None

    def __get_kick_table_op_string(self, kick_table):
        mask = kick_table['t_mask'].value
        task_id = self.helper.chipdata.cast(
            kick_table['kt']['ep'].value, "BGINT"
        )['id'].value
        operator = self.helper.get_operator_by_taskid(task_id)

        result = "  Operator %s %s (mask: %s)\n" % (
            hex(operator.op_ep_id),
            operator.cap_data.name,
            bin(mask)
        )
        return result

    def __get_kick_table_ep_str(self, kick_table):
        mask = kick_table['t_mask'].value
        ep_compact_name = self.helper.streams_analysis.get_endpoint_by_address(
            kick_table['kt']['ep'].value
        ).compact_name()
        return "  " + ep_compact_name + " (mask: " + bin(mask) + ')\n'

    def __get_raw_kick_table_string(self):
        kick_table_string = "+++++++++++++++++++++++++++\n"
        for prop in self.prop_table:
            kick_table_string += "--\n"
            kick_table_string += str(prop)
        kick_table_string += "+++++++++++++++++++++++++++\n"
        return kick_table_string

    def __str__(self):
        return self.title_str + '\n' + self.desc_str

    @property
    def title_str(self):
        """Returns the title string of the object."""
        return 'Operator ' + hex(self.id) + ', ep_op_id ' + \
            hex(self.op_ep_id) + ', ' + self.cap_data.name

    @property
    def desc_str(self):
        """Returns the description string of the object."""
        # Pylint thinks that _desc_str is not callable due to the @property
        retval = self._desc_str()  # pylint: disable=not-callable
        return retval

    def _channel_string(self):
        """Returns the common channel data description string."""
        mystr = ""
        channel_p = self.op_entry['cap_class_ext'].value
        if channel_p != 0:
            channel = self.helper.chipdata.cast(channel_p, "MULTI_CHANNEL_DEF")
            mystr += (str(channel))

            # Remove the last line and first line. Note -2 is used because the
            # string terminates in \n
            mystr = "\n".join(mystr.split("\n")[1:-2])
            # Give a better title for the channel description
            mystr = "Multi channel data:\n" + mystr + "\n"

            first_channel = channel['first_active']
            if first_channel != 0:
                channels_str = ""
                channels = ch.parse_linked_list(
                    first_channel,
                    "next_active"
                )
                for chan in channels:
                    chanstr = str(chan)
                    # remove the first line
                    chanstr = "\n".join(chanstr.split("\n")[1:])
                    chanstr = insert_spaces(chanstr)
                    channels_str += insert_spaces("channel:\n" + chanstr)

                mystr += insert_spaces(channels_str)

            chan_obj_size = channel['chan_obj_size'].value
            chan_data_address = channel['chan_data'].value
            channel_data = self.helper.chipdata.get_data(
                chan_data_address,
                self.cap_data.max_sources * Arch.addr_per_word * chan_obj_size
            )
            if channel_data:
                channel_data_str = "[ "
                count = 1
                for val in channel_data:
                    channel_data_str += "0x%08x" % val
                    if count == len(channel_data):
                        channel_data_str += " ]"
                    else:
                        if count % chan_obj_size == 0:
                            channel_data_str += ",\n  "
                        else:
                            channel_data_str += ", "
                    count += 1

                channel_data_str = insert_spaces(channel_data_str)
                # And now print the channel data
                channel_data_str = "chan_data at 0x08%x:\n" % \
                        chan_data_address + channel_data_str
                mystr += insert_spaces(channel_data_str + "\n")

            else:
                mystr += "WARNING: Cannot retrieve the channel data.\n"

        return mystr

    def _encoder_string(self):
        """Returns the common encoder description string."""
        mystr = ""
        encoder_p = self.op_entry['cap_class_ext'].value
        if encoder_p != 0:
            encoder_param = self.helper.chipdata.cast(
                encoder_p, "ENCODER_PARAMS"
            )
            mystr += (str(encoder_param))

            # Remove the last line and first line. Note -2 is used because the
            # string terminates in \n
            mystr = "\n".join(mystr.split("\n")[1:-2])
            # Give a better title for the encoder_param description
            mystr = "Common encoder param:\n" + mystr + "\n"

        return mystr

    def _common_base_class_data(self):
        """Returns the common base class data string from cap_class_ext.

        This can be encoder or channel data dependent on the operator
        type.
        """
        # The current implementation is slightly fragile because it relies on
        # the capability name to decide if the operator is an encoder.
        # Currently celt, aptx and sbc encode are the only operators using
        # encoder common parameters. All the others operators based on the
        # base_op_multi_channel (dbe, vse and peq) are using the common base
        # class data to store channel information.
        if "encode" in self.cap_data.name.lower():
            return self._encoder_string()

        try:
            return self._channel_string()

        except KeyError:
            # FIXME: The exception trap is a temporary fix until B-303426 is
            #        being addressed.
            logger.warning("Can't determine the common base class data string")
            return ""

    def _desc_str(self):
        mystr = 'Connection ID: ' + hex(self.con_id) + '\n'
        mystr += 'Task ID: ' + hex(self.task_id) + '\n'
        mystr += ('Creator client ID: ' +
                  hex(self.creator_client_id) + '\n')
        op_state = self.helper.debuginfo.get_enum('OP_STATE', self.state)[0]
        mystr += 'State: ' + op_state.replace("OP_", "") + '\n'
        mystr += 'Kicks ignored at %s \n' % self.stop_chain_kicks

        if self.logging_enabled is not None:
            mystr += 'logging_enabled: ' + str(self.logging_enabled) + '\n'
        if self.pio_num is not None and self.pio_num is not None:
            mystr += 'pio_num: ' + str(self.pio_num) + '\n'
            mystr += 'pio_pattern: ' + str(self.pio_pattern) + '\n'

        src_endpoints = []  # List of Stream.Endpoint objects
        sink_endpoints = []  # List of Stream.Endpoint objects
        for port in range(self.cap_data.max_sources):
            ep_id = Opmgr.ep_id_from_port_opid_and_dir(port, self.op_ep_id, 1)
            associated_ep = self.helper.streams_analysis.get_endpoint(ep_id)
            if associated_ep is not None:
                # associated_ep could be None if we've destroyed (or never
                # created) the Endpoint to go with this operator.
                src_endpoints.append(associated_ep)

        for port in range(self.cap_data.max_sinks):
            ep_id = Opmgr.ep_id_from_port_opid_and_dir(port, self.op_ep_id, 0)
            associated_ep = self.helper.streams_analysis.get_endpoint(ep_id)
            if associated_ep is not None:
                # associated_ep could be None if we've destroyed (or never
                # created) the Endpoint to go with this operator.
                sink_endpoints.append(associated_ep)

        mystr += str(len(src_endpoints)) + ' source endpoint(s): '
        for source in src_endpoints:
            mystr += (hex(source.key) + ', ')
        # Remove the last ", "
        mystr = mystr[:-2]
        mystr += ('\n')

        mystr += (str(len(sink_endpoints)) + ' sink endpoint(s): ')
        for sink in sink_endpoints:
            mystr += (hex(sink.key) + ', ')
        # Remove the last ", "
        mystr = mystr[:-2]
        mystr += ('\n')

        # Print out the raw table. For test only
        # mystr += self.__get_raw_kick_table_string()
        try:
            if self.kicked_op_sources:
                mystr += ("kicked operator sources (forward kicks):\n")
                for kick_table in self.kicked_op_sources:
                    mystr += self.__get_kick_table_op_string(kick_table)

            if self.kicked_ep_sources:
                mystr += ("kicked source endpoints (forward kicks):\n")
                for kick_table in self.kicked_ep_sources:
                    mystr += self.__get_kick_table_ep_str(kick_table)

            if self.kicked_op_sinks:
                mystr += ("kicked operator sinks (backward kicks):\n")
                for kick_table in self.kicked_op_sinks:
                    mystr += self.__get_kick_table_op_string(kick_table)

            if self.kicked_ep_sinks:
                mystr += ("kicked sink endpoints (backward kicks):\n")
                for kick_table in self.kicked_ep_sinks:
                    mystr += self.__get_kick_table_ep_str(kick_table)
        except BaseException:
            # the operator does not kick so just pass
            pass

        # print out the common base class data (cap_class_ext). If all the
        # debug information is loaded and ready.
        if self.cap_data.handler_table:
            mystr += (self._common_base_class_data())

        if self.extra_op_data_type is None:
            mystr += (
                "extra_op_data address: " + hex(self.extra_op_data_address)
            )
            elf_id = self.cap_data.elf_id
            if self.helper.debuginfo.is_elf_loaded(elf_id):
                mystr += (
                    "\nUse the following command to properly display the"
                    " operator data structure"
                )
                mystr += (
                    '\nopmgr.set_extra_op_data_type( %s,\n\t\"<the type '
                    'used to cast '
                    'op_data->extra_op_data to>\")' % hex(self.op_ep_id)
                )
            else:
                mystr += "\nCannot display operator.\n"\
                    "Bundle with elf id {0} is missing!\n"\
                    "Use -j option or load_bundle(r\"<path>\") in "\
                    "interactive "\
                    "mode to add bundles to ACAT!".format(hex(elf_id))

        else:
            mystr += str(self.extra_op_data)
        return mystr

    def cast_extra_data(self, type_str):
        """Sets the extra operator data type for an operator.

        Args:
            type_str (str): Type string for example "RTP_DECODE_OP_DATA".
        """
        self.extra_op_data_type = type_str
        self.extra_op_data = self.helper.chipdata.cast(
            self.extra_op_data_address, type_str, elf_id=self.cap_data.elf_id
        )


def insert_spaces(input_string):
    """Inserts spaces before each line for a given string."""
    return "  " + "  ".join(input_string.splitlines(True))





class Capability(object):
    """Class to encapsulate a capability.

    Args:
        cap_entry
        helper
        cap_elf_id
    """

    def __init__(self, cap_entry, helper, cap_elf_id):
        self.helper = helper
        self.address = cap_entry.address
        # if this capability is from downloaded capabilities, store the file id
        self.elf_id = cap_elf_id
        # Capability ID
        self.id = cap_entry['id'].value

        recheck_name = False
        # Capability name
        try:
            # try to get the name from the built in capability names
            self.name = helper.debuginfo.get_enum('CAP_ID', self.id)[
                0].replace("CAP_ID_", "")
        except KeyError:
            # Not a built in capability. Capability IDs are allocated as
            # follows:
            #
            # 0x0000-0x3FFF capabilities built in to a Kymera image.
            # 0x4000-0x7FFF Qualcomm-provided downloadable capabilities.
            # 0x8000-0xBFFF are internally reserved IDs.
            # 0xC000-0xC0FF are for private use in customer projects.
            # 0xC100-0xFFFF Qualcomm eXtension programme partners.
            if self.id >= 0x4000 and self.id <= 0x7FFF:
                self.name = "(Qualcomm_Provided_Capability:%s)" % hex(self.id)
            elif self.id >= 0x8000 and self.id <= 0xBFFF:
                # Probably these IDs will never be used.
                self.name = "(Reserved_Capability:%s)" % hex(self.id)
            elif self.id >= 0xC000 and self.id <= 0xC0FF:
                self.name = "(Customer_Capability:%s)" % hex(self.id)
            elif self.id >= 0xC100 and self.id <= 0xFFFF:
                self.name = "(Qualcomm_eXtension_Capability:%s)" % hex(self.id)
            else:
                # could be any downloaded capability
                self.name = "(Downloaded_Capability:%s)" % hex(self.id)
            # try to better guess the name later.
            recheck_name = True

        # Capability version
        # do the masking
        msw = cap_entry['version_msw'].value
        lsw = cap_entry['version_lsw'].value
        self.version = (msw << 16) + lsw

        # max sinks and source
        self.max_sinks = cap_entry['max_sinks'].value
        self.max_sources = cap_entry['max_sources'].value

        self.process_data = cap_entry['process_data'].value

        self.instance_data_size = cap_entry['instance_data_size'].value

        # Read the Opmsg and Opcmd handlers
        self.opmsg_handler_table_pointer = cap_entry['opmsg_handler_table'].value
        self.handler_table_pointer = cap_entry['handler_table'].value
        # Preset the handler tables
        self.opmsg_handler_table = []
        self.handler_table = []
        self.is_elf_loaded = helper.debuginfo.is_elf_loaded(cap_elf_id)
        # Check if the elf file is loaded to the system. Bail out early if not.
        if not self.is_elf_loaded:
            return

        try:
            handler_table = helper.debuginfo.get_var_strict(
                self.handler_table_pointer, cap_elf_id
            )
        except DebugInfoNoVariableError:
            # Something went wrong reading the handler table.
            handler_table = None
        # If the capability name is still unknown try to guess it from the
        # handle table name.
        if recheck_name and handler_table is not None:
            if "_handler_table" in handler_table.name:
                # Use the handle table name as capability name
                guessed_name = handler_table.name
                # remove the beginning
                guessed_name = guessed_name.replace("$_", "")
                # remove the ending
                guessed_name = guessed_name.replace("_handler_table", "")
                # make it upper case
                guessed_name = guessed_name.upper()
                # finally mark it as downloaded
                guessed_name = "DOWNLOADED_" + guessed_name

                self.name = guessed_name

        if handler_table is not None:
            type_id = helper.debuginfo.get_kymera_debuginfo().types[
                helper.debuginfo.get_kymera_debuginfo().get_type_info(
                    'handler_lookup_struct')[1]
            ].ref_type_id
            # get the chipdata
            handler_table = helper.chipdata.get_var_strict(
                            self.handler_table_pointer, cap_elf_id
                        )
            members = helper.debuginfo.get_kymera_debuginfo().types[type_id].members
            for index, value in enumerate(members):
                self.handler_table.append(
                    [index, handler_table[value.name].value]
                )

        try:
            opmsg_handler_table = helper.chipdata.get_var_strict(
                self.opmsg_handler_table_pointer, cap_elf_id
            )
        except (SystemExit, KeyboardInterrupt, GeneratorExit):
            raise

        except Exception:
            # the capability may reuse a handle table from kymera
            try:
                opmsg_handler_table = helper.chipdata.get_var_strict(
                    self.opmsg_handler_table_pointer,
                    helper.debuginfo.get_kymera_debuginfo().elf_id
                )
            except DebugInfoNoVariableError:
                opmsg_handler_table = []

        self.opmsg_handler_table = [
            handler_entry.value for handler_entry in opmsg_handler_table
            if handler_entry.value[0] != 0
        ]

    def __str__(self):
        return self.title_str + '\n' + self.desc_str

    @property
    def title_str(self):
        """Returns the title string of the object."""
        return self.name + ' id: ' + hex(self.id)

    @staticmethod
    def _display_handler(handler_address):
        """
        Returns a string representation of a handler_address

        Args:
            handler_address(int) - Hanlder address.
        Returns:
            String representation of the handler_address.
        """
        return "  0x" + format(handler_address, '08x')


    def _display_opcmd_name_opmsg_id(self, handler_type, value):
        """
        Returns a string representation of an operator command or an operator
        message.

        Args:
            handler_type(str) - Hanlder type which can be "Opmsg" or "Opcmd".
            value(int) - Operator message or command id.
        Returns:
            String representation of the operator command or message.
        """
        if handler_type == "Opcmd":
            return " " * 2 + self.helper.debuginfo.get_enum('OPCMD_ID', value)[0]
        elif handler_type == "Opmsg":
            return "  0x" + format(value, '04x')
        else:
            raise RuntimeError("Invalid arguments" + str(handler_type))

    def _handle_table_to_str(self, handler_type):
        """
        Returns a string representation of a operator handle table.
        This table can be "Opmsg" or "Opcmd".

        Args:
            handler_type - Hanlder type which can be "Opmsg" or "Opcmd".
        Returns:
            String representation of a handle table.
        """
        ret_str = ""
        # populate handler_table, handler_table_pointer and name_translator
        # based on the type of the handler.
        if handler_type == "Opcmd":
            handler_table = self.handler_table
            handler_table_pointer = self.handler_table_pointer
        elif handler_type == "Opmsg":
            handler_table = self.opmsg_handler_table
            handler_table_pointer = self.opmsg_handler_table_pointer
        else:
            raise RuntimeError("Invalid arguments" + str(handler_type))

        if handler_table:
            for handler in handler_table:
                ret_str += self._display_opcmd_name_opmsg_id(
                    handler_type, handler[0]
                )
                try:
                    ret_str += (
                        " - " + self.helper.debuginfo.get_source_info(handler[1])
                        .module_name + '\n'
                    )
                except BundleMissingError:
                    ret_str += (
                        self._display_handler(handler[1]) +
                        " - Bundle missing. Cannot display handler.\n"
                    )
        else:
            ret_str += self._display_handler(handler_table_pointer)
            if self.is_elf_loaded:
                ret_str += " - Invalid " + handler_type + " handle table pointer.\n"
            else:
                ret_str += (
                    " - Bundle missing. Cannot display " +
                    handler_type + " handle table.\n"
                )
        return ret_str

    @property
    def desc_str(self):
        """Returns the description string of the object."""
        mystr = ('version: ' + hex(self.version) + '\n')
        mystr += ('max_sinks: ' + str(self.max_sinks) + '\n')
        mystr += ('max_sources: ' + str(self.max_sources) + '\n')
        mystr += ('instance_data_size: ' + str(self.instance_data_size) + '\n')

        mystr += ('handler_table: \n')
        mystr += (self._handle_table_to_str("Opcmd"))

        mystr += ('opmsg_handler_table: \n')
        mystr += (self._handle_table_to_str("Opmsg"))

        try:
            process_data_name = self.helper.debuginfo.get_source_info(
                self.process_data
            ).module_name
        except BundleMissingError:
            # Bundle not loaded. Bail out early.
            process_data_name = (
                "0x" + format(self.process_data, '08x') +
                " - Bundle missing. Cannot display handler.\n"
            )
        mystr += ('process_data: ' + process_data_name + '\n')
        return mystr


def operator_factory(operator_var, analysis):
    """Operator creator function."""
    operator = None
    # This operation cannot fail.
    cap_data = analysis.find_capability(
                operator_var['cap_data'].value
            )

    try:
        if cap_data.name in ("SOURCE_SYNC", "DOWNLOAD_SOURCE_SYNC"):
            from ACAT.Analysis.SourceSync import SourceSync
            # use source sync operator
            operator = SourceSync(operator_var, analysis, cap_data)
        elif cap_data.name in ("SPLITTER", "DOWNLOAD_SPLITTER"):
            from ACAT.Analysis.Splitter import Splitter
            # use splitter operator
            operator = Splitter(operator_var, analysis, cap_data)

        elif cap_data.name in ("RTP_DECODE", "DOWNLOAD_RTP_DECODE"):
            from ACAT.Analysis.RtpDecode import RtpDecode
            # use RtpDecode operator
            operator = RtpDecode(operator_var, analysis, cap_data)
    except ImportError:
        # It is possible that some capabilities will be hidden. In this, case
        # the Analysis will be removed causing an import error.
        pass

    if not operator:
        # Use the generic operator
        operator = Operator(operator_var, analysis, cap_data)

    return operator


class Opmgr(Analysis.Analysis):
    """Encapsulates analysis for operators.

    Args:
        **kwarg: Arbitrary keyword arguments.
    """

    def __init__(self, **kwarg):
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwarg)
        self.op_list_object = []
        # We need a Streams analysis so that we can look up endpoint/transform
        # information. Give it a null formatter though, just to make sure it
        # doesn't start outputting stuff unbidden.
        try:
            self.streams_analysis = self.interpreter.get_analysis(
                "stream", self.chipdata.processor
            )
        except KeyError:
            raise AnalysisError(
                "OPMGR analysis doesn't work without Stream analysis.  "
                "Please check the available analyses and make sure that "
                "the Stream is ahead of OPMGR."
            )

    @staticmethod
    def get_opid_from_ep_id(ep_id):
        """Returns the opid, given an endpoint id.

        If passed something that was already an opid, will return the
        original value.

            e.g. 'ep_id' = 0x6040 -> opid = 1
                 'ep_id' = 2 -> opid = 2

        Args:
            ep_id
        """
        # All of this stuff is defined in opmgr.c and needs to be kept in
        # sync with it if anything ever changes.
        # static unsigned int get_opid_from_ep(unsigned int opidep)
        # {
        #    return ((opidep & STREAM_EP_OPID_MASK) >> STREAM_EP_OPID_POSN);
        # }
        # define STREAM_EP_OPID_MASK  0x1fc0
        # define STREAM_EP_OPID_POSN  6
        real_opid = (ep_id & 0x1fc0) >> 6
        # opid wraps if (id > (STREAM_EP_OPID_MASK >> STREAM_EP_OPID_POSN))
        if ep_id < (0x1fc0 >> 6):
            # User actually supplied a real opid
            real_opid = ep_id
        return real_opid

    @staticmethod
    def get_opidep_from_opid(opid):
        """Returns the operator endpoint id (op_ep_id), given an opid.

        If passed something that was already an ep_id, will return the
        op_ep_id equivalent.

            e.g. 'opid' = 1 -> op_ep_id = 0x4040
                 'opid' = 0xe040 -> op_ep_id = 0x4040
                 'opid' = 0x4040 -> op_ep_id = 0x4040

        Args:
            opid
        """
        # All of this stuff is defined in opmgr.c and needs to be kept in
        # sync with it if anything ever changes.
        # define STREAM_EP_SINK_BIT  0x8000
        # define STREAM_EP_OP_BIT    0x4000
        # define STREAM_EP_EP_BIT    0x2000
        # static unsigned int get_opidep_from_opid(unsigned int opid)
        # {
        #    return (((opid << STREAM_EP_OPID_POSN) & STREAM_EP_OPID_MASK) |
        #            STREAM_EP_OP_BIT );
        # }
        # See above for where the other magic numbers come from
        if opid & 0x4000 != 0:
            # Already an ep_id, convert to op_ep_id
            return opid & ~0xA000
        return ((opid << 6) & 0x1fc0) | 0x4000

    @staticmethod
    def ep_id_from_port_opid_and_dir(port, op_ep_id, direction):
        """Returns EP ID.

        Takes a port number (e.g. 0), op_ep_id (e.g. 0x4040) and direction
        (0 for sink, 1 for source).

        Args:
            port
            op_ep_id
            direction
        """
        # static unsigned int get_port_from_opidep(unsigned int opidep)
        # {
        #    return ((opidep & STREAM_EP_CHAN_MASK) >> STREAM_EP_CHAN_POSN);
        # }
        # define STREAM_EP_CHAN_MASK  0x003f
        # define STREAM_EP_CHAN_POSN  0
        # See above for where the other magic numbers come from.
        ep_id = op_ep_id | 0x2000  # Set 'is endpoint' bit
        if not direction:
            ep_id = ep_id | 0x8000  # Set 'is sink' bit
        ep_id = ep_id | port
        return ep_id

    def run_all(self, detailed=True):
        """Perform analysis and spew the output to the formatter.

        It analyses all the operators and the available capabilities.

        Args:
            detailed (bool, optional): Detailed capability display.
        """
        self.formatter.section_start('Opmgr')
        self.analyse_all_operators()
        self.analyse_downloaded_bundles()
        self.analyse_all_capabilities(detailed)
        self.formatter.section_end()

    def get_operator(self, opid):
        """Returns the operator (an Operator object) that has ID 'opid'.

        Note:
            'operator ID' can be an actual opid (1, 2, etc.) or an
            operator ep_id (0x4040, 0xe040) - we'll infer which one it is.
            If force_read == True, the op_list variable is read again (if
            contents have changed).

        Args:
            opid (int)

        Returns:
            None: If opid was not found.
        """
        real_opid = Opmgr.get_opid_from_ep_id(opid)
        self.op_list_object = self.get_oplist('object')
        for operator in self.op_list_object:
            if operator.id == real_opid:
                return operator

        raise AnalysisError("No operator found with id: " + hex(opid))

    def set_extra_op_data_type(self, opid, type_string):
        """Sets the typed of the extra_op_data field for an operator.

        Args:
            opid (int): Operator ID.
            type_str (str): Type string for example "RTP_DECODE_OP_DATA".
        """
        operator = self.get_operator(opid)
        operator.cast_extra_data(type_string)

        return operator

    def get_operator_by_taskid(self, taskid):
        """Returns the operator (an Operator object) that has a given Task ID.

        Args:
            taskid
        """
        self.op_list_object = self.get_oplist('object')
        taskid = taskid & 0x1FFFFF
        for operator in self.op_list_object:
            if (operator.task_id & 0x1FFFFF) == taskid:
                return operator

        return None

    @Analysis.cache_decorator
    def get_oplist(self, mode='id'):
        """Returns a list of all operators in opmgr's oplist.

        Args:
            mode (str, optional): Selects which information to return.
              'entry' - a list of op_entry elements.
              'id' - a list of operator IDs.
              'object' - a list of Operator objects (NB: could be slow).
              'name' - a list of operator names and their ids.

        Raises:
            UsageError: Invalid oplist mode.
        """
        op_entries = self._read_raw_oplist()

        if mode == 'entry':
            return op_entries
        elif mode == 'id':
            return [op['id'].value for op in op_entries]
        elif mode == 'object':
            return [operator_factory(op, self) for op in op_entries]
        elif mode == 'name':
            return_list = []
            for operator in op_entries:
                operator = operator_factory(operator, self)
                return_list.append(
                    operator.cap_data.name +
                    " " +
                    hex(
                        operator.op_ep_id))
            return return_list
        else:
            raise UsageError('Invalid oplist mode')

    @Analysis.cache_decorator
    def _read_raw_oplist(self):
        oplist_head = self.chipdata.get_var_strict("$_oplist_head")
        return [op for op in ch.parse_linked_list(oplist_head, "next")]

    @Analysis.cache_decorator
    def get_capability_list(self):
        """Returns a list of all capability data."""
        # read the capability table
        capability_data_table = self.chipdata.get_var_strict(
            "$_capability_data_table"
        )
        # the capability table is null terminated and we know the exact size
        capability_length = (
            capability_data_table.size - 1
        ) // Arch.addr_per_word

        if capability_length == 0:
            raise AnalysisError("Invalid capability table")

        capabilities = [
            Capability(
                self.chipdata.cast(
                    capability_data_table.value[count], "CAPABILITY_DATA"
                ), self, self.debuginfo.get_kymera_debuginfo().elf_id
            ) for count in range(capability_length)
        ]

        # Now deal with downloaded capabilities if they exist
        downloaded_capabilites_entries = self._get_downloaded_capabilities()
        if downloaded_capabilites_entries is not None:
            for entry in downloaded_capabilites_entries:
                # for now there is no direct way of determining the
                # capability id for capability entry, so we rely on
                # extracting that information from the mapped table.
                # For downloaded capablilities at least one (probably all
                # of them) of the members below should be mapped however
                # for the sake of safety check them all
                kymera_elf_id = self.debuginfo.get_kymera_debuginfo().elf_id
                cap_elf_id = kymera_elf_id

                temp_elf_id = self.debuginfo.table.get_elf_id_from_address(
                    entry['handler_table'].value
                )
                if temp_elf_id and temp_elf_id != kymera_elf_id:
                    cap_elf_id = temp_elf_id

                temp_elf_id = self.debuginfo.table.get_elf_id_from_address(
                    entry['opmsg_handler_table'].value
                )
                if temp_elf_id and temp_elf_id != kymera_elf_id:
                    cap_elf_id = temp_elf_id

                temp_elf_id = self.debuginfo.table.get_elf_id_from_address(
                    entry['process_data'].value
                )
                if temp_elf_id and temp_elf_id != kymera_elf_id:
                    cap_elf_id = temp_elf_id

                cap = Capability(entry, self, cap_elf_id)

                capabilities.append(cap)
        return capabilities

    def _get_downloaded_capabilities(self):
        """Looks how many downloaded capability is loaded in the system.

        Note:
            A bundle can contain multipe downloadable capabilities.

        Returns:
            cap_entries The available downloadable capabilities.
        """
        cap_entries = []
        try:
            head = self.chipdata.get_var_strict(
                '$_cap_download_data_list_shared'
            ).value
        except DebugInfoNoVariableError:
            # if previous variable not found that means single core build is
            # used
            try:
                head = self.chipdata.get_var_strict(
                    '$_cap_download_data_list_aux'
                ).value
            except DebugInfoNoVariableError:
                # if this value is not found older build is probably used
                # without support for downloaded capabilities.
                return None
        # if variable exist but it points to NULL, no capabilities are present
        if head == 0:
            return None
        database = self.chipdata.cast(head, 'DOWNLOAD_CAP_DATA_DB')
        for database_entry in ch.parse_linked_list(database, 'next'):
            cap_entry = self.chipdata.cast(
                database_entry['cap'].value, 'CAPABILITY_DATA'
            )
            cap_entries.append(cap_entry)
        return cap_entries

    def find_capability(self, address):
        """Search for the capability data by address.
        If the capability is not found a new one is created.

        Args:
            address
        """
        capabilities = self.get_capability_list()
        for capability in capabilities:
            if capability.address == address:
                return capability

        # The Capability is possibly a downloaded one there was no way
        # that this could be known before so try to create the capability now
        capability = Capability(
            self.chipdata.cast(
                address, "CAPABILITY_DATA"
            ), self, None
        )
        self.formatter.alert(capability.name + " is a patched capability.")
        return capability

    @Analysis.cache_decorator
    def get_extra_op_data_type(self, function_address, elf_id):
        """Finding the casting of extra_op_data.

        It takes the function address and searches for the nearest label
        in the listing file in order to find the casting of extra_op_data.

        Args:
            function_address
            elf_id: The bundle elf id where the capability can be found.
        """
        address_in_dowload_cap = False
        if elf_id != self.debuginfo.get_kymera_debuginfo().elf_id:
            if not self.debuginfo.table.is_addr_in_table(function_address):
                # if we are searching for a type in downloaded capability,
                # we only check the addresses that are mapped from file to
                # coredump. This is not always the case since some entries
                # in handler table and opmsg handler table are referred to
                # original lst file. It happens when their handlers inherit
                # from other  capabilities such as base_op
                # In the later case we are sure that type will not be found
                # from this address thus return None
                return None
            else:
                # the given address is in a downloadable capability, therefore
                # when searching in the .lst file we need to convert the address
                # to capability address.
                address_in_dowload_cap = True

        mmap_lst = self.debuginfo.get_mmap_lst(elf_id)
        if not mmap_lst:
            # the .lst file is missing.
            self.formatter.alert(
                "One of the listing .lst file is missing "
                "which is needed for pre MAP_INSTANCE_DATA capabilities."
            )
            return None

        # adjust the function address and convert it to hex string
        try:
            nearest_label = self.debuginfo.get_nearest_label(function_address)
        except (BundleMissingError, DebugInfoNoLabelError):
            nearest_label = None

        if nearest_label is not None:
            func_address = nearest_label.address
            if address_in_dowload_cap:
                # Convert the address to capability address (file address)
                # as the function address is in the downloaded table.
                func_address = self.debuginfo.table.convert_addr_to_download(
                    func_address, elf_id
                )
            func_address = hex(func_address)
        else:
            func_address = hex(function_address)

        func_address = func_address.replace("0x", "").encode('utf-8')
        # get the first match of the address
        address_pos = mmap_lst.find(func_address + b":")

        if address_pos == -1:
            # Address not found. Probably it's in another build
            return None

        mmap_lst.seek(address_pos)

        current_line = ""
        start_time = import_time.time()
        gurard_time = 2  # sec

        # until the nearest label is our function search for cast or timeout.
        while import_time.time() - start_time < gurard_time:
            # read new line from correct file opened
            current_line = mmap_lst.readline()
            # An rts instruction means that we are out of the original function
            if b"rts;" in current_line:
                return None

            # filter out lines which has no relation to the extra_op_data.
            if b"extra_op_data" in current_line:
                # regular expression to search for casting of the extra_op_data
                cast_type = re.search(
                    b'\=[\ ]*\([\ ]*([A-Za-z0-9\_]*)[\ ]*\*[\ ]*\).*->[\ ]*extra_op_data.*\;',
                    current_line
                )
                if cast_type is not None:
                    type_name = cast_type.group(1).decode()
                    return type_name
            elif b"get_instance_data" in current_line:
                # regular expression to search for casting of the extra_op_data
                # which is done using get_instance_data
                cast_type = re.search(
                    b'[\ ]*([A-Za-z0-9\_]*)[\ ]*\*[\ ]*[A-Za-z0-9\_]*[\ ]*\=[\ ]*get_instance_data',
                    current_line
                )
                if cast_type is not None:
                    type_name = cast_type.group(1).decode()
                    return type_name

        return None

    #######################################################################
    # Analysis methods - public since we may want to call them individually
    #######################################################################

    def analyse_all_operators(self):
        """Displays all the operators in use."""
        self.formatter.section_start('Operators in use')
        self.op_list_object = self.get_oplist('object')
        for operator in self.op_list_object:
            self.formatter.section_start(operator.title_str)
            self.formatter.output(operator.desc_str)
            self.formatter.section_end()
        self.formatter.section_end()

    def analyse_downloaded_bundles(self):
        """Displays all the downloaded bundles."""
        self.formatter.section_start('Downloaded bundles')
        self.formatter.output(str(self.debuginfo.table))
        self.formatter.section_end()

    def analyse_all_capabilities(self, detailed=True):
        """Displays all the available capabilities.

        Args:
            detailed (bool, optional): Detailed capability display.
        """
        self.formatter.section_start('Available capabilities')
        capabilities = self.get_capability_list()
        for cap in capabilities:
            if detailed:
                self.formatter.section_start(cap.title_str)
                self.formatter.output(cap.desc_str)
                self.formatter.section_end()
            else:
                self.formatter.output(cap.name)
        self.formatter.section_end()

    #######################################################################
    # Private methods - don't call these externally.
    #######################################################################
