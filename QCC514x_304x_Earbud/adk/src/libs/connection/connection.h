/****************************************************************************
Copyright (c) 2004 - 2021 Qualcomm Technologies International, Ltd.


FILE NAME
    connection.h

DESCRIPTION
    Header file for the Connection library.
*/


/*!
\defgroup connection connection
\ingroup vm_libs

\brief    Header file for the Connection library.

\section connection_intro INTRODUCTION
        This file provides documentation for the BlueLab connection library
        API including BLE functions.

@{

*/



#ifndef    CONNECTION_H_
#define    CONNECTION_H_

#ifndef DISABLE_BLE
/* Enable BLE message ids in the ConnectionMessageId enumeration, in
 * connection_no_ble.h
 */
#define ENABLE_BLE_MESSAGES
#endif


#include "connection_no_ble.h"


#ifndef DISABLE_BLE

#define BLE_AD_PDU_SIZE         31
#define BLE_SR_PDU_SIZE         31

/*! \name OWN_ADDRESS

    \brief Own address type for BLE Scanning, Advertising and connection
           parameters.
*/
/*!\{ */
/*! \brief Use the Public Address. */
#define OWN_ADDRESS_PUBLIC              (0x00)
/*! \brief Use a generated Random Address. */
#define OWN_ADDRESS_RANDOM              (0x01)
/*! \brief Generate a Resolvable Private Address (RPA), fallback to the
           Public address if an RPA is not available.
*/
#define OWN_ADDRESS_GENERATE_RPA_FBP    (0x02)
/*! \brief Generate a Resolvable Private Address (RPA), fallback to the
           Random address if an RPA is not available.
*/
#define OWN_ADDRESS_GENERATE_RPA_FBR    (0x03)
/*! \} */

/*!
    \brief Enable or disable Bluetooth low energy (BLE) scanning.

    \param enable Enable scanning if TRUE, otherwise scanning is disabled.

    Low energy scanning cannot be enabled if the device is advertising itself.
    ConnectionBleAddAdvertisingReportFilter() can be used to filter the
    advertising reports sent to the VM by Bluestack.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetScanEnable(bool enable);

/*!
    \brief Enable or disable Bluetooth low energy (BLE) scanning.

    \param theAppTask Task to send the resulting confirmation message to.

    \param enable Enable scanning if TRUE, otherwise scanning is disabled.

    Low energy scanning cannot be enabled if the device is advertising itself.
    ConnectionBleAddAdvertisingReportFilter() can be used to filter the
    advertising reports sent to the VM by Bluestack.

    \return A message of type #CL_DM_BLE_SET_SCAN_ENABLE_CFM is sent to
    the task indicated by the theAppTask parameter.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetScanEnableReq(Task theAppTask, bool enable);

/*!
    \brief Sent in response to ConnectionDmBleSetScanEnable().

    This is a BT4.0 only message.
*/
typedef struct
{
    hci_status status;  /*!< Status of the operation */
} CL_DM_BLE_SET_SCAN_ENABLE_CFM_T;

/*!
    \brief Set parameters for Bluetooth low energy (BLE) scanning.

    To use a random address for scanning, it is necessary to generate it
    calling ConnectionDmBleConfigureLocalAddressReq() or
    ConnectionDmBleConfigureLocalAddressAutoReq() before this.

    To use the public Bluetooth address after using a random one,
    it is necessary only to use this, setting own_address to OWN_ADDRESS_RANDOM.

    \param enable_active_scanning If TRUE SCAN_REQ packets may be sent (default:
           FALSE).
    \param own_address See the #OWN_ADDRESS defines for allowed values.
    \param white_list_only If TRUE then advertising packets from devices that
           are not on the White List for this device will be ignored (default:
           FALSE).
    \param scan_interval Scan interval in steps of 0.625ms, range 0x0004
           (2.5 ms) to 0x4000 (10.24 s).
    \param scan_window Scan window in steps of 0.625ms, range 0x0004 (2.5ms) to
           0x4000 (10.24 s). Must be less than or equal to the scan_interval
           parameter value.

    \return A message of type #CL_DM_BLE_SET_SCAN_PARAMETERS_CFM_T
    is sent to the  task that initialised the Connection library when operation
    has completed.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetScanParametersReq(
        bool    enable_active_scanning,
        uint8   own_address,
        bool    white_list_only,
        uint16  scan_interval,
        uint16  scan_window
        );

/*!
    \brief Sent in response to ConnectionDmBleSetScanParametersReq() to the task that
    intialised the Connection library.

    This is a BT4.0 only message.
*/
typedef struct
{
    connection_lib_status           status;     /*!< Status of the operation */
} CL_DM_BLE_SET_SCAN_PARAMETERS_CFM_T;


/*!
    \brief Set the data to be put in BLE Scan Responses sent by this
    device.

    \param size_sr_data The length of the scan response data, maximum is 31
           octets.
    \param sr_data pointer to the Scan Response data.

    The Scan Response data is copied. The pointer to the data is not
    freed by this function. A #CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T message
    will be sent to the task that initialised the Connection library to indicate
    the status.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetScanResponseDataReq(
        uint8 size_sr_data,
        const uint8 *sr_data
        );

/*!
    \brief Sent in response to setting data for the BLE Scan Response to
    the task that initialised the Connection library.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Advertising Data status.*/
    connection_lib_status           status;
}
CL_DM_BLE_SET_SCAN_RESPONSE_DATA_CFM_T;

/*!
    \brief AD type - to be used when setting Advertising Report Filters
    (ConnectionBleAddAdvertisingReportFilter()).

    This is a BT4.x only type.

*/
typedef enum
{
    /*! Flags. */
    ble_ad_type_flags                       = 0x01,
    /*! Service - More 16-bit UUIDs available. */
    ble_ad_type_more_uuid16                 = 0x02,
    /*! Service - Complete list of 16-bit UUIDs available. */
    ble_ad_type_complete_uuid16             = 0x03,
    /*! Service - More 32-bit UUIDs available. */
    ble_ad_type_more_uuid32                 = 0x04,
    /*! Service - Complete list of 32-bit UUIDs available. */
    ble_ad_type_complete_uuid32             = 0x05,
    /*! Service - More 128-bit UUIDs available. */
    ble_ad_type_more_uuid128                = 0x06,
    /*! Service - Complete list of 128-bit UUIDs available. */
    ble_ad_type_complete_uuid128            = 0x07,
    /*! Local Name - Shortened local name. */
    ble_ad_type_shortened_local_name        = 0x08,
    /*! Local Name - Complete local name. */
    ble_ad_type_complete_local_name         = 0x09,
    /*! TX Power Level. */
    ble_ad_type_tx_power_level              = 0x0A,
    /*! Simple Pairing optional OOB tags. */
    ble_ad_type_ssp_oob_class_of_device     = 0x0D,
    /*! SSP OOB - Hash C. */
    ble_ad_type_ssp_oob_hash_c              = 0x0E,
    /*! SSP OOB - Rand R (R-192). */
    ble_ad_type_ssp_oob_rand_r              = 0x0F,
    /*! Security Manager TK value. */
    ble_ad_type_sm_tk_value                 = 0x10,
    /*! Security Manager OOB Flags. */
    ble_ad_type_sm_oob_flags                = 0x11,
    /*! Slave Connection Interval range. */
    ble_ad_type_slave_conn_interval_range   = 0x12,
    /*! Service solicitation - List of 16-bit Service UUID. */
    ble_ad_type_service_16bit_uuid          = 0x14,
    /*! Service solicitation - List of 128-bit Service UUID. */
    ble_ad_type_service_128bit_uuid         = 0x15,
    /*! Service Data (16-bit default). */
    ble_ad_type_service_data                = 0x16,
    /*! Public Target Address. */
    ble_ad_type_public_target_address       = 0x17,
    /*! Random Target Address. */
    ble_ad_type_random_target_address       = 0x18,
    /*! Appearance. */
    ble_ad_type_appearance                  = 0x19,
    /*! Advertising interval. */
    ble_ad_type_advertising_interval        = 0x1A,
    /*! LE Bluetooth Device Address. */
    ble_ad_type_bluetooth_device_address    = 0x1B,
    /*! LE Role. */
    ble_ad_type_role                        = 0x1C,
    /*! Simple Pairing Hash C-256. */
    ble_ad_type_simple_pairing_hash_c256    = 0x1D,
    /*! Simple Pairing Randomizer R-256. */
    ble_ad_type_simple_pairing_rand_r256    = 0x1E,
    /*! Service solicitation - List of 32-bit Service UUID. */
    ble_ad_type_service_32bit_uuid          = 0x1F,
    /*! Service Data - 32-Bit UUID. */
    ble_ad_type_service_data_32bit          = 0x20,
    /*! Service Data - 128-Bit UUID. */
    ble_ad_type_service_data_128bit         = 0x21,
    /*! LE Secure Connections Confirmation Value. */
    ble_ad_type_connection_conf_value       = 0x22,
    /*! LE Secure Connections Random Value. */
    ble_ad_type_connection_rand_value       = 0x23,
    /*! Universal Resource Indicator. */
    ble_ad_type_uri                         = 0x24,
    /*! Indoor Positioning, Service v1.0 or later. */
    ble_ad_type_indoor_positioning          = 0x25,
    /*! Transport Discovery Data, Service b1.0 or later. */
    ble_ad_type_transport_discovery_data    = 0x26,
    /*! RSI adv tag */
    ble_ad_type_rsi_data                   = 0x2E,
    /*! 3D Information Data, 3D Synchronisation Profile, V1.0 or later. */
    ble_ad_type_3d_information_data         = 0x3d,
    /*! Manufacturer Specific Data. */
    ble_ad_type_manufacturer_specific_data  = 0xFF
} ble_ad_type;

/*!
    \brief Bluetooth Low Energy GAP flags

    If any of the flags is non-zero advertisement data shall contain
    the flags within ::ble_ad_type_flags field.

    This is a BT4.0 only type.
*/
/*! LE Limited Discoverable Mode */
#define BLE_FLAGS_LIMITED_DISCOVERABLE_MODE     0x01
/*! LE General Discoverable Mode */
#define BLE_FLAGS_GENERAL_DISCOVERABLE_MODE     0x02
/*! BR/EDR Not Supported */
#define BLE_FLAGS_SINGLE_MODE                   0x04
/*! Simultaneous LE and BR/EDR to Same Device Capable (Controller) */
#define BLE_FLAGS_DUAL_CONTROLLER               0x08
/*! Simultaneous LE and BR/EDR to Same Device Capable (Host) */
#define BLE_FLAGS_DUAL_HOST                     0x10

/*!
    \brief Add a Advertising Report filter so that only Advertising Reports
    matching the defined criteria are reported to the VM.

    \param ad_type The ble_ad_type data in the report to filter on.
    \param interval The step size within the report data to check for the
    pattern.
    \param size_pattern The length of the pattern data.
    \param *pattern Pointer to he pattern to look for in the report. The data
    is copied, so if a memory slot is used, the application is responsible for
    freeing it.

    \return TRUE if the filter was added, otherwise FALSE.

    Adding a Filter is an OR operation. If multiple filters are added then if
    any of those filters is satisfied, the advertising report will be sent to
    the VM.

    Filters can be cleared using the ConnectionBleClearAdvertisingReportFilter()
    function.

    Filters should be set  before calling ConnectionBleSetScanEnable() to enable
    scanning.

    The maximum number of filters that can be added is controlled by the PS Key
    PSKEY_BLE_MAX_ADVERT_FILTERS.

    This is a BT4.0 only feature.

*/
bool ConnectionBleAddAdvertisingReportFilter(
            ble_ad_type ad_type,
            uint16 interval,
            uint16 size_pattern,
            const uint8* pattern
            );


/*!
    \brief Clear all Advertising Report Filters.
    \return TRUE if successful, otherwise FALSE.

    Clears all existing Advertising Report Filters that may have been previously
    added using the ConnectionBleAddAdvertisingReportFilter() function.

    This is a BT4.0 only feature.
*/
bool ConnectionBleClearAdvertisingReportFilter(void);

/*!
    \brief Set the data to be put in BLE Advertising Reports sent by this
    device.

    \param size_ad_data The length of the advertising data, maximum is 31
           octets.
    \param ad_data pointer to the advertising data.

    The advertising data is copied. The pointer to the advertising data is not
    freed by this function. A #CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T message will
    be sent to the task that initialised the Connection library to indicate the
    status.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetAdvertisingDataReq(
        uint8 size_ad_data,
        const uint8 *ad_data
        );


/*!
    \brief Sent in response to setting data for the BLE advertising message to
    the task that initialised the Connection library.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Advertising Data status.*/
    connection_lib_status           status;
}
CL_DM_BLE_SET_ADVERTISING_DATA_CFM_T;


/*!
    \brief Enable or disable Bluetooth low energy (BLE) advertising.

    \param enable Enable advertising if TRUE, otherwise advertising is disabled.

    Advertising cannot be enabled if the device is scanning. Data to be
    advertised can be set using ConnectionBleAddAdvertisingReportFilter().

    Initiating a GATT Slave Connection will automatically cause broadcast of
    the set advertising data.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetAdvertiseEnable(bool enable);

/*!
    \brief Enable or disable Bluetooth low energy (BLE) advertising.

    \param theAppTask Task to send the resulting confirmation message to.

    \param enable Enable advertising if TRUE, otherwise advertising is disabled.

    Advertising cannot be enabled if the device is scanning. Data to be
    advertised can be set using ConnectionBleAddAdvertisingReportFilter().

    Initiating a GATT Slave Connection will automatically cause broadcast of
    the set advertising data.

    \return A message of type #CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM is sent to
    the task indicated by the theAppTask parameter.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetAdvertiseEnableReq(Task theAppTask, bool enable);

/*!
    \brief Sent in response to ConnectionDmBleSetAdvertiseEnableReq().

    This is a BT4.0 only message
 */
typedef struct
{
    hci_status status;       /*!> Status of the operation */
} CL_DM_BLE_SET_ADVERTISE_ENABLE_CFM_T;

/*!
    \brief Advertising Event Type.

    This is a BT4.0 only type.
*/
typedef enum
{
    /*! Connectable Undirected Advert. */
    ble_adv_event_connectable_undirected,
    /*! Connectable Directed Advert. */
    ble_adv_event_connectable_directed,
    /*! Discoverable advert. */
    ble_adv_event_discoverable,
    /*! Non-connectable. */
    ble_adv_event_non_connectable,
    /*! Scan Response. */
    ble_adv_event_scan_response,
    /*! Unknown event type.*/
    ble_adv_event_unknown
} ble_advertising_event_type;


