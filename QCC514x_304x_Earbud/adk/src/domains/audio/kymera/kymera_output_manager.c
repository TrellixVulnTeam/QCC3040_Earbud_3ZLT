/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Kymera manager of the output chain
*/
#include <stdlib.h>
#include "kymera.h"
#include "kymera_output_if.h"
#include "kymera_output_private.h"
#include "kymera_output_common_chain_config.h"
#include <panic.h>
#include <logging.h>

#define kymera_Register(registry, user_info) \
    do { \
    registry.entries = PanicNull(realloc(registry.entries, (registry.length + 1) * sizeof(*registry.entries))); \
    registry.entries[registry.length++] = user_info; \
    } while(0)

#define kymera_SendIndication(function, ...) \
    do { \
        const indications_registry_entry_t *entries = state.indications_registry.entries; \
        for(unsigned i = 0; i < state.indications_registry.length; i++) \
        { \
            PanicFalse(entries[i] != NULL); \
            if (entries[i]->function) \
                entries[i]->function(__VA_ARGS__); \
        } \
    } while(0)

typedef const output_registry_entry_t* registry_entry_t;

typedef struct
{
    uint8 length;
    registry_entry_t *entries;
} registry_t;

typedef const output_indications_registry_entry_t* indications_registry_entry_t;

typedef struct
{
    uint8 length;
    indications_registry_entry_t *entries;
} indications_registry_t;

static struct
{
    registry_t registry;
    indications_registry_t indications_registry;
    output_users_t ready_users;
    output_users_t connected_users;
    kymera_output_chain_config current_chain_config;
} state =
{
    .registry = {0, NULL},
    .ready_users = output_user_none,
    .connected_users = output_user_none,
    .current_chain_config = {0},
};

static registry_entry_t kymera_GetUserRegistryEntry(output_users_t user)
{
    for(unsigned i = 0; i < state.registry.length; i++)
    {
        if (state.registry.entries[i]->user == user)
            return state.registry.entries[i];
    }

    return NULL;
}

static inline registry_entry_t kymera_AssertValidUserRegistryEntry(output_users_t user)
{
    return PanicNull((void *)kymera_GetUserRegistryEntry(user));
}

static inline output_connection_t kymera_GetUserConnectionType(output_users_t user)
{
    return kymera_AssertValidUserRegistryEntry(user)->connection;
}

static bool kymera_IsUserAssumedChainCompatible(output_users_t user)
{
    return kymera_AssertValidUserRegistryEntry(user)->assume_chain_compatibility;
}

static inline bool kymera_IsRegisteredUser(output_users_t user)
{
    return (kymera_GetUserRegistryEntry(user) != NULL);
}

static void kymera_RegisterForIndications(const indications_registry_entry_t user_info)
{
    kymera_Register(state.indications_registry, user_info);
}

static void kymera_RegisterUser(const output_registry_entry_t *user_info)
{
    kymera_Register(state.registry, user_info);
}

static bool kymera_IsMainConnection(output_connection_t connection)
{
    switch(connection)
    {
        case output_connection_mono:
        case output_connection_stereo:
            return TRUE;
        default:
            return FALSE;
    }
}

static bool kymera_IsAuxConnection(output_connection_t connection)
{
    return (connection == output_connection_aux);
}

static bool kymera_CanConnectConcurrently(output_connection_t a, output_connection_t b)
{
    if (kymera_IsMainConnection(a) && kymera_IsMainConnection(b))
        return FALSE;

    if (kymera_IsAuxConnection(a) && kymera_IsAuxConnection(b))
        return FALSE;

    return TRUE;
}

static void kymera_UpdateInputSampleRate(output_users_t user, const kymera_output_chain_config *user_config)
{
    output_connection_t connection = kymera_GetUserConnectionType(user);

    if (kymera_IsMainConnection(connection))
        KymeraOutput_SetMainSampleRate(user_config->rate);

    if (kymera_IsAuxConnection(connection))
        KymeraOutput_SetAuxSampleRate(user_config->rate);
}

static void kymera_CreateOutputChain(const kymera_output_chain_config *output_config)
{
    DEBUG_LOG("kymera_CreateOutputChain");
    state.current_chain_config = *output_config;
    KymeraOutput_CreateOperators(&state.current_chain_config);
    KymeraOutput_ConnectChain();
    appKymeraExternalAmpControl(TRUE);
}

static void kymera_DestroyOutputChain(void)
{
    DEBUG_LOG("kymera_DestroyOutputChain");
    memset(&state.current_chain_config, 0, sizeof(state.current_chain_config));
    appKymeraExternalAmpControl(FALSE);
    KymeraOutput_DestroyChain();
}

