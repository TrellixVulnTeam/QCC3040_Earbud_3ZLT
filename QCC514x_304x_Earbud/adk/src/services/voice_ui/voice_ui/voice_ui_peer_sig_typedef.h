/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The voice_ui_peer_sig c type definitions. This file is generated by C:/Qualcomm_Prog/qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _VOICE_UI_PEER_SIG_TYPEDEF_H__
#define _VOICE_UI_PEER_SIG_TYPEDEF_H__

#include <csrtypes.h>

/*! Voice Assistant Provider update sent from Primary to Secondary */
typedef struct 
{
    /*! Voice Assistant Provider */
    uint8 va_provider;
    /*! Reboot required */
    uint8 reboot;
} voice_ui_selected_va_provider_t;

#endif /* _VOICE_UI_PEER_SIG_TYPEDEF_H__ */