/*
    \brief BLE Advertising Reports received that meet the criteria set by the
    BLE Advertising Filters (ConnectionBleAddAdvertisingReportFilter(),
    ConnectionBleClearAdvertisingReportFilter()), when scanning has been
    enabled using ConnectionBleSetScanEnable().

    This message will be received by the task that initialised the Connection
    library.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Number of reports in this ind. */
    uint8                           num_reports;
    /*! What type of advert report has been received. */
    ble_advertising_event_type      event_type;
    /*! Current device address. */
    typed_bdaddr                    current_taddr;
    /*! Permanent device address. */
    typed_bdaddr                    permanent_taddr;
    /*! Received Signal Strength Indication of the advertising message. */
    int8                            rssi;
    /*! Length of advertising data. */
    uint8                           size_advertising_data;
    /*! Advertising data. */
    uint8                           advertising_data[1];
}
CL_DM_BLE_ADVERTISING_REPORT_IND_T;


/*!
    \brief Advertising policy

   Filter policy to filter advertising packets.
*/
typedef enum
{
    /*! Allow scan and connect request from any */
    ble_filter_none = 0x00,

    /*! Allow scan request from white list only, allow any connect */
    ble_filter_scan_only = 0x01,

    /*! Allow any scan, Allow connect from white list only */
    ble_filter_connect_only = 0x02,

    /*! Allow scan and connect request from white list only */
    ble_filter_both = 0x03

} ble_adv_filter_policy;


/*!
    \brief Advertising type

    This is used to determine the packet type that is used for advertising
    when advertising is enabled.
*/
typedef enum
{
    /*! Connectable Undirected Advertising. */
    ble_adv_ind,
    /*! Direct advert - same as high duty */
    ble_adv_direct_ind,
    /*! High duty cycle direct advertising. */
    ble_adv_direct_ind_high_duty,
    /*! Discoverable advertising. */
    ble_adv_scan_ind,
    /*! Non-connectable advertising. */
    ble_adv_nonconn_ind,
    /*! Low duty cycle direct advertising. */
    ble_adv_direct_ind_low_duty
} ble_adv_type;

/*!
    \brief  BLE Directed Advertising Parameters

    This structure contains the Direct Address to advertise through
    when the #ble_adv_type is ble_adv_direct_ind_high_duty.
    NOTE: ble_adv_direct_ind is the same as ble_adv_direct_ind_high_duty,
    which is kept for backwards compatibility.

    If NULL or address is empty then VM will Panic.
*/
typedef struct
{
    /*! FALSE for public remote address and TRUE for random address. */
    bool random_direct_address;

    /*! Public or random address to be connected too. */
    bdaddr direct_addr;

} ble_directed_adv_params_t;

/*!
    \brief  BLE Undirected Advertising Parameters

    This structure contains the Advertising interval max and min range and
    filtering policy to employ. These are used when the #ble_adv_type is
    OTHER than ble_adv_direct_ind.

    For #ble_adv_type
        ble_adv_scan_ind
        ble_adv_nonconn_ind
    the advertising interval max range minimum is 0x00A0. If set less, this
    value shall be used instead.

    If NULL default values (indicated below) will be used.
*/
typedef struct
{
    /*! Minimum advertising interval.
        Range: 0x0020..0x4000
        Default: 0x0800 (1.28s) */
    uint16 adv_interval_min;

    /*! Maximum advertising interval.
        Range: 0x0020..0x4000
        Default: 0x0800 (1.28s) */
    uint16 adv_interval_max;

    /*! Filter policy  - Default: ble_adv_ind */
    ble_adv_filter_policy filter_policy;

} ble_undirected_adv_params_t;


/*!
    \brief  BLE Low Duty Directed Advertising Parameters

    This structure contains the Direct Address to advertise through
    when the #ble_adv_type is ble_adv_direct_ind_low_duty.

    If NULL or address is empty then VM will Panic.
*/
typedef struct
{
    /*! FALSE for public remote address and TRUE for random address. */
    bool random_direct_address;

    /*! Public or random address to be connected  too. */
    bdaddr direct_addr;

    /*! Minimum advertising interval.
        Range: 0x0020..0x4000
        Default: 0x0800 (1.28s) */
    uint16 adv_interval_min;

    /*! Maximum advertising interval.
        Range: 0x0020..0x4000
        Default: 0x0800 (1.28s) */
    uint16 adv_interval_max;

} ble_directed_low_duty_adv_params_t;


/*!
    \brief  Advertising Parameters

    The param structure used depends on the #ble_adv_type.

    For bls_adv_direct_ind, the directed_adv element shall be used. For all
    other #ble_adv_type the undirected_adv element shall be used.
*/
typedef union
{
    /*! Params specific to undirected advertising. */
    ble_undirected_adv_params_t undirect_adv;

    /*! Params specific to high duty directed advertising. */
    ble_directed_adv_params_t   direct_adv;

    /*! params specific to low duty directed advertising. */
    ble_directed_low_duty_adv_params_t low_duty_direct_adv;

} ble_adv_params_t;

/*! Channel Map values. Bit wise OR these values to use one or more channels */
#define  BLE_ADV_CHANNEL_37     0x01
#define  BLE_ADV_CHANNEL_38     0x02
#define  BLE_ADV_CHANNEL_39     0x04
#define  BLE_ADV_CHANNEL_ALL \
                  (BLE_ADV_CHANNEL_37|BLE_ADV_CHANNEL_38|BLE_ADV_CHANNEL_39)

/*!
    \brief Set Advertising parameters for  advertising

    To use a random address for advertising, it is necessary to generate it
    calling ConnectionDmBleConfigureLocalAddressReq() or
    ConnectionDmBleConfigureLocalAddressAutoReq() before this.

    To use the public Bluetooth address after using a random one,
    it is necessary only to use this, setting own_address to OWN_ADDRESS_RANDOM.

    \param adv_type Advertising packet type used for advertising.
    \param own_address See the #OWN_ADDRESS defines for allowed values.
    \param channel_map  Advertising channels to be used. At least one should
    be used. If none are set, all channels shall be used by default.
    \param adv_params undirected or directed advertising specific params. If
    NULL and #ble_adv_type is 'ble_adv_direct_ind' then the CL will Panic.
    For other #ble_adv_type, parameters are validated by BlueStack.

    \return A #CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T message will be received
    indicating if the advertising params has been set successfully, by
    the task that initialised the Connection library.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleSetAdvertisingParamsReq(
        ble_adv_type adv_type,
        uint8 own_address,
        uint8 channel_map,
        const ble_adv_params_t *adv_params
        );


/*!
    \brief Sent in response to setting BLE Advertising parameters with the
    ConnectioniDmBleSetAdvertisingParamsReq() function.

    This message is sent to the task that initialised the Connection library.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Indicates if setting the advertising parameters was successful. */
    connection_lib_status           status;
}
CL_DM_BLE_SET_ADVERTISING_PARAMS_CFM_T;


/*!
    \brief The level of security to be applied to a BLE Connection.

    Authenticated connections are, by default, encrypted.
*/
typedef enum
{
    /*! BLE Connection is encrypted. */
    ble_security_encrypted,
    /*! BLE connection is encrypted and bonded. */
    ble_security_encrypted_bonded,
    /*! BLE connection is to be encrypted and authenticated. */
    ble_security_authenticated,
    /*! BLE connection is to be encrypted, authenticated and bonded. */
    ble_security_authenticated_bonded,
    /*! BLE connection encryption is to be refreshed. */
    ble_security_refresh_encryption,
    /*! BLE Security last - should not be used. */
    ble_security_last
} ble_security_type;

/*!
    \brief The BLE connection type.

    Similar to the #gatt_connection_type but the BREDR Master connection
    type is not in context here.
*/
typedef enum
{
    /*! BLE Master Directed. */
    ble_connection_master_directed          = 0x01,
    /*! BLE Master Whitelist. */
    ble_connection_master_whitelist         = 0x02,
    /*! BLE Slave Directed. */
    ble_connection_slave_directed           = 0x03,
    /*! BLE Slave Whitelist. */
    ble_connection_slave_whitelist          = 0x04,
    /*! BLE Slave Undirected.*/
    ble_connection_slave_undirected         = 0x05,
    /*! BLE Connection last - should not be used. */
    ble_connection_last
} ble_connection_type;

/*!
    \brief Start security for a Bluetooth low energy (BLE) connection.

    \param theAppTask The client task.
    \param *taddr Pointer to the address of the remote device.
    \param security The required security (ble_security_type).
    \param conn_type The type of BLE Connection (ble_connection_type).

    This will initiate Pairing and General Bonding for an existing LE
    connection.

    If bonding is required, then this must also be set as a parameter during the
    IO Capability exchange (see the ConnectionSmIoCapabilityResponse()
    function).

    LE Secure Connections are enabled by default, regardless of the Connection
    Library Initialisation options, which are for BR/EDR Secure Connections
    (see the ConnectionInitEx3() function and CONNLIB_OPTIONS_*).

    A CL_DM_BLE_SECURITY_CFM message will be received indicating the outcome.
*/
void ConnectionDmBleSecurityReq(
        Task                    theAppTask,
        const typed_bdaddr      *taddr,
        ble_security_type       security,
        ble_connection_type     conn_type
        );

/*!
    \brief The BLE Security confirm status.

    Status returned to security request from app.
*/
typedef enum
{
    /*! BLE Security cfm success. */
    ble_security_success                          = 0x00,
    /*! BLE Security cfm pairing in progress. */
    ble_security_pairing_in_progress              = 0x01,
    /*! BLE Security cfm link key missing */
    ble_security_link_key_missing                 = 0x02,
    /*! BLE Security cfm failed. */
    ble_security_fail                             = 0x03
}ble_security_status;

/*!
    \brief Returned in response to the ConnectionBleDmSecurityReq() function.

    Indicates if the specified security was successfully set.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Security cfm status.*/
    ble_security_status           status;
    /*! The remote device address. */
    typed_bdaddr                    taddr;
} CL_DM_BLE_SECURITY_CFM_T;


/*!
    \brief Bluetooth low energy link key distribution flags.

    CSRK (Signing) keys are not yet supported.

    This is BT4.0 only .
*/
/*! \{ */
/*! No keys - no bonding, only STK is used. */
#define KEY_DIST_NONE                   (0x0000)
/*! Responder distributes LTK, EDIV and RAND to the Initiator. */
#define KEY_DIST_RESPONDER_ENC_CENTRAL  (0x0100)
/*! Initiator distributes LTK, EDIV and RAND to the Responder. */
#define KEY_DIST_INITIATOR_ENC_CENTRAL  (0x0001)
/*! Responder distributes the IRK to the Initiator. */
#define KEY_DIST_RESPONDER_ID           (0x0200)
/*! Initiator distributes the IRK to the Responder. */
#define KEY_DIST_INITIATOR_ID           (0x0002)
/*! \} */


/*!
    \brief Bluetooth Low Energy connection and advertisment configuration
    parameters
*/
typedef struct
{
    /*! \brief LE scan interval

    The ttime interval from when the Controller started its last LE scan
    until it begins the subsequent LE scan.

    Scan interval in units of 0.625 ms. The allowed range is between
    0x0004 (2.5 ms) and 0x4000 (10240 ms). */
    uint16 scan_interval;
    /*! \brief LE scan window

    Amount of time for the duration of the LE scan. LE Scan Window shall be
    less than or equal to LE Scan Interval.

    Scan window in units of 0.625 ms. The allowed range is between
    0x0004 (2.5 ms) and 0x4000 (10.240 s). */
    uint16 scan_window;
    /*! \brief Minimum value for the connection event interval.

    This shall be less than or equal to Conn Interval Max.

    Connection interval in units of 1.25 ms. The allowed range is between
    0x0006 (7.5 ms) and 0x0c80 (4 s). */
    uint16 conn_interval_min;
    /*! \brief Maximum value for the connection event interval.

    This shall be greater than or equal to Conn Interval Min.

    Connection interval in units of 1.25 ms. The allowed range is between
    0x0006 (7.5 ms) and 0x0c80 (4 s). */
    uint16 conn_interval_max;
    /*! \brief Slave latency for the connection in number of connection events.

    The allowed range is between 0x0000 and 0x01f4. */
    uint16 conn_latency;
    /*! \brief Supervision timeout for the LE Link

    Supervision timeout in units of 10 ms. The allowed range is between
    0x000a (100 ms) and 0x0c80 (32 s). */
    uint16 supervision_timeout;
    /*! \brief LE connection attempt timeout

    Equivalent of Page Timeout in BR/EDR. */
    uint16 conn_attempt_timeout;
    /*! \brief Minimum advertising interval for non-directed advertising.

    The maximum allowed slave latency that is accepted if slave requests
    connection parameter update once connected. */
    uint16 conn_latency_max;
    /*! \brief Minimum allowed supervision timeout

    The minimum allowed supervision timeout that is accepted if slave requests
    connection parameter update once connected. */
    uint16 supervision_timeout_min;
    /*! \brief Maximum allowed supervision timeout

    The maximum allowed supervision timeout that is accepted if slave requests
    connection parameter update once connected. */
    uint16 supervision_timeout_max;
    /*! \brief Own Address type used  in LE connnect requests by the device.

    See #OWN_ADDRESS defines for allowed values. */
    uint8 own_address_type;
} ble_connection_params;

/*!
    \brief Set default Bluetooth Low Energy connection and advertising
    parameters

    To use a random address for connections, it is necessary to generate it
    calling ConnectionDmBleConfigureLocalAddressReq() or
    ConnectionDmBleConfigureLocalAddressAutoReq() before this.

    To use the public Bluetooth address after using a random one,
    it is necessary only to use this, setting the own_address_type parameter of
    ble_connection_params with #TYPED_BDADDR_PUBLIC.

    \param params The connection and advertising default parameters.

    This will set the default values for Bluetooth Low Energy connections
    establishment and advertising.

    This is a BT4.0 only feature.

    \return Message \link CL_DM_BLE_SET_CONNECTION_PARAMETERS_CFM_T
    CL_DM_BLE_SET_CONNECTION_PARAMETERS_CFM\endlink is sent
    to the client task when operation has finished.
*/
void ConnectionDmBleSetConnectionParametersReq(
    const ble_connection_params *params);

/*!
    \brief Sent in response to ConnectionDmBleSetConnectionParametersReq().

    This is a BT4.0 only message.
*/
typedef struct
{
    connection_lib_status           status;     /*!< Status of the operation */
} CL_DM_BLE_SET_CONNECTION_PARAMETERS_CFM_T;

