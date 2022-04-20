/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Mapping between GAIA Context IDs and UI Providers and their context values
*/

#include "ui_user_config_context_id_to_ui_provider_map.h"

#include <csrtypes.h>
#include <logging.h>
#include <panic.h>
#include <stdlib.h>

#include "ui_user_config.h"
#include "ui_inputs.h"

typedef struct
{
    ui_providers_t provider;
    ui_user_config_context_id_map * map;
    uint8 map_length;
} registered_provider_map_data_t;

static registered_provider_map_data_t * provider_context_id_mappings = NULL;
static uint8 num_registered_providers = 0;

void UiUserConfig_AddProviderMap(
        ui_providers_t provider,
        const ui_user_config_context_id_map * map,
        uint8 map_length)
{
    registered_provider_map_data_t * new_provider_map_entry;
    size_t new_size;

    DEBUG_LOG_VERBOSE("UiUserConfig_AddProviderMap enum:ui_providers_t:%d", provider);

    PanicFalse((!num_registered_providers && !provider_context_id_mappings) ||
               ( num_registered_providers &&  provider_context_id_mappings));

    new_size = sizeof(registered_provider_map_data_t) * (num_registered_providers + 1);
    provider_context_id_mappings = PanicNull(realloc(provider_context_id_mappings, new_size));

    new_provider_map_entry = &provider_context_id_mappings[num_registered_providers];
    new_provider_map_entry->provider = provider;
    new_provider_map_entry->map = (ui_user_config_context_id_map *)map;
    new_provider_map_entry->map_length = map_length;

    num_registered_providers++;
}

bool UiUserConfig_LookUpUiProviderAndContext(
        ui_user_config_context_id_t context_id,
        ui_providers_t * provider,
        unsigned *context)
{
    bool found = FALSE;

    /* For each registered provider */
    for (uint8 provider_index = 0; provider_index < num_registered_providers; provider_index++)
    {
        registered_provider_map_data_t * curr_provider_entry = &provider_context_id_mappings[provider_index];
        ui_user_config_context_id_map * curr_map = curr_provider_entry->map;

        /* For each Context ID to context mapping registered */
        for (uint8 map_index = 0; map_index < curr_provider_entry->map_length; map_index++)
        {
            if (curr_map[map_index].context_id == context_id)
            {
                found = TRUE;
                *provider = curr_provider_entry->provider;
                *context = curr_map[map_index].context;
            }
        }
    }
    return found;
}

void UiUserConfig_ResetMap(void)
{
    if (provider_context_id_mappings != NULL)
    {
        free(provider_context_id_mappings);
    }
    provider_context_id_mappings = NULL;
    num_registered_providers = 0;
}
