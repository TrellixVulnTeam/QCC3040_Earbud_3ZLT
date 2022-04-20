/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Implementation of module managing BT Inquiry.
*/

#include "inquiry_manager.h"

#include <logging.h>
#include <message.h>
#include <task_list.h>
#include <unexpected_message.h>

#include <panic.h>
#include <app/bluestack/hci.h>

/*! \brief Macro for simplifying creating messages */
#define MAKE_INQUIRY_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);

#define INQUIRY_MANAGER_CLIENT_TASKS_LIST_INIT_CAPACITY 1

/*! Inquiry manager state */
typedef enum
{
    INQUIRY_MANAGER_STATE_IDLE,
    INQUIRY_MANAGER_STATE_INQUIRY,
} inquiry_manager_state_t;

/*! Inquiry Manager data */
typedef struct
{
    /*! Init's local task */
    TaskData task;

    /*! Configured collection of parameters */
    const inquiry_manager_scan_parameters_t *parameter_set;

    /*! Number of parameters in configured collection */
    uint16 parameter_set_length;

    /*! List of clients */
    TASK_LIST_WITH_INITIAL_CAPACITY(INQUIRY_MANAGER_CLIENT_TASKS_LIST_INIT_CAPACITY) clients;

    /*! Inquiry manager state */
    inquiry_manager_state_t state;

    /*! The collection index chosen for the inquiry scan */
    uint16 set_filter;

} inquiry_manager_data_t;

inquiry_manager_data_t inquiry_manager_data;

/*! Get pointer to Inquiry Manager task*/
#define InquiryManager_GetTask() (&inquiry_manager_data.task)

/*! Get pointer to Inquiry Manager data structure */
#define InquiryManager_GetTaskData() (&inquiry_manager_data)

/*! Get pointer to Inquiry Manager client list */
#define InquiryManager_GetClientList() (task_list_flexible_t *)(&inquiry_manager_data.clients)

/*! \brief Handler for Inquiry results from the connection library
           Sends a INQUIRY_MANAGER_RESULT if a result is returned from the library.

           If the Status is inquiry_status_ready but no device is found, the
           repeat limit has not been reached and a immediate stop has not been requested via
           InquiryManager_Stop() then the repeat counter will be decremented and a new inquiry
           scan begun.

           Once atleast one device candidate has been found or if the repeats have been exhausted
           then the INQUIRY_MANAGER_SCAN_COMPLETE shall be sent*/
static void inquiryManager_HandleClDmInquireResult(const CL_DM_INQUIRE_RESULT_T *result)
{
    DEBUG_LOG_FN_ENTRY("inquiryManager_HandleClDmInquireResult");

    if (result->status == inquiry_status_result)
    {
        DEBUG_LOG_DEBUG("inquiryManager_HandleClDmInquireResult, bdaddr 0x%04x 0x%02x 0x%06lx rssi %d cod %lx",
                  result->bd_addr.nap,
                  result->bd_addr.uap,
                  result->bd_addr.lap,
                  result->rssi,
                  result->dev_class);

        MAKE_INQUIRY_MESSAGE(INQUIRY_MANAGER_RESULT);

        message->bd_addr = result->bd_addr;
        message->dev_class = result->dev_class;
        message->clock_offset = result->clock_offset;
        message->page_scan_rep_mode = result->page_scan_rep_mode;
        message->page_scan_mode = result->page_scan_mode;
        message->rssi = result->rssi;

        TaskList_MessageSend(&inquiry_manager_data.clients, INQUIRY_MANAGER_RESULT, message);
    }
    else
    {
        DEBUG_LOG_DEBUG("inquiryManager_HandleClDmInquireResult: Scan Complete");

        TaskList_MessageSendId(&inquiry_manager_data.clients, INQUIRY_MANAGER_SCAN_COMPLETE);
        inquiry_manager_data.state = INQUIRY_MANAGER_STATE_IDLE;
        inquiry_manager_data.set_filter = 0;
    }
}

/*! \brief Handler for connection library messages.*/
static void inquiryManager_HandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);
    switch (id)
    {
        case CL_DM_INQUIRE_RESULT:
            inquiryManager_HandleClDmInquireResult((CL_DM_INQUIRE_RESULT_T *)message);
        break;

        default:
            UnexpectedMessage_HandleMessage(id);
        break;
    }
}

void InquiryManager_RegisterParameters(const inquiry_manager_scan_parameters_t *params, uint16 set_length)
{
    DEBUG_LOG_FN_ENTRY("InquiryManager_RegisterParameters %p , length:%d", params, set_length);

    PanicNull((void*)params);
    InquiryManager_GetTaskData()->parameter_set = params;
    InquiryManager_GetTaskData()->parameter_set_length = set_length;

}

bool InquiryManager_Start(uint16 filter_id)
{
    DEBUG_LOG_FN_ENTRY("InquiryManager_Start filter:enum:inquiry_manager_filter_t:%d", filter_id);

    if (InquiryManager_IsInquiryActive())
    {
        DEBUG_LOG_DEBUG("InquiryManager_Start: Cannot Start. Inquiry already in progress");
        return FALSE;
    }

    if (InquiryManager_GetTaskData()->parameter_set == NULL)
    {
        DEBUG_LOG_ERROR("InquiryManager_Start: Inquiry Manager not configured");
        Panic();
    }

    if (filter_id >= InquiryManager_GetTaskData()->parameter_set_length)
    {
        DEBUG_LOG_ERROR("InquiryManager_Start: filter_id out of bounds");
        return FALSE;
    }

    inquiry_manager_data.set_filter = filter_id;
    inquiry_manager_data.state = INQUIRY_MANAGER_STATE_INQUIRY;

    /* Start inquiry */
    ConnectionWriteInquiryMode(InquiryManager_GetTask(), inquiry_mode_rssi);
    ConnectionInquire(InquiryManager_GetTask(),
                      HCI_INQ_CODE_GIAC,
                      inquiry_manager_data.parameter_set[filter_id].max_responses,
                      inquiry_manager_data.parameter_set[filter_id].timeout,
                      inquiry_manager_data.parameter_set[filter_id].class_of_device
                      );

    TaskList_MessageSendId(&inquiry_manager_data.clients, INQUIRY_MANAGER_SCAN_STARTED);
    return TRUE;
}

bool InquiryManager_Init(Task init_task)
{
    UNUSED(init_task);
    DEBUG_LOG_FN_ENTRY("InquiryManager_Init");

    inquiry_manager_data.state = INQUIRY_MANAGER_STATE_IDLE;

    inquiry_manager_data.parameter_set = NULL;

    TaskList_InitialiseWithCapacity(InquiryManager_GetClientList(),INQUIRY_MANAGER_CLIENT_TASKS_LIST_INIT_CAPACITY);

    inquiry_manager_data.task.handler = inquiryManager_HandleMessage;
    return TRUE;

}

bool InquiryManager_ClientRegister(Task client_task)
{
    return TaskList_AddTask(&inquiry_manager_data.clients, client_task);
}

bool InquiryManager_IsInquiryActive(void){
    return (inquiry_manager_data.state >= INQUIRY_MANAGER_STATE_INQUIRY);
}

void InquiryManager_Stop(void)
{
    ConnectionInquireCancel(InquiryManager_GetTask());
}

bool InquiryManager_ClientUnregister(Task client_task)
{
    return TaskList_RemoveTask(TaskList_GetFlexibleBaseTaskList(InquiryManager_GetClientList()), client_task);
}
