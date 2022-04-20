/*!
\copyright  Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Internal defines used by the advertising manager
*/

#ifndef LE_ADVERTISING_MANAGER_PRIVATE_H_

#define LE_ADVERTISING_MANAGER_PRIVATE_H_

#include "le_advertising_manager.h"
#include "logging.h"


/*! Macro to make a message based on type. */
#define MAKE_MESSAGE(TYPE) TYPE##_T *message = PanicUnlessNew(TYPE##_T);

/*! Macro to make a variable length message based on type. */
#define MAKE_MESSAGE_VAR(VAR, TYPE) TYPE##_T *VAR = PanicUnlessNew(TYPE##_T);

/*! Logging Macros */
#define DEBUG_LOG_EXTRA
#if defined DEBUG_LOG_EXTRA

#define DEBUG_LOG_LEVEL_1 DEBUG_LOG_VERBOSE /* Additional Message Handler/Failure Logs */
#define DEBUG_LOG_LEVEL_2 DEBUG_LOG_V_VERBOSE /* Additional Information Logs */

#else

#define DEBUG_LOG_LEVEL_1(...) ((void)(0))
#define DEBUG_LOG_LEVEL_2(...) ((void)(0))

#endif

#define DEFAULT_ADVERTISING_INTERVAL_MIN_IN_SLOTS 148 /* This value is in units of 0.625 ms */
#define DEFAULT_ADVERTISING_INTERVAL_MAX_IN_SLOTS 160 /* This value is in units of 0.625 ms */

/* Number of clients supported that can register callbacks for advertising data */
#define MAX_NUMBER_OF_CLIENTS 15

/*! Given the total space available, returns space available once a header is included

    This makes allowance for being passed negative values, or space remaining
    being less that that needed for a header.

    \param[in] space    Pointer to variable holding the remaining space
    \returns    The usable space, having allowed for a header
*/
#define USABLE_SPACE(space)             ((*space) > AD_DATA_HEADER_SIZE ? (*space) - AD_DATA_HEADER_SIZE : 0)

/*! Helper macro to return total length of a field, once added to advertising data

    \param[in] data_length  Length of field

    \returns    Length of field, including header, in octets
*/
#define AD_FIELD_LENGTH(data_length)    (data_length + 1)

/*! Size of flags field in advertising data */
#define FLAGS_DATA_LENGTH           (0x02)

/*! Calculate value for the maximum possible length of a name in advertising data */
#define MAX_AD_NAME_SIZE_OCTETS     (MAX_AD_DATA_SIZE_IN_OCTETS - AD_DATA_HEADER_SIZE)

/*! Minimum length of the local name being advertised, if we truncate */
#define MIN_LOCAL_NAME_LENGTH       (0x10)

typedef struct
{
    bool enable;
    Task task;
}LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE_T;

typedef struct
{
    bool allow;
    Task task;
}LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING_T;

typedef struct
{
    le_adv_data_set_handle handle;
}
LE_ADV_MGR_INTERNAL_RELEASE_DATASET_T;

typedef struct
{
    bool   action;
} LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING_T;

/*! Enumerated type for messages sent within the advertising manager only. */
enum adv_mgr_internal_messages_t
{
        /*! Start advertising using this advert */
    ADV_MANAGER_START_ADVERT = INTERNAL_MESSAGE_BASE + 1,
        /*! Set advertising data using this advert. Used for connections (from Gatt) */
    ADV_MANAGER_SETUP_ADVERT,
    LE_ADV_INTERNAL_MSG_ENABLE_ADVERTISING,
    LE_ADV_INTERNAL_MSG_NOTIFY_RPA_CHANGE,
    LE_ADV_MGR_INTERNAL_START,
    LE_ADV_MGR_INTERNAL_MSG_NOTIFY_INTERVAL_SWITCHOVER,
    LE_ADV_MGR_INTERNAL_DATA_UPDATE,
    LE_ADV_MGR_INTERNAL_ENABLE_CONNECTABLE,
    LE_ADV_MGR_INTERNAL_ALLOW_ADVERTISING,
    LE_ADV_MGR_INTERNAL_RELEASE_DATASET,
    LE_ADV_MGR_INTERNAL_PARAMS_UPDATE,
    LE_ADV_MGR_INTERNAL_GOT_TP_CONNECT_IND,

        /*! This must be the final message */
    LE_ADV_MGR_INTERNAL_MESSAGE_END
};
ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(LE_ADV_MGR_INTERNAL_MESSAGE_END)


/*! Enumerated type used to note reason for blocking

    Advertising operations can be delayed while a previous operation completes.
    The reason for the delay is recorded using these values */