/*!
    \brief Request to update the BLE connection parameters.

    \param theAppTask The client task.
    \param *taddr Pointer to the address of the master.
    \param min_interval Minimum requested connection interval.
    \param max_interval Maximum requested connection interval.
    \param latency Slave latency.
    \param timeout Supervision timeout.
    \param min_ce_length Minimum length of connection.
    \param max_ce_length Maximum length of connection.

    The Connection Parameter Update Request allows the LE peripheral to
    request a set of new connection parameters. The LE central device may
    reject the request.

    If the requesting device is Central then the connection changes will be
    carried out locally.

    For more information about the parameters see documentation for
    ble_connection_params.

    This is a BT4.0 only feature.

    \return Message \link CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM_T
    CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM\endlink is sent
    to the client task when operation has finished.
*/
void ConnectionDmBleConnectionParametersUpdateReq(
        Task theAppTask,
        typed_bdaddr *taddr,
        uint16 min_interval,
        uint16 max_interval,
        uint16 latency,
        uint16 timeout,
        uint16 min_ce_length,
        uint16 max_ce_length
        );

/*!
    \brief Sent in response to ConnectionDmBleConnectionParametersUpdateReq().

    This is a BT4.0 only message.
*/
typedef struct
{
    typed_bdaddr            taddr;  /*!< The address of the master. */
    connection_lib_status   status; /*!< Status of the operation */
} CL_DM_BLE_CONNECTION_PARAMETERS_UPDATE_CFM_T;


/*!
    \brief Read the total size of the BLE device White List.

    This is a BT4.0 only feature.

    \return Message #CL_DM_BLE_READ_WHITE_LIST_SIZE_CFM_T is sent in response.
*/
void ConnectionDmBleReadWhiteListSizeReq(void);

/*!
    \brief Sent in response to ConnectionDmBleReadWhitListSizeReq().

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Status of request. */
    connection_lib_status           status;
    /*! Total size of entries that can be stored in the controller. */
    uint8                           white_list_size;
} CL_DM_BLE_READ_WHITE_LIST_SIZE_CFM_T;


/*!
    \brief Clear the BLE Device White List.

    Clears the White List of devices stored in the controller. This command
    will fail in the following scenarios:
    - Advertising is enabled and the advertising filter policy uses the white
      List.
    - Scanning is enables and the scanning filter policy uses the white list.
    - The initiator filter policy uses the white list and a BLE Connection is
      being created.

    This is a BT4.0 only feature.

    \return Message #CL_DM_BLE_CLEAR_WHITE_LIST_CFM_T is sent in response.
*/
void ConnectionDmBleClearWhiteListReq(void);

/*!
    \brief Sent in response to ConnectionDmBleClearWhiteListReq().

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Status of request. */
    connection_lib_status       status;
} CL_DM_BLE_CLEAR_WHITE_LIST_CFM_T;


/*!
    \brief Add a device to the BLE White List.

    Add a single device to the BLE White list stored in the controller.
    This command will fail in the following scenarios:
    - Advertising is enabled and the advertising filter policy uses the white
      List.
    - Scanning is enables and the scanning filter policy uses the white list.
    - The initiator filter policy uses the white list and a BLE Connection is
      being created.

    \param bd_addr_type Device address type, either #TYPED_BDADDR_PUBLIC or
    #TYPED_BDADDR_RANDOM (bdaddr_.h).
    \param *bd_addr The device bluetooth address.

    This is a BT4.0 only feature.

    \return Message #CL_DM_BLE_ADD_DEVICE_TO_WHITE_LIST_CFM_T is sent in
    response.
*/
void ConnectionDmBleAddDeviceToWhiteListReq(
        uint8 bd_addr_type,
        const bdaddr *bd_addr
        );

/*!
    \brief Sent in response to ConnectionDmBleAddDeviceToWhiteListReq().

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Status of request. */
    connection_lib_status       status;
} CL_DM_BLE_ADD_DEVICE_TO_WHITE_LIST_CFM_T;

/*!
    \brief Remove a device from the BLE White List.

    Remove a single device from the BLE White list stored in the controller.
    This command will fail in the following scenarios:
    - Advertising is enabled and the advertising filter policy uses the white
      List.
    - Scanning is enables and the scanning filter policy uses the white list.
    - The initiator filter policy uses the white list and a BLE Connection is
      being created.

    \param bd_addr_type Device address type, either TYPED_BDADDR_PUBLIC or
    TYPED_BDADDR_RANDOM (bdaddr_.h).
    \param *bd_addr The device bluetooth address.

    This is a BT4.0 only feature.

    \return Message #CL_DM_BLE_REMOVE_DEVICE_FROM_WHITE_LIST_CFM_T is sent in
    response.
*/
void ConnectionDmBleRemoveDeviceFromWhiteListReq(
        uint8 bd_addr_type,
        const bdaddr *bd_addr
        );

/*!
    \brief Sent in response to ConnectionDmBleRemoveDeviceToWhiteListReq().

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Status of request. */
    connection_lib_status       status;
} CL_DM_BLE_REMOVE_DEVICE_FROM_WHITE_LIST_CFM_T;


/*!
    \brief Add devices in the Trusted Device List to the low energy white-
    list.

    Devices in the non-volatile Trusted Device List (TDL) will be added to
    the Bluetooth low energy (BLE) white list.

    \param ble_only_devices If TRUE, then only devices with a BLE link key type
    will be added to the white-list. If FALSE, then all devices in the TDL will
    be added to the white-list.

    This is a BT4.0 only feature.

    \return For each device added to the white-list, the task which initialised
    the Connection library will receive a
    #CL_DM_BLE_ADD_DEVICE_TO_WHITE_LIST_CFM_T message.
*/
void ConnectionDmBleAddTdlDevicesToWhiteListReq(bool ble_only_devices);

/*!
    \brief Sent in to indicate that a Secure Simple Pairing procedure has
    completed.

    This message is only sent for a BLE link.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Status of Pairing procedure. */
    connection_lib_status       status;
    /*! Address of the remote Bluetooth Device. */
    tp_bdaddr                   tpaddr;
    /*! Flags. */
    uint16                      flags;
    /*! The remote device permanent address when using a Resolvable Random
        address.
     */
    typed_bdaddr                permanent_taddr; /* TODO: I will get to this. */
} CL_SM_BLE_SIMPLE_PAIRING_COMPLETE_IND_T;

/*!
    \brief Check the link keys stored in the Paired Device List, to see if
    an IRK key has been stored, indicating that this device has bonded to
    another that is using the BLE Privacy feature.

    The IRK key is used for BLE Privacy. If this key has been stored, then
    it indicates that at least one bonded device is using the BLE Privacy
    feature. This may change the App behaviour in regard to whether it uses
    a whitelist to filter connection adverts from peripheral devices.

    This is only applicable to BLE connections.

    This is a BT4.0 only feature.

*/
bool ConnectionBondedToPrivacyEnabledDevice(void);

/*!
    \brief Defaults for BLE Resolvable Private Address regeneration timer.
*/
/*! \{ */
/*! Minimum RPA TGAP Time out - 1 s  */
#define BLE_RPA_TIMEOUT_MINIMUM     (0x0001)
/*! Default RPA TGAP Time out - 900 s = 15 m */
#define BLE_RPA_TIMEOUT_DEFAULT     (0x0384)
/*! Maximum RPA TGAP Time out - 41400 s = ~11.5 h */
#define BLE_RPA_TIMEOUT_MAXIMUM     (0xA1B8)
/*! \} */

/*!
    \brief Permanent Address Type to configure

    Used with the ConnectionDmBleConfigureLocalAddressReq() function,
    as well as ConnectionDmBleExtAdvSetRandomAddressReq().

    Only the values #ble_local_addr_write_static, 
    #ble_local_addr_generate_static, #ble_local_addr_generate_non_resolvable,
    and #ble_local_addr_generate_resolvable are accepted for 
    ConnectionDmBleConfigureLocalAddressReq().

    All values may be used with 
    ConnectionDmBleExtAdvSetRandomAddressReq()
*/
typedef enum
{
    /*! Use the specified static address. */
    ble_local_addr_write_static,
    /*! Generate a static address. */
    ble_local_addr_generate_static,
    /*! Generate non-resolvable address. */
    ble_local_addr_generate_non_resolvable,
    /*! Generate a resolvable address. */
    ble_local_addr_generate_resolvable,

    /*! Use the specified non-resolvable address
        (ConnectionDmBleExtAdvSetRandomAddressReq() only). */
    ble_local_addr_write_non_resolvable,
    /*! Use the specified resolvable address
        (ConnectionDmBleExtAdvSetRandomAddressReq() only). */
    ble_local_addr_write_resolvable,
    /*! Use the same resolvable address. as generated for 
        legacy advertising. 
        (ConnectionDmBleExtAdvSetRandomAddressReq() only). */
    ble_local_addr_use_global,

    /*! Always the last enum in this type - do not use. */
    ble_local_addr_last
} ble_local_addr_type;

/*!
    \brief Configure the local address to be used in connections.

    If the option to generate an address is used, the address generated by
    Bluestack will be returned in the
    #CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM_T message returned in response.

    If the ble_local_addr_type is \link ble_local_addr_generate_resolvable
    \endlink then a Tgap Timeout of #BLE_RPA_TIMEOUT_DEFAULT (15 m) will be
    used to regenerate the RPA. The timer will only start when the generated
    RPA is first used for advertising or scanning.

    This has to be used to generate a random address for use with BLE.
    To configure the generated address after this, it is necessary to use
    ConnectionDmBleSetConnectionParametersReq() for connections,
    ConnectionDmBleSetScanParametersReq() for scanning and
    ConnectionDmBleSetAdvertisingParamsReq() for advertising.

    \param addr_type The address type to configure see the
    \link ble_local_addr_type \endlink.

    \param static_taddr The address to use, when the option \link
    ble_local_addr_write_static \endlink is used. Setting this parameter to
    0 will pass a null address.

    This is only applicable to BLE connections.

    This is a BT4.0 only feature.

*/
void ConnectionDmBleConfigureLocalAddressReq(
        ble_local_addr_type     addr_type,
        const typed_bdaddr*     static_taddr
        );

/*!
    \brief Configure the local address to be used in connections with a timer
    to regenerate Resolvable Private Addresses (RPA), if that type is selected.

    If the option to generate an address is used, the address generated by
    Bluestack will be returned in the
    #CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM_T message returned in response.

    If the ble_local_addr_type is \link ble_local_addr_generate_resolvable
    \endlink then a Tgap Timeout will be used to regenerate the RPA. The timer
    will only start when the generated RPA is first used for advertising,
    scanning or connection.

    This has to be used to generate a random address for use with BLE.
    To configure the generated address after this, it is necessary to use
    ConnectionDmBleSetConnectionParametersReq() for connections,
    ConnectionDmBleSetScanParametersReq() for scanning and
    ConnectionDmBleSetAdvertisingParamsReq() for advertising.

    \param addr_type The address type to configure see the
    \link ble_local_addr_type \endlink.

    \param static_taddr The address to use, when the option \link
    ble_local_addr_write_static \endlink is used. Setting this parameter to
    0 will pass a null address.

    \param rpa_tgap_timeout The time, in seconds, after which an RPA will be
    regenerated, if that address type is requested. The timeout must be in the
    range #BLE_RPA_TIMEOUT_MINIMUM (30 s) to #BLE_RPA_TIMEOUT_MAXIMUM (~11.5 h).

    This is only applicable to BLE connections.
*/

void ConnectionDmBleConfigureLocalAddressAutoReq(
        ble_local_addr_type     addr_type,
        const typed_bdaddr*     static_taddr,
        uint16                  rpa_tgap_timeout
        );

/*!
    \brief Sent in response to ConnectionDmBleConfigureLocalAddressReq().

    If the status indicates success then the 'random_taddr' field is the
    device address that will be used for BLE connections.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! Status of configuring the local device address. */
    connection_lib_status       status;
    /*! local address type that has been configured. */
    ble_local_addr_type         addr_type;
    /*! The random address that will be used (if status is 'success') */
    typed_bdaddr                random_taddr;
} CL_DM_BLE_CONFIGURE_LOCAL_ADDRESS_CFM_T;

/*!
    \brief Flags used to identify whether it is the local or peer device
    random address to read when calling the
    ConnectionSmBleReadRandomAddressReq() function.

    These enum values represent bit flags.
*/
typedef enum {
    /*! Read the Random Address configured for the local device. */
    ble_read_random_address_local = 0x0001,
    /*! Read the Random Address of a peer device. */
    ble_read_random_address_peer  = 0x0002
} ble_read_random_address_flags;

/*!
    \brief Read Random Address of either the local device or a peer device.

    \param flags Indicate whether it is the local device or a peer device,
    that the Read request is for. See #ble_read_random_address_flags.

    \param peer_tpaddr Transport typed Bluetooth Device Address of the Peer
    device. If the request is to read the Local device's Random address, this
    will be ignored.

    \return A message of type #CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T
    is sent to the  task that initialised the Connection library when operation
    has completed.

*/
void ConnectionSmBleReadRandomAddressReq(
    ble_read_random_address_flags   flags,
    const tp_bdaddr                 *peer_tpaddr
    );

/*!
    \brief Read Random Address of either the local device or a peer device.

    \param theAppTask The client task.

    \param flags Indicate whether it is the local device or a peer device,
    that the Read request is for. See #ble_read_random_address_flags.

    \param peer_tpaddr Transport typed Bluetooth Device Address of the Peer
    device. If the request is to read the Local device's Random address, this
    will be ignored.

    \return A message of type #CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T
    is sent to the specified client task.
*/
void ConnectionSmBleReadRandomAddressTaskReq(
    Task                            theAppTask,
    ble_read_random_address_flags   flags,
    const tp_bdaddr                 *peer_tpaddr
    );

/*!
    \brief Received in response to ConnectionSmBleReadRandomAddressReq().
*/
typedef struct
{
    /*! Result status of the request */
    connection_lib_status           status;
    /*! Peer device address, as provided in the original request. */
    tp_bdaddr                       peer_tpaddr;
    /*! Flags from the request, indicating if it was for local or peer device.*/
    ble_read_random_address_flags   flags;
    /*! Local or peer device Random Address, depending on request flags. */
    tp_bdaddr                       random_tpaddr;
} CL_SM_BLE_READ_RANDOM_ADDRESS_CFM_T;

