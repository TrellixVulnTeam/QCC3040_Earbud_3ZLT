############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2021-2022 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.firmware_component import FirmwareComponent
from csr.dev.model import interface

class LeBroadcastManager(FirmwareComponent):
    '''
        Decoder for the LE Broadcast Manager.
    '''

    def __init__(self, env, core, parent=None):
        FirmwareComponent.__init__(self, env, core, parent=parent)

        try:
            self._le_broadcast_manager = env.cu.le_broadcast_manager_source.local.le_broadcast_manager
        except AttributeError:
            raise self.NotDetected

    @property
    def le_broadcast_manager(self):
        ''' Returns the LE Broadcast Manager instance '''
        return self._le_broadcast_manager

    def target_bis_present(self, target_bis):
        '''Given a le_broadcast_manager_bis_sync_state_t structure, provides
           a multi-line representation of the content.
           This is the number of subgroups and then the uint32's
           Uses a limit of 16 items'''
        subgroups = target_bis.num_subgroups.value
        if subgroups == 0:
            return "None"
        else:
            if subgroups < 16:
                return "\n".join(["{subgroups} subgroups".format(subgroups=subgroups)]+[str(target_bis.bis_sync[x]) for x in range(subgroups)])
            else:
                return "{subgroups} subgroups.\nAssuming error.".format(subgroups=subgroups)

    def bis_index_present(self, bis_index):
        return "Subgroup: {}\nIndex:{}".format(bis_index.subgroup.value, bis_index.index.value)

    def _generate_report_body_elements(self):
        ''' Report the LE Broadcast Manager state. '''
        grp = interface.Group("LE Broadcast Manager")

        overview = interface.Table(['Name', 'Value'])
        # Form a list of sources (if any)
        # These are converted to an "item" value style when reported
        source_items = ["ID", "Sync Handle", "Target PA",
                        "Target BIS", "BIS L", "BIS R",
                        "BIS Handle", "BIG Handle", "Sample Rate",
                        "Frame Duration (ms)",
                        "Octets per frame",
                        "Presentation Delay(ms)",
                        "BIG Encryption", "Remove Pending"]
        sources = list()
        sources.append(source_items)
        source_headers = []

        with self._le_broadcast_manager.footprint_prefetched():
            lebm = self._le_broadcast_manager
            overview.add_row(['Sync State', lebm.sync_state])
            overview.add_row(['Sync Source ID', lebm.sync_source_id])
            target_bis = self.target_bis_present(lebm.pending_bis_sync_state)
            overview.add_row(['Pending BIS Sync State', target_bis])
            overview.add_row(['Assistant Scan', lebm.assistant_scan])

            # Create columns for each active source
            # ( do it here because we don't know source #0, source #1 inside the loop )
            source_headers = ["Name"] + ["Source #{}".format(src) for src in(range(lebm.broadcast_source_receive_state.num_elements)) 
                                                                      if lebm.broadcast_source_receive_state[src].source_id.value != 0]

            for source in lebm.broadcast_source_receive_state:
                if source.source_id.value != 0:
                    # Narrow name used for the target PA state
                    target_pa = source.target_pa_sync_state
                    target_pa = "{name} ({value})".format(name = target_pa.symbolic_value[len("scan_delegator_client_pa_sync_"):],
                                                          value = target_pa.value)
                    target_bis = self.target_bis_present(source.target_bis_sync_state)
                    sample_rate = source.codec_config.sample_rate.value
                    frame_ms = source.codec_config.frame_duration.value/1000
                    octets_per_frame = source.codec_config.octets_per_frame.value
                    presentation_delay_ms = source.presentation_delay.value/1000
                    encrypted = bool(source.big_encryption.value)
                    remove_pending = bool(source.remove_pending.value)

                    sources.append([source.source_id,
                                    source.sync_handle,
                                    target_pa,
                                    target_bis,
                                    self.bis_index_present(source.base_bis_index_l),
                                    self.bis_index_present(source.base_bis_index_r),
                                    source.bis_handle,
                                    source.big_handle,
                                    sample_rate,
                                    frame_ms,
                                    octets_per_frame,
                                    presentation_delay_ms,
                                    encrypted,
                                    remove_pending])

        grp.append(overview)

        if len(sources) > 1:
            sourcesTable = interface.Table(source_headers)
            for entries in zip(*sources):
                sourcesTable.add_row(entries)
            grp.append(sourcesTable)
        return [grp]
