/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       gaia_debug_plugin.h
\brief      Header file for Gaia Debug Feature plugin, which provides some
            debug feature that enables followings:
                - Transfer panic log from the device to the host (e.g. smartphone)
                  over Bluetooth connection (NB: RFCOMM only at the moment).
                  Panic log can contain debug information such as stack trace,
                  core dump, and logs stored to the 'Debug Partition' of the flash
                  memory.

Dependencies:
            This Feature requires the following functionalities provided by the
            Apps-P0 firmware:
             - 'Debug Partition': This is a space of the flash memory allocated
               for storing debug information when the chip code Application crashes.
*/

#ifndef GAIA_DEBUG_PLUGIN_H_
#define GAIA_DEBUG_PLUGIN_H_



#include <debug_partition_api.h>

#include <gaia_features.h>
#include "gaia_framework.h"




/*! \brief Gaia Debug Feature plugin version.
*/
/* Debug Feature plugin history:
        1   The first version that support only the Primary device.
        2   The Secondary device support is added. */
#define GAIA_DEBUG_FEATURE_PLUGIN_VERSION (2)

/*! \brief Command PDU version for 'Get Debug Log Info' command.
           The current implementation gives only the size of panic log stored
           in the 'Debug Partition'. This can be expanded in the future.
    Command PDU Version History:
        0:  Payload[0] Debug Info PDU Version (= zero).
            Payload[1] Panic log size (MSB).
            Payload[2] Panic log size (Second SB).
            Payload[3] Panic log size (Third SB).
            Payload[4] Panic log size (LSB).
*/
#define GAIA_DEBUG_FEATURE_DBG_INFO_CMD_PDU_VERSION (0)

/*! \brief The parameter value required to erase the debug partition. */
#define GAIA_DEBUG_FEATURE_ERASE_PANIC_LOG_CMD_PARAM_ERASE_ALL          (0x0000)


/*! \brief Gaia Debug Feature commands.
*/
typedef enum
{
    /*! Get Debug Log information */
    get_debug_log_info = 1,
    /*! Configure Debug Logging settings */
    configure_debug_logging,
    /*! Set up a data transfer session to get debug log from the 'Debug Partition'. */
    setup_debug_log_transfer,
    /*! Erase panic log stored in the 'Debug Partition' */
    erase_panic_log,
    /*! 'Debug Tunnel To Chip' command */
    debug_tunnel_to_chip,

    /*! Total number of commands */
    number_of_debug_commands,
} debug_plugin_pdu_ids_t;


/*! \brief Gaia Debug Feature notifications.
*/
typedef enum
{
    /*! 'Debug Tunnel From Chip' notification */
    debug_tunnel_from_chip_notification = 0,

    /*! Total number of notifications */
    number_of_debug_notifications,
} debug_plugin_notifications_t;


/*! \brief Gaia Debug Feature Error Status Codes.
    \note  Valid range is 128 to 255 for the Feature.
           (Values 0~127 are reserved for Gaia Framework.)
*/
typedef enum
{
    /*! Request is processed successfully. */    
    debug_plugin_status_success                     = 0x80,
    /*! The requested data is not available. */
    debug_plugin_status_no_data                     = 0x81, 

    /*! No space is available (e.g. The Debug Partition is full with panic log.) */
    debug_plugin_status_not_enough_space            = 0x82, 
    /*! Request cannot be completed because resource is busy (e.g. Trying to erase the partition while its stream is not closed yet.) */
    debug_plugin_status_busy                        = 0x83, 

    /*! The command ID is invalid. */
    debug_plugin_status_invalid_command             = 0x84, 
    /*! The command parameters are invalid. */
    debug_plugin_status_invalid_parameters          = 0x85, 

    /*! The debug partition is not found (not in the flash image header). */
    debug_plugin_status_no_debug_partition          = 0x86,

    /*! Error caused by unknown reason. */
    debug_plugin_status_unknown_error               = 0x8F,

    /*! Total number of Error/Status codes for Debug Feature */
    number_of_debug_error_codes
} debug_plugin_status_code_t;


/*! \brief 'Get Debug Log Info' command parameter positions. */
typedef enum
{
    debug_plugin_get_debug_log_info_cmd_log_info_key_msb        = 0,
    debug_plugin_get_debug_log_info_cmd_log_info_key_lsb        = 1,

    number_of_debug_plugin_get_debug_log_info_cmd_bytes,
} debug_plugin_get_debug_log_info_cmd_t;

