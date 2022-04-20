/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Charger Case Protocol
*/

#ifndef CCP_H_
#define CCP_H_

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "wire.h"
#include "charger_comms.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#define CCP_MAX_PAYLOAD_SIZE (CHARGER_COMMS_MAX_MSG_LEN - 3)

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    CCP_MSG_STATUS        = 0,
    CCP_MSG_EARBUD_STATUS = 1,
    CCP_MSG_RESET         = 2,
    CCP_MSG_STATUS_REQ    = 3,
    CCP_MSG_LOOPBACK      = 4,
    CCP_MSG_EARBUD_CMD    = 5,
    CCP_MSG_EARBUD_RSP    = 6
}
CCP_MESSAGE;

typedef enum
{
    CCP_CH_CASE_INFO = 0,
    CCP_CH_DTS       = 1
}
CCP_CHANNEL;

typedef enum
{
    CCP_EC_PEER_PAIRING    = 0,
    CCP_EC_HANDSET_PAIRING = 1,
    CCP_EC_SHIPPING_MODE   = 2
}
CCP_EC;

typedef enum
{
    CCP_IT_BT_ADDRESS = 0
}
CCP_INFO_TYPE;

typedef struct
{
    void (*rx_earbud_status)(
        uint8_t earbud,
        uint8_t pp,
        uint8_t chg_rate,
        uint8_t battery,
        uint8_t charging);

    void (*rx_bt_address)(
        uint8_t earbud, uint16_t nap, uint8_t uap, uint32_t lap);

    void (*ack)(uint8_t earbud);
    void (*nack)(uint8_t earbud);
    void (*give_up)(uint8_t earbud);
    void (*no_response)(uint8_t earbud);
    void (*abort)(uint8_t earbud);
    void (*broadcast_finished)(void);
    void (*loopback)(uint8_t earbud, uint8_t *data, uint16_t len);
    void (*shipping)(uint8_t earbud, uint8_t sm);
}
CCP_USER_CB;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void ccp_init(const CCP_USER_CB *user_cb);
void ccp_periodic(void);
bool ccp_tx_short_status(bool lid, bool charger, bool charge_rate);

bool ccp_tx_status(
    bool lid,
    bool charger_connected,
    bool charging,
    bool charge_rate,
    uint8_t battery_case,
    uint8_t battery_left,
    uint8_t battery_right,
    uint8_t charging_left,
    uint8_t charging_right);

bool ccp_tx_shipping_mode(uint8_t earbud);
bool ccp_tx_loopback(uint8_t earbud, uint8_t *data, uint16_t len);
bool ccp_tx_status_request(uint8_t earbud);
bool ccp_tx_xstatus_request(uint8_t earbud, uint8_t info_type);
bool ccp_tx_reset(uint8_t earbud, bool factory);
bool ccp_at_command(uint8_t cli_source, WIRE_DESTINATION dest, char *at_cmd);

#endif /* CCP_H_ */
