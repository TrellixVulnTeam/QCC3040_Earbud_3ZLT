/*!
\copyright  Copyright (c) 2019 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   handset_service Handset Service
\ingroup    services
\brief      Configurable values for the Handset Service.
*/

#ifndef HANDSET_SERVICE_CONFIG_H_
#define HANDSET_SERVICE_CONFIG_H_

#include "handset_service.h"
#include "rtime.h"

/*! Maximum permitted number of BR/EDR ACL connections
*/
#define HANDSET_SERVICE_MAX_PERMITTED_BREDR_CONNECTIONS 2

/*! Maximum number of handset ACL connect requests to attempt.

    This is the maximum number of times the BR/EDR ACL connection to the
    handset will be attempted for a single client handset connect request.

    After this the connection request will be completed with a fail status.
*/
uint8 handsetService_BredrAclConnectAttemptLimit(void);

/*! Time delay between each handset ACL connect retry.

    The delay is in ms.
*/
#define handsetService_BredrAclConnectRetryDelayMs() (500)

/*! Maximum permitted number of BR/EDR ACL connections

    Can be modified using HandsetService_Configure
*/
uint8 handsetService_BredrAclMaxConnections(void);

/*! Handle creation of the SELF device

  This is the time when data can be stored in the device database.
*/
void handsetService_HandleSelfCreated(void);

/*! Handle update of handset_service_config property
    It is expected to be called on the secondary earbud.
*/
void handsetService_HandleConfigUpdate(void);

/*! Maximum permitted number of LE ACL connections
*/
uint8 handsetService_LeAclMaxConnections(void);

/*! Initialise handset service configuration to defaults
*/
void HandsetServiceConfig_Init(void);

/*! Time to stop advertising for when a device connects that could pair

    The advertising suspension will finish earlier if pairing completes.
 */
#define handsetService_AdvertisingSuspensionForPairingMs() (5000)


#endif /* HANDSET_SERVICE_CONFIG_H_ */
