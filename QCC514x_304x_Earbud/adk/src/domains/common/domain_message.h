/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file
\brief      Header file for global message definitions.

            This header file uses X-macros to convert lists of components
            into enumerations of {component}_MESSAGE_GROUP and 
            {component}_MESSAGE_BASE.

            Groups may be larger than the default. Define this by requesting
            a multiple 

            e.g. 
                X(INTERNAL,2) will produce a group twice the normal size

            Message IDs for a component range between 
                \li (component)_MESSAGE_BASE and
                \li (component)_MESSAGE_LIMIT

            When defining messages for a group, the last message ID should be
            checked, using the macro ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED.
            Pass the message group name and the last message ID

            e.g.
                ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(SYSTEM_STATE, SYSTEM_STATE_MESSAGE_END)

\note   When registering for domain messages using
        MessageBroker_RegisterInterestInMsgGroups the registrant much comply
        with the requirements of the domain component's message interface. In
        particular, this means the registrant _must_ respond to messages that
        require a response. Not responding to messages that require a response
        is likely to result in unexpected behavior. The comment after each
        component in #FOREACH_DOMAINS_MESSAGE_GROUP identifies the name of the
        enumeration/typedef that defines the component's interface.
*/
#ifndef DOMAIN_MESSAGE_H_
#define DOMAIN_MESSAGE_H_

#include "message_broker.h"
#include "library.h"

/*! \brief Macro to convert a message group, to a message ID */
#define MSG_GRP_TO_ID(x)        ((x)<<6)
/*! \brief Macro to return the last message id in a message group */
#define LAST_ID_IN_MSG_GRP(grp) (grp ## _MESSAGE_LIMIT)

/* \brief macro to check if a message is in allowed range for a group */
#define ID_IN_MSG_GRP(grp, id) (grp##_MESSAGE_BASE <= (id) && (id) <= grp##_MESSAGE_LIMIT)

/*! \brief Macro to convert a message ID, to a message group 
    If possible use #ID_IN_MSG_GRP. ID_TO_MSG_GRP does not directly support
    large message groups */
#define ID_TO_MSG_GRP(x)        ((message_group_t)((x)>>6))

