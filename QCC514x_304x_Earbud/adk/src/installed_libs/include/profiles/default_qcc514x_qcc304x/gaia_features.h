/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file containing gaia plugin feature id's
*/

#ifndef GAIA_FEATURES_H_
#define GAIA_FEATURES_H_


/*! @todo Move to the correct place */
#define GAIA_V3_VENDOR_ID 0x001D

typedef enum
{
    GAIA_CORE_FEATURE_ID = 0x00,                /*<! Gaia core plugin feature ID */
    GAIA_EARBUD_FEATURE_ID = 0x01,              /*<! Gaia earbud plugin feature ID */
    GAIA_AUDIO_CURATION_OLD_FEATURE_ID  = 0x02, /*<! NOTE: This feature ID is deprecated and shouldn't be used. */
    GAIA_VOICE_UI_FEATURE_ID = 0x03,            /*<! Gaia voice UI plugin feature ID */
    GAIA_DEBUG_FEATURE_ID = 0x04,               /*<! Gaia debug plugin feature ID */
    GAIA_MUSIC_PROCESSING_FEATURE_ID = 0x05,    /*<! Gaia media processing plugin feature ID */
    GAIA_DFU_FEATURE_ID = 0x06,                 /*<! Gaia DFU plugin feature ID */
    GAIA_HANDSET_SERVICE_FEATURE_ID = 0x07,                 /*<! Gaia DFU plugin feature ID */
    GAIA_AUDIO_CURATION_FEATURE_ID  = 0x08,     /*<! Gaia audio curation plugin feature ID */
    GAIA_FIT_TEST_FEATURE_ID = 0x09,            /*<! Gaia fit test plugin feature ID */
    GAIA_VOICE_ENHANCEMENT_FEATURE_ID = 0x0a,   /*<! Gaia voice enhancement plugin feature ID */
} gaia_features_t;

#endif /* GAIA_FEATURES_H_ */