/*!
    \brief Sent in when a BLE Update to connection parameters is sent from
    a remote device.

    The application must respond to accept (or reject) the update to connection
    parameters using the ConnectionDmBleAcceptConnectionParUpdateResponse()
    function.

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! The remote device address. */
    typed_bdaddr    taddr;
    /*! L2CAP signal identifier of the connection. */
    uint16          id;
    /*! The minimum allowed connection interval. */
    uint16          conn_interval_min;
    /*! The maximum allowed connection interval. */
    uint16          conn_interval_max;
    /*! The connection slave latency. */
    uint16          conn_latency;
    /*! Link supervision time out. */
    uint16          supervision_timeout;
} CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T;

/*!
    \brief Sent when a BLE Connection Update command was issued
    by the Host or if the connection parameters are updated following a request
    from the peer device. If no parameters are updated following a request from
    the peer device then this event shall not be issued

    This is a BT4.0 only message.
*/
typedef struct
{
    /*! The remote device address. */
    typed_bdaddr    taddr;
    /*! L2CAP signal identifier of the connection. */
    uint16          id;
    /*! The status of request. */
    uint16          status;
    /*! The negotiated connection interval. */
    uint16          conn_interval;
    /*! The connection slave latency. */
    uint16          conn_latency;
    /*! Link supervision time out. */
    uint16          supervision_timeout;
} CL_DM_BLE_CONNECTION_UPDATE_COMPLETE_IND_T;

/*!
    \brief Used to accept (or reject) an update to the parameters of a
    BLE connection from a remote device.

    This function should be called in response to receiving a \link
    CL_DM_BLE_ACCEPT_CONNECTION_PAR_UPDATE_IND_T \endlink message.

    \param accept_update TRUE to accept the parameter updates.
    \param taddr The remote device address.
    \param id The L2CAP signal identifier as indicated in the IND message.
    \param conn_interval_min The minimum allowed connection interval.
    \param conn_interval_max The maximum allowed connection interval.
    \param conn_latency The connection slave latency.
    \param supervision_timeout Link supervision time out.

    This is only applicable to BLE connections.

    This is a BT4.0 only feature.
*/
void ConnectionDmBleAcceptConnectionParUpdateResponse(
        bool                accept_update,
        const typed_bdaddr  *taddr,
        uint16              id,
        uint16              conn_interval_min,
        uint16              conn_interval_max,
        uint16              conn_latency,
        uint16              supervision_timeout
        );

/*!
    \brief Indication received when BLE Advertising parameters have been
    updated.

    NOTE: The application should only consider parameters relevant to the
    advertising type and ignore the others.

    This is a BT4.2 only message.
*/
typedef struct
{
    /*! Minimum advertising interval.  Range: 0x0020..0x4000,
        Default: 0x0800 (1.28s) */
    uint16          adv_interval_min;
    /*! Maximum advertising interval.  Range: 0x0020..0x4000,
        Default: 0x0800 (1.28s) */
    uint16          adv_interval_max;
    /*! Advertising type. */
    ble_adv_type    advertising_type;
    /*! Own address type: #TYPED_BDADDR_PUBLIC or #TYPED_BDADDR_RANDOM. */
    uint8           own_address_type;
    /*! Direct address type: #TYPED_BDADDR_PUBLIC or #TYPED_BDADDR_RANDOM. */
    uint8           direct_address_type;
    /*! Directed advertising Bluetooth device address. */
    bdaddr          direct_bd_addr;
    /*! Advertising channel map. */
    uint8           advertising_channel_map;
    /*! Advertising filter policy. */
    uint8           advertising_filter_policy;
} CL_DM_BLE_ADVERTISING_PARAM_UPDATE_IND_T;

/****************************************************************************
NAME
    ConnectionDmBleCheckTdlDeviceAvailable

FUNCTION
    Check in the TDL if any LE device is paired
RETURNS

*/
bool ConnectionDmBleCheckTdlDeviceAvailable(void);

/*!
    @brief Send to the task that initialised the connection library to confirm
    that the slave latency has changed.
*/
typedef struct
{
    /*! The remote device address. */
    tp_bdaddr       taddr;
    /*! Request was to Enable/disable zero LE slave latency */
    bool            zero_latency;
    /*! HCI status code. Success or failure*/
    hci_status      status;
} CL_DM_ULP_ENABLE_ZERO_SLAVE_LATENCY_CFM_T;

/*!
    \brief Enable or disable BLE slave latency

    \param taddr The remote device address.
    \param zero_latency Request zero latency if TRUE, otherwise disabled.

    Send a Zero Slave Latency request

    This is a BT4.0 only feature.
*/
void ConnectionDmUlpEnableZeroSlaveLatency(
        const tp_bdaddr  *tpaddr,
        bool  zero_latency);

/*! \brief Bluetooth Low Energy Channel Selection Algorithm employed for
    a connection.

    Values 0x02 to 0xFF are reserved for future use.
*/
typedef enum
{
    /*! BLE Channel Selection Algorithm #1. */
    channel_selection_algorithm_1 = 0x00,
    /*! BLE Channel Selection Algorithm #2. */
    channel_selection_algorithm_2 = 0x01,
    /*! Invalid value indicated from Bluestack. */
    channel_selection_algorithm_invalid
} channel_selection_algorithm_t;


/*!
    \brief Indicates which BLE Channel Selection Algorithm has been used
    by the link controller.

    This indication should follow the CL_DM_ACL_OPENED_IND indication message.
    The application may use this information to enable specific services, such
    as isochronous services.
*/
typedef struct
{
    /*! The remote device address. */
    tp_bdaddr                       tpaddr;
    /*! The channel selection algorithm used in connection. */
    channel_selection_algorithm_t   selected_algorithm;
} CL_DM_BLE_CHANNEL_SELECTION_ALGORITHM_IND_T;


/*!
    \brief Set PHY preferences for a given LE connection.

    \param tpaddr The remote device address.
    \param min_tx Minimum preferred tx rate
    \param max_tx Maximum preferred tx rate
    \param min_rx Minimum preferred rx rate
    \param max_rx Minimum preferred rx rate
    \param flags  Reserved for future use, set to zero

    Actual PHY parameters selected will be indicated in CM_DM_ULP_SET_PHY_CFM
*/
void ConnectionDmUlpSetPhy(
        const tp_bdaddr  *tpaddr,
        const uint8 min_tx,
        const uint8 max_tx,
        const uint8 min_rx,
        const uint8 max_rx,
        const uint8 flags
        );

/*!
    \brief Set default PHY preferences for all upcoming
    connections after this request is processed successfully

    \param min_tx Minimum preferred tx rate
    \param max_tx Maximum preferred tx rate
    \param min_rx Minimum preferred rx rate
    \param max_rx Minimum preferred rx rate
    \param flags  Reserved for future use, set to zero
*/
void ConnectionDmUlpSetDefaultPhy(
        const uint8 min_tx,
        const uint8 max_tx,
        const uint8 min_rx,
        const uint8 max_rx,
        const uint8 flags
        );

/*! \brief Valid PHY rate values that can be used depending on an
    application's PHY preferences for a connection or as default
    preference
*/
typedef enum
{
    phy_rate_min    = 0x00,
    /* Coded PHY, S=8 */
    phy_rate_125K   = 0x01,
    /* Coded PHY, S=2 */
    phy_rate_500K   = 0x02,
    /* Uncoded PHY */
    phy_rate_1M     = 0x03,
    /* Uncoded PHY */
    phy_rate_2M     = 0x04,
    phy_rate_MAX    = 0xff
} phy_rate;


/*! \brief Valid PHY type values that are used to indicate
    PHY type of a link.
 */
typedef enum
{
    /* Uncoded PHY */
    phy_type_1M    = 0x01,
    /* Uncoded PHY */
    phy_type_2M    = 0x02,
    /* Coded PHY */
    phy_type_coded = 0x03
} phy_type;


/*!
    \brief Indicates a change in PHY of a connection

*/
typedef struct
{
    /*! The remote device address. */
    tp_bdaddr       tpaddr;
    /*! updated TX PHY type of the connection */
    phy_type        tx_phy_type;
    /*! updated RX PHY type of the connection */
    phy_type        rx_phy_type;
} CL_DM_ULP_PHY_UPDATE_IND_T;



/*!
    \brief Indicates a confirmation of a PHY change

*/
typedef struct
{
    /*! The remote device address. */
    tp_bdaddr       tpaddr;
    /*! updated TX PHY type of the connection */
    phy_type        tx_phy_type;
    /*! updated RX PHY type of the connection */
    phy_type        rx_phy_type;
    /*! hci status, non zero is failure */
    hci_status      status;
} CL_DM_ULP_SET_PHY_CFM_T;

/*!
    \brief Indicates a confirmation of a default
    PHY change

*/
typedef struct
{
    /*! hci status, non zero is failure */
    hci_status      status;
} CL_DM_ULP_SET_DEFAULT_PHY_CFM_T;

/*! \brief Privacy mode type values that are used to indicate
    privacy mode type of a link.
 */
typedef enum
{
    /* Network Privacy Mode - default */
    privacy_mode_network = 0x00,
    /* Device Privacy Mode */
    privacy_mode_device = 0x01,
    /*! Privacy Mode last - should not be used. */
    privacy_mode_last
} privacy_mode;

/*!
    \brief Set privacy mode for a given LE connection.

    \param peer_taddr The remote device address.
    \param mode Selected Privacy Mode

    A #CL_DM_ULP_SET_PRIVACY_MODE_CFM_T message will be sent
    in response.
*/
void ConnectionDmUlpSetPrivacyModeReq(
        const typed_bdaddr  *peer_taddr,
        const privacy_mode mode
        );

/*!
    \brief Indicates a confirmation of a privacy
    mode change
*/
typedef struct
{
    /*! hci status, non zero is failure */
    hci_status  status;
} CL_DM_ULP_SET_PRIVACY_MODE_CFM_T;

/*!
    \brief Request to read the BLE Advertising channel transmit power

    \param theAppTask The client task.

    A #CL_DM_BLE_READ_ADVERTISING_CHANNEL_TX_POWER_CFM_T message will be sent
    in response.
*/
void ConnectionDmBleReadAdvertisingChannelTxPower(Task theAppTask);

/*!
    \brief The BLE Advertising Channel Transmit Power, as requested.
*/
typedef struct
{
    /*! Advetising Channel Transmit power. */
    int8 tx_power;
    /*! hci status code. */
    hci_status      status;
} CL_DM_BLE_READ_ADVERTISING_CHANNEL_TX_POWER_CFM_T;

/*!
    \brief Received in response to ConnectionBleTransmitterTest().
*/
typedef struct
{
    hci_status      status;
} CL_DM_BLE_TRANSMITTER_TEST_CFM_T;

/*!
    \brief Received in response to ConnectionBleReceiverTest().
*/
typedef struct
{
    hci_status      status;
} CL_DM_BLE_RECEIVER_TEST_CFM_T;

/*!
    \brief Received in response to ConnectionBleTestEnd().
*/
typedef struct
{
    hci_status      status;
    uint16          number_of_rx_packets;
} CL_DM_BLE_TEST_END_CFM_T;

/*!
  \brief Received if Selective Cross Transport Key Derivation has been
  enabled, during pairing where the link key for the other transport could
  be derived. See #CONNLIB_OPTIONS_SELECTIVE_CTKD.

  The application must respond using the
  ConnectionSmGenerateCrossTransKeyRequestResponse() function.
*/
typedef struct
{
    /*! The remote device address. */
    tp_bdaddr   tpaddr;
    /*! Unique connection identifier to be returned in the response. */
    uint8       identifier;
    /*! Reserved for future use. */
    uint16      flags;
} CL_SM_GENERATE_CROSS_TRANS_KEY_REQUEST_IND_T;

/*!
    \brief Cross Transport Key flags type

    Used for the flags parameter in the
    ConnectionSmgenerateCrossTransKeyRequestRespons() function.
*/
typedef enum {
    /*! Disable Cross Transport Key Derivation for this device connection. */
    cross_trans_key_disable,
    /*! Enable Cross Transport Key Derivation for this device conneciton. */
    cross_trans_key_enable,

    /*! Always the last enumeration. */
    cross_trans_key_last
} ctk_flags_type;

/*!
    \brief ConnectionSmGenerateCrossTransKeyRequestResponse

    Used to respond to the #CL_SM_GENREATE_CROS_TRANS_KEY_REQUEST_IND message.

    \param tpaddr The peer device address from the IND message.
    \param identifier Connection identifier from the IND message.
    \param flags Flags defined in the #ctk_flags_type.
 */
void ConnectionSmGenerateCrossTransKeyRequestResponse(
        const tp_bdaddr *tpaddr,
        uint8           identifier,
        ctk_flags_type  flags
        );


/*!
    \brief ConnectionSmSirkOperationReq

    Used for SIRK encryption or decryption procedure.

    \param theAppTask Task to send the resulting confirmation message to.
    \param tpaddr The peer device address from the IND message.
    \param flags reserved for future use.
    \param key key to be encrypted or decrypted.
 */
void ConnectionSmSirkOperationReq(
    Task theAppTask,
    const tp_bdaddr *tpaddr,
    uint16 flags,
    uint8 sirk_key[CL_SM_SIRK_KEY_LEN]
    );


/*****************************************************************************
 *             Extended Advertising/Scanning prims/APIs                      *
 *****************************************************************************/

