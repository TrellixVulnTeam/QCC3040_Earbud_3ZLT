/*!
\copyright  Copyright (c) 2019 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       earbud_temperature_config.h
\brief      Application specific voltage->temperature configuration
*/

#ifndef EARBUD_TEMPERATURE_CONFIG_H_
#define EARBUD_TEMPERATURE_CONFIG_H_

#include "temperature.h"

/*! \brief Return the voltage->temperature lookup configuration table for the earbud application.

    The configuration table can be passed directly to the temperature component in
    domains.

    \param table_length - used to return the number of rows in the config table.

    \return Application specific temperature configuration table.
*/
const temperature_lookup_t* EarbudTemperature_GetConfigTable(unsigned* table_length);

#endif /* EARBUD_TEMPERATURE_CONFIG_H_ */

