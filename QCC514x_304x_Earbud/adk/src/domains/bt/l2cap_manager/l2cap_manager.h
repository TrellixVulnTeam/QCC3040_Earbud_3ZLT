/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       l2cap_manager.h
\brief	    Interface to module providing L2CAP connections.

    The L2CAP Manager abstracts the details of setting parameters and messages
    provided by the upper stack (Synergy or the connection library).
    SDP search/registration are managed by this manager to obtain a remote PSM
    and/or to make the PSM connectable from other devices.
    
    Once a client registers its handler functions, all the events on the PSM and
    events such as establishing a L2CAP connection by remote device are notified
    through those callback functions.
*/

#ifndef L2CAP_MANAGER_H_
#define L2CAP_MANAGER_H_

#ifdef INCLUDE_L2CAP_MANAGER

#include <csrtypes.h>
#include <bdaddr.h>
#include <message.h>
#include <sink.h>
#include <source.h>



/*! \brief Invalid PSM value. */
#define L2CAP_MANAGER_PSM_INVALID                       (0x0000)

/*! \brief A PSM parameter used for requesting a dynamically allocated PSM. */
#define L2CAP_MANAGER_PSM_DYNAMIC_ALLOCATION            L2CAP_MANAGER_PSM_INVALID


/*! \brief Invalid PSM instance ID. */
#define L2CAP_MANAGER_PSM_INSTANCE_ID_INVALID           (0x0000)


/*! \brief A unique identifier of an L2CAP service/protocol instance managed by
           the L2CAP Manager.

    \note A single L2CAP Manager instance can have more than one L2CAP link.
*/
typedef uint16 l2cap_manager_instance_id;


/*! \brief Enumeration of L2CAP Manager status codes.
*/
typedef enum
{
    /*! Operation success. */
    l2cap_manager_status_success,
    /*! Operation fail. */
    l2cap_manager_status_failure,

    /*! Rejected due to ongoing */
    l2cap_manager_status_rejected_due_to_ongoing_handover,

    /*! Failed to allocate an instance. */
    l2cap_manager_failed_to_allocate_an_instance,

    /* Detailed error status codes will be added here. */

} l2cap_manager_status_t;


/*!
    \brief L2CAP connect status.

    This is the status returned in an \ref l2cap_manager_connect_cfm_t message
    indicating that an L2CAP connection has been established.
*/
typedef enum
{
    /*! L2CAP connection successfully established. */
    l2cap_manager_connect_status_success,
    /*! L2CAP connection is pending. */
    l2cap_manager_connect_status_pending,

    /*! The L2CAP connect attempt failed because either the local or remote end
      issued a disconnect before the connection was fully established. */
    l2cap_manager_connect_status_failed = 0x80,

    /*! The connection attempt failed due to the failure to find the matching
        SDP records within the number of retries specified. */
    l2cap_manager_connect_status_failed_sdp_search,
    /*! The connection attempt failed due to an internal error in the
      Connection library. */
    l2cap_manager_connect_status_failed_internal_error,
    /*! The connection attempt failed because the remote end rejected the
      connection request. */
    l2cap_manager_connect_status_failed_remote_reject,
    /*! The connection attempt failed because the remote device rejected our
      configuration request. */
    l2cap_manager_connect_status_failed_config_rejected,
    /*! The connection attempt failed due to security requirements. */
    l2cap_manager_connect_status_failed_security,
    /*! The connection was terminated by the local host. */
    l2cap_manager_connect_status_terminated_by_host,
    /*! The connection attempt failed because the remote device closed the
        connection. */
    l2cap_manager_connect_status_failed_remote_disc,
    /*! The conftab sent to Bluestack was invalid and rejected immediately. */
    l2cap_manager_connect_status_failed_invalid_conftab,
    /*! The connection attempt timed out. */
    l2cap_manager_connect_status_timeout,
    /*! The connection attempt failed because the key is missing. */
    l2cap_manager_connect_status_failed_key_missing,
    /*! The connection attempt failed because of an error. */
    l2cap_manager_connect_status_error,

    /*! Unknown status. */
    l2cap_manager_connect_status_unknown = 0xFF

    /* ToDo:
     *      At the moment, this connection status enum is just a copy from
     *      the connection library ('l2cap_connect_status').
     * 
     *      This must be harmonised with the Synergy stuff.
     *      Any feedback is really appreciated.
     */

} l2cap_manager_connect_status_t;


