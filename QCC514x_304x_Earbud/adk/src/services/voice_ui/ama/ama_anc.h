/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_anc.h
\brief      File consists of function decalration for Amazon Voice Service's ANC handling.
*/
#ifndef AMA_ANC_H
#define AMA_ANC_H

/*! \brief Initialize the AMA ANC module.
*/
bool AmaAnc_Init(void);

/*! \brief Update the ANC enabled status
    \param[in] bool Enabled
*/
void AmaAnc_EnabledUpdate(bool enabled);

#endif