/*!
    \brief Supplies information on what APIs are available and size limitations.

           A message of type CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM is returned
           to the task indicated by the theAppTask parameter.

    \param theAppTask Task to send the resulting confirmation message to.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleGetAdvScanCapabilitiesReq(Task theAppTask);

/*!
    \brief  Sent in response to requesting the advertising and scanning
            capabilities using the ConnectionDmBleGetAdvScanCapabilitiesReq()
            function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! 0 = success, else error. */
    connection_lib_status    status;
    /*! What AE subsystem APIs are allowed to be used. If bit set then API can be used.
     *  bit 0 - LE Legacy Advertising/Scanning API
     *  bit 1 - LE ExtAdv/ExtScan API
     *  bit 2 - LE PerAadv/PerScan API
     *
     *  Note) These are set to reflect what the controller can support and what
     *  LEGACY_ADV_SCAN_API_CONFIG has configured.
     *
     *  Note) MIB key legacy_hci_only can be forced to use legacy at build time
     *  via the build define AE_USES_LEGACY_HCI. */
    uint8         available_api;
    /*! Number of advertising sets available to be used by application/profiles.
     *  0 or 4
     *  Note) If legacy controller then this will be 0.The number can vary at
     *  anytime due to resource limitations.  This is unlikely. */
    uint8         available_adv_sets;
    /*! Number of advertising sets available to be used by upper stack. 0 or 1
     *
     *  Note) This is resource reserved to allow the stack to be able to
     *  advertise. It will only be used if instigated by another feature being
     *  enabled by the application (e.g. GAM). */
    uint8         stack_reserved_adv_sets;
    /*! The maximum number of periodic train sync records allowed to be stored
     *  by a periodic scanner. 0 to 3
     *  Note) This information is used to allow a Periodic Scanner to sync on
     *  to 1 of the periodic trains. Refer to ConnectionDmBlePeriodicScanSyncTrainReq.
     *  The number can vary at any time due to resource limitations. This is unlikely. */
    uint8         max_periodic_sync_list_size;
    /*! Phys available to be used. If bit set then the phy is usable.
     *  bit 0 = LE 1M
     *  bit 1 = LE 2M
     *  bit 2 = LE Coded */
    uint16        supported_phys;
    /*! The potential max amount of advertising data or scan response data that
     *  can be advertised. 31 to 251 octets
     *
     *  Note) This could be reduced depending on how an advertising set is
     *  configured (e.g. allow stack to use some of the space).
     *
     *  Note) If legacy controller then this will be 31. */
    uint16        max_potential_size_of_tx_adv_data;
    /*! The potential max amount of periodic advertising data that can be
     *  advertised. This could be reduced depending on how an advertising set is
     *  configured (e.g. allow stack to use some of the space). 0 to 252 octets */
    uint16        max_potential_size_of_tx_periodic_adv_data;
    /*! The potential max amount of advertising data or scan response data that
     *  can be processed by a scanner. Any advertising data larger than this
     *  will be thrown away. 31 to 1650 octets
     *
     *  Note) If legacy controller then this will be 31. */
    uint16        max_potential_size_of_rx_adv_data;
    /*! The potential max amount  of periodic advertising data that can be
     *  processed on the receive side of a periodic advertising train. Any
     *  periodic advertising data larger than this will be thrown away.
     *  0 to 1650 octets */
    uint16        max_potential_size_of_rx_periodic_adv_data;
} CL_DM_BLE_GET_ADV_SCAN_CAPABILITIES_CFM_T;

/*!
    \brief Enable or disable Bluetooth low energy (BLE) extended scanning.

           A message of type CL_DM_BLE_EXT_SCAN_ENABLE_CFM is returned to
           the task indicated by the theAppTask parameter.

    \param theAppTask Task to send the resulting confirmation message to.

    \param enable Enable extended scanning if TRUE, otherwise scanning is disabled.

    \param num_of_scanners 1 to 5 - Number of scanners to be enabled or disabled.

    \param scan_handle The scanner to enable/disable.

    \param duration 0 - Scanning until disabled. (Only used when enabling)

                    1 to 0xFFFF - Reserved for future use

                    Note) Field only used when enable is true. Set to 0 when not used.

    LE extended scanning cannot be enabled if the device is advertising itself.
    ConnectionBleAddExtAdvertisingReportFilter() can be used to filter the
    advertising reports sent to the VM by Bluestack.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtScanEnableReq(Task theAppTask, bool enable, uint8 num_of_scanners, uint8 scan_handle[], uint16 duration[]);


typedef struct
{
    hci_status status;  /*!< Status of the operation */
} CL_HCI_STATUS_STANDARD_COMMAND_CFM_T;


/*!
    \brief Sent in response to ConnectionDmBleExtScanEnableReq().

    This is a BT5.0+ message.
*/
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_EXT_SCAN_ENABLE_CFM_T;


typedef enum {
    passive_scanning    = 0x00,
    active_scanning     = 0x01
} scanning_type;

/*!
    \brief  A CL variant of an HCI struct to be used to pass PHY scan
            parameters to the ConnectionDmBleSetExtScanParametersReq
            function.
*/
typedef struct
{
    scanning_type   scan_type;
    uint16  scan_interval;
    uint16  scan_window;
} CL_ES_SCANNING_PHY_T;

/* This must always match DM_ULP_EXT_SCAN_MAX_SCANNING_PHYS in dm_prim.h */
#define EXT_SCAN_MAX_SCANNING_PHYS 2

/*!
    \brief  Read the global parameters to be used when scanning.

            A message of type CL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_CFM
            is returned to the task indicated by the theAppTask parameter.

    \param theAppTask Task to send the resulting confirmation message to.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtScanGetGlobalParamsReq(Task theAppTask);

/*!
    \brief  Sent in response to reading the global parameters to be used when
            scanning using the ConnectionDmBleExtScanGetGlobalParamsReq() function.

            This message is sent to the task specified in the above function.

    \param flags    bit 0..1 -  Extended scanning AD structure chain length check.
                                Check that all AD Structure lengths add up to
                                total length of advert data.

                        0 = Only send adv reports which pass check
                        1 = Only send adv reports which pass check or
                            are terminated with a 0 length AD structure
                        2 = Send all adv reports

                    bit 2 - Use WoS - Wake on Smart (Future option always set to 0)
                        0 = False
                        1 = True

    \param own_address_type See the OWN_ADDRESS #defines for allowed values.

    \param scanning_filter_policy   0 - Accept all advertising packets, except
                                        directed not addressed to device

                                    1 - White list only

                                    2 - Initiators Identity address is not this device.

                                    3 - White list only and Initiators Identity address identifies this device.

    \param filter_duplicates Filter duplicates in controller using DID/SID cache.

    \param scanning_phys Bitfield denoting PHYs allowed to be used on the
           primary advertising channel:
           Bit 0    - LE 1m
           Bit 1    - Invalid on primary advertising channel (e.g. LE 2M)
           Bit 2    - LE Coded

    \param phys     A pointer to a fixed-length array of structs containing
                    the required params for each PHY. This is passed by
                    reference.

    This is a BT5.0+ message.
*/
typedef struct
{
    uint8                   flags;
    uint8                   own_address_type;
    uint8                   scanning_filter_policy;
    uint8                   filter_duplicates;
    uint16                  scanning_phys;
    CL_ES_SCANNING_PHY_T    phys[EXT_SCAN_MAX_SCANNING_PHYS];
} CL_DM_BLE_EXT_SCAN_GET_GLOBAL_PARAMS_CFM_T;

/*!
    \brief Set parameters for Bluetooth low energy (BLE) Extended scanning.

    A message of type CL_DM_BLE_SET_EXT_SCAN_PARAMETERS_CFM_T
    is returned to the  task that initialised the Connection library
    when operation has completed. This will contain aconnection_lib_status:
    Sucess for a valid set of parameters, and error codes for various failures.

    \param theAppTask Task to send the resulting confirmation message to.

    \param flags    Bit field for use in future.  Always set to 0.
                    bit 0 - Use WoS (Wake on Smart) = False

    \param own_address_type See the OWN_ADDRESS #defines for allowed values.

    \param white_list_only If TRUE then advertising packets from devices that
           are not on the White List for this device will be ignored (default:
           FALSE).

    \param filter_duplicates Filter duplicates in controller using DID/SID cache.

    \param scanning_phys Bitfield denoting PHYs allowed to be used on the
           primary advertising channel:
           Bit 0    - LE 1m
           Bit 1    - Invalid on primary advertising channel (e.g. LE 2M)
           Bit 2    - LE Coded

    \param phy_params A pointer to a fixed-length array of structs containing
                      the required params for each PHY. This is passed by
                      reference.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtScanSetParamsReq(
        Task                    theAppTask,
        uint8                   flags,
        uint8                   own_address_type,
        uint8                   scan_filter_policy,
        uint8                   filter_duplicates,
        uint8                   scanning_phys,
        CL_ES_SCANNING_PHY_T    phy_params[EXT_SCAN_MAX_SCANNING_PHYS]
        );


typedef struct
{
    connection_lib_status           status;     /*!< Status of the operation */
} CL_STATUS_STANDARD_COMMAND_CFM_T;


/*!
    \brief Sent in response to ConnectionDmBleExtScanSetParamsReq() to the task that
    intialised the Connection library.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_SET_EXT_SCAN_PARAMS_CFM_T;

/* This must always match DM_ULP_EXT_SCAN_MAX_REG_AD_TYPES in dm_prim.h */
#define EXT_SCAN_MAX_REG_AD_TYPES 10

/* This must always match DM_ULP_AD_STRUCT_INFO_BYTE_PTRS in dm_prim.h */
#define CL_AD_STRUCT_INFO_BYTE_PTRS 8

/*!
    \brief Register a scanner and filter rules to be used.

    A scanner has to be configured with filter rules.  It is designed to only
    report what a user requires so limiting unwanted data.  To do this it has 2
    stages of filtering:

    adv_filter - Used to match types of advertising reports like:
        advertising data that belongs to extended advertising
        advertising data that has the limited discovery flag set.
        etc...
    ad_structure_filter -   Used to drill into the ad_structure data to look for
                            particular ad_types or combinations of ad_types like:
        Only report the AD Structure data for the ad_types registered
        in reg_ad_types.
        Only report the AD Structure data for the ad_types registered in reg_ad_types
        and if all ad_types registered are in the advertising report.
        etc...

    The adv_filter will be applied first and will throw away any extended
    adverting reports that do not meet this filter.  The ad_structure_filter
    will be applied next.  The extended advertising report will be thrown away
    if no ad_types of interest are found. Else this report will be sent as a
    DM_ULP_EXT_SCAN_FILTERED_ADV_REPORT_IND with the AD Type Structures of
    interest in a MBLK chain.

    A message of type CL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_CFM is returned to
    the task indicated by the theAppTask parameter.

    \param flags        bit 0..1 - Flags AD structure Filter. Only receive
                                   advertising reports that match the following:

                            0 = Receive all
                            1 = Receive advertising reports which have AD data
                                containing a Flags AD type.
                            2 = Receive advertising reports which have AD data
                                containing a Flags AD type with LE Limited
                                Discoverable Mode set or LE General Discoverable
                                Mode set.
                            3 = Receive advertising reports which have AD data
                                containing a Flags AD type with LE Limited
                                Discoverable Mode set.

    \param adv_filter   0 - Receive all advertising data
                        1 - Do not receive any advertising data
                        2 - Only receive legacy advertising data
                            (Primary advertising channel)
                        3 - Only receive extended advertising data
                            (Secondary adverting channel).
                        4 - Only receive extended advertising data with info
                            about periodic advertising.

    \param adv_filter_sub_field1    (Future options always set to 0)

                                    Further filter options to extend the capabilities of adv_filter.

                                    bits 0 - 1 (Type of advertising)
                                    0 - Any
                                    1 - connectable
                                    2 - scannable
                                    3 - non-connectable and non scannable

                                    bits 2 - 3 (Directed Advertising)
                                    0 - directed or undirected
                                    1 - undirected
                                    2 - directed

                                    bits 4 - 5 (Flags AD Type)
                                    0 - Receive all (Do not care if present or not).
                                    1 - Only receive advertising data with flags AD
                                        type and the LE Limited Discoverable Mode
                                        flag set.
                                    2 - Only receive advertising data with flags AD
                                        type and the LE Limited Discoverable Mode
                                        flag or LE General Discoverable Mode flag
                                        set.
                                    3 - Only receive advertising data with flags AD type.

    \param adv_filter_sub_field2    (Future options always set to 0)

    \param ad_structure_filter      (Future options always set to 0)
                                    0 - Send whole report, if all registered
                                        ad_types are in advertising report.
                                        (Uses more memory)
                                    1 - Send whole report, if a registered
                                        ad_type is in advertising report.
                                        (Uses more memory)

    \param ad_structure_filter_sub_field1   Always set to 0. Here to allow future
                                            options like length of a key/value
                                            configuration table.

    \param ad_structure_filter_sub_field2   Always set to 0.  Here to allow future
                                            options like a uint16 pointer to a
                                            key/value configuration table.

    \param ad_structure_info_len        (Future options always set to 0)
                                        Length of ad_structure_info.

    \param ad_structure_info[CL_AD_STRUCT_INFO_BYTE_PTRS]   (Future options always set to 0 x 8)
                                                            Pointer to a 32 octet buffer.
                                                            Please note that the connection
                                                            library will cause this memory to be
                                                            freed.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtScanRegisterScannerReq(
        Task    theAppTask,
        uint32  flags,
        uint16  adv_filter,
        uint16  adv_filter_sub_field1,
        uint32  adv_filter_sub_field2,
        uint16  ad_structure_filter,
        uint16  ad_structure_filter_sub_field1,
        uint32  ad_structure_filter_sub_field2,
        uint16  ad_structure_info_len,
        uint8   *ad_structure_info[CL_AD_STRUCT_INFO_BYTE_PTRS]
        );

/*!
    \brief Sent in response to registering a scanner and filtering rules with
    the ConnectionDmBleExtScanRegisterScannerReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! 0 = success, else error. */
    connection_lib_status           status;
    /*! 0 to 0xFF - Unique identifier for the scanner configured. */
    uint8                           scan_handle;
} CL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_CFM_T;

/*!
    \brief Unregister a scanner.

           A message of type CL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_CFM is
           returned to the task indicated by the theAppTask parameter.

    \param theAppTask Task to send the resulting confirmation message to.

    \param scan_handle The scanner to unregister.

    \return Boolean:    True if the unregister request was successfully sent
                        False if the requested handle was not found or no more
                        handles actually exist to be terminated.

    This is a BT5.0+ feature.
*/
bool ConnectionDmBleExtScanUnregisterScannerReq(Task theAppTask, uint8 scan_handle);

