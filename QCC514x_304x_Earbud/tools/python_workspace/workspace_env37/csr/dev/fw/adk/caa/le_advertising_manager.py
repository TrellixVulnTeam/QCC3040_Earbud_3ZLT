############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface
from .structs import IAdkStructHandler

class LeAdvertisingManager(FirmwareComponent):
    ''' This class reports the state of the LE Advertising Manager service '''

    @property
    def adv_state(self):
        ''' Returns the LE Advertising Managers state for legacy advertising'''
        return self.env.cu.le_advertising_manager_sm.local.sm.deref.state

    @property
    def ext_adv_state(self):
        ''' Returns the LE Advertising Managers state for extended advertising'''
        try:
            return self.env.gbl.le_adv_mgr_extended_state
        except AttributeError:
            return None

    @property
    def allowed_advert_types(self):
        '''Return a list of allowed advertisement types (enum)
           If no adverts are enabled then None is returned'''
        enabled = self.env.gbl.app_adv_manager.mask_enabled_events.value
        if not enabled:
            return None
        enabled_adverts = []
        for enum_name in self.env.enum.le_adv_event_type_t.keys():
            enum_value = int(self.env.enum.le_adv_event_type_t[enum_name])
            if enabled & enum_value:
                # To make sure we can decode the enum correctly, create a 
                # variable of the correct type locally. i.e. PC memory
                enum = self.env.cast(0, "le_adv_event_type_t", data_mem=[None]*4)
                enum.value = enum_value
                enabled_adverts.append(enum)
        return enabled_adverts

    @property
    def blocking_condition(self):
        ''' Returns the blocking condition cast to the enum type'''
        condition = self.env.gbl.app_adv_manager.blockingCondition
        named = self.env.cast(condition, "le_adv_blocking_condition_t")
        return named

    @property
    def blocked_by(self):
        '''Indicates if advertising is blocked.
           Returns a list of blockers by enum (name).
           or True if only topology can block.
           If not blocked, None is returned.
        '''
        try:
            block = self.env.gbl.app_adv_manager.is_advertising_blocked.value
            if block:
                who = []
                for enum_name in self.env.enum.le_adv_advertising_blocks_t.keys():
                    enum_value =  int(self.env.enum.le_adv_advertising_blocks_t[enum_name])
                    if block & enum_value:
                        # To make sure we can decode the enum correctly, create a 
                        # variable of the correct type locally. i.e. PC memory
                        enum = self.env.cast(0, "le_adv_advertising_blocks_t", data_mem=[None]*4)
                        enum.value = enum_value
                        who.append(enum)
                return who
            return None
        except AttributeError:
            allow = self.env.gbl.app_adv_manager.is_advertising_allowed.value
            if allow:
                return None
        return [True]

    @property
    def legacy_sets(self):
        '''Returns a string for selected sets'''
        params = self.env.var.start_params
        set = params.set.value
        if not set:
            return None
        selected = []
        for enum in self.env.enum.le_adv_data_set_t.keys():
            if set & int(self.env.enum.le_adv_data_set_t[enum]):
                selected.append(enum)
        return selected

    @property
    def legacy_type(self):
        return self.env.var.start_params.event

    @property
    def legacy_interval_type(self):
        try:
            return self.env.gbl.app_adv_manager.params_handle.deref.active_params_set
        except InvalidDereference:
            return None

    @property
    def legacy_advertising_rates(self):
        try:
            range = self.env.gbl.app_adv_manager.params_handle.deref.params_set.deref.set_type[self.legacy_interval_type.value]
        except:
            return None
        else:
            min_interval = range.le_adv_interval_min
            max_interval = range.le_adv_interval_max
            return (min_interval, max_interval)

    def _generate_report_body_elements(self):
        ''' Report the overall state of LE Advertising Manager'''
        status = interface.Group("Overall status")

        tbl = interface.Table(['State','Value'])
        tbl.add_row(["Legacy advertising state", self.adv_state])

        ext_state = self.ext_adv_state or "Extended advertising not supported"
        tbl.add_row(["Extended advertising state", ext_state])
        selected_sets = self.legacy_sets
        tbl.add_row(["Active advertising set(s)", selected_sets])

        allowed = self.allowed_advert_types or None
        tbl.add_row(["Allowed advert type(s)", allowed])

        tbl.add_row(["SM blocking condition", self.blocking_condition])

        # The plugin supports an extended blocking mechanism to ban advertisements
        # If original method, make more human friendly
        blocked = self.blocked_by or "Not disabled"
        if blocked and blocked[0] is True:
            blocked = "Topology"
        tbl.add_row(["Advertising disabled by", blocked])
        status.append(tbl)

        settings = interface.Group("Current settings")
        if selected_sets:
            tbl = interface.Table(['State','Value'])
            tbl.add_row(["Type of advertising", self.legacy_type])
            tbl.add_row(["Current speed", self.legacy_interval_type])

            rates = self.legacy_advertising_rates or "Could not access advertising rate"
            if rates:
                rates = "({min},{max}) = {min_ms}-{max_ms}ms".format(
                            min=rates[0], max=rates[1], 
                            min_ms=rates[0].value * 0.625, max_ms=rates[1].value * 0.625)
            tbl.add_row(["Advertising Interval", rates])
            settings.append(tbl)
        else:
            settings.append("Not advertising")

        return [status, settings]

