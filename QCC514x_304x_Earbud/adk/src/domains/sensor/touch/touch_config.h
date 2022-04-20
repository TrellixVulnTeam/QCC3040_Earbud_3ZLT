/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.

\file       

\brief      Configuration related definitions for touch / capacitive sensor support.
*/

#ifndef TOUCH_CONFIG_H_
#define TOUCH_CONFIG_H_


/*! Enumeration of touch actions that can send to its clients */
typedef enum
{
    /* DON'T insert new event within the block */
    /* Multi click events */
    SINGLE_PRESS = 0x0001,
    DOUBLE_PRESS,
    TRIPLE_PRESS,
    FOUR_PRESS,
    FIVE_PRESS,
    SIX_PRESS,
    SEVEN_PRESS,
    EIGHT_PRESS,
    NINE_PRESS,
    MAX_PRESS_SUPPORT,

    DOUBLE_PRESS_HOLD,
    TRIPLE_PRESS_HOLD,
    FOUR_PRESS_HOLD,
    FIVE_PRESS_HOLD,

    LONG_PRESS,
    VERY_LONG_PRESS,
    VERY_VERY_LONG_PRESS,
    VERY_VERY_VERY_LONG_PRESS,

    SLIDE_UP,
    SLIDE_DOWN,
    SLIDE_LEFT,
    SLIDE_RIGHT,
    HAND_COVER,
    HAND_COVER_RELEASE,

    TAP_SLIDE_UP,
    TAP_SLIDE_DOWN, // 26 (must be < 0x20 )
    /* DON'T insert new event within the block */
    /* Touch events with timing, 1 second resolution*/
    TOUCH_PRESS_HOLD_OFFSET = 0x0020,
    TOUCH_ONE_SECOND_PRESS = 0x0021,
    TOUCH_TWO_SECOND_PRESS,
    TOUCH_THREE_SECOND_PRESS,
    TOUCH_FOUR_SECOND_PRESS,
    TOUCH_FIVE_SECOND_PRESS,
    TOUCH_SIX_SECOND_PRESS,
    TOUCH_SEVEN_SECOND_PRESS,
    TOUCH_EIGHT_SECOND_PRESS,
    TOUCH_NINE_SECOND_PRESS,
    TOUCH_TEN_SECOND_PRESS,
    TOUCH_ELEVEN_SECOND_PRESS,
    TOUCH_TWELVE_SECOND_PRESS,
    TOUCH_FIFTEEN_SECOND_PRESS = 0x002F,
    TOUCH_THIRTY_SECOND_PRESS = 0x003E,
    TOUCH_SIXTY_SECOND_PRESS = 0x005C,

    /* DON'T insert new event within the block */
    /* Touch release events with timing, 1 second resolution*/
    TOUCH_PRESS_RELEASE_OFFSET = 0x0080,
    TOUCH_ONE_SECOND_PRESS_RELEASE = 0x0081,
    TOUCH_TWO_SECOND_PRESS_RELEASE,
    TOUCH_THREE_SECOND_PRESS_RELEASE,
    TOUCH_FOUR_SECOND_PRESS_RELEASE,
    TOUCH_FIVE_SECOND_PRESS_RELEASE,
    TOUCH_SIX_SECOND_PRESS_RELEASE,
    TOUCH_SEVEN_SECOND_PRESS_RELEASE,
    TOUCH_EIGHT_SECOND_PRESS_RELEASE,
    TOUCH_NINE_SECOND_PRESS_RELEASE,
    TOUCH_TEN_SECOND_PRESS_RELEASE,
    TOUCH_ELEVEN_SECOND_PRESS_RELEASE,
    TOUCH_TWELVE_SECOND_PRESS_RELEASE,
    TOUCH_FIFTEEN_SECOND_PRESS_RELEASE = 0x008F,
    TOUCH_THIRTY_SECOND_PRESS_RELEASE = 0x009E,
    TOUCH_SIXTY_SECOND_PRESS_RELEASE = 0x00BC,

    /* Double Touch hold and release events */
    TOUCH_DOUBLE_PRESS_HOLD_OFFSET = 0x0100,
    TOUCH_DOUBLE_ONE_SECOND_PRESS = 0x0101,
    TOUCH_DOUBLE_THREE_SECOND_PRESS = 0x0103,
    TOUCH_DOUBLE_SIX_SECOND_PRESS = 0x0106,    
    TOUCH_DOUBLE_EIGHT_SECOND_PRESS = 0x0108,
    TOUCH_DOUBLE_NINE_SECOND_PRESS = 0x0109,
    TOUCH_DOUBLE_TWELVE_SECOND_PRESS = 0x010C,
    TOUCH_DOUBLE_FIFTEEN_SECOND_PRESS = 0x010F,
    TOUCH_DOUBLE_THIRTY_SECOND_PRESS = 0x011E,
    TOUCH_DOUBLE_SIXTY_SECOND_PRESS = 0x013C,

    TOUCH_DOUBLE_PRESS_HOLD_RELEASE_OFFSET = 0x0150,
    TOUCH_DOUBLE_ONE_SECOND_PRESS_RELEASE = 0x0151,
    TOUCH_DOUBLE_THREE_SECOND_PRESS_RELEASE = 0x0153,
    TOUCH_DOUBLE_SIX_SECOND_PRESS_RELEASE = 0x0156,
    TOUCH_DOUBLE_EIGHT_SECOND_PRESS_RELEASE = 0x0158,
    TOUCH_DOUBLE_NINE_SECOND_PRESS_RELEASE = 0x0159,
    TOUCH_DOUBLE_TWELVE_SECOND_PRESS_RELEASE = 0x015C,
    TOUCH_DOUBLE_FIFTEEN_SECOND_PRESS_RELEASE = 0x015F,
    TOUCH_DOUBLE_THIRTY_SECOND_PRESS_RELEASE = 0x016E,
    TOUCH_DOUBLE_SIXTY_SECOND_PRESS_RELEASE = 0x018C,

    MAX_ACTION,
} touch_action_t;

typedef struct
{
    touch_action_t  action;
    MessageId       message;

} touch_event_config_t;


/*! touch config type */
typedef struct __touch_config_t touchConfig;

/*! The touch sensor configuration */
extern const touchConfig touch_config;

/*! Returns the touch sensor configuration */
#define appConfigTouch() (&touch_config)

/*! Number of clients expected. More clients are allowed */
#define TOUCH_CLIENTS_INITIAL_CAPACITY      1

#endif /* TOUCH_CONFIG_H_ */