/*!
    \brief  Sent in response to unregistering a scanner with the
            ConnectionDmBleExtScanUnregisterScannerReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_EXT_SCAN_UNREGISTER_SCANNER_CFM_T;

/*!
    \brief  Get information on how the LE controller's scanner has been configured.

            Note) There is only 1 LE controller scanner shared by many scanners.
            2 or more scanners may have to share the controller's LE scanner.
            In which case largest window and smallest interval will be used
            from the active scanners.

            It is important to remember that the scan_interval and scan_window
            parameters are recommendations from the Host to the Controller.
            The frequency and length of the scan is implementation specific.

            A message of type CL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_CFM
            is returned to the task indicated by the theAppTask parameter.

    \param theAppTask Task to send the resulting confirmation message to.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtScanGetCtrlScanInfoReq(Task theAppTask);

/*!
    \brief  Sent in response to requesting  information on how the LE
            controller's scanner has been configured using the
            ConnectionDmBleExtScanGetGlobalParamsReq() function.

            This message is sent to the task specified in the above function.

    \param num_of_enabled_scanners  0 to 0xFF - Number of scanners currently
                                    scanning.  If 0, the controller's scanner is
                                    disabled, else enabled with the below parameters.

    \param legacy_scanner_enabled   0 - Legacy scanner not enabled
                                    1 - Legacy scanner enabled.

    \param duration                 0 - Scanning until disabled.
                                    1 to 0xFFFF - Reserved for future use

    \param scanning_phys Bitfield denoting PHYs allowed to be used on the
           primary advertising channel:
           Bit 0    - LE 1m
           Bit 1    - Invalid on primary advertising channel (e.g. LE 2M)
           Bit 2    - LE Coded

    \param phys     A pointer to a fixed-length array of structs containing
                    the required params for each PHY. This is passed by
                    reference.

    This is a BT5.0+ message.
*/
typedef struct
{
    uint8                   num_of_enabled_scanners;
    uint8                   legacy_scanner_enabled;
    uint16                  duration;
    uint16                  scanning_phys;
    CL_ES_SCANNING_PHY_T    phys[EXT_SCAN_MAX_SCANNING_PHYS];
} CL_DM_BLE_EXT_SCAN_GET_CTRL_SCAN_INFO_CFM_T;

/*!
    \brief  Sent any time the Contoller's LE Scanner config is changed or new
            scanners are enabled/disabled.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! The reason why the indication was sent.
        0 - Application has sent DM_ULP_EXT_SCAN_SET_GLOBAL_PARAMS_REQ
        1 - A scanner has been enabled.
        2 - A scanner has been disabled.
        3 - A scanner has changed how it is scanning.*/
    uint8                   reason;
    /*! Did the Controller's LE Scanner config need changing due to this change.
        0 - False
        1 - True */
    uint8                   controller_updated;
    /*! 0 to 0xFF - Number of scanners currently scanning. If 0, the
        controller's scanner is disabled, else enabled with the below parameters. */
    uint8                   num_of_enabled_scanners;
    /*! 0 - Legacy scanner not enabled
     *  1 - Legacy scanner enabled.
     *  This will be due to the qbluestack legacy API being called to enable
     *  scanning (e.g. DM_HCI_ULP_SET_SCAN_ENABLE_REQ). Expect to receive
     *  DM_HCI_ULP_ADVERTISING_REPORT_IND. You may also receive
     *  DM_ULP_EXT_SCAN_FILTERED_ADV_REPORT_IND if an extended scanner is also
     *  enabled. */
    uint8                   legacy_scanner_enabled;
    /*! How long the Controller's LE Scanner will scan for.
        0 - Scanning until disabled.
        1 to 0xFFFF - Reserved for future use */
    uint16                  duration;
    /*! Controller's LE Scanner Phys being used on the primary advertising
        channel (bit field)

        bit 0 - LE 1M
        bit 1 - Invalid on primary advertising channel (e.g. LE 2M).
        bit 2 - LE Coded */
    uint16                  scanning_phys;
    /*! A pointer to a fixed-length array of structs containing the required
     *  params for each PHY. This is passed by reference. */
    CL_ES_SCANNING_PHY_T    phys[EXT_SCAN_MAX_SCANNING_PHYS];
} CL_DM_BLE_EXT_SCAN_CTRL_SCAN_INFO_IND_T;

/*!
    \brief  Sent any time a duration timer expires for a scanner. The scanner
            will no longer be scanning. This message will be sent to the task
            that registered the expired scan_handle.

            Note:
            If the scan handle has been unregistered, Connection library will
            also remove the association to the registering task from its
            tracking struct. Any advertising reports from this scanner in the
            stream that have not yet been processed will be silently consumed.

    \param  scan_handle     The scan handle of the scanner that has stopped
                            scanning due to its duration timer expiring.

    \param scan_handle_unregistered     0 - (False)
                                        The scanner has NOT been unregistered
                                        and maybe enabled again.
                                        Always the case if the duration timeout
                                        was setup using
                                        CL_INTERNAL_DM_BLE_EXT_SCAN_ENABLE_REQ.

                                        1 - (True)
                                        The scanner has been unregistered.

    This is a BT5.0+ message.
*/
typedef struct
{
    uint8 scan_handle;
    uint8 scan_handle_unregistered;
} CL_DM_BLE_EXT_SCAN_DURATION_EXPIRED_IND_T;

/* Max potential number of adv sets in CL_DM_BLE_SET_EXT_ADV_SETS_INFO_CFM.

   This needs to track DM_ULP_EXT_ADV_MAX_REPORTED_ADV_SETS in dm_prim.h.*/
#define CL_DM_BLE_EXT_ADV_MAX_REPORTED_ADV_SETS 11

typedef struct
{
    uint8           registered;  /*! Has this adv set been registered */
    uint8           advertising; /*! Is this adv set advertising */
    uint16          info;        /*! Reserved for future use */
} CL_DM_EA_SET_INFO_T;

/*!
    \brief Reports information about all advertising sets (e.g. advertising/registered).

           A message of type CL_DM_BLE_SET_EXT_ADV_SETS_INFO_CFM is returned to
           the task indicated by the theAppTask parameter.

    \param theAppTask Task to send the resulting confirmation message to.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvSetsInfoReq(Task theAppTask);

/*!
    \brief  Sent in response to requesting the advertising sets' information
            using the ConnectionDmBleExtAdvSetsInfoReq() function.

            Note:   Adv set 0 is for the legacy advertising/connect API usage
                    and will always be registered and may show as advertising if
                    enabled by the legacy API.  

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! Bit settings for field flags
     *  bit 0 - Is any advertising set advertising? (true if set)
     *  bit 1 - 15 - Unspecified. May be any value.
     *
     *  Note) Always use bit masks to access these bits. */
    uint16              flags;
    /*! X adv sets reported in prim.
     *  This will always be the max supported by device. */
    uint8               num_adv_sets;
    /*! See above struct (CL_DM_EA_SET_INFO) for more details. */
    CL_DM_EA_SET_INFO_T adv_sets[CL_DM_BLE_EXT_ADV_MAX_REPORTED_ADV_SETS];
} CL_DM_BLE_SET_EXT_ADV_SETS_INFO_CFM_T;

/*!
    \brief Enable or disable Bluetooth low energy (BLE) Extended advertising.

           A message of type CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM is returned to
           the task indicated by the theAppTask parameter.

    \param theAppTask Task to send the resulting confirmation message to.

    \param enable Enable extended advertising if TRUE, otherwise advertising is disabled.

    \param adv_handle A registered adv_handle.

    Extended advertising cannot be enabled if the device is scanning. Data to
    be advertised can be set using ConnectionBleAddAdvertisingReportFilter().

    Initiating a GATT Slave Connection will automatically cause broadcast of
    the set advertising data.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvertiseEnableReq(Task theAppTask, bool enable, uint8 adv_handle);

/*!
    \brief Sent in response to ConnectionDmBleExtAdvertiseEnableReq().

    This is a BT5.0+ message
 */
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_EXT_ADVERTISE_ENABLE_CFM_T;

typedef struct
{
    uint8  adv_handle;         /*! Advertising set. */
    uint8  max_ea_events;      /*! Max number of ext adv events allowed. */
    uint16 duration;           /*! How long to advertise. */
} CL_EA_ENABLE_CONFIG_T;

/* Max number of advertising sets that can be enabled or disabled in one go. */
#define CL_DM_BLE_EXT_ADV_MAX_NUM_ENABLE 4

/*!
    \brief  Enable advertising for X advertising set. This allows multiple
            advertising sets to have advertising enabled or disabled.
            It also allows advertising to occur for a fixed duration or number
            of extended advertising events.

            A message of type CL_DM_BLE_EXT_ADV_MULTI_ENABLE_CFM is returned to
            the task indicated by the theAppTask parameter.

            Note:   Connectable advertising may stop advertising due to connection
                    establishment or high duty cycle directed advertising timeout.
                    If this occurs a CL_DM_BLE_EXT_ADV_TERMINATED_IND is generated.

            Note:   Only one CL_DM_BLE_EXT_ADV_TERMINATED_IND will be genrated
                    for an advertising set when it is stopped for any of the
                    above reasons (e.g. the first to occur).
                    It will not be generated if the application stops advertising.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  num_sets    1 to 4 - X number of advertising sets to be enabled or
                        disabled by this prim.

    \param  config      See above struct (CL_EA_ENABLE_CONFIG) for more details.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvMultiEnableReq(   Task theAppTask,
                                            uint8 enable,
                                            uint8 num_sets,
                                            CL_EA_ENABLE_CONFIG_T config[CL_DM_BLE_EXT_ADV_MAX_NUM_ENABLE]);

/*!
    \brief  Sent in response to enabling advertising for multiple advertising
            set using the ConnectionDmBleExtAdvMultiEnableReq() function.

    \param  status

    \param  max_adv_sets

    \param adv_bits

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! 0 = success, else error. */
    connection_lib_status   status;
    /*! Number of adv set adv_bits reported in prim. This will always be the max
     *  supported adv sets including one for supporting legacy advertising/
     *  connecting API (e.g. adv set 0). */
    uint8                   max_adv_sets;
    /*! Says the state of advertising for all adverting sets, including adv
     *  set 0. Each bit represents an advertising set with adv_handle 0
     *  (adv set 0) being the LSB and max_adv_sets being the MSB. Each bit will
     *  be set as follows:
     *
     *  0 - Advertising disabled
     *  1 - Advertising enabled */
    uint32                  adv_bits;
} CL_DM_BLE_EXT_ADV_MULTI_ENABLE_CFM_T;

/*!
    \brief  Set parameters for Bluetooth low energy (BLE) Extended Advertising.

            A message of type CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    \param  adv_event_properties    How you want to advertise (e.g. connectable, scannable, legacy, direct, etc.).  (bit field)

            1 - Connectable
            2 - Scannable
            4 - Directed Advertising
            8 - High Duty Cycle
            16 - Use Legacy advertising PDUs
            32 - Omit advertisers address

    \param  primary_adv_interval_min    Minimum advertising interval.
                                        N = 0x20 to 0xFFFFFF  (Time = N * 0.625 ms)

    \param  primary_adv_interval_max    Maximum advertising interval.
                                        N = 0x20 to 0xFFFFFF  (Time = N * 0.625 ms)

    \param  primary_adv_channel_map     Bit mask, if set will be used. (bit 0 = Channel 37, bit 1 = Channel 38 and bit 2 = Channel 39)

    \param  own_addr_type
            0 - Public (Next 2 primitive fields not used)
            1 - Random (Next 2 primitive fields not used)
            2 - Resolvable Private Address/Public
            3 - Resolvable Private Address/Random

    \param  peer_addr   Used to find the local IRK in the resolving list.
                        0xYYXXXXXXXXXXXX
                        YY = 0 - Public Device Address, 1 - Random Device Address
                        XX = 6 octet address

    \param  adv_filter_policy   How to process scan and connect requests.
                                0 - Process scan and connect requests from all devices.
                                1 - Process connect requests from all devices and scan requests from devices on the white list.
                                2 - Process scan requests from all devices and connect requests from devices on the white list.
                                3 - Process scan and connect requests from devices on the white list.

    \param  primary_adv_phy     The phy to transmit the advertising packets on the primary advertising channels.
                                1 - LE 1M
                                3 - LE Coded

    \param  secondary_adv_max_skip  0x0 to 0xFF - Maximum advertising events on the primary advertising channel that can be skipped before sending an AUX_ADV_IND.

    \param  secondary_adv_phy   The phy to transmit the advertising packets on the secondary advertising channel (e.g. AUX PDUs).
                                1 - LE 1M
                                2 - LE 2M
                                3 - LE Coded

    \param  adv_sid     A value in the range 0 to 15 that can be used by a scanner to identify desired extended advertising from a device (On air will be in the ADI field).
                        Recommendation is to assign this to 0 (general_sid), unless you need a unique advertising SID value.

                        bit 0 to 7: Sender assigned value (0 to 15) - Only allow to be assigned if bit 8 set AND not equal to general_sid AND not already used unless bit 9
                        set on all advertising sets that use this value AND bit 10 not set.

                        bit 8: Allow sender to assign number.

                        bit 9: If set more than 1 advertising set can have the same value, if all allow.

                        bit 10: Stack allocates a unique Advertising SID not used by any other advertising set.  Starting from 15 and going down.

                        bit 11-15: Reserved set to 0.

                        If set to 0 then general_sid value used.  This should be used when you don't care about another advertising set using it.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvSetParamsReq( Task theAppTask,
                                        uint8 adv_handle,
                                        uint16 adv_event_properties,
                                        uint32 primary_adv_interval_min,
                                        uint32 primary_adv_interval_max,
                                        uint8 primary_adv_channel_map,
                                        uint8 own_addr_type,
                                        typed_bdaddr taddr,
                                        uint8 adv_filter_policy,
                                        uint16 primary_adv_phy,
                                        uint8 secondary_adv_max_skip,
                                        uint16 secondary_adv_phy,
                                        uint16 adv_sid);

/*!
    \brief Sent in response to setting BLE Extended Advertising parameters with
    the ConnectionDmBleExtAdvSetParamsReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! Indicates if setting the extended advertising parameters was successful. */
    connection_lib_status           status;
    uint8                           adv_sid;
} CL_DM_BLE_SET_EXT_ADV_PARAMS_CFM_T;

/*!
    \brief  Set the advertising set's random device address to be used when
            configured for use in ConnectionDmBleExtAdvSetParamsReq.

            A message of type CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM is
            returned to the task indicated by the theAppTask parameter.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  adv_handle  A registered adv_handle.

    \param  action      Allows a static address to be set or generates address
                        static/NRPA/RPA. See above enum for more details.

    \param  random_addr Random Advertiser's address.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvSetRandomAddressReq(  Task theAppTask,
                                                uint8 adv_handle,
                                                ble_local_addr_type action,
                                                bdaddr random_addr);

/*!
    \brief  Sent in response to setting an advertising set's random advertising
            address using the ConnectionDmBleExtAdvSetRandomAddressReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    hci_status  status;         /*!< hci_success, hci_error_controller_busy or other error. */
    uint8       adv_handle;     /*!< Advertising set whose address was set. */
    bdaddr      random_addr;    /*!< Random address written. */
} CL_DM_BLE_EXT_ADV_SET_RANDOM_ADDRESS_CFM_T;

