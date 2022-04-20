/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera private header with operator message related definitions
*/

#ifndef KYMERA_OP_MSG_H_
#define KYMERA_OP_MSG_H_

/*! Kymera operator messages are 3 words long, with the ID in the 2nd word */
#define KYMERA_OP_MSG_LEN                   (3)
#define KYMERA_OP_MSG_WORD_MSG_ID           (1)
#define KYMERA_OP_MSG_WORD_EVENT_ID         (3)
#define KYMERA_OP_MSG_WORD_PAYLOAD_0        (4)

#define KYMERA_OP_MSG_WORD_PAYLOAD_NA       (0xFFFF)

/*! \brief The kymera operator unsolicited message ids. */
typedef enum
{
    /*! Kymera ringtone generator TONE_END message */
    KYMERA_OP_MSG_ID_TONE_END = 0x0001U,
    /*! AANC Capability  INFO message Kalsim testing only */
    KYMERA_OP_MSG_ID_AANC_INFO = 0x0007U,
    /*! AANC Capability EVENT_TRIGGER message  */
    KYMERA_OP_MSG_ID_AANC_EVENT_TRIGGER = 0x0008U,
    /*! AANC Capability EVENT_CLEAR aka NEGATIVE message  */
    KYMERA_OP_MSG_ID_AANC_EVENT_CLEAR = 0x0009U,
    /*! Earbud Fit Test result message */
    KYMERA_OP_MSG_ID_FIT_TEST = 0x000BU,
} kymera_op_unsolicited_message_ids_t;

/*! \brief The kymera AANC operator event ids. */
typedef enum
{
    /*! Gain unchanged for 5 seconds when EDs inactive */
    KYMERA_AANC_EVENT_ED_INACTIVE_GAIN_UNCHANGED = 0x0000U,
    /*! Either ED active for more than 5 seconds */
    KYMERA_AANC_EVENT_ED_ACTIVE = 0x0001U,
    /*! Quiet mode  */
    KYMERA_AANC_EVENT_QUIET_MODE = 0x0002U,
    /*! Bad environment updated for x secs above spl threshold and cleared immediately */
    KYMERA_AANC_EVENT_BAD_ENVIRONMENT = 0x0006U,
} kymera_aanc_op_event_ids_t;

#define KYMERA_FIT_TEST_EVENT_ID (0x0U)
#define KYMERA_FIT_TEST_RESULT_BAD (0x0U)

#endif /* KYMERA_OP_MSG_H_ */
