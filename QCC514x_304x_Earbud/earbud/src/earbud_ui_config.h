/*!
\copyright  Copyright (c) 2008 - 2019 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_ui_config.h
\brief      Application specific ui configuration
*/

#ifndef EARBUD_UI_CONFIG_H_
#define EARBUD_UI_CONFIG_H_

#include "ui.h"
#include <touch.h>
/*! \brief Return the ui configuration table for the earbud application.

    The configuration table can be passed directly to the ui component in
    domains.

    \param table_length - used to return the number of rows in the config table.

    \return Application specific ui configuration table.
*/
const ui_config_table_content_t* EarbudUi_GetConfigTable(unsigned* table_length);

/*! \brief Configures the Focus Select module in the framework with the
    source prioritisation for the Earbud Application.
*/
void EarbudUi_ConfigureFocusSelection(void);

#ifdef INCLUDE_CAPSENSE
const touch_event_config_t* EarbudUi_GetCapsenseEventTable(unsigned* table_length);
#endif

#endif /* EARBUD_UI_CONFIG_H_ */
