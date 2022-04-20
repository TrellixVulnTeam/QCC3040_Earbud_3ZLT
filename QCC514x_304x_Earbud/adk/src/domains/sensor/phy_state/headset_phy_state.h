/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       headset_phy_state.h
\brief	    Definition of the physical state of a Headset.
*/

#ifndef HEADSET_PHY_STATE_H
#define HEADSET_PHY_STATE_H

#include "domain_message.h"
#include "task_list.h"

/*! \brief Messages which may be sent by the Headset Physical State module. */
typedef enum headset_phy_state_messages
{
    /*! Initialisation of headset phy state is complete. */
    HEADSET_PHY_STATE_INIT_CFM = HEADSET_PHY_STATE_MESSAGE_BASE,
    /*! Indication of a changed headset physical state */
    HEADSET_PHY_STATE_CHANGED_IND,

    /*! This must be the final message */
    HEADSET_PHY_STATE_MESSAGE_END
} HEADSET_PHY_STATE_MSG;

typedef enum
{
    headset_phy_state_event_on_head,
    headset_phy_state_event_off_head,
    headset_phy_state_event_in_motion,
    headset_phy_state_event_not_in_motion,
} headset_phy_state_event;

/*! Defines physical state client task list initial capacity */
#define HEADSET_PHY_STATE_CLIENT_TASK_LIST_INIT_CAPACITY 6

/*! \brief Enumeration of the physical states an Headset can be in.
 */
typedef enum
{
    /*! The headset physical state is unknown.
        This state value will not be reported to clients. */
    HEADSET_PHY_STATE_UNKNOWN,
    /*! The headset is not on the head.
     *  It *may* be in motion or at rest. */
    HEADSET_PHY_STATE_OFF_HEAD,
    /*! The headset is not on the head, and no motion
     * has been detected for configurable period of time */
    HEADSET_PHY_STATE_OFF_HEAD_AT_REST,
    /*! The headset is on the head and usuable as a microphone and speaker. */
    HEADSET_PHY_STATE_ON_HEAD
} headsetPhyState;

/*! \brief Definition of #HEADSET_PHY_STATE_CHANGED_IND message. */
typedef struct
{
    /*! The physical state which the device is now in. */
    headsetPhyState new_state;
    headset_phy_state_event event;
} HEADSET_PHY_STATE_CHANGED_IND_T;

/*! \brief Physial State module state. */
typedef struct
{
    /*! Physical State module message task. */
    TaskData task;
    /*! Current physical state of the device. */
    headsetPhyState state;
    /*! List of tasks to receive #HEADSET_PHY_STATE_CHANGED_IND notifications. */
    TASK_LIST_WITH_INITIAL_CAPACITY(HEADSET_PHY_STATE_CLIENT_TASK_LIST_INIT_CAPACITY)   client_tasks;
    /*! Stores the motion state */
    bool in_motion;
    /*! Stores the proximity state */
    bool in_proximity;
    /*! Lock used to conditionalise sending of PHY_STATE_INIT_CFM. */
    uint16 lock;
    /*! Last state reported to clients. */
    headsetPhyState reported_state;

} headsetPhyStateTaskData;

/*!< Physical state of the Headset. */
extern headsetPhyStateTaskData app_headset_phy_state;

/*! Get pointer to physical state data structure */
#define HeadsetPhyStateGetTaskData()   (&app_headset_phy_state)

/*! Get pointer to physical state client tasks */
#define HeadsetPhyStateGetClientTasks()   (task_list_flexible_t *)(&app_headset_phy_state.client_tasks)

/*! \brief Register a task for notification of changes in state.
    @param[in] client_task Task to receive HEADSET_PHY_STATE_CHANGED_IND messages.
 */
void appHeadsetPhyStateRegisterClient(Task client_task);

/*! \brief Unregister a task for notification of changes in state.
    @param[in] client_task Task to unregister.
 */
void appHeadsetPhyStateUnregisterClient(Task client_task);

/*! \brief Get the current physical state of the device.
    \return headsetPhyState Current physical state of the device.
*/
headsetPhyState appHeadsetPhyStateGetState(void);

/*! \brief Check whether on/off head detection is supported.
    \return bool TRUE if on/off head detection is supported, else FALSE. */
bool appHeadsetPhyStateIsOnHeadDetectionSupported(void);

/*! \brief Handle notification that Headset is now on head. */
void appHeadsetPhyStateOnHeadEvent(void);

/*! \brief Handle notification that Headset is now off the head. */
void appHeadsetPhyStateOffHeadEvent(void);

/*! \brief Handle notification that Headset is now moving */
void appHeadsetPhyStateMotionEvent(void);

/*! \brief Handle notification that Headset is now not moving. */
void appHeadsetPhyStateNotInMotionEvent(void);

/*! \brief Tell the headset phy state module to prepare for entry to dormant.
           Headset phy state unregisters itself as a client of all sensors which (if
           headset phy state is the only remaining client), will cause the sensors to
           switch off or enter standby.
 */
void appHeadsetPhyStatePrepareToEnterDormant(void);

/*! \brief Initialise the module.
    \note #HEADSET_PHY_STATE_INIT_CFM is sent when the phy state is known.
*/
bool appHeadsetPhyStateInit(Task init_task);

#endif /* HEADSET_PHY_STATE_H */
