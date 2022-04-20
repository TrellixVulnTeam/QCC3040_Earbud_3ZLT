/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework command data
*/

#ifndef GAIA_FRAMEWORK_COMMAND_H_
#define GAIA_FRAMEWORK_COMMAND_H_

#include <gaia.h>

#include "gaia_framework.h"


/*  Gaia Command ID
 *  15 bits           9        8        7                 0
 *  +--------+--------+--------+--------+--------+--------+
 *  |   Feature  ID   |    PDU  Type    | PDU SPECIFIC ID |
 *  +--------+--------+--------+--------+--------+--------+
 */
#define gaiaFrameworkCommand_GetFeatureID(command_id) (command_id >> 9)
#define gaiaFrameworkCommand_GetPduType(command_id) ((command_id >> 7) & 0x0003)
#define gaiaFrameworkCommand_GetPduSpecificId(command_id) (command_id & 0x007F)

#define gaiaFramework_BuildCommandId(feature_id, pdu_type, pdu_id) (((feature_id << 9) | (pdu_type << 7)) | pdu_id)


typedef enum
{
    pdu_type_command = 0,
    pdu_type_notification = 1,
    pdu_type_response = 2,
    pdu_type_error = 3
} pdu_types_t;

/*! \brief Gaia framework main command handler

    \param transport  GAIA transport command received on
    \param command_id ID of command
    \param vendor_id  ID of vendor
    \param size_payload  Size of payload
    \param payload Pointer to payload, memory own by transport
*/
void GaiaFrameworkCommand_CommandHandler(const GAIA_COMMAND_IND_T *ind);

/*! \brief Resets the vendor specific message handler
*/
void GaiaFrameworkCommand_ResetVendorCommandHandler(void);

/*! \brief Registers a vendor specific command handler in the main handler

    \param command_handler  Vendor specific command handler

    \return True if successfull
*/
bool GaiaFrameworkCommand_RegisterVendorCommandHandler(gaia_framework_vendor_command_handler_fn_t command_handler);


#endif /* GAIA_FRAMEWORK_COMMAND_H_ */
