/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_config.h
\brief  Configuration for audio functionality for Amazon Voice Service
*/
#ifndef AMA_CONFIG_H
#define AMA_CONFIG_H

#include "ama_data.h"

#define AMA_CONFIG_DEVICE_TYPE          "A32E8VQVU960EJ"
#define AMA_CONFIG_TEMP_SERIAL_NUMBER   "19348"             /* Arbitary - only used if problem retrieving serial number */
#define AMA_DEFAULT_CODEC_OVER_RFCOMM   ama_codec_msbc      /* Compile time choice between ama_codec_msbc or ama_codec_opus */
#define AMA_DEFAULT_CODEC_OVER_IAP2     ama_codec_msbc      /* Compile time choice between ama_codec_msbc or ama_codec_opus */
#define AMA_DEFAULT_OPUS_CODEC_BIT_RATE AMA_OPUS_16KBPS     /* Compile time choice between AMA_OPUS_16KBPS or AMA_OPUS_32KBPS */

#define AMA_MAX_NUMBER_OF_MICS          (1)                 /* Max number of microphones to use, based on HW availability less may be used */
#define AMA_MIN_NUMBER_OF_MICS          (1)                 /* Min number of microphoness to use, app will panic if not enough are available */

#define MAX_AMA_LOCALES (12)

#ifndef AMA_DEFAULT_LOCALE
#define AMA_DEFAULT_LOCALE              "en-GB"             /* Compile time choice for default locale (must exist in RO file system) */
#endif

#ifndef AMA_AVAILABLE_LOCALES
/* List of locales available in the RO file system. Names are as defined by AVS documentation. Change as necessary */
#define AMA_AVAILABLE_LOCALES           "de-DE", "en-AU", "en-CA", "en-GB", "en-IN", "en-US", "es-ES", "es-MX", "fr-CA", "fr-FR", "it-IT", "ja-JP"
#endif

#ifndef AMA_LOCALE_TO_MODEL_OVERRIDES
/* Extend this list for new locales that use a model defined for another locale e.g. locale:English Canadian uses locale:English US */
#define AMA_LOCALE_TO_MODEL_OVERRIDES   {"en-CA", "en-US"},/* {<locale name>, <model name>}, {<locale name>, <model name>} */
#endif

#ifdef HAVE_RDP_UI
#define ama_GetActionMapping() (1)  /* "Custom RDP event translation" */
#else
#define ama_GetActionMapping() (0)  /* "Dedicated assistant physical button (one button)" */
#endif /* (HAVE_RDP_HW_YE134) */

#endif // AMA_CONFIG_H
