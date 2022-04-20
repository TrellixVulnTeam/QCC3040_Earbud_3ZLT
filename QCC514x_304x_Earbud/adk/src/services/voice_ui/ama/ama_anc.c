/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_anc.c
\brief      Implementation of the ANC handling for Amazon AVS
*/

#ifdef INCLUDE_AMA

#include <logging.h>
#include "ama_anc.h"
// #include "ama_send_command.h"

void AmaAnc_EnabledUpdate(bool enabled)
{
    DEBUG_LOG("AmaAnc_EnabledUpdate(%d)", enabled);
}

/***************************************************************************/

bool AmaAnc_Init(void)
{
    DEBUG_LOG("AmaAnc_Init");
    return TRUE;
}

#endif

