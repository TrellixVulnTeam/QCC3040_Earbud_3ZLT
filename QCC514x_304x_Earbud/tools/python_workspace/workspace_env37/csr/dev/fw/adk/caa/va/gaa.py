############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.env.env_helpers import InvalidDereference
from csr.dev.model import interface
from csr.wheels import CLang


class Gaa(FirmwareComponent):
    """
    Gaa Voice Assistant analysis class
    """

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)
        try:
            self._gaa = env.econst.voice_ui_provider_gaa
        except AttributeError:
            raise self.NotDetected()

    @property
    def active(self):
        try:
            active_va = self.env.vars['active_va'].deref.voice_assistant.deref.va_provider.value
            return active_va == self._gaa
        except InvalidDereference:
            return False

    def _convert_binary_to_fp64(self, value):
        if(value == '0b0'):
            fp64_value = 0.0
        else:
            # Bit 64
            sign = int(value[2:3], 2)
            # Bit 63-53
            exponent = int(value[3:14], 2)
            offset = 1023

            if(exponent):
                exponent = 2 ** (exponent - offset)
            else:
                exponent = 2 ** (offset - 1)

            # Bit 52-0
            fraction = int(value[14:], 2)

            if sign == 0:
                exponent = exponent * -1

            fp64_value = float(str(exponent) + '.' + str(fraction))
        return fp64_value

    def _create_device_actions_group(self):
        try:
            command_table = self.env.vars['command_table']
        except KeyError:
            return None

        device_actions_grp = interface.Group("Gaa device actions")

        headers = ["all"]
        row = [self.env.vars['update_all_storage']]

        for entry in self.env.var.device_action_list:
            try:
                headers.append(self.env.vars[entry.deref.address].replace("_device_action", ""))
                row.append(entry.deref.update.storage)
            except InvalidDereference:
                pass

        device_actions_storage_ptr_tbl = interface.Table(headers)
        device_actions_storage_ptr_tbl.add_row(row)
        device_actions_grp.append(device_actions_storage_ptr_tbl)

        device_actions_command_tbl = interface.Table(["Command", "Value Kind", "Value"])
        with command_table.footprint_prefetched():
            for entry in command_table:
                value_kind = entry.value_kind
                if value_kind.value == self.env.econst.EXECUTION_VALUE__KIND_NULL_VALUE:
                    value = entry.u.null_value.value
                elif value_kind.value == self.env.econst.EXECUTION_VALUE__KIND_NUMBER_VALUE:
                    value = self._convert_binary_to_fp64(bin(entry.u.number_value.value))
                elif value_kind.value == self.env.econst.EXECUTION_VALUE__KIND_STRING_VALUE:
                    value = entry.u.string_value.value
                elif value_kind.value == self.env.econst.EXECUTION_VALUE__KIND_BOOL_VALUE:
                    value = "TRUE" if entry.u.bool_value.value else "FALSE"
                else:
                    value = 'No value found!'

                device_actions_command_tbl.add_row([entry.command, value_kind, value])

        device_actions_grp.append(device_actions_command_tbl)

        try:
            update_table = self.env.vars['update_table']
        except KeyError:
            return None

        device_actions_update_tbl = interface.Table(["Update", "Value"])
        for entry in update_table:
            device_actions_update_tbl.add_row([entry.update, entry.value.value])
        device_actions_grp.append(device_actions_update_tbl)

        return device_actions_grp

    def _create_model_files_group(self):
        try:
            trace_table = self.env.vars['gaa_ota_hotword_trace']
        except KeyError:
            return None

        group = interface.Group("Gaa Hotword models in VA filesystem")
        text = interface.Text("Number of models: {}".format(trace_table.index.value))
        table = interface.Table(["Model Filenames"])

        with trace_table.footprint_prefetched():
            for model in trace_table.models_in_va_fs:
                filename = CLang.get_string(model.value)
                if filename:
                    table.add_row([filename])

        group.append(text)
        group.append(table)
        return group

    def _generate_report_body_elements(self):

        content = []

        grp = interface.Group("Gaa status")
        tbl = interface.Table(["Active"])
        tbl.add_row([
            "Y" if self.active else "N"
        ])
        grp.append(tbl)
        content.append(grp)

        content.append(self._create_device_actions_group())

        content.append(self._create_model_files_group())

        return content
