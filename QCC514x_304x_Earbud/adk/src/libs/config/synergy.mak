
ifeq (synergy, $(LIBTYPE))
    CONFIG_FEATURES+=CONFIG_SYNERGY

    CONFIG_DIRS_FILTER+= connection gatt a2dp hfp avrcp gatt_client gatt_server gatt_manager gatt_battery_server gatt_device_info_server gatt_ascs_server gatt_aics_client gatt_bass_client gatt_bass_server gatt_cas_server gatt_csis_server gatt_pacs_server gatt_telephone_bearer_server gatt_vcs_client gatt_vcs_server gatt_service_discovery gatt_vocs_client vcp bap_server spp_common sppc spps ccp
    SYNERGY_DIRS = a2dp_synergy connection_synergy hfp_synergy avrcp_synergy gatt_synergy gatt_client_synergy gatt_server_synergy gatt_manager_synergy

    TEMP_CONFIG_DIRS_FILTER := $(CONFIG_DIRS_FILTER)
    CONFIG_DIRS_FILTER = $(filter-out $(SYNERGY_DIRS), $(TEMP_CONFIG_DIRS_FILTER))
endif
