/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaming_mode.h
\defgroup   gaming_mode Gaming Mode
\ingroup    services
\brief      A component responsible for controlling gaming mode. 
*/

#ifndef GAMING_MODE_H_
#define GAMING_MODE_H_

#include "domain_message.h"
#ifndef INCLUDE_STEREO
#include "earbud_sm_marshal_defs.h"
#endif
#include "peer_signalling.h"

/*! \brief Message IDs for Gaming mode messages to UI clients */
enum gaming_mode_ui_events
{
    GAMING_MODE_ON = GAMING_MODE_UI_MESSAGE_BASE,
    GAMING_MODE_OFF,

    /*! This must be the final message */
    GAMING_MODE_UI_MESSAGE_END
};

#ifdef INCLUDE_GAMING_MODE
/*! \brief Enable gaming mode
    \return TRUE if gaming mode was enabled
*/
bool GamingMode_Enable(void);

/*! \brief Disable gaming mode
    \return TRUE if gaming mode was disabled
*/
bool GamingMode_Disable(void);

/*! \brief Initialise gaming mode module
    \param[in] init_task    Init Task handle
    \param[out] bool        Result of intialization
*/    
bool GamingMode_init(Task init_task);

/*! \brief Check if Gaming Mode is enabled
    \param[out] TRUE if Enabled, FALSE otherwise
*/
bool GamingMode_IsGamingModeEnabled(void);
#ifndef INCLUDE_STEREO
/*! \brief Handle Gaming Mode message from Primary
    \param[in] msg    Gaming mode message
*/
void GamingMode_HandlePeerMessage(earbud_sm_msg_gaming_mode_t *msg);

/*! \brief Handle peer signalling channel connecting/disconnecting
    \param[in] ind The connect/disconnect indication
*/
void GamingMode_HandlePeerSigConnected(const PEER_SIG_CONNECTION_IND_T *ind);

/*! \brief Handle peer signalling message tx confirmation
    \param[in] cfm message confirmation status
*/
void GamingMode_HandlePeerSigTxCfm(const PEER_SIG_MARSHALLED_MSG_CHANNEL_TX_CFM_T* cfm);
#endif /*INCLUDE_STEREO */
#else
#define GamingMode_Enable()
#define GamingMode_Disable()
#define GamingMode_init(tsk) (FALSE)
#define GamingMode_IsGamingModeEnabled() (FALSE)
#ifndef INCLUDE_STEREO
#define GamingMode_HandlePeerMessage(msg)
#define GamingMode_HandlePeerSigConnected(ind)
#define GamingMode_HandlePeerSigTxCfm(cfm)
#endif /*INCLUDE_STEREO */
#endif /* INCLUDE_GAMING_MODE */
#endif /* GAMING_MODE_H_ */