static bool kymera_ConnectToOutputChain(output_users_t user, const output_source_t *sources)
{
    bool status = TRUE;
    output_connection_t connection = kymera_GetUserConnectionType(user);
    DEBUG_LOG("kymera_ConnectToOutputChain: enum:output_users_t:%d, enum:output_connection_t:%d", user, connection);
    kymera_SendIndication(OutputConnectingIndication, user, connection);

    switch(connection)
    {
        case output_connection_mono:
            KymeraOutput_ConnectToMonoMainInput(sources->mono);
            break;
        case output_connection_aux:
            KymeraOutput_ConnectToAuxInput(sources->aux);
            break;
        case output_connection_stereo:
            KymeraOutput_ConnectToStereoMainInput(sources->stereo.left, sources->stereo.right);
            break;
        default:
            Panic();
            break;
    }

    return status;
}

static void kymera_DisconnectFromOutputChain(output_users_t user)
{
    output_connection_t connection = kymera_GetUserConnectionType(user);
    DEBUG_LOG("kymera_DisconnectFromOutputChain: enum:output_users_t:%d, enum:output_connection_t:%d", user, connection);

    switch(connection)
    {
        case output_connection_mono:
            KymeraOutput_DisconnectMonoMainInput();
            break;
        case output_connection_aux:
            KymeraOutput_DisconnectAuxInput();
            break;
        case output_connection_stereo:
            KymeraOutput_DisconnectStereoMainInput();
            break;
        default:
            Panic();
            break;
    }

    kymera_SendIndication(OutputDisconnectedIndication, user, connection);
}

static bool kymera_IsCurrentChainCompatible(const kymera_output_chain_config *config)
{
    return (memcmp(config, &state.current_chain_config, sizeof(*config)) == 0);
}

static inline bool kymera_IsConnectedUser(output_users_t user)
{
    return (state.connected_users & user) != output_user_none;
}

static inline output_users_t kymera_GetCurrentUsers(void)
{
    return state.ready_users | state.connected_users;
}

static inline bool kymera_IsCurrentUser(output_users_t user)
{
    return (kymera_GetCurrentUsers() & user) != output_user_none;
}

static inline bool kymera_NoCurrentUsers(void)
{
    return kymera_GetCurrentUsers() == output_user_none;
}

static bool kymera_IsCompatible(output_users_t user, const kymera_output_chain_config *config)
{
    // If there are no current users there is no chain to be incompatible with
    if (kymera_NoCurrentUsers())
        return TRUE;

    if (kymera_IsUserAssumedChainCompatible(user))
        return TRUE;

    return kymera_IsCurrentChainCompatible(config);
}

static bool kymera_IsInputConnectable(output_connection_t input)
{
    const registry_entry_t *entries = state.registry.entries;
    // If there are no users the chain hasn't been created yet
    if (kymera_NoCurrentUsers() || (input == output_connection_none))
        return FALSE;

    for(unsigned i = 0; i < state.registry.length; i++)
    {
        if (kymera_IsConnectedUser(entries[i]->user))
        {
            if (kymera_CanConnectConcurrently(entries[i]->connection, input) == FALSE)
                return FALSE;
        }
    }

    return TRUE;
}

static bool kymera_IsUserConnectable(output_users_t user)
{
    return kymera_IsRegisteredUser(user) && kymera_IsInputConnectable(kymera_GetUserConnectionType(user));
}

static bool kymera_DisconnectUser(output_users_t user)
{
    bool no_current_users = FALSE;
    bool disconnect = kymera_IsConnectedUser(user);
    state.connected_users &= ~user;
    state.ready_users &= ~user;

    DEBUG_LOG_DEBUG("kymera_DisconnectUser: enum:output_users_t:%d", user);

    if (disconnect)
        kymera_DisconnectFromOutputChain(user);

    if (kymera_NoCurrentUsers())
    {
        kymera_DestroyOutputChain();
        no_current_users = TRUE;
    }

    return no_current_users;
}

static bool kymera_CanDisconnectUsers(output_users_t users)
{
    const registry_entry_t *entries = state.registry.entries;
    bool status = TRUE;

    for(unsigned i = 0; i < state.registry.length; i++)
    {
        if (entries[i]->user & users)
        {
            PanicFalse(kymera_IsCurrentUser(entries[i]->user));

            if (entries[i]->callbacks && entries[i]->callbacks->OutputDisconnectRequest &&
                entries[i]->callbacks->OutputDisconnectRequest())
            {
                DEBUG_LOG_VERBOSE("kymera_CanDisconnectUsers: Can disconnect enum:output_users_t:%d", entries[i]->user);
            }
            else
            {
                DEBUG_LOG_WARN("kymera_CanDisconnectUsers: Cannot disconnect enum:output_users_t:%d", entries[i]->user);
                status = FALSE;
            }
        }
    }

    return status;
}

static void kymera_DisconnectUsers(output_users_t users)
{
    const registry_entry_t *entries = state.registry.entries;

    for(unsigned i = 0; i < state.registry.length; i++)
    {
        if (entries[i]->user & users)
        {
            PanicFalse(kymera_IsCurrentUser(entries[i]->user));

            if (entries[i]->callbacks && entries[i]->callbacks->OutputDisconnectPrepare)
                entries[i]->callbacks->OutputDisconnectPrepare();

            kymera_DisconnectUser(entries[i]->user);

            if (entries[i]->callbacks && entries[i]->callbacks->OutputDisconnectComplete)
                entries[i]->callbacks->OutputDisconnectComplete();
        }
    }
}

