/*!
\copyright  Copyright (c) 2005 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       earbud_primary_rules.h
\brief      Earbud application rules run when in a Primary earbud role.
*/

#ifndef _EARBUD_PRIMARY_RULES_H_
#define _EARBUD_PRIMARY_RULES_H_

#include <domain_message.h>
#include <rules_engine.h>
#include <kymera.h>

enum earbud_primary_rules_messages
{
    /*! Use Peer Signalling (AVRCP) to send link-keys message. */
    CONN_RULES_PEER_SEND_LINK_KEYS = CONN_RULES_MESSAGE_BASE,

#ifdef INCLUDE_FAST_PAIR
    /*! Use Peer Signalling to send fast pair account keys sync message. */
    CONN_RULES_PEER_SEND_FP_ACCOUNT_KEYS,
#endif

    /*! Start handset pairing. */
    CONN_RULES_HANDSET_PAIR,

    /*! Start timer to pause A2DP streaming. */
    CONN_RULES_A2DP_TIMEOUT,

    /*! Start timer to pause A2DP streaming. */
    CONN_RULES_SCO_TIMEOUT,

    /*! Enable LEDs when out of ear. */
    CONN_RULES_LED_ENABLE,

    /*! Disable LEDs when in ear. */
    CONN_RULES_LED_DISABLE,

    /*! Enter DFU mode as we have entered the case with DFU pending */
    CONN_RULES_ENTER_DFU,

    /*! Update upgrade state */
    CONN_RULES_DFU_ALLOW,

    /*! Transfer SCO Audio from handset to the earbud */
    CONN_RULES_SCO_TRANSFER_TO_EARBUD,

    /*! Transfer SCO Audio from Earbud to the handset */
    CONN_RULES_SCO_TRANSFER_TO_HANDSET,

    /*! Control if peer SCO is enabled or disabled. */
    CONN_RULES_PEER_SCO_CONTROL,
    /*! Start ANC */
    CONN_RULES_ANC_ENABLE,

    /*! Stop ANC */
    CONN_RULES_ANC_DISABLE,

    /*! Start ANC Tuning */
    CONN_RULES_ANC_TUNING_START,

    /*! Stop ANC Tuning */
    CONN_RULES_ANC_TUNING_STOP,

    /*! Stop Adaptive ANC Tuning */
    CONN_RULES_ADAPTIVE_ANC_TUNING_STOP,

    /*! Set the current Primary or Secondary role */
    CONN_RULES_ROLE_DECISION,

    /*! Cancel running timers for pausing A2DP media */
    CONN_RULES_A2DP_TIMEOUT_CANCEL,

    /*! Start playing media */
    CONN_RULES_MEDIA_PLAY,

    /*! Set the remote audio mix */
    CONN_RULES_SET_REMOTE_AUDIO_MIX,

    /*! Set the local audio mix */
    CONN_RULES_SET_LOCAL_AUDIO_MIX,

    /*! Any rules with RULE_FLAG_PROGRESS_MATTERS are no longer in progress. */
    CONN_RULES_NOP,

    /*! Accept incoming call. */
    CONN_RULES_ACCEPT_INCOMING_CALL,

    /*! Stop Leakthrough. */
    CONN_RULES_LEAKTHROUGH_DISABLE,

    /*! Start timer for mute of LE broadcast due to out of ear */
    CONN_RULES_BROADCAST_TIMEOUT,

    /*! Resume broadcast audio */
    CONN_RULES_BROADCAST_RESUME,

    /* ADD NEW CONN_RULES HERE */

    /*! This marks the end of the primary rules and must always be the final
        member of the enum. This value is used to enumerate the start of the
        secondary rules. This ensures the uniqueness of the primary/secondary
        rule IDs. */
    END_OF_PRIMARY_CONN_RULES,
};


/*! \brief Actions to take after connecting handset. */
typedef enum
{
    RULE_POST_HANDSET_CONNECT_ACTION_NONE,       /*!< Do nothing more */
    RULE_POST_HANDSET_CONNECT_ACTION_PLAY_MEDIA, /*!< Play media */
} rulePostHandsetConnectAction;

/*! \brief Definition of #CONN_RULES_PEER_SCO_CONTROL action message. */
typedef struct
{
    /*! TRUE enable peer SCO, FALSE disable peer SCO. */
    bool enable;
} CONN_RULES_PEER_SCO_CONTROL_T;

/*! \brief Definition of #CONN_RULES_DFU_ALLOW action message. */
typedef struct
{
    /*! TRUE    Upgrades (DFU) are allowed.
        FALSE   Upgrades are not allowed*/
    bool enable;
} CONN_RULES_DFU_ALLOW_T;

/*! \brief Definition of #CONN_RULES_ROLE_DECISION message. */
typedef struct
{
    /*! TRUE this Earbud is Primary.
        FALSE this Earbud is Secondary. */
    bool primary;
} CONN_RULES_ROLE_DECISION_T;