/*!
    \brief L2CAP disconnect status.

    This is the status returned in an \ref l2cap_manager_disconnect_ind_t message
    indicating that an L2CAP connection has been disconnected.
*/
typedef enum
{
    /*! The L2CAP connection was disconnected successfully. */
    l2cap_manager_disconnect_successful,
    /*! The L2CAP disconnect attempt timed out. */
    l2cap_manager_disconnect_timed_out,
    /*! The L2CAP disconnect attempt returned an error. */
    l2cap_manager_disconnect_error,
    /*! The L2CAP connection could not be disconnected because a null sink was
      passed in. */
    l2cap_manager_disconnect_no_connection,
    /*! The L2CAP connection was disconnected due to link loss. */
    l2cap_manager_disconnect_link_loss,
    /*! The L2CAP connection was disconnected due it being transferred. */
    l2cap_manager_disconnect_transferred,

    /*! The L2CAP connection was disconnected due to unknown reason. */
    l2cap_manager_disconnect_unknown_reason

} l2cap_manager_disconnect_status_t;


/*! \brief Data struct for an SDP record.
*/
typedef struct
{
    /*! Pointer to an SDP record.*/
    const uint8    *service_record;
    /*! The number of bytes in the SDP record. */
    uint16          service_record_size;
    /*! The position of the local PSM to be inserted in the SDP record.
        This is a byte offset from the start of the SDP record. */
    uint16          offset_to_psm;

} l2cap_manager_sdp_record_t;


/*! \brief Data struct for an SDP search pattern.
*/
typedef struct
{
    /*! Maximum number of retries that the client wants the L2CAP Manager to search the SDP pattern. */
    uint8           max_num_of_retries;
    /*! The maximum number of attributes. */
    uint16          max_attributes;
    /*! The pattern to search for. */
    const uint8    *search_pattern;
    /*! The size of search_pattern. */
    uint16          search_pattern_size;
    /*! The attribute list. */
    const uint8    *attribute_list;
    /*! The size of the attribute_list. */
    uint16          attribute_list_size;

} l2cap_manager_sdp_search_pattern_t;


/*! \brief Data struct for the configuration of an L2CAP link.
*/
typedef struct
{
    /*! The length of the configuration table data array.
       Note that this is the number of uint16 entries in the array. */
    uint16          conftab_length; 
    /*! Pointer to a configuration table of uint16 values. These are key value
        pairs defining configuration options to be passed to the upper stack.
        The memory allocated for it will be released by the upper stack or the
        L2CAP Manager. */
    const uint16   *conftab;

} l2cap_manager_l2cap_link_config_t;


/*!
    \brief Data struct to inform a client about an incoming L2CAP connection.

    This message is used to notify a client that a remote device is attempting
    to create an L2CAP connection to this device.
*/
typedef struct
{
    /*! Transport Bluetooth Address of the remote device that initiated the
        connection. */
    tp_bdaddr   tpaddr;
    /*! Local PSM that the remote device is attempting to connect to. */
    uint16      local_psm;
    /*! Remote PSM that the remote device is attempting to connect from. */
    uint16      remote_psm;

    /*! The channel identifier.
        NB: No need to be copied directly into the response, as this is notified
            by the callback function. */
    uint8       identifier;

    /*! Unique signal identifier for the connection attempt.
        NB: No need to be copied directly into the response, as this is notified
            by the callback function. */
    uint16      connection_id;

} l2cap_manager_connect_ind_t;


/*!
    \brief Data struct to inform the L2CAP Manager whether or not to accept an
           incoming connection request.

    \note The identifier and the connection ID are not included as they are
          managed by the L2CAP Manager.
*/
typedef struct
{
    /*! Set to TRUE to accept the incoming connection or FALSE to reject it. */
    bool            response;

    /*! The length of the configuration table data array.
        Note that this is the number of uint16 entries in the array. */
    uint16          conftab_length;
    /*! Pointer to a configuration table of uint16 values. These are key value
        pairs defining configuration options to be passed to the upper stack.
        The memory allocated for it will be released by the upper stack or the
        L2CAP Manager. */
    const uint16*   conftab;

} l2cap_manager_connect_rsp_t;