/*! \brief 'Get Debug Log Info' response parameter positions. */
typedef enum
{
    debug_plugin_get_debug_log_info_rsp_pdu_version             = 0,
    debug_plugin_get_debug_log_info_rsp_info_key_msb            = 1,
    debug_plugin_get_debug_log_info_rsp_info_key_lsb            = 2,
    debug_plugin_get_debug_log_info_rsp_panic_log_size_msb      = 3,
    debug_plugin_get_debug_log_info_rsp_panic_log_size_2nd_sb   = 4,
    debug_plugin_get_debug_log_info_rsp_panic_log_size_3rd_sb   = 5,
    debug_plugin_get_debug_log_info_rsp_panic_log_size_lsb      = 6,

    number_of_debug_plugin_get_debug_log_info_rsp_bytes,
} debug_plugin_get_debug_log_info_rsp_t;


/*! \brief 'Configure Debug Logging' command parameter positions. */
typedef enum
{
    debug_plugin_configure_debug_logging_cmd_config_key_msb     = 0,
    debug_plugin_configure_debug_logging_cmd_config_key_lsb     = 1,
    debug_plugin_configure_debug_logging_cmd_config_val_msb     = 2,
    debug_plugin_configure_debug_logging_cmd_config_val_2nd_sb  = 3,
    debug_plugin_configure_debug_logging_cmd_config_val_3rd_sb  = 4,
    debug_plugin_configure_debug_logging_cmd_config_val_lsb     = 5,

    number_of_debug_plugin_configure_debug_logging_cmd_bytes
} debug_plugin_configure_debug_logging_cmd_t;

/*! \brief 'Configure Debug Logging' response parameter positions. */
typedef enum
{
    debug_plugin_configure_debug_logging_rsp_config_key_msb     = 0,
    debug_plugin_configure_debug_logging_rsp_config_key_lsb     = 1,
    debug_plugin_configure_debug_logging_rsp_config_val_msb     = 2,
    debug_plugin_configure_debug_logging_rsp_config_val_2nd_sb  = 3,
    debug_plugin_configure_debug_logging_rsp_config_val_3rd_sb  = 4,
    debug_plugin_configure_debug_logging_rsp_config_val_lsb     = 5,

    number_of_debug_plugin_configure_debug_logging_rsp_bytes
} debug_plugin_configure_debug_logging_rsp_t;


/*! \brief 'Get Panic Log' command parameter positions. */
typedef enum
{
    /* This command has no parameters. */

    number_of_get_panic_log_cmd_bytes
} debug_plugin_get_panic_log_cmd_t;

/*! \brief 'Get Panic Log' response parameter positions. */
typedef enum
{
    debug_plugin_get_panic_log_rsp_session_id_msb               = 0,
    debug_plugin_get_panic_log_rsp_session_id_lsb               = 1,
    debug_plugin_get_panic_log_rsp_panic_log_size_msb           = 2,
    debug_plugin_get_panic_log_rsp_panic_log_size_2nd_sb        = 3,
    debug_plugin_get_panic_log_rsp_panic_log_size_3rd_sb        = 4,
    debug_plugin_get_panic_log_rsp_panic_log_size_lsb           = 5,

    number_of_get_panic_log_rsp_bytes
} debug_plugin_get_panic_log_rsp_t;


/*! \brief 'Erase Panic Log' command parameter positions. */
typedef enum
{
    debug_plugin_erase_panic_log_cmd_reserved00                 = 0,
    debug_plugin_erase_panic_log_cmd_reserved01                 = 1,

    number_of_debug_plugin_erase_panic_log_cmd_bytes
} debug_plugin_erase_panic_log_cmd_t;

/*! \brief 'Erase Panic Log' response parameter positions. */
typedef enum
{
    /* This response has no parameters. */

    number_of_debug_plugin_erase_panic_log_rsp_bytes
} debug_plugin_erase_panic_log_rsp_t;



/*! \brief Gaia Debug Feature initialisation function.
           This function registers the command/notification handler of the GAIA
           Debug Feature. The Application that uses this Feature must call this
           function before using any functions of the Feature.
*/
bool GaiaDebugPlugin_Init(Task init_task);

#endif /* GAIA_DEBUG_PLUGIN_H_ */