/*! \brief Definition of #CONN_RULES_SET_REMOTE_AUDIO_MIX message. */
typedef struct
{
    /*! TRUE - remote earbud renders stereo mix of left and right.
        FALSE - remote earbuds renders mono left or right. */
    bool stereo_mix;
} CONN_RULES_SET_REMOTE_AUDIO_MIX_T;

/*! \brief Definition of #CONN_RULES_SET_LOCAL_AUDIO_MIX message. */
typedef struct
{
    /*! TRUE - local earbud renders stereo mix or left and right.
        FALSE - local earbuds renders mono left or right. */
    bool stereo_mix;
} CONN_RULES_SET_LOCAL_AUDIO_MIX_T;


/*! Definition of all the events that may have rules associated with them */
#define RULE_EVENT_STARTUP                       (1ULL << 0)     /*!< Startup */

#define RULE_EVENT_HANDSET_CONNECTED             (1ULL << 1)     /*!< Handset connected */
#define RULE_EVENT_HANDSET_DISCONNECTED          (1ULL << 2)     /*!< Handset disconnected */
#define RULE_EVENT_PEER_CONNECTED                (1ULL << 3)     /*!< Peer connected */
#define RULE_EVENT_PEER_DISCONNECTED             (1ULL << 4)     /*!< Peer disconnected */
#define RULE_EVENT_PEER_UPDATE_LINKKEYS          (1ULL << 5)    /*!< Linkey for handset updated, potentially need to forward to peer */

#define RULE_EVENT_IN_EAR                        (1ULL << 6)    /*!< Earbud put in ear. */
#define RULE_EVENT_OUT_EAR                       (1ULL << 7)    /*!< Earbud taken out of ear. */
#define RULE_EVENT_IN_CASE                       (1ULL << 8)    /*!< Earbud put in the case. */
#define RULE_EVENT_OUT_CASE                      (1ULL << 9)    /*!< Earbud taken out of case. */

#define RULE_EVENT_PEER_IN_CASE                  (1ULL << 10)    /*!< Peer put in the case */
#define RULE_EVENT_PEER_OUT_CASE                 (1ULL << 11)    /*!< Peer taken out of the case */
#define RULE_EVENT_PEER_IN_EAR                   (1ULL << 12)    /*!< Peer put in the ear */
#define RULE_EVENT_PEER_OUT_EAR                  (1ULL << 13)    /*!< Peer taken out of ear */

#define RULE_EVENT_SCO_ACTIVE                    (1ULL << 14)    /*!< SCO call active */

#define RULE_EVENT_CHECK_DFU                     (1ULL << 15)    /*!< Check whether upgrades should be allowed */

#define RULE_EVENT_ROLE_SWITCH                   (1ULL << 16)    /*!< Role switch occured */

/*! \brief Connection Rules task data. */
typedef struct
{
    rule_set_t rule_set;
} PrimaryRulesTaskData;

/*! \brief Initialise the connection rules module. */
bool PrimaryRules_Init(Task init_task);

/*! \brief Get handle on the primary rule set, in order to directly set/clear events.
    \return rule_set_t The primary rule set.
 */
rule_set_t PrimaryRules_GetRuleSet(void);

/*! \brief Set an event or events
    \param[in] event Events to set that will trigger rules to run
    This function is called to set an event or events that will cause the relevant
    rules in the rules table to run.  Any actions generated will be sent as message
    to the client_task
*/
void PrimaryRules_SetEvent(rule_events_t event_mask);

/*! \brief Reset/clear an event or events
    \param[in] event Events to clear
    This function is called to clear an event or set of events that was previously
    set. Clear event will reset any rule that was run for event.
*/
void PrimaryRules_ResetEvent(rule_events_t event);

/*! \brief Get set of active events
    \return The set of active events.
*/
rule_events_t PrimaryRules_GetEvents(void);

/*! \brief Mark rules as complete from messaage ID
    \param[in] message Message ID that rule(s) generated
    This function is called to mark rules as completed, the message parameter
    is used to determine which rules can be marked as completed.
*/
void PrimaryRules_SetRuleComplete(MessageId message);

/*! \brief Mark rules as complete from message ID and set of events
    \param[in] message Message ID that rule(s) generated
    \param[in] event Event or set of events that trigger the rule(s)
    This function is called to mark rules as completed, the message and event parameter
    is used to determine which rules can be marked as completed.
*/
void PrimaryRulesSetRuleWithEventComplete(MessageId message, rule_events_t event);

/*! \brief Decides if primary earbud should transfer sco audio back to handset if
           it is out of ear or not.
    \param[in] void
    \return rule_action_ignore if sco audio should not to be transferred back to the handset.
            rule_action_run if sco audio should be transferred back to the handset.
*/
rule_action_t PrimaryRuleTransferScoToHandsetIfOutOfEar(void);

#endif /* _EARBUD_PRIMARY_RULES_H_ */