/*!
    \brief  This enum contains the four allowed operation in SetDataReq prims, i.e.
            what part of the data is contained in the prim.

            0 = Intermittent fragment
            1 = First fragment
            2 = Last fragment
            3 = Complete data
*/
typedef enum
{
    intermittent_fragment = 0,
    first_fragment,
    last_fragment,
    complete_data
} set_data_req_operation;


/*!
    \brief  Set data for Bluetooth low energy (BLE) Extended Advertising.

            A message of type CL_DM_BLE_SET_EXT_ADV_DATA_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    \param  operation   What part of data is in the prim:

                        0 = Intermittent fragment
                        1 = First fragment
                        2 = Last fragment
                        3 = Complete data

                        This allows the sender to fragment the advertising data it sends to the controller over HCI.
                        The sender can only use 3 while the advertising set is advertising.

    \param  adv_data_len    0 to 251 octets.

    \param  adv_data[8]     pointer to a 32 octet buffer

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvSetDataReq(Task theAppTask, uint8 adv_handle, set_data_req_operation operation, uint8 adv_data_len, uint8 *adv_data[8]);


/*!
    \brief  Set scan response data for Bluetooth low energy (BLE) Extended Advertising.

            A message of type CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    \param  operation   What part of data is in the prim:

                        0 = Intermittent fragment
                        1 = First fragment
                        2 = Last fragment
                        3 = Complete data

                        This allows the sender to fragment the advertising data it sends to the controller over HCI.
                        The sender can only use 3 while the advertising set is advertising.

    \param  reserved            Always set to 0.
    \param  scan_resp_data_len  0 to 251 octets.
    \param  scan_resp_data[8]   pointer to a 32 octet buffer

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvSetScanRespDataReq(Task theAppTask, uint8 adv_handle, set_data_req_operation operation, uint8 scan_resp_data_len, uint8 *scan_resp_data[8]);


/*!
    \brief Sent in response to setting data for the BLE extended advertising
    message to the task that initialised the Connection library.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_SET_EXT_ADV_DATA_CFM_T;


/*!
    \brief  Read the max allowed advertising data length for an advertising set.

            A message of type CL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_CFM is
            returned to the task indicated by the theAppTask parameter.

            Note:   Lengths can vary between advertising sets due to how it was
                    registered using ConnectionDmBleExtAdvRegisterAppAdvSetReq
                    and the flags field. At the moment flags are reserved so the
                    length will always be 251 or less if the controller does not
                    support that much.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  adv_handle  A registered adv_handle or 0 (legacy API adv set).

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvReadMaxAdvDataLenReq(Task theAppTask, uint8 adv_handle);

/*!
    \brief  Sent in response to reading the maximum length for advertising data
            using the ConnectionDmBleExtAdvReadMaxAdvDataLenReq() function.

            NOTE:   The receiving function is responsible for freeing any memory
                    linked to by the pointer fields of this message.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    connection_lib_status   status;                 /*! HCI_SUCCESS or error. */
    uint16                  max_adv_data;           /*! 0 to 251 octets - In future could be bigger. */
    uint16                  max_scan_resp_data;     /*! 0 to 251 octets - In future could be bigger. */
} CL_DM_BLE_EXT_ADV_READ_MAX_ADV_DATA_LEN_CFM_T;


/*!
    \brief  Register an application for use of an advertising set.

            A message of type CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvRegisterAppAdvSetReq(Task theAppTask, uint8 adv_handle);


/*!
    \brief Sent in response to ConnectionDmBleExtAdvRegisterAppAdvSetReq()

    This is a BT5.0+ message
 */
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_EXT_ADV_REGISTER_APP_ADV_SET_CFM_T;

/*!
    \brief  Allows an application to unregister use of an advertising set.

            A message of type CL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtAdvUnregisterAppAdvSetReq(Task theAppTask, uint8 adv_handle);


/*!
    \brief Sent in response to ConnectionDmBleExtAdvUnregisterAppAdvSetReq()

    This is a BT5.0+ message
 */
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_EXT_ADV_UNREGISTER_APP_ADV_SET_CFM_T;

/*!
    \brief Sent to the task that initialised the Connection library in
    response to setting data for the BLE Scan Response in extended advertising.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_EXT_ADV_SET_SCAN_RESPONSE_DATA_CFM_T;

/*!
    \brief  Sent any time advertising is stopped by the controller due to
            duration expiring or max extended advertising event limit reached
            or connection establishment.
            This message will be sent to the task that registered the connection
            library.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! The advertising set that has stopped advertising. */
    uint8 adv_handle;
    /*! The reason why advertising has stopped:
     *  0 - Connection established on advertising set.
     *  1 - Advertising stopped due to duration expiring.
     *  2 - Advertising stopped due to max extended advertising event limit
     *      reached. */
    uint8 reason;
    /*! The peer device address that has connected.
     *  0xYYXXXXXXXXXXXX
     *  YY =    0 - Public Device Address
     *          1 - Random Device Address
     *  XX = 6 octet address */
    typed_bdaddr taddr;
    /*! 0 to 0xFF - Number of extended advertising events occured.
     *  0xFF may mean more events than 255. */
    uint8 ea_events;
    /*! Number of adv set adv_bits reported in prim. This will always be the
     *  max supported adv sets including one for supporting legacy advertising/
     *  connecting API (e.g. adv set 0). */
    uint8 max_adv_sets;
    /*! Says the state of advertising for all adverting sets, including adv set
     *  0. Each bit represents an advertising set with adv_handle 0 (adv set 0)
     *  being the LSB and max_adv_sets being the MSB. Each bit will be set as
     *  follows:
     *  0 - Advertising disabled
     *  1 - Advertising enabled */
    uint32 adv_bits;
} CL_DM_BLE_EXT_ADV_TERMINATED_IND_T;

/*!
    \brief  A Controller's EA report that has been filtered and may have had
            unwanted data removed as specified by the scanners.

            Note that data should be used as needed before returning out of the
            handling function, as the memory containing them will be freed at
            that point.

    \param  event_type      Type of advertising received.  (bit field)

                            bit 0 - Connectable Advertising
                            bit 1 - Scannable Advertising
                            bit 2 - Directed Advertising
                            bit 3 - Scan Response
                            bit 4 - Legacy advertising PDUs used

    \param  primary_phy     The primary phy the advert was received on.

                            1 - LE 1M
                            3 - LE Coded

    \param  secondary_phy   The secondary phy the advert was received on.

                            0 - No packets on secondary advertising channel.
                            1 - LE 1M
                            2 - LE 2M
                            3 - LE Coded

    \param  adv_sid         The Advertising SID used to identify an advertising
                            set from many advertising sets on a device.
                            0x00 - 0x0F

    \param  current_addr	Advertiser's address. Will be the identity address
                            if controller resolved.

    \param  permanent_addr	Advertiser's identity address if resolved by host.
                            Otherwise same as current_addr_type.

    \param  direct_addr     The address that the advert is meant for when a
                            directed advert is received. If this is resolved it
                            will be this device's own address. Otherwise it will
                            be a Random Device Address (Controller unable to resolve).

    \param  tx_power	signed integer (-127 to 126 dBm)
                        127 - Tx power information not available.

    \param  rssi        signed integer (-127 to 20 dBm)
                        127 - RSSI not available.

    \param  periodic_adv_interval	Interval of the periodic advertising.

                                    N = 0x6 to 0xFFFF  (Time = N * 1.25 ms)
                                    0 = No periodic advertising

    \param  adv_data_info	Information about the AD Structure chain:

                            Length check bits 0-1:
                            0 - AD Structure chain OK
                            1 - Zero length AD Structure found so terminated AD
                                Structure chain.
                            2 - Length error.  Sum of the Length of all AD
                                structures does not match total length of adv_data.

                            Reserved bits 2-6:
                            These can be any value.

                            AD flags present bit 7:
                            0 - No AD flags type in advertising report.
                            1 - AD flags type in advertising report.  ad_flags field below holds the AD flags data.

    \param  ad_flags	AD Flags data from flags AD structure.Reference
                        Supplement to the Bluetooth Core Specification (CSS).

    \param  adv_data_len	Length of advertising data

    \param  adv_data        Pointer to the advertising data.

    This is a BT5.0+ message.
*/
typedef struct
{
    uint16          event_type;
    uint16          primary_phy;
    uint16          secondary_phy;
    uint8           adv_sid;
    typed_bdaddr    current_addr;
    typed_bdaddr    permanent_addr;
    typed_bdaddr    direct_addr;
    int8            tx_power;
    int8            rssi;
    uint16          periodic_adv_interval;
    uint8           adv_data_info;
    uint8           ad_flags;
    uint16          adv_data_len;
    const uint8     *adv_data;
} CL_DM_BLE_EXT_SCAN_FILTERED_ADV_REPORT_IND_T;

/*!
    \brief Sent in response to setting BLE Periodic Advertising parameters.

    This message is sent to the task that initialised the Connection library.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_PER_ADV_SET_PARAMS_CFM_T;

/*!
    \brief Sent in response to setting data for the BLE periodic advertising
    message to the task that initialised the Connection library.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_PER_ADV_SET_DATA_CFM_T;

/*!
    \brief Sent in the event of loss of Periodic Advertising sync.

    This is a BT5.0+ message.
*/
typedef struct
{
    uint16      sync_handle;
} CL_DM_BLE_PERIODIC_SCAN_SYNC_LOST_IND_T;

/*
    \brief BLE Periodic Advertising Reports received.

    This message will be received by the task that initialised the Connection
    library.

    This is a BT5.0+ message.
*/
typedef struct
{
    uint16                          sync_handle;
    uint8                           tx_power;
    uint8                           rssi;
    uint8                           cte_type;
    uint16                          adv_data_len;
    const uint8                     *adv_data;
} CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_IND_T;

typedef struct
{
    uint8 adv_sid;
    typed_bdaddr taddr;
} CL_DM_ULP_PERIODIC_SCAN_TRAINS_T;

/*!
    \brief Allows any primitive handler to register an interest in receiving ad_types
    from advertising reports or extended advertising reports.

    \return None

    This is a BT5.0+ feature.
*/
void ConnectionDmBleExtScanRegisterAdTypesReq(uint8 action, uint8 num_of_ad_types, uint8 ad_types[]);

/* This must always match DM_MAX_PERIODIC_TRAIN_LIST_SIZE in dm_prim.h */
#define CL_MAX_PERIODIC_TRAIN_LIST_SIZE 3

/*!
    \brief Establish sync to one of the periodic trains in the primitive.

    A message of type CL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_CFM is returned to
    the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param report_periodic  Should periodic advertising data be reported.
                            0 - no
                            1 - yes

    \param skip Max number of periodic advertising events that can be skipped
                after a successful receive.
                0x0 to 0x01F3

    \param sync_timeout Synchronization timeout for the periodic advertising
                        train once synchronised.  This is the maximum permitted
                        time between successful receives.
                        0xA to 0x4000 (Time = n * 10 ms)

    \param sync_cte_type    Always set to 0.  Reserved for future use.

    \param attempt_sync_for_x_seconds   Always set to 0.  Reserved for future use.
                                        0 to 3600 seconds - Attempt to sync for
                                        specified time.  If set to 0 then carry
                                        on attempting to sync until
                                        DM_HCI_ULP_PERIODIC_SCAN_SYNC_CANCEL_REQ
                                        received.

    \param number_of_periodic_trains    1 to 3 - Number of train records in primitive.

    \param periodic_train_adv_sid[x]    Advertising SID in ADI field of extended
                                        advertising PDU.  Used to identify a train
                                        from many coming from the same device.
                                        Range = 0 to 15

    \param periodic_train_adv_address[x]    Bluetooth address of the device that
                                            the periodic advertising train is
                                            originating from.
                                            0xYYXXXXXXXXXXXX
                                            YY =    0 - Public Device Address,
                                                    1 - Random Device Address
                                            XX = 6 octet address

    \return None

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePeriodicScanSyncTrainReq(
    Task    theAppTask,
    uint8 report_periodic,
    uint16 skip,
    uint16 sync_timeout,
    uint8 sync_cte_type,
    uint16 attempt_sync_for_x_seconds,
    uint8 number_of_periodic_trains,
    CL_DM_ULP_PERIODIC_SCAN_TRAINS_T periodic_trains[CL_MAX_PERIODIC_TRAIN_LIST_SIZE]);


/*!
    \brief Sent in response to syncing to a periodic train using the
    ConnectionDmBlePeriodicScanSyncTrainReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! 0 = success - synced to a periodic advertising train (sync_handle is valid).
        0xFFFF = pending - attempting to sync (sync_handle and below invalid).
        ... = else error. */
    uint16              status;
    /*! 0x0000 to 0x0EFF - A periodic trains currently synced by controller. */
    uint16              sync_handle;
    /*! The Advertising SID used to identify an advertising set from many
        advertising sets on a device.
        0x00 - 0x0F */
    uint8               adv_sid;
    /*! Bluetooth address of advertiser.
        0xYYXXXXXXXXXXXXXX
        YY = Public Device Address, Random Device Address, Public Identity Address, etc..
        XX = 6 octet address */
    typed_bdaddr        taddr;
    /*! The secondary phy the advert was received on.
        1 - LE 1M
        2 - LE 2M
        3 - LE Coded */
    uint8               adv_phy;
    /*! Interval of the periodic advertising.
        N = 0x6 to 0xFFFF  (Time = N * 1.25 ms)
        0 = No periodic advertising */
    uint16              periodic_adv_interval;
    /*! 0x00 = 500 ppm
        0x01 = 250 ppm
        0x02 = 150 ppm
        0x03 = 100 ppm
        0x04 = 75 ppm
        0x05 = 50 ppm
        0x06 = 30 ppm
        0x07 = 20 ppm */
    uint8               adv_clock_accuracy;
} CL_DM_BLE_PERIODIC_SCAN_SYNC_TO_TRAIN_CFM_T;

/*!
    \brief Cancel an attempt to synchronise on to a periodic train.

    A message of type CL_DM_BLE_PERIODIC_SCAN_SYNC_CANCEL_CFM is returned to
    the task which initiated the synchronisation.
 */
void ConnectionDmBlePeriodicScanSyncCancelReq(void);

/*!
    \brief Sent in response to ConnectionDmBlePeriodicScanSyncCancelReq().

    This is a BT5.0+ message.
*/
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_PERIODIC_SCAN_SYNC_CANCEL_CFM_T;

