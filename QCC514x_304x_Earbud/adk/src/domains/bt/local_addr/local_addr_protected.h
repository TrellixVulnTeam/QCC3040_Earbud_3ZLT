/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Local address functions that should only be used, carefully, by other 
            domain components.
*/

#ifndef LOCAL_ADDR_PROTECTED_H_
#define LOCAL_ADDR_PROTECTED_H_

#include "local_addr.h"


typedef struct local_address_context_struct *local_address_context_t;

/*! \brief Override the settings for Local Address.

    The function replaces current settings (if any) with those supplied.
    A context is returned, to be used when the override is no longer
    required.

    \note Use of this function is not recommended. Earlier system settings will
        be replaced which could affect operation.

    \param task The Task requesting the override.
    \param host Host settings to apply. See LocalAddr_ConfigureBleGeneration()
    \param controller Controller settings to apply. See LocalAddr_ConfigureBleGeneration()

    \return The old context. This should be passed to LocalAddr_ReleaseOverride() 
            when the override is no longer needed.
*/
local_address_context_t LocalAddr_OverrideBleGeneration(
                                    Task task, 
                                    local_addr_host_gen_t host, 
                                    local_addr_controller_gen_t controller);


/*! \brief Stop overriding the local address configuration
    
    \param context Pointer to a value holding the context returned earlier by 
            LocalAddr_OverrideBleGeneration. The context will be released
            and the value set to NULL.
*/
void LocalAddr_ReleaseOverride(local_address_context_t *context);


#endif /* LOCAL_ADDR_PROTECTED_H_ */
