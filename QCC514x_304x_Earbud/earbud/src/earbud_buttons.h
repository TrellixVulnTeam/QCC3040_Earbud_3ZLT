/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       
\brief      Header to include relevant button files

    Header file to act as a helper if different button configurations are being
    used.

    
*/

#ifndef EARBUD_BUTTONS_H
#define EARBUD_BUTTONS_H

#if defined(HAVE_1_BUTTON)
#include "1_button.h"
#elif defined(HAVE_2_BUTTONS)
#include "2_button.h"
#elif defined(HAVE_3_BUTTONS)
#include "3_buttons.h"
#elif defined(HAVE_4_BUTTONS)
#include "4_buttons.h"
#elif defined(HAVE_5_BUTTONS)
#include "5_buttons.h"
#elif defined(HAVE_6_BUTTONS)
#include "6_buttons.h"
#elif defined(HAVE_7_BUTTONS)
#include "7_buttons.h"
#elif defined(HAVE_9_BUTTONS)
#include "9_buttons.h"
#else
#error "No button support. Unexpected"
#endif

#endif /* EARBUD_BUTTONS_H */
