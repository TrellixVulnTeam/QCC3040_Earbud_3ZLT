/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       ama_log.h
\brief      Logs AMA commands and responses
*/
#ifndef AMA_LOG_H
#define AMA_LOG_H

#include "accessories.pb-c.h"

#define AMA_LOG_SENDING     TRUE
#define AMA_LOG_RECEIVING   FALSE

/*! \brief Log the contents of a ControlEnvelope
 *  \param sending TRUE (AMA_LOG_SENDING) if sending, else FALSE (AMA_LOG_RECEIVING) if receiving
 *  \param control_envelope pointer to ControlEnvelope structure to log
 *  \param packed_envelope array for the packed envelope
 *  \param envelope_size size of the packed envelope
 */
void AmaLog_ControlEnvelope(bool sending,
                                     ControlEnvelope* control_envelope,
                                     uint8 *packed_envelope,
                                     size_t envelope_size);

/*! \brief Log with vprintf where DEBUG_LOG... cannot be used.
 *  \param fmt printf format string
 *  \param variable number of parameters as defined by the fmt string
 */
void AmaLog_LogVaArg(const char* fmt, ...);

#endif  /* AMA_LOG_H */

