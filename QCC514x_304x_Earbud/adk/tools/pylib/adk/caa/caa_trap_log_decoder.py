############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2020 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################

from csr.dev.fw.debug_log import TrapLogDecoder

class CaaTrapLogDecoder(TrapLogDecoder):
    ''' Provides further decoding of CAA/ADK specific ids and types '''

    def __init__(self, fw_env, core):
        TrapLogDecoder.__init__(self, fw_env, core)

        # Add Caa component message id enums listed in the file
        # adk\src\domains\common\domain_message.h
        caa_enums = [
            'av_status_messages',
            'pairing_messages',
            'hfp_profile_messages',
            'av_status_messages',
            'hfp_profile_messages',
            'pairing_messages',
            'av_headet_gaia_messages',
            'dfu_messages_t',
            'av_headset_conn_manager_messages',
            'peer_signalling_messages',
            'handset_signalling_messages',
            'phy_state_messages',
            'headset_phy_state_messages',
            'battery_messages',
            'adv_mgr_messages_t',
            'mirror_profile_msg_t',
            'proximity_messages',
            'accelerometer_messages',
            'hall_effect_messages',
            'touch_sensor_messages',
            'chargerMessages',
            'deviceMessages',
            'profile_manager_messages',
            'av_headet_gatt_messages',
            'powerClientMessages',
            'kymera_messages',
            'temperatureMessages',
            'audio_sync_msg_t',
            'volume_domain_messages',
            'peer_pair_le_message_t',
            'peer_find_role_message_t',
            'key_sync_messages',
            'bredr_scan_manager_messages',
            'ui_message_t',
            'av_ui_messages',
            'av_avrcp_messages',
            'powerUiMessages',
            'dfu_peer_messages',
            'telephony_domain_messages',
            'scan_manager_messages',
            'handover_profile_messages',
            'local_name_message_t',
            'local_addr_message_t',
            'device_test_service_message_t',
            'anc_messages_t',
            'leakthrough_msg_t',
            'qcm_msgs_t',
            'wired_audio_detect_msg_t',
            'case_message_t',
            'handset_service_msg_t',
            'state_proxy_messages',
            'hdma_messages_t',
            'volume_service_messages',
            'voice_ui_msg_id_t',
            'audio_curation_messages',
            'tws_topology_message_t',
            'tws_topology_client_notifier_message_t',
            'headset_topology_message_t',
        ]

        for e in caa_enums:
            self._add_enumeration(e)

    def get_task_handler(self, task):
        return self._get_handler_name(self._handler_from_task(task))
