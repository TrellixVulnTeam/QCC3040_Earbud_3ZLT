############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2015 - 2016 Qualcomm Technologies International, Ltd.
#   %%version
#
############################################################################
"""
Copy of interesting information in system_message.h
"""

class SystemMessage(object):
    """
    Potential extension: Dynamically generate this from system_message.h
    """
    BASE_              =  0x8000
    BLUESTACK_BASE_           = (BASE_) 
    BLUESTACK_LC_PRIM         = (BLUESTACK_BASE_ + 1)
    BLUESTACK_LM_PRIM         = (BLUESTACK_BASE_ + 2)
    BLUESTACK_HCI_PRIM        = (BLUESTACK_BASE_ + 3)
    BLUESTACK_DM_PRIM         = (BLUESTACK_BASE_ + 4)
    BLUESTACK_L2CAP_PRIM      = (BLUESTACK_BASE_ + 5)
    BLUESTACK_RFCOMM_PRIM     = (BLUESTACK_BASE_ + 6)
    BLUESTACK_SDP_PRIM        = (BLUESTACK_BASE_ + 7)
    BLUESTACK_BCSP_LM_PRIM    = (BLUESTACK_BASE_ + 8)
    BLUESTACK_BCSP_HQ_PRIM    = (BLUESTACK_BASE_ + 9)
    BLUESTACK_BCSP_BCCMD_PRIM = (BLUESTACK_BASE_ + 10)
    BLUESTACK_CALLBACK_PRIM   = (BLUESTACK_BASE_ + 11)
    BLUESTACK_TCS_PRIM        = (BLUESTACK_BASE_ + 12)
    BLUESTACK_BNEP_PRIM       = (BLUESTACK_BASE_ + 13)
    BLUESTACK_TCP_PRIM        = (BLUESTACK_BASE_ + 14)
    BLUESTACK_UDP_PRIM        = (BLUESTACK_BASE_ + 15)
    BLUESTACK_FB_PRIM         = (BLUESTACK_BASE_ + 16)
    BLUESTACK_ATT_PRIM        = (BLUESTACK_BASE_ + 18)
    BLUESTACK_MDM_PRIM        = (BLUESTACK_BASE_ + 22)
    BLUESTACK_END_            = (BLUESTACK_BASE_ + 23)
    FROM_HOST                 = (BASE_ + 32)
    MORE_DATA                 = (BASE_ + 33)
    MORE_SPACE                = (BASE_ + 34)
    PIO_CHANGED               = (BASE_ + 35)
    FROM_KALIMBA              = (BASE_ + 36)
    ADC_RESULT                = (BASE_ + 37)
    STREAM_DISCONNECT         = (BASE_ + 38)
    ENERGY_CHANGED            = (BASE_ + 39)
    STATUS_CHANGED            = (BASE_ + 40)
    SOURCE_EMPTY              = (BASE_ + 41)
    FROM_KALIMBA_LONG         = (BASE_ + 42)
    USB_ENUMERATED            = (BASE_ + 43)
    USB_SUSPENDED             = (BASE_ + 44)
    CHARGER_CHANGED           = (BASE_ + 45)
    PSFL_FAULT                = (BASE_ + 46)
    USB_DECONFIGURED          = (BASE_ + 47)
    USB_ALT_INTERFACE         = (BASE_ + 48)
    USB_ATTACHED              = (BASE_ + 49)
    USB_DETACHED              = (BASE_ + 50)
    KALIMBA_WATCHDOG_EVENT    = (BASE_ + 51)
    TX_POWER_CHANGE_EVENT     = (BASE_ + 52)
    CAPACITIVE_SENSOR_CHANGED = (BASE_ + 53)
    STREAM_SET_DIGEST         = (BASE_ + 55)
    STREAM_PARTITION_VERIFY   = (BASE_ + 56)
    STREAM_REFORMAT_VERIFY    = (BASE_ + 57)
    INFRARED_EVENT            = (BASE_ + 58)
    DFU_SQIF_STATUS           = (BASE_ + 59)
    FROM_OPERATOR             = (BASE_ + 60)
    EXE_FS_VALIDATION_STATUS  = (BASE_ + 61)
    FROM_OPERATOR_FRAMEWORK   = (BASE_ + 62)
    IMAGE_UPGRADE_ERASE_STATUS = (BASE_ + 63)
    CHARGER_DETECTED          = (BASE_ + 64)
    CHARGER_STATUS            = (BASE_ + 65)
    CHARGER_EMERGENCY         = (BASE_ + 66)
    SUBSYSTEM_VERSION_INFO    = (BASE_ + 70)
    SUBSYSTEM_EVENT_REPORT    = (BASE_ + 71)
    IMAGE_UPGRADE_COPY_STATUS = (BASE_ + 72)
    IMAGE_UPGRADE_AUDIO_STATUS= (BASE_ + 75)
    IMAGE_UPGRADE_HASH_ALL_SECTIONS_UPDATE_STATUS = (BASE_ + 76)
    SD_MMC_INSERTED           = (BASE_ + 77)
    SD_MMC_REMOVED            = (BASE_ + 78)
    RA_PARTITION_BG_ERASE_STATUS = (BASE_ + 79)
    SOURCE_AUDIO_SYNCHRONISED = (BASE_ + 80)
    SINK_AUDIO_SYNCHRONISED   = (BASE_ + 81)
    CHARGERCOMMS_IND          = (BASE_ + 82)
    CHARGERCOMMS_STATUS       = (BASE_ + 83)


    id_to_type = {
                  MORE_DATA : "MessageMoreData",
                  MORE_SPACE : "MessageMoreSpace",
                  PIO_CHANGED : "MessagePioChanged",
                  STREAM_DISCONNECT : "MessageStreamDisconnect",
                  SOURCE_EMPTY : "MessageSourceEmpty",
                  USB_ALT_INTERFACE : "MessageUsbAltInterface",
                  USB_SUSPENDED : "MessageUsbSuspended"
                  }