/*!
    \brief L2CAP Quality of Service Parameters

    The Quality of Service parameters are negotiated before an L2CAP connection
    is established.  A detailed explanation of each of these parameters can be
    found in the L2CAP section of the Bluetooth specification.
*/
typedef struct
{
    /*! Level of the service required e.g. best effort. */
    uint8       service_type;
    /*! Average data rate with which data is transmitted. */
    uint32      token_rate;
    /*! Specifies a limit on the "burstiness" with which data may be
      transmitted. */
    uint32      token_bucket;
    /*! This limits how fast L2CAP packets can be sent back-to-back. */
    uint32      peak_bw;
    /*! Maximum acceptable latency of an L2CAP packet. */
    uint32      latency;
    /*! Difference between the maximum and minimum acceptable delay of an L2CAP
      packet. */
    uint32      delay_var;

} l2cap_manager_qos_flow;


/*!
    \brief Data struct to inform the result of the L2CAP connection attempt.

    This message is returned to both the initiator and acceptor of the L2CAP
    connection and is used to inform them whether the connection was
    successfully established or not.  Once this message has been received the
    connection can be used to transfer data.
*/
typedef struct
{
    /*! Indicates whether or not the connection is successfully established. */
    l2cap_manager_connect_status_t  status;
    /*! The local PSM that is connected to. */
    uint16                          local_psm;
    /*! The remote PSM that is connected from. */
    uint16                          remote_psm;
    /*! Sink identifying the connection. The sink is used to send data to the
        remote device and must be stored by the client task. */
    Sink                            sink;

    /*! Unique identifier for the connection attempt, allows the client to
        match this CFM message to the response sent to the connection lib where
        multiple connections are being established simultaneously to the same
        device. */
    uint16                          connection_id;

    /*! The Bluetooth device address of the connecting device. */
    tp_bdaddr                       tpaddr;
    /*! The MTU advertised by the remote device. */
    uint16                          mtu_remote;
    /*! The flush timeout in use by the remote device. */
    uint16                          flush_timeout_remote;
    /*! The Quality of Service settings of the remote device. */
    l2cap_manager_qos_flow          qos_remote;
    /*! The flow mode agreed with the remote device */
    uint8                           mode;

} l2cap_manager_connect_cfm_t;


/*!
    \brief Data struct to inform that an L2CAP connection has been disconnected.
    
    The sink will remain valid for reading any remaining data that may be in
    the buffer until the client' #handle_disconnect_ind callback function
    returns the control back to the L2CAP Manager.
*/
typedef struct
{
    /*! The channel identifier.
        NB: No need to be copied directly into the response, as this is notified
            by the callback function. */
    uint8                               identifier;

    /*! Indicates the L2CAP connection has been disconnected and the status of
        the disconnect. */
    l2cap_manager_disconnect_status_t   status;
    /*! Sink identifying the L2CAP connection that was disconnected. */
    Sink                                sink;

} l2cap_manager_disconnect_ind_t;


/*!
    \brief Data struct to inform that an L2CAP connection has been disconnected.

    The sink is no longer valid and cannot be used to send data to the remote end.
*/
typedef struct
{
    /*! Indicates the L2CAP connection has been disconnected and the status of
        the disconnect. */
    l2cap_manager_disconnect_status_t   status;
    /*! Sink identifying the L2CAP connection that was disconnected. */
    Sink                                sink;

} l2cap_manager_disconnect_cfm_t;


/*!
    \brief Data struct that indicates that a source associated with an L2CAP
           connection has received data.
*/
typedef struct  
{
    /*! Unique identifier for the connection, to which new data has arrived. */
    uint16  connection_id;
    /*! The source that has more data in it. */
    Source  source;    

} l2cap_manager_message_more_data_t;

/*!
    \brief Data struct that indicates that a sink associated with an L2CAP
           connection has more space.
*/
typedef struct 
{
    /*! Unique identifier for the connection, which gets a space to send data. */
    uint16  connection_id;

    /*! The sink that has more space in it. */
    Sink    sink;    

} l2cap_manager_message_more_space_t;




/*! \brief Table of callback handler funcitons, which are called by the L2CAP
           Manager to notify events or to get information such as SDP record.
 */
