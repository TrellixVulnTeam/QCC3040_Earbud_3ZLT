/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    case_comms
\brief      Case comms scheme B transport hardware setup.
*/
/*! \addtogroup case_comms
@{
*/

#ifdef INCLUDE_CASE_COMMS
#ifdef HAVE_CC_TRANS_SCHEME_B

#include "cc_protocol_trans_schemeB_hw.h"
#include "cc_protocol_private.h"

#include <pio_common.h>
#include <pio.h>
#include <logging.h>
#include <panic.h>

/*! PIOs used for Scheme B chargercomms on Intelligent Charger Case dev board.
@{ */
#define SCHEME_B_PULL_CTRL          (17)
#define SCHEME_B_DC_ENABLE_PIO      (18)
#define SCHEME_B_DC_SELECT_PIO      (19)
#define SCHEME_B_ISOLATION          (20)
/*!@} */

void ccProtocol_TransSchemeBHwSetup(void)
{
    cc_protocol_t* td = CcProtocol_GetTaskData();

    /* On the case supply PIOs to P0 to control VCHG (DC Enable), Pull Control
       to raise 1v8 comms level and isolation. */
    if (td->mode == CASECOMMS_MODE_CASE)
    {
        PioSetMapPins32Bank(PioCommonPioBank(SCHEME_B_DC_ENABLE_PIO),
                            PioCommonPioMask(SCHEME_B_DC_ENABLE_PIO),
                            0);
        PanicFalse(PioSetFunction(SCHEME_B_DC_ENABLE_PIO, CHARGER_COMMS_VCHG_REG));

        PioSetMapPins32Bank(PioCommonPioBank(SCHEME_B_ISOLATION),
                            PioCommonPioMask(SCHEME_B_ISOLATION),
                            0);
        PanicFalse(PioSetFunction(SCHEME_B_ISOLATION, CHARGER_COMMS_ISOLATE));

        PioSetMapPins32Bank(PioCommonPioBank(SCHEME_B_PULL_CTRL),
                            PioCommonPioMask(SCHEME_B_PULL_CTRL),
                            0);
        PanicFalse(PioSetFunction(SCHEME_B_PULL_CTRL, CHARGER_COMMS_PULL));
    }
}

#endif /* HAVE_CC_TRANS_SCHEME_B */
#endif /* INCLUDE_CASE_COMMS */
/*! @} End of group documentation */