/*! The user of a messages group shall use this compile-time assertion to check that the 
    messages defined in their group do not overflow into the next group's allocation 
    \param message_group_base starting message ID of messages group
    \param last_used_message last message ID of messages group 
*/
#ifndef HOSTED_TEST_ENVIRONMENT
#define ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(group, last_used_message) \
    COMPILE_TIME_ASSERT(last_used_message <= (LAST_ID_IN_MSG_GRP(group)), group##_MaxMessagesViolation);
#else
#define ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(message_group_base, last_used_message)
#endif

/*! Internal messages, whether starting from 0, or INTERNAL_MESSAGE_BASE
    should use this macro to check their range.

    \param last_used_message last message ID of messages group 
 */
#ifndef HOSTED_TEST_ENVIRONMENT
#define ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(last_used_message) \
    COMPILE_TIME_ASSERT(last_used_message <= (LAST_ID_IN_MSG_GRP(INTERNAL)), last_used_message##_MaxMessagesViolation);
#else
#define ASSERT_INTERNAL_MESSAGES_NOT_OVERFLOWED(last_used_message)
#endif


/*! A table of domain component names. 

    If a group requires more messages it can be extended by adding a parameter.
    X(MYGROUP,2) will allocate 3 groups of messages to the group.
*/
#define FOREACH_DOMAINS_MESSAGE_GROUP(X) \
    X(INTERNAL,1) \
    X(AV)                   /* See #av_status_messages */ \
    X(APP_HFP)              /* See #hfp_profile_messages */ \
    X(PAIRING)              /* See #pairing_messages */ \
    X(AV_GAIA)              /* See #av_headet_gaia_messages */ \
    X(DFU)                  /* See #dfu_messages_t */ \
    X(CON_MANAGER)          /* See #av_headset_conn_manager_messages */ \
    X(PEER_SIG)             /* See #peer_signalling_messages */ \
    X(HANDSET_SIG)          /* See #handset_signalling_messages */ \
    X(PHY_STATE)            /* See #phy_state_messages */ \
    X(HEADSET_PHY_STATE)    /* See #headset_phy_state_messages */ \
    X(BATTERY_APP)          /* See #battery_messages */ \
    X(ADV_MANAGER)          /* See #adv_mgr_messages_t */ \
    X(MIRROR_PROFILE)       /* See #mirror_profile_msg_t */ \
    X(PROXIMITY)            /* See #proximity_messages */ \
    X(ACCELEROMETER)        /* See #accelerometer_messages */ \
    X(HALL_EFFECT)          /* See #hall_effect_messages */ \
    X(TOUCH)                /* See #touch_sensor_messages */ \
    X(CHARGER)              /* See #chargerMessages */ \
    X(DEVICE)               /* See #deviceMessages */ \
    X(PROFILE_MANAGER)      /* See #profile_manager_messages */\
    X(APP_GATT)             /* See #av_headet_gatt_messages */ \
    X(POWER_APP)            /* See #powerClientMessages */ \
    X(KYMERA) \
    X(TEMPERATURE)          /* See #temperatureMessages */ \
    X(AUDIO_SYNC)           /* See #audio_sync_msg_t */ \
    X(VOLUME)               /* See #volume_domain_messages */ \
    X(REMOTE_NAME)          /* See #remote_name_message_t */ \
    X(PEER_PAIR_LE)         /* See #peer_pair_le_message_t */ \
    X(PEER_FIND_ROLE)       /* See #peer_find_role_message_t */ \
    X(KEY_SYNC)             /* See #key_sync_messages */ \
    X(BREDR_SCAN_MANAGER)   /* See #bredr_scan_manager_messages */ \
    X(UI)                   /* See #ui_message_t */ \
    X(PROMPTS) \
    X(AV_UI)                /* See #av_ui_messages */ \
    X(AV_AVRCP)             /* See #av_avrcp_messages */ \
    X(POWER_UI)             /* See #powerUiMessages */ \
    X(DFU_PEER)             /* See #dfu_peer_messages */ \
    X(TELEPHONY)            /* See #telephony_domain_messages */ \
    X(LE_SCAN_MANAGER)      /* See #scan_manager_messages */ \
    X(HANDOVER_PROFILE)     /* See #handover_profile_messages */ \
    X(LOCAL_NAME)           /* See #local_name_message_t */ \
    X(LOCAL_ADDR)           /* See #local_addr_message_t */ \
    X(DEVICE_TEST)          /* See #device_test_service_message_t */ \
    X(BATTERY_REGION)       /* See #battery_region_messages */ \
    X(STATE_OF_CHARGE)      /* See #soc_messages */ \
    \
    /* logical input message groups need to be consecutive */ \
    /* Note the symbol LOGICAL_INPUT_MESSAGE_BASE is assumed by ButtonParseXML.py */ \
    X(DEVICE_SPECIFIC_LOGICAL_INPUT) \
    X(LOGICAL_INPUT,2) \
    \
    X(ANC)                  /* See #anc_messages_t */ \
    X(LEAKTHROUGH)          /* See leakthrough_msg_t */\
    X(FIT_TEST)             /* See #fit_test_msg_t */\
    X(QCOM_CON_MANAGER)     /* See #qcm_msgs_t */\
    X(WIRED_AUDIO_DETECT)   /* Wired Audio Detect messages */\
    X(USB_DEVICE)           /* USB Device messages */\
    X(USB_AUDIO)            /* USB Audio messages */\
    X(LE_AUDIO)             /* LE Audio messages */\
    X(CASE)                 /* See #case_message_t */\
    X(GAMING_MODE_UI)       /* See #gaming_mode_ui_events */\
    X(LE_BROADCAST_MANAGER) /* See #le_broadcast_manager_messages_t */\
    X(SYSTEM_STATE)         /* See #system_state_messages_t */\
    X(BT_DEVICE)            /* See #bt_device_messages_t */\
    X(DEVICE_SYNC)          /* See #device_sync_messages_t */\
    X(APP_AGHFP)            /* See #aghfp_profile_messages */\
    X(GATT_LEA_DISCOVERY)   /* GATT LEA Discovery messages */\
    X(INQUIRY_MANAGER)   /* GATT LEA Discovery messages */\
    X(RSSI_PAIRING)   /* GATT LEA Discovery messages */

/*! A table of service component names */
#define FOREACH_SERVICES_MESSAGE_GROUP(X) \
    X(HANDSET_SERVICE)          /* See #handset_service_msg_t */ \
    X(STATE_PROXY)              /* See #state_proxy_messages */ \
    X(HDMA)                     /* See #hdma_messages_t */ \
    X(VOLUME_SERVICE)           /* See #volume_service_messages */ \
    X(VOICE_UI_SERVICE)         /* See #voice_ui_msg_id_t */ \
    X(AUDIO_CURATION_SERVICE)   /* See #audio_curation_messages */

/*! A table of topology component names */
#define FOREACH_TOPOLOGY_MESSAGE_GROUP(X) \
    X(TWS_TOPOLOGY)                 /* See #tws_topology_message_t */ \
    X(TWS_TOPOLOGY_CLIENT_NOTIFIER) /* See #tws_topology_client_notifier_message_t */ \
    X(HEADSET_TOPOLOGY)             /* See #headset_topology_message_t */ \

/*! A table of app/system component names */
#define FOREACH_APPS_MESSAGE_GROUP(X) \
    X(SYSTEM) \
    X(CONN_RULES) \
    X(EARBUD_ROLE) \
    X(CHARGER_CASE) \
    X(USB_DONGLE)

/*! A table of UI inputs names */
#define FOREACH_UI_INPUTS_MESSAGE_GROUP(X) \
    X(UI_INPUTS_TELEPHONY) \
    X(UI_INPUTS_MEDIA_PLAYER) \
    X(UI_INPUTS_PEER) \
    X(UI_INPUTS_DEVICE_STATE) \
    X(UI_INPUTS_VOLUME) \
    X(UI_INPUTS_HANDSET) \
    X(UI_INPUTS_AUDIO_CURATION) \
    X(UI_INPUTS_VOICE_UI) \
    X(UI_INPUTS_GAMING_MODE) \
    X(UI_INPUTS_APP)\
    X(UI_INPUTS_BOUNDS_CHECK)

/*! This expansion macro concatenates the component name with the text
    _MESSAGE_GROUP. For example, an item in a table 'FOO' will be expanded to the
    enumerated name FOO_MESSAGE_GROUP,.
    
    A define is also created for the last message group for the component.
    This allows for components to define a message range larger than default.
*/
#define EXPAND_AS_MESSAGE_GROUP_ENUM_IMPL(component_name,size,...)  \
                        component_name##_MESSAGE_GROUP, \
                        component_name##_LAST_MESSAGE_GROUP = component_name##_MESSAGE_GROUP + (size-1), 
#define EXPAND_AS_MESSAGE_GROUP_ENUM(...) EXPAND_AS_MESSAGE_GROUP_ENUM_IMPL(__VA_ARGS__,1,_unused)

/*! A type to enumerate the message groups available in the system */
enum message_groups
{
    FOREACH_DOMAINS_MESSAGE_GROUP(EXPAND_AS_MESSAGE_GROUP_ENUM)
    FOREACH_SERVICES_MESSAGE_GROUP(EXPAND_AS_MESSAGE_GROUP_ENUM)
    FOREACH_TOPOLOGY_MESSAGE_GROUP(EXPAND_AS_MESSAGE_GROUP_ENUM)
    FOREACH_APPS_MESSAGE_GROUP(EXPAND_AS_MESSAGE_GROUP_ENUM)
    FOREACH_UI_INPUTS_MESSAGE_GROUP(EXPAND_AS_MESSAGE_GROUP_ENUM)
};

/*! The first UI inputs message group ID */
#define UI_INPUTS_MESSAGE_GROUP_START UI_INPUTS_TELEPHONY_MESSAGE_GROUP

/*! This expansion macro concatenates the component name with the text
    _MESSAGE_BASE and assigns a value to the name using the MSG_GROUP_TO_ID
    macro (where the message group is defined in the #message_groups enum).
    For example, an item in a table 'FOO' will be expanded to the
    enumerated name FOO_MESSAGE_BASE=MSG_GROUP_TO_ID(FOO_MESSAGE_GROUP),.
    
    A define is also created for the last valid message identifier.
        FOO_MESSAGE_LIMIT. This is done by finding the first message
        of the \b next message group and subtracting one.
*/
#define EXPAND_AS_MESSAGE_BASE_ENUM_IMPL(component_name,...) \
                    component_name##_MESSAGE_BASE  = MSG_GRP_TO_ID(component_name##_MESSAGE_GROUP),\
                    component_name##_MESSAGE_LIMIT = MSG_GRP_TO_ID(component_name##_LAST_MESSAGE_GROUP + 1) -1,
#define EXPAND_AS_MESSAGE_BASE_ENUM(...) EXPAND_AS_MESSAGE_BASE_ENUM_IMPL(__VA_ARGS__,_unused)


/*!@{   @name Message ID allocations for each application component
        @brief Each component in the application that sends messages is assigned
               a base message ID. Each component then defines message IDs starting
               from that base ID.
        @note There is no checking that the messages assigned by one component do
              not overrun into the next component's message ID allocation.
*/
typedef enum
{
    FOREACH_DOMAINS_MESSAGE_GROUP(EXPAND_AS_MESSAGE_BASE_ENUM)
    FOREACH_SERVICES_MESSAGE_GROUP(EXPAND_AS_MESSAGE_BASE_ENUM)
    FOREACH_TOPOLOGY_MESSAGE_GROUP(EXPAND_AS_MESSAGE_BASE_ENUM)
    FOREACH_APPS_MESSAGE_GROUP(EXPAND_AS_MESSAGE_BASE_ENUM)
    FOREACH_UI_INPUTS_MESSAGE_GROUP(EXPAND_AS_MESSAGE_BASE_ENUM)
    MESSAGE_GROUPS_MAX      /* This is the NEXT message ID after the group allocation */
} message_base_t;

#ifndef HOSTED_TEST_ENVIRONMENT
/*! Adding compile time assert to protect against message overflow */
COMPILE_TIME_ASSERT((MESSAGE_GROUPS_MAX) <= CL_MESSAGE_BASE, MaxMessagesViolation);
#endif

typedef enum
{
    PAGING_START = SYSTEM_MESSAGE_BASE,
    PAGING_STOP,
} sys_msg;

/*! Helper macro to create a message broker group regisration.
    Registrations created using this macro are placed in a const linker data section.
*/
#define MESSAGE_BROKER_GROUP_REGISTRATION_MAKE(MESSAGE_GROUP_NAME, REGISTER_FUNCTION, UNREGISTER_FUNCTION) \
_Pragma("datasection message_broker_group_registrations") \
const message_broker_group_registration_t message_broker_group_registration_##MESSAGE_GROUP_NAME = \
    { MESSAGE_GROUP_NAME##_MESSAGE_GROUP, MESSAGE_GROUP_NAME##_LAST_MESSAGE_GROUP, \
      REGISTER_FUNCTION, UNREGISTER_FUNCTION }

/*! Linker defined consts referencing the location of the section containing
    the message broker group registrations. */
extern const message_broker_group_registration_t message_broker_group_registrations_begin[];
extern const message_broker_group_registration_t message_broker_group_registrations_end[];

/*@} */
#endif /* DOMAIN_MESSAGE_H_ */
