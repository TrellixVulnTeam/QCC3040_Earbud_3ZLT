/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for the gaia framework API
*/

#define DEBUG_LOG_MODULE_NAME gaia_framework
#include <logging.h>

#include "gaia_framework_command.h"
#include "gaia_framework_feature.h"
#include <gaia_features.h>
#include <gaia.h>
#include <panic.h>


static gaia_framework_vendor_command_handler_fn_t vendor_specific_handler;


static void gaiaFrameworkCommand_SendV2Packet(GAIA_TRANSPORT *t, uint16 vendor_id, uint16 command_id, uint16 status,
                                              uint16 payload_length, uint8 *payload)
{
    DEBUG_LOG("gaiaFramework_SendV2Packet, command_id %04x, status %u, payload_length %u",
              command_id, status, payload_length);

    GaiaSendPacket(t, vendor_id, command_id, status, payload_length, payload);
}

static void gaiaFrameworkCommand_SendV2Response(GAIA_TRANSPORT *t, uint16 vendor_id, uint16 command_id, uint16 status,
                                                uint16 payload_length, uint8 *payload)
{
    gaiaFrameworkCommand_SendV2Packet(t, vendor_id, command_id | GAIA_ACK_MASK, status,
                                      payload_length, payload);
}

static void gaiaFrameworkCommand_ReplyV2GetApiVersion(GAIA_TRANSPORT *t, uint16 vendor_id, uint16 command_id, uint8 protocol_version)
{
    uint8 payload[3] = { protocol_version, GAIA_V3_VERSION_MAJOR, GAIA_V3_VERSION_MINOR };
    gaiaFrameworkCommand_SendV2Response(t, vendor_id, command_id, GAIA_STATUS_SUCCESS, 3, payload);
}

void GaiaFrameworkCommand_CommandHandler(const GAIA_COMMAND_IND_T *ind)
{
    DEBUG_LOG_DEBUG("gaiaFramework_CommandHandler, vendor_id %d, command_id 0x%04x, size_payload %u",
                    ind->vendor_id, ind->command_id, ind->size_payload);

    const uint8 feature_id = gaiaFrameworkCommand_GetFeatureID(ind->command_id);
    const uint8 pdu_type = gaiaFrameworkCommand_GetPduType(ind->command_id);
    const uint8 pdu_specific_id = gaiaFrameworkCommand_GetPduSpecificId(ind->command_id);

    switch (ind->vendor_id)
    {
        case GAIA_V3_VENDOR_ID:
        {
            DEBUG_LOG("gaiaFramework_CommandHandler, feature_id %u, pdu_type %u, pdu_specific_id %u",
                      feature_id, pdu_type, pdu_specific_id);

            gaia_framework_command_status_t status = GaiaFrameworkFeature_SendToFeature(ind->transport, feature_id, pdu_type, pdu_specific_id, ind->size_payload, ind->payload);
            switch (status)
            {
                case feature_not_handled:
                    DEBUG_LOG_ERROR("gaiaFramework_CommandHandler, unsupported feature_id %u, pdu_type %u, pdu_specific_id %u",
                                    feature_id, pdu_type, pdu_specific_id);
                    GaiaFramework_SendError(ind->transport, feature_id, pdu_specific_id, feature_not_supported);
                    Gaia_CommandResponse(ind->transport, ind->size_payload, ind->payload);
                    break;

                case command_not_handled:
                    DEBUG_LOG_ERROR("gaiaFramework_CommandHandler, unhandled command, feature_id %u, pdu_type %u, pdu_specific_id %u",
                                    feature_id, pdu_type, pdu_specific_id);
                    GaiaFramework_SendError(ind->transport, feature_id, pdu_specific_id, command_not_supported);
                    Gaia_CommandResponse(ind->transport, ind->size_payload, ind->payload);
                    break;

                case command_handled:
                    Gaia_CommandResponse(ind->transport, ind->size_payload, ind->payload);
                    break;

                case command_pending:
                    DEBUG_LOG_DEBUG("gaiaFramework_CommandHandler, pending feature_id %u, pdu_type %u, pdu_specific_id %u",
                                    feature_id, pdu_type, pdu_specific_id);
                    break;
            }

        }
        break;

        case GAIA_VENDOR_QTIL:
        {
            if ((ind->command_id & GAIA_COMMAND_TYPE_MASK) == GAIA_COMMAND_GET_API_VERSION)
            {
                uint32 version;
                PanicFalse(Gaia_TransportGetInfo(ind->transport, GAIA_TRANSPORT_PROTOCOL_VERSION, &version));
                DEBUG_LOG("gaiaFramework_CommandHandler, send Get API V2 reply with V3 version");
                gaiaFrameworkCommand_ReplyV2GetApiVersion(ind->transport, ind->vendor_id, ind->command_id, version);
            }
            else
            {
                DEBUG_LOG_ERROR("gaiaFramework_CommandHandler, vendor_id %u, command_id %u, send V2 status not supported", ind->vendor_id, ind->command_id);
                gaiaFrameworkCommand_SendV2Response(ind->transport, ind->vendor_id, ind->command_id, GAIA_STATUS_NOT_SUPPORTED, 0, NULL);
            }

            Gaia_CommandResponse(ind->transport, ind->size_payload, ind->payload);
        }
        break;

        default:
        {
            /* Call vendor specific command handler if it has been set */
            if (vendor_specific_handler)
            {
                DEBUG_LOG_INFO("gaiaFramework_CommandHandler, calling vendor specific command handler");
                vendor_specific_handler(ind->transport, ind->vendor_id, feature_id, pdu_type, pdu_specific_id, ind->size_payload, ind->payload);
            }
            else
            {
                DEBUG_LOG_ERROR("gaiaFramework_CommandHandler, no registered vendor specific command handler");
                GaiaFramework_SendVendorError(ind->transport, ind->vendor_id, feature_id, pdu_specific_id, command_not_supported);
            }

            /* Indciate to GAIA that command has been handled */
            Gaia_CommandResponse(ind->transport, ind->size_payload, ind->payload);
        }
        break;
    }
}

void GaiaFrameworkCommand_ResetVendorCommandHandler(void)
{
    DEBUG_LOG("GaiaFrameworkCommand_ResetVendorCommandHandler");
    vendor_specific_handler = NULL;
}

bool GaiaFrameworkCommand_RegisterVendorCommandHandler(gaia_framework_vendor_command_handler_fn_t command_handler)
{
    DEBUG_LOG("GaiaFrameworkCommand_RegisterVendorCommandHandler");
    if (!vendor_specific_handler)
    {
        vendor_specific_handler = command_handler;
        return TRUE;
    }
    else
        return FALSE;
}

