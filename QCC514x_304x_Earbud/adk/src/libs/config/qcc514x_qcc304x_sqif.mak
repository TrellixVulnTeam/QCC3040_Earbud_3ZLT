# Pull in the Kymera build configuration
-include config/hydracore.mak

CONFIG_DIRS_FILTER := $(CONFIG_DIRS_FILTER) nfc_api nfc_cl 
# audio_input_i2s csr_i2s_audio_plugin audio_output csr_cvc_common_plugin

TEMP := $(CONFIG_DIRS_FILTER)
CONFIG_DIRS_FILTER = $(filter-out anc, $(TEMP))

CONFIG_FEATURES+=CONFIG_KEY_MANIPULATION

ifeq (synergy, $(LIBTYPE))
    CONFIG_FEATURES+=CONFIG_QCC514X_QCC304X CONFIG_SYNERGY CONFIG_HANDOVER
    CONFIG_FEATURES-=CONFIG_CRYPTO_FP
    CONFIG_DIRS_FILTER+=connection gatt a2dp hfp avrcp gatt_client gatt_server gatt_manager
else
    CONFIG_FEATURES+=CONFIG_QCC514X_QCC304X CONFIG_HANDOVER
    CONFIG_DIRS_FILTER+=a2dp_synergy connection_synergy hfp_synergy avrcp_synergy gatt_synergy gatt_client_synergy gatt_server_synergy gatt_manager_synergy
endif
