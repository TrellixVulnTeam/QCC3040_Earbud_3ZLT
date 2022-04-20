/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\defgroup   handover_profile Handover Profile
\ingroup    profiles
\brief      Handover Profile appsP1 interface
*/

#ifndef HANDOVER_PROFILE_APPS_P1_H_
#define HANDOVER_PROFILE_APPS_P1_H_

#ifdef INCLUDE_MIRRORING

#include "handover_profile.h"

/*! \brief Checks if any of the appsP1 clients vetoes handover.
    \return Status of operation.
*/
handover_profile_status_t handoverProfile_VetoP1Clients(void);

/*! \brief Calls abort function of the appsP1 clients.
    \return Status of operation.
*/
handover_profile_status_t handoverProfile_AbortP1Clients(void);

/*! \brief Marshal appsP1 clients data into a source.
    \param bd_addr Bluetooth address of the link to be marshalled.
    \return 0 if marshalling P1 clients failed, a valid source containing the
            P1 marshal data for the defined bd_addr otherwise.
*/
Source handoverProfile_MarshalP1Clients(const tp_bdaddr *bd_addr);

/*! \brief Unmarshal a single appsP1 client.
    \param bd_addr Bluetooth address of the link being unmarshalled.
    \param src_addr Address of data to unmarshal.
    \param src_len Number of bytes available.
    \param consumed Returns the number of byte consumed by the unmarshalling.
    \return TRUE on success otherwise FALSE.
*/
bool handoverProfile_UnmarshalP1Client(tp_bdaddr *addr, const uint8 *src_addr, uint16 src_len, uint16 *consumed);

/*! \brief Calls commit function of the appsP1 clients.
    \param addr Device address.
    \param is_primary TRUE if the new role if primary, otherwise FALSE.
*/
void handoverProfile_CommitP1Clients(const tp_bdaddr *addr, bool is_primary);

/*! \brief Calls complete function of the appsP1 clients.
    \param is_primary TRUE if the new role if primary, otherwise FALSE.
 */
void handoverProfile_CompleteP1Clients(bool is_primary);

#endif /* INCLUDE_MIRRORING */
#endif /* HANDOVER_PROFILE_APPS_P1_H_ */