/*!
    \brief Establish sync to one of the periodic trains in the primitive.

    A message of type CL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_CFM is returned to
    the task indicated by the theAppTask parameter.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  sync_handle A periodic trains currently synced by controller.

    \return Boolean:    True if the terminate request was successfully sent
                        False if the requested handle was not found or no more
                        handles actually exist to be terminated.

    This is a BT5.0+ feature.
*/
bool ConnectionDmBlePeriodicScanSyncTerminateReq(Task theAppTask, uint16 sync_handle);

/*!
    \brief Sent in response to ConnectionDmBlePeriodicScanSyncTerminateReq().

    This is a BT5.0+ message.
 */
typedef struct
{
    connection_lib_status status;       /*!> Status of the operation */
    uint16 sync_handle;
} CL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_CFM_T;

/*!
    \brief Using the sync handle provided by the local Controller after synchronising
    to a periodic advertising train, instruct the Controller to transfer SyncInfo
    related to this PA train to a connected peer.

    A message of type CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_CFM is returned to
    the task indicated by the theAppTask parameter.

    \param  theAppTask      Task to send the resulting confirmation message to.

    \param  taddr           Address of peer device

    \param  service_data    Data provided by Host.

    \param  sync_handle     A periodic trains currently synced by controller.

    \return None

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePeriodicScanSyncTransferReq(Task theAppTask,
                                                typed_bdaddr taddr,
                                                uint16 service_data,
                                                uint16 sync_handle);

/*!
    \brief Sent in response to ConnectionDmBlePeriodicScanSyncTransferReq().

    This is a BT5.0+ message.
 */
typedef CL_DM_BLE_PERIODIC_SCAN_SYNC_TERMINATE_CFM_T CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_CFM_T;

/*!
    \brief  Configures the Controller's future default response for all incoming
            sync information procedures.

    A message of type CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_CFM is returned to
    the task indicated by the theAppTask parameter.

    \param  theAppTask      Task to send the resulting confirmation message to.

    \param  taddr           Address of peer device

    \param  skip            Sets the number of periodic advertising reports that
                            can be skipped after a report has been delivered to
                            the Host. (Only takes effect when periodic advertising
                            reports are enabled.)

    \param  sync_timeout    Synchronisation timeout for any future periodic advertising train.

    \param  mode            Sets the action to take when an incoming PAST request
                            is received from a connected peer.

                            0x00 - No attempt is made to synchronize to the
                            periodic advertising and no events are sent to the
                            Host. The local Host never knows the peer sent us
                            any sync info.

                            0x01 - An attempt will be made to synchronize to the
                            periodic advertising train. Host is notified of
                            success/failure but advertising reports are disabled.

                            0x02 - Same as mode 0x01 but periodic advertising
                            reports will be enabled. There is a separate prim
                            defined by the AE design to disable this later if needed.

    \param  cte_type        Type of Constant Tone Extension that must be present
                            in an advertising train before synchronisation will
                            be attempted.
    \return None

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePeriodicScanSyncTransferParamsReq(
    Task theAppTask,
    typed_bdaddr taddr,
    uint16 skip,
    uint16 sync_timeout,
    uint8 mode,
    uint8 cte_type);

/*!
    \brief Sent in response to ConnectionDmBlePeriodicScanSyncTransferParamsReq().

    This is a BT5.0+ message.
 */
typedef struct
{
    typed_bdaddr taddr;
    connection_lib_status status;
} CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_PARAMS_CFM_T;

/*!
    \brief  Response that the application has stopped reading periodic train
            adv data for this train. Must be sent when a
            DM_HCI_ULP_PERIODIC_SCAN_SYNC_LOSS_IND is received.

    \param sync_handle A periodic train that the controller has lost sync to.

    This is a BT5.0+ message.
*/
void ConnectionDmBlePeriodicScanSyncLostRsp(uint16 sync_handle);

/*!
    \brief  Allows the application to update an existing association between a
            sync_handle and its task, effectively switching which task the
            sync reports get routed to, as well as being allowed to terminate
            that sync.

    \param sync_handle  The periodic train whose task association should be changed.

    \param theAppTask   The new task with which the sync_handle should be associated.

    \return connection_lib_status - A fail would mean the provided sync_handle
                                    was not found.
*/
connection_lib_status ConnectionUpdateTaskToSyncHandleAssociation(uint16 sync_handle, Task theAppTask);

/*!
    \brief  An indication sent to the Profile/Application following an attempt
            by the local Controller to synchronize to a periodic advertising
            train. A status code will indicate if the attempts was successful.

    This is a BT5.0+ message.
*/
typedef struct
{
    /*! A status value indicating if the local Controller has synchronized to a
        periodic advertising stream after receiving sync info from a connected
        peer. */
    connection_lib_status status;
    /*! Value of the adverting SID subfield in the ADI field of the PDU. */
    uint8 adv_sid;
    /*! A handle to identify the periodic adverting train that the Controller
        has synchronized to. */
    uint16 sync_handle;
    /*!  */
    uint16 service_data;
    /*! The Bluetooth address of the advertiser transmitting the periodic train
        that has been synchronized to. */
    typed_bdaddr adv_addr;
} CL_DM_BLE_PERIODIC_SCAN_SYNC_TRANSFER_IND_T;

/*!
    \brief  Set parameters for Bluetooth low energy (BLE) Periodic Advertising.

            A message of type CL_DM_BLE_PER_ADV_SET_PARAMS_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    \param  periodic_adv_interval_min   Minimum periodic advertising interval.
                                        N = 0x6 to 0xFFFFFF  (Time = N * 1.25 ms)
                                        Default = 0x320 (Time = 1 second)

    \param  periodic_adv_interval_max   Maximum periodic advertising interval.
                                        N = 0x6 to 0xFFFFFF  (Time = N * 1.25 ms)
                                        Default = 0x640 (Time = 2 second)

    \param  periodic_adv_properties     Always set to 0

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePerAdvSetParamsReq( Task theAppTask,
                                        uint8 adv_handle,
                                        uint32 flags,
                                        uint16 periodic_adv_interval_min,
                                        uint16 periodic_adv_interval_max,
                                        uint16 periodic_adv_properties);

/*!
    \brief Sent in response to setting BLE Periodic Advertising parameters with
    the ConnectionDmBlePerAdvSetParamsReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_SET_PER_ADV_PARAMS_CFM_T;

/*!
    \brief  Set data for Bluetooth low energy (BLE) Periodic Advertising.

            A message of type CL_DM_BLE_PER_ADV_SET_DATA_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    \param  operation   What part of data is in the prim:

                        0 = Intermittent fragment
                        1 = First fragment
                        2 = Last fragment
                        3 = Complete data

                        This allows the sender to fragment the advertising data it sends to the controller over HCI.
                        The sender can only use 3 while the advertising set is advertising.

    \param  adv_data_len    0 to 251 octets.

    \param  adv_data[8]     pointer to a 32 octet buffer

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePerAdvSetDataReq(Task theAppTask, uint8 adv_handle, set_data_req_operation operation, uint8 adv_data_len, uint8 *adv_data[8]);

/*!
    \brief  Starts a periodic advertising train.

            A message of type CL_DM_BLE_PER_ADV_START_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePerAdvStartReq(Task theAppTask, uint8 adv_handle);

/*!
    \brief Sent in response to starting BLE Periodic Advertising with
    the ConnectionDmBlePerAdvStartReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_PER_ADV_START_CFM_T;

/*!
    \brief  Stops a periodic advertising train or just the associated extended advertising.

            A message of type CL_DM_BLE_PER_ADV_STOP_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask Task to send the resulting confirmation message to.

    \param  adv_handle A registered adv_handle.

    \param  stop_advertising    This field allows the stopping of periodic
                                advertising and associated extended advertising.

                                0 - Stop periodic advertising.
                                1 - Stop extended advertising and then stop
                                    periodic advertising.
                                2 - Stop extended advertising.

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePerAdvStopReq(Task theAppTask, uint8 adv_handle, uint8 stop_advertising);

/*!
    \brief Sent in response to stopping BLE Periodic Advertising with
    the ConnectionDmBlePerAdvStopReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef CL_HCI_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_PER_ADV_STOP_CFM_T;

/*!
    \brief  Instructs the Controller to communicate sync info for an advertising train
            that is being broadcast from the local Controller to a connected Peer.

            A message of type CL_DM_BLE_PER_ADV_SET_TRANSFER_CFM is returned to
            the task indicated by the theAppTask parameter.

    \param  theAppTask      Task to send the resulting confirmation message to.

    \param  taddr           Address of peer device.

    \param  service_data    Service data for peer's Host.

    \param  adv_handle      Identifies the adv. set to local Controller

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePerAdvSetTransferReq(Task theAppTask,
                                        typed_bdaddr taddr,
                                        uint16 service_data,
                                        uint8 adv_handle);

/*!
    \brief  Sent in response to requesting a SyncInfo transfer for an active
            advertising set to a connected peer using the
            ConnectionDmBlePerAdvSetTransferReq() function.

            Note that this message does not indicate that the remote Controller
            has synchronised successfully to the related PA train.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    uint8 adv_handle;
    hci_status status;
} CL_DM_BLE_PER_ADV_SET_TRANSFER_CFM_T;

/*!
    \brief  Read the max allowed periodic advertising data length for an
            advertising set.

            A message of type CL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_CFM is
            returned to the task indicated by the theAppTask parameter.

            Note:   Lengths can vary between advertising sets due to how it was
                    registered using ConnectionDmBlePerAdvSetParamsReq and the
                    flags field. At the moment flags are reserved so the length
                    will always be 252 or less if the controller does not
                    support that much.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  adv_handle  A registered adv_handle or 0 (legacy API adv set).

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePerAdvReadMaxAdvDataLenReq(Task theAppTask, uint8 adv_handle);

/*!
    \brief  Sent in response to reading the maximum length for periodic advertising
            data using the ConnectionDmBlePerAdvReadMaxAdvDataLenReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef struct
{
    connection_lib_status   status;             /*! HCI_SUCCESS or error. */
    uint16                  max_adv_data;       /*! 0 to 252 octets - In future could be bigger. */
} CL_DM_BLE_PER_ADV_READ_MAX_ADV_DATA_LEN_CFM_T;

/*!
    \brief  Search for periodic trains that meet a specified ad_structure filter.

            A message of type CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM is
            returned to the task indicated by the theAppTask parameter.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  flags   bit 0..1 -  Flags AD structure filter. Only receive
                                advertising reports that match the following:

                        0 = Receive all
                        1 = Receive advertising reports which have AD data
                            containing a Flags AD type.
                        2 = Receive advertising reports which have AD data
                            containing a Flags AD type with LE Limited Discoverable
                            Mode set or LE General Discoverable Mode set.
                        3 = Receive advertising reports which have AD data
                            containing a Flags AD type with LE Limited Discoverable
                            Mode set.

                    Flags used to say how the periodic scan should be done.

                    bit 2 report_once - Always set this bit to 0. (Future option)
                                        if bit set then report once by use
                                        periodic cache to limit reports.

                    bit 3 - 7 reserved (set to 0)

    \param  scan_for_x_seconds  0 - Scan until DM_ULP_PERIODIC_SCAN_STOP_FIND_TRAINS_REQ sent.
                                1 to 3600 - Scan for X seconds.

    \param  ad_structure_filter     Always set to 0. (Future option)

                                    0 - Send whole report, if all registered
                                        ad_types are in advertising report.
                                    1 - Send whole report, if a registered
                                        ad_type is in advertising report.

    \param  ad_structure_filter_sub_field1  Always set to 0. Here to allow future
                                            options like length of a key/value
                                            configuration table.

    \param  ad_structure_filter_sub_field2  Always set to 0. Here to allow future
                                            options like a uint16 pointer to a
                                            key/value configuration table.

    \param  ad_structure_info_len   (Future options always set to 0)
                                    Length of ad_structure_info.

    \param  ad_structure_info[CL_AD_STRUCT_INFO_BYTE_PTRS]  (Future options always set all to 0)
                                                            Pointers to 8 x 32 octet buffers.
                                                            Please note that the connection
                                                            library will cause this memory to be
                                                            freed.

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePeriodicScanStartFindTrainsReq(
    Task    theAppTask,
    uint32  flags,
    uint16  scan_for_x_seconds,
    uint16  ad_structure_filter,
    uint16  ad_structure_filter_sub_field1,
    uint32  ad_structure_filter_sub_field2,
    uint16  ad_structure_info_len,
    uint8   *ad_structure_info[CL_AD_STRUCT_INFO_BYTE_PTRS]);

/*!
    \brief  Sent in response to a requesting to start finding trains using the
            ConnectionDmBlePeriodicScanStartFindTrainsReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef CL_DM_BLE_EXT_SCAN_REGISTER_SCANNER_CFM_T CL_DM_BLE_PERIODIC_SCAN_START_FIND_TRAINS_CFM_T;

/*!
    \brief  Stop scanning for periodic trains.

            A message of type CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM is
            returned to the task indicated by the theAppTask parameter.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  scan_handle Handle return in DM_ULP_PERIODIC_SCAN_START_FIND_TRAINS_CFM.

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePeriodicScanStopFindTrainsReq(
    Task    theAppTask,
    uint8   scan_handle);

/*!
    \brief  Sent in response to a requesting to stop finding trains using the
            ConnectionDmBlePeriodicScanStopFindTrainsReq() function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_PERIODIC_SCAN_STOP_FIND_TRAINS_CFM_T;

/*!
    \brief  Sets whether a DM_ULP_PERIODIC_SCAN_SYNC_ADV_REPORT_IND should be
            reported for a synced periodic train.

            A message of type CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_CFM
            is returned to the task indicated by the theAppTask parameter.

    \param  theAppTask  Task to send the resulting confirmation message to.

    \param  sync_handle A periodic trains currently synced by controller.
                        Note) Handle passed back in DM_ULP_PERIODIC_SCAN_SYNC_TO_TRAIN_CFM.

    This is a BT5.0+ feature.
*/
void ConnectionDmBlePeriodicScanSyncAdvReportEnableReq(
    Task    theAppTask,
    uint16  sync_handle,
    uint8   enable);

/*!
    \brief  Sent in response to a setting the reporting of periodic scan sync
            advertising reports using the ConnectionDmBlePeriodicScanStopFindTrainsReq()
            function.

    This message is sent to the task specified in the above function.

    This is a BT5.0+ message.
*/
typedef CL_STATUS_STANDARD_COMMAND_CFM_T CL_DM_BLE_PERIODIC_SCAN_SYNC_ADV_REPORT_ENABLE_CFM_T;

#endif /* DISABLE_BLE */

#endif    /* CONNECTION_H_ */

/** @} */

