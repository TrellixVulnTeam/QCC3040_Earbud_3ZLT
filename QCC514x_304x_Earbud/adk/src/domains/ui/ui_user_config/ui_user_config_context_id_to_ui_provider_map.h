/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   ui_domain UI
\ingroup    domains
\brief      Header file for mapping between GAIA Context IDs and UI Providers and Contexts
*/

#include <csrtypes.h>

#include "ui_user_config.h"
#include "ui_inputs.h"

bool UiUserConfig_LookUpUiProviderAndContext(
        ui_user_config_context_id_t context_id,
        ui_providers_t *provider,
        unsigned *context);

void UiUserConfig_AddProviderMap(
        ui_providers_t provider,
        const ui_user_config_context_id_map * map,
        uint8 map_length);

void UiUserConfig_ResetMap(void);