typedef struct
{
    /*! Indication that a PSM registration is processed. This callback is called
        when the registration started by a call of L2capManager_Register(...) is
        completed. */
    void (*registered_ind)(l2cap_manager_status_t status);

    /*! Get SDP record and the position of the PSM (to be inserted) in the record. */
    /*! \param[in]  local_psm   The local PSM to be inserted to the SDP record. */
    /*! \param[out] sdp_record  SDP record, its size, and the position of the
                                PSM to be inserted within the record. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*get_sdp_record)(uint16 local_psm, l2cap_manager_sdp_record_t *sdp_record);

    /*! Get SDP search pattern.
        This function is called if the remote PSM is unknown and it needs to be
        read from the remote device's SDP record. */
    /*! \param[in]  tpaddr              Transport Bluetooth Address of the
                                        remote device that the L2CAP Manager
                                        will carry out the SDP search. */
    /*! \param[out] sdp_search_pattern  SDP search pattern used for obtaining
                                        the remote PSM from the peer. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*get_sdp_search_pattern)(const tp_bdaddr *tpaddr, l2cap_manager_sdp_search_pattern_t *sdp_search_pattern);

    /*! Get the configuration for an L2CAP link.
        This function is called when a client initiates a connection request to
        a remote device. If the remote device's PSM is not known, the L2CAP
        Manager attempts to get the PSM with SDP search(s). After that, this
        function is called to provide the configurations for the L2CAP link. */
    /*! \param[in]  tpaddr  Transport Bluetooth Address of the remote device
                            that the L2CAP Manager will try to connect. */
    /*! \param[out] config  Pointer to the configuration parameters for the
                            L2CAP link to be established.
                            Note that the client may set NULL to this pointer,
                            if it just wants to use the default setting. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*get_l2cap_link_config)(const tp_bdaddr *tpaddr, l2cap_manager_l2cap_link_config_t *config);

    /*! Handle an incoming L2CAP connection request.
        This function is called when a remote device is attempting to create an
        L2CAP connection to this device. */
    /*! \param[in]  ind     Indication that contains the Bluetooth address of
                            the remote device attempting to create an L2CAP
                            connection, and the local PSM to be connected to. */
    /*! \param[out] rsp     Response to this connect indication. The client must
                            specify if it accepts the connection or not.
                            If the client wants to change some connection
                            parameters, its preferred setting can be provided
                            as a configuration table. */
    /*! \param[out] context Pointer to context-dependent data. The client may
                            use this to store a pointer to any data relevant to
                            the link. The L2CAP Manager just provides this as a
                            place holder for the client.
                            The value is held till the disconnection of the
                            link, which event is notified to the client with
                            'handle_disconnect_cfm' callback function. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*respond_connect_ind)(const l2cap_manager_connect_ind_t *ind, l2cap_manager_connect_rsp_t *rsp, void **context);

    /*! Handle the result of an L2CAP connection attempt initiated by either
        the remote or this device. */
    /*! \param[in]  cfm     Confirmation that contains the status of the
                            attempted connection. On successful connection,
                            a valid sink to the link, the Bluetooth address
                            of the remote device, and several connection 
                            paramemters are provided. */
    /*! \param[in] context  Pointer to context-dependent data. The client may
                            use this to store a pointer to any data relevant to
                            the link. The L2CAP Manager just provides this as a
                            place holder for the client. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*handle_connect_cfm)(const l2cap_manager_connect_cfm_t *cfm, void *context);

    /*! Handle an event that an L2CAP connection has been disconnected.
        Both the sink and source will remain valid till the client returns the
        control from this callback function. The client may read any remaining
        data that may be in the buffer. The source will be emptied by the L2CAP
        Manager.
        Once the control is back to the L2CAP Manager, both the sink and the
        source are no longer valid, and any remaining data in the buffer will
        be lost. */
    /*! \param[in]  ind     Indication that contains the disconnection status
                            and the sink. */
    /*! \param[in] context  Pointer to context-dependent data. The client may
                            use this to store a pointer to any data relevant to
                            the link. The L2CAP Manager just provides this as a
                            place holder for the client. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*respond_disconnect_ind)(const l2cap_manager_disconnect_ind_t *ind, void *context);

    /*! Handle an event that an L2CAP connection has been disconnected and the
        sink is no longer valid. */
    /*! \param[in]  cfm     Confirmation that contains the disconnection status
                            and the sink. */
    /*! \param[in] context  Pointer to context-dependent data. The client may
                            use this to store a pointer to any data relevant to
                            the link. The L2CAP Manager just provides this as a
                            place holder for the client.
                            The value is held till the disconnection of the
                            link, which event is notified to the client with
                            'handle_disconnect_cfm' callback function. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*handle_disconnect_cfm)(const l2cap_manager_disconnect_cfm_t *cfm, void *context);

    /*! Process that a source associated with an L2CAP connection has received data. */
    /*! \param[in]  more_data   Data struct that contains the source, in which
                                new data is received. */
    /*! \param[in] context      Pointer to context-dependent data. The client may
                                use this to store a pointer to any data relevant to
                                the link. The L2CAP Manager just provides this as a
                                place holder for the client. */
    /*! \note If this handler is set, the client must set the 'process_more_space'
              handler too. Both the handlers must be set or be NULL. Setting only
              one of them causes the L2CAP Manager to panic. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*process_more_data)(const l2cap_manager_message_more_data_t *more_data, void *context);

    /*! Process that a sink associated with an L2CAP connection has more space. */
    /*! \param[in/out] more_space   Data struct that contains the sink that gets
                                    some space to send data. */
    /*! \param[in] context      Pointer to context-dependent data. The client may
                                use this to store a pointer to any data relevant to
                                the link. The L2CAP Manager just provides this as a
                                place holder for the client. */
    /*! \note If this handler is set, the client must set the 'process_more_data'
              handler too. Both the handlers must be set or be NULL. Setting only
              one of them causes the L2CAP Manager to panic. */
    /*! \return Returns #l2cap_manager_status_success if the request has been
                processed, otherwise returns an error code. */
    l2cap_manager_status_t (*process_more_space)(l2cap_manager_message_more_space_t *more_space, void *context);

} l2cap_manager_functions_t;



