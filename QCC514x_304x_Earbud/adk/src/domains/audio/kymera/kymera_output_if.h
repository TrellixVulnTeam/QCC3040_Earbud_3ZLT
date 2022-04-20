/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\brief      Interface for other kymera modules to connect to the output chain
*/

#ifndef KYMERA_OUTPUT_IF_H
#define KYMERA_OUTPUT_IF_H

#include "kymera_output_chain_config.h"
#include "kymera_output.h"

/*! List of every possible output user. */
typedef enum
{
    output_user_none      = 0,
    output_user_a2dp      = (1 << 0),
    output_user_prompt    = (1 << 1),
    output_user_usb_audio = (1 << 2),
    output_user_aec_leakthrough = (1 << 3),
    output_user_wired_analog = (1 << 4),
    output_user_sco = (1 << 5),
    output_user_sco_fwd = (1 << 6),
    output_user_usb_voice = (1 << 7),
    output_user_le_audio = (1 << 8),
    output_user_le_voice = (1 << 9),
    output_user_loopback = (1 << 10),
    output_user_fit_test = (1 << 11),
    output_user_common_chain = (1 << 12),
} output_users_t;

/*! List of every possible output chain user channel connections */
typedef enum
{
    output_connection_none,
    output_connection_mono,
    output_connection_aux,
    output_connection_stereo,
} output_connection_t;

/*! Sources to connect to output chain channels, must match the output_connection_t given at registration */
typedef union
{
    Source mono;
    Source aux;
    struct
    {
        Source left;
        Source right;
    } stereo;
} output_source_t;

/*! Callbacks to users, all fields are OPTIONAL */
typedef struct
{
    /*! \brief May only be called when the user has prepared and/or connected to the output chain.
               Used to ask the user whether it may be disconnected at this point.
        \return Return TRUE to indicate the user can be disconnected.
                Return FALSE to indicate the user cannot be disconnected at this time,
                this may interfere with another user attempting to prepare/connect.
    */
    bool (*OutputDisconnectRequest)(void);

    /*! \brief May only be called immediately after user has indicated it can be disconnected.
               If users chain is connected to the output chain, its chain should be stopped but not destroyed at this point.
               Will be called regardless of whether the users chain is connected to the output chain,
               as long as the user has prepared and/or connected to the output chain.
    */
    void (*OutputDisconnectPrepare)(void);

    /*! \brief May only be called after user has indicated it can be disconnected and
               immediately after the user has been disconnected from the output chain.
               Will be called regardless of whether the users chain is connected to the output chain,
               as long as the user has prepared and/or connected to the output chain.
    */
    void (*OutputDisconnectComplete)(void);

    /*! \brief User should provide a prediction of the output chain configuration it may use in the future (or currently uses).
               This is used to allow users to co-exist when possible.
        \param chain_config The chain configuration predicted/used.
        \return TRUE on success, FALSE if such information is not available.
    */
    bool (*OutputGetPreferredChainConfig)(kymera_output_chain_config *chain_config);
} output_callbacks_t;

/*! User registration entry */
typedef struct
{
    /* MANDATORY */
    output_users_t user;
    output_connection_t connection;
    // If TRUE it will be assumed the client is compatible with all other output chain configurations
    unsigned assume_chain_compatibility:1;
    /* OPTIONAL */
    // The client wants to use other parameters instead of using its own
    output_users_t prefer_chain_config_from_user;
    const output_callbacks_t *callbacks;
} output_registry_entry_t;

typedef struct
{
    /*! \brief Notify the user that another user is about to connect to the output chain.
     *  \param connecting_user The user that's connecting to the output chain
     *  \param connection_type The user's connection type
    */
    void (*OutputConnectingIndication)(output_users_t connecting_user, output_connection_t connection_type);

    /*! \brief Notify the user that a user has disconnected from the output chain.
     *  \param disconnected_user The user that has been disconnected from the output chain
     *  \param connection_type The user's connection type
    */
    void (*OutputDisconnectedIndication)(output_users_t disconnected_user, output_connection_t connection_type);

    /*! \brief Notify the user the output chain is idle (no active users/the chain is destroyed).
    */
    void (*OutputIdleIndication)(void);
} output_indications_registry_entry_t;

/*! \brief Users must register before using the rest of the API.
    \param user_info User information for its registration. Pointer MUST REMAIN VALID after the call.
 */
void Kymera_OutputRegister(const output_registry_entry_t *user_info);

/*! \brief Prepare the output chain (make sure a compatible chain is created).
           To undo simply call the disconnect API.
    \param user The ID of the respective output chain user.
    \param chain_config The configuration to use for the output chain.
    \return TRUE on success, FALSE otherwise.
 */
bool Kymera_OutputPrepare(output_users_t user, const kymera_output_chain_config *chain_config);

/*! \brief Connect user to the output chain.
           Chain needs to be started afterwards.
    \param user The ID of the respective output chain user.
    \param sources The sources to connect to the output chain input.
    \return TRUE on success, FALSE otherwise.
 */
bool Kymera_OutputConnect(output_users_t user, const output_source_t *sources);

/*! \brief Disconnect user from the output chain.
    \param user The ID of the respective output chain user.
 */
void Kymera_OutputDisconnect(output_users_t user);

/*! \brief Register to receive indications for the events in output_indications_registry_entry_t
 *  \param user_info Callbacks to handle the indications. Pointer MUST REMAIN VALID after the call.
*/
void Kymera_OutputRegisterForIndications(const output_indications_registry_entry_t *user_info);
/*! \brief Get the buffer size at the main input of the volume operator.
    \return Since the buffer size has to be positive, zero indicates a failure (chain not prepared yet).
 */
unsigned Kymera_OutputGetMainVolumeBufferSize(void);

/*! \brief Check if AEC REF is always going to be included in the output chain.
 */
bool Kymera_OutputIsAecAlwaysUsed(void);

/*! \brief Check if anyone is using output chain.
 */
bool Kymera_OutputIsChainInUse(void);

#endif // KYMERA_OUTPUT_IF_H