static bool kymera_AttemptToDisconnectUsers(output_users_t users)
{
    if (kymera_CanDisconnectUsers(users))
    {
        kymera_DisconnectUsers(users);
        return TRUE;
    }
    else
        return FALSE;
}

static bool kymera_PopulatePreferredChainConfig(kymera_output_chain_config *preferred_config, output_users_t user)
{
    registry_entry_t entry = kymera_GetUserRegistryEntry(user);
    return entry && entry->callbacks->OutputGetPreferredChainConfig && entry->callbacks->OutputGetPreferredChainConfig(preferred_config);
}

static bool kymera_PopulateCommonChainConfig(kymera_output_chain_config *common_config)
{
    const kymera_output_chain_config *config = Kymera_OutputCommonChainGetConfig();
    if (config)
    {
        *common_config = *config;
    }
    return config != NULL;
}

static void kymera_PopulateOutputChainConfig(kymera_output_chain_config *config, output_users_t user,
                                             const kymera_output_chain_config *user_config)
{
    if (kymera_PopulateCommonChainConfig(config) && kymera_IsCompatible(user, config))
    {
        DEBUG_LOG_DEBUG("kymera_PopulateOutputChainConfig: Override using common config");
    }
    else if (kymera_PopulatePreferredChainConfig(config, kymera_AssertValidUserRegistryEntry(user)->prefer_chain_config_from_user) &&
             kymera_IsCompatible(user, config))
    {
        DEBUG_LOG_DEBUG("kymera_PopulateOutputChainConfig: Override using config from enum:output_users_t:%d",
                        kymera_AssertValidUserRegistryEntry(user)->prefer_chain_config_from_user);
    }
    else
    {
        *config = *user_config;
    }
}

static bool kymera_PrepareUser(output_users_t user, const kymera_output_chain_config *user_config)
{
    bool status = TRUE;
    kymera_output_chain_config config = {0};
    kymera_PopulateOutputChainConfig(&config, user, user_config);

    if (kymera_NoCurrentUsers())
        kymera_CreateOutputChain(&config);
    else if (kymera_IsCompatible(user, &config) == FALSE)
    {
        if (kymera_AttemptToDisconnectUsers(kymera_GetCurrentUsers()))
        {
            kymera_PopulateOutputChainConfig(&config, user, user_config);
            kymera_CreateOutputChain(&config);
        }
        else
            status = FALSE;
    }

    if (status)
        kymera_UpdateInputSampleRate(user, user_config);

    return status;
}

void Kymera_OutputRegister(const output_registry_entry_t *user_info)
{
    DEBUG_LOG_DEBUG("Kymera_OutputRegister: enum:output_users_t:%d", user_info->user);
    PanicFalse(kymera_IsRegisteredUser(user_info->user) == FALSE);
    kymera_RegisterUser(user_info);
}

bool Kymera_OutputPrepare(output_users_t user, const kymera_output_chain_config *chain_config)
{
    if (kymera_IsRegisteredUser(user) == FALSE)
        return FALSE;

    if (kymera_PrepareUser(user, chain_config) == FALSE)
        return FALSE;

    DEBUG_LOG_DEBUG("Kymera_OutputPrepare: enum:output_users_t:%d", user);
    state.ready_users |= user;
    return TRUE;
}

bool Kymera_OutputConnect(output_users_t user, const output_source_t *sources)
{
    if (kymera_IsUserConnectable(user) == FALSE)
        return FALSE;

    if (kymera_ConnectToOutputChain(user, sources) == FALSE)
        return FALSE;

    DEBUG_LOG_DEBUG("Kymera_OutputConnect: enum:output_users_t:%d", user);
    state.connected_users |= user;
    return TRUE;
}

void Kymera_OutputDisconnect(output_users_t user)
{
    if (kymera_IsCurrentUser(user) && kymera_DisconnectUser(user))
        kymera_SendIndication(OutputIdleIndication,);
}

void Kymera_OutputRegisterForIndications(const output_indications_registry_entry_t *user_info)
{
    DEBUG_LOG_DEBUG("Kymera_OutputRegisterForIndications");
    PanicNull((void*)user_info);
    kymera_RegisterForIndications(user_info);
}
unsigned Kymera_OutputGetMainVolumeBufferSize(void)
{
    return state.current_chain_config.source_sync_output_buffer_size_samples;
}

bool Kymera_OutputIsChainInUse(void)
{
    return state.connected_users!=output_user_none;
}

bool Kymera_OutputIsAecAlwaysUsed(void)
{
    const kymera_output_chain_config * common_chain = Kymera_OutputCommonChainGetConfig();
    bool is_aec_in_forced_config = common_chain && common_chain->chain_include_aec;

    return KymeraOutput_MustAlwaysIncludeAec() || is_aec_in_forced_config;
}
