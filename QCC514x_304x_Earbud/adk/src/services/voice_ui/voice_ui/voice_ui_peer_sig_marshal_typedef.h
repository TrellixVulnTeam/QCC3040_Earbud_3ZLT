/*!
    \copyright Copyright (c) 2022 Qualcomm Technologies International, Ltd.
        All Rights Reserved.
        Qualcomm Technologies International, Ltd. Confidential and Proprietary.
    \version %%version
    \file 
    \brief The voice_ui_peer_sig marshal type declarations. This file is generated by C:/Qualcomm_Prog/qcc514x-qcc304x-src-1-0_qtil_standard_oem_earbud-ADK-21.1-CS2-MR1/adk/tools/packages/typegen/typegen.py.
*/

#ifndef _VOICE_UI_PEER_SIG_MARSHAL_TYPEDEF_H__
#define _VOICE_UI_PEER_SIG_MARSHAL_TYPEDEF_H__

#include <csrtypes.h>
#include <app/marshal/marshal_if.h>


#define VOICE_UI_PEER_SIG_MARSHAL_TYPES_TABLE(ENTRY)\
    ENTRY(voice_ui_selected_va_provider_t)

#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum VOICE_UI_PEER_SIG_MARSHAL_TYPES
{
    VOICE_UI_PEER_SIG_MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_VOICE_UI_PEER_SIG_MARSHAL_TYPES
} ;

#undef EXPAND_AS_ENUMERATION

extern const marshal_type_descriptor_t * const voice_ui_peer_sig_marshal_type_descriptors[];
#endif /* _VOICE_UI_PEER_SIG_MARSHAL_TYPEDEF_H__ */