typedef enum {
    le_adv_blocking_condition_none,
    le_adv_blocking_condition_data_cfm = 1,
    le_adv_blocking_condition_params_cfm = 2,
    le_adv_blocking_condition_scan_response_cfm = 3,
    le_adv_blocking_condition_enable_cfm = 4,
    le_adv_blocking_condition_register_cfm = 5,
    le_adv_blocking_condition_enable_terminate_ind = 6,
    le_adv_blocking_condition_enable_connect_ind = 7,
    le_adv_blocking_condition_invalid = 0xFF
} le_adv_blocking_condition_t;

/*! Advertising manager task structure */
typedef struct
{
    /*! Task for advertisement management */
    TaskData                    task;
    /*! Bitmask for allowed advertising event types */
    uint8                       mask_enabled_events;
    /*! Flag to indicate enabled/disabled state of all advertising event types */
    bool                        is_advertising_allowed;
    /*! Flag to indicate if legacy data update is required */
    bool                        is_legacy_data_update_required;
    /*! Flag to indicate if legacy data update is required */
    bool                        is_extended_data_update_required;
    /*! Flag to indicate if parameters update is required */
    bool                        is_params_update_required;
    /*! Flag to indicate whether to keep advertising or restart advertising on data notify */
    bool                        keep_advertising_on_notify;
    /*! Flag to indicate if controller supports extended advertising and scanning or not */
    bool                        is_extended_advertising_and_scanning_enabled;
    /*! Selected handset advertising data set for the undirected advertising */
    le_adv_data_set_handle      dataset_handset_handle;
    /*! Selected peer advertising data set for the undirected advertising */
    le_adv_data_set_handle      dataset_peer_handle; 
    /*! Selected extended advertising handset data set for the undirected advertising */
    le_adv_data_set_handle      dataset_extended_handset_handle;
    /*! Configured advertising parameter set for the undirected advertising */
    le_adv_params_set_handle    params_handle;
    /*! The condition (internal) that the blocked operation is waiting for */
    uint16    blockingCondition;
    /*! Number of remaining attempts to configure random address for extended advert */
    uint8                       extended_advert_rpa_retries;
    /*! Task for legacy advertisement */
    TaskData                    legacy_task;
    /*! Task for extended advertisement */
    TaskData                    extended_task;
} adv_mgr_task_data_t;

/*!< Task information for the advertising manager */
extern adv_mgr_task_data_t  app_adv_manager;

/*! Get the advertising manager data structure */
#define AdvManagerGetTaskData()      (&app_adv_manager)

/*! Get the advertising manager task */
#define AdvManagerGetTask()  (&app_adv_manager.task)

/*! Get the legacy advertising manager task */
#define AdvManagerGetLegacyTask()  (&app_adv_manager.legacy_task)

/*! Get the legacy advertising manager task */
#define AdvManagerGetExtendedTask()  (&app_adv_manager.extended_task)

/* Get the State of LE Advertising Being Allowed/Disallowed */
#define leAdvertisingManager_IsAdvertisingAllowed() (app_adv_manager.is_advertising_allowed)

/* Set the current blocking condition equal to the given adv_mgr_blocking_state_t */
void leAdvertisingManager_SetBlockingCondition(uint16 condition);

/* Get the current blocking condition. See adv_mgr_blocking_state_t */
#define leAdvertisingManager_GetBlockingCondition() (app_adv_manager.blockingCondition)

/* Check if the current blocking condidtion is the same as the given adv_mgr_blocking_state_t */
bool leAdvertisingManager_CheckBlockingCondition(uint16 condition);

/* Check if Connectable LE Advertising is Enabled/Disabled */
bool leAdvertisingManager_IsConnectableAdvertisingEnabled(void);

struct _le_adv_data_set
{
    Task task;
    le_adv_data_set_t set;
};

struct _le_adv_params_set
{
    /*! Registered advertising parameter sets */
    le_adv_parameters_set_t * params_set;
    /*! Registered advertising parameter config table */
    le_adv_parameters_config_table_t * config_table;
    /*! Selected config table entry */
    uint8 index_active_config_table_entry;
    /*! Selected advertising parameter set */
    le_adv_preset_advertising_interval_t active_params_set;
};

typedef struct
{
    le_adv_data_set_t set;

} LE_ADV_MGR_INTERNAL_START_T;

/*! Data type for the supported advertising events */
typedef enum
{
    le_adv_event_type_connectable_general = 1UL<<0,
    le_adv_event_type_connectable_directed = 1UL<<1,
    le_adv_event_type_nonconnectable_discoverable = 1UL<<2,
    le_adv_event_type_nonconnectable_nondiscoverable = 1UL<<3

} le_adv_event_type_t;

/*! Data structure to specify the input parameters for leAdvertisingManager_Start() API function */
typedef struct
{
    le_adv_data_set_t set;
    le_adv_data_set_t set_awaiting_select_cfm_msg;
    le_adv_event_type_t event;    
}le_advert_start_params_t;

#endif