/*! \brief Initialise the L2CAP Manager module.

    \param[in] init_task    (Not used.)

    \return TRUE (Always).

    \note This initialisation function must be called once prior to any other
          L2CAP Manager functions, typically from the initialisation code of
          the Application.
*/
bool L2capManager_Init(Task init_task);


/*! \brief Register an L2CAP PSM with the upper stack.

    If the caller specifies a local Protocol/Service Multiplexer (PSM) such as
    one of the Bluetooth-SIG defined PSMs, the PSM is used. Otherwise, a PSM
    assigned by the upper stack will be used.

    If 'get_sdp_record' callback function is not NULL, this function registers
    the SDP record provided by the callback funciton.

    Once those registrations have completed, 'registerd_ind' callback function
    will be called.

    After that relevant callback functions are called on their events.
    For example, another device is attempting to create an L2CAP connection to
    the registered PSM, 'respond_connect_ind' callback function is called.

    \param[in] local_psm    Local PSM to be registered if one of the pre-defined
                            PSM such as HID Control (0x11), AVCTP (0x17), or
                            AVDTP (0x19) are used. Otherwise, this value is set
                            to zero (an invalid PSM) to obtain a PSM assigned by
                            the upper stack.

    \param[in] functions    Pointer to the table of the handler functions.

    \param[out] instance_id Pointer to a unique instance ID assigned to the PSM.

    \return l2cap_manager_status_success on success, otherwise one of the error code.
*/
l2cap_manager_status_t L2capManager_Register(uint16 psm, const l2cap_manager_functions_t* functions, l2cap_manager_instance_id *instance_id);


/*! \brief Initiate to create an L2CAP connection to a particular device.

    The PSM must be registered with 'L2capManager_Register(...)', which assign
    an instance ID to be used with this function.

    If the PSM is dynamically assigned rather than pre-defined one, thus the
    remote PSM is unknown, this function calls 'get_sdp_search_pattern' callback
    function and carry out an SDP search to obtain the remote PSM.

    The result of the connection attempt will be informed to the client with
    'handle_connect_cfm' callback function.

    \param[in] tpaddr       Transport Bluetooth Address of connected device.

    \param[in] instance_id  A unique instance ID assigned to the PSM.

    \param[out] context     Pointer to context-dependent data. The client may
                            use this to store a pointer to any data relevant to
                            the link. The L2CAP Manager just provides this as a
                            place holder for the client.
                            The value is held till the disconnection of the
                            link, which event is notified to the client with
                            'handle_disconnect_cfm' callback function.

    \return l2cap_manager_status_success on success, otherwise one of the error code.
*/
l2cap_manager_status_t L2capManager_Connect(const tp_bdaddr *tpaddr, l2cap_manager_instance_id instance_id, void *context);


/*! \brief Disconnect an L2CAP link associated with a PSM.

    The callback function 'handle_disconnect_cfm' will be called to inform the
    outcome of this request.

    \param[in] sink         A sink allocated for the L2CAP link to be disconnected.

    \param[in] instance_id  A unique instance ID associated with the PSM.

    \return l2cap_manager_status_success on success, otherwise one of the error code.
*/
l2cap_manager_status_t L2capManager_Disconnect(Sink sink, l2cap_manager_instance_id instance_id);


#endif /* INCLUDE_L2CAP_MANAGER */
#endif /* L2CAP_MANAGER_H_ */
