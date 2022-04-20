/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Functionality of the secondary device (that becomes primary) after
            handover.
*/
#ifndef HANDOVER_PROTOCOL_SECONDARY_H_
#define HANDOVER_PROTOCOL_SECONDARY_H_

#ifdef INCLUDE_MIRRORING

#include "handover_profile.h"

/*! \brief Start handover procedure.
    \param req The start request message received from the primary earbud.
    \return The status.
*/
handover_profile_status_t handoverProfile_SecondaryStart(const HANDOVER_PROTOCOL_START_REQ_T *req);

/*! \brief Cancel handover procedure. */
void handoverProfile_SecondaryCancel(void);

/*! \brief Handle appsP1 data and unmarshal it.
    \param source Source of marshal data.
    \param len Length of data in the source.
    \return Status of the operation.

    \note This function will always drop len bytes from the source, even on failure.
*/
handover_profile_status_t handoverProfile_SecondaryHandleAppsP1Data(Source source, uint16 len);

/*! \brief Handle Bluetooth stack data and unmarshal it.
    \param source Source of marshal data.
    \param len Length of data in the source.
    \return Status of the operation.

    \note This function will always drop len bytes from the source, even on failure.
    \note After calling this function, the earbud will complete the handover process
          and become primary if this is the final Bluetooth stack data packet.
*/
handover_profile_status_t handoverProfile_SecondaryHandleBtStackData(Source source, uint16 len);

/*! \brief Query if unmarshalling of appsP1 data is complete.
    \return TRUE if appsP1 unmarshalling is complete.
*/
bool handoverProfile_SecondaryIsAppsP1UnmarshalComplete(void);

#endif
#endif
