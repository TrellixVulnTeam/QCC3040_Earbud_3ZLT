/*!
\copyright  Copyright (c) 2005 - 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief	    Header file for the production test mode
*/

#ifndef EARBUD_PRODUCTION_TEST_H_
#define EARBUD_PRODUCTION_TEST_H_

#ifdef PRODUCTION_TEST_MODE

/*! \brief SM boot mode */
typedef enum
{
    sm_boot_normal_mode,
    sm_boot_production_test_mode,
    sm_boot_mute_mode,

} sm_boot_mode_t;

/*! \brief Handle request to enter FCC test mode.
*/
void appSmHandleInternalEnterProductionTestMode(void);

/*! \brief Handle request to enter DUT test mode.
*/
void appSmHandleInternalEnterDUTTestMode(void);

/*! \brief Request To enter Production Test mode
*/
void appSmEnterProductionTestMode(void);

/*! \brief Write Test mode PS Key
*/
void appSmTestService_SaveBootMode(sm_boot_mode_t mode);

/*! \brief Check boot mode, expose API so earbud UI can rely on this
    to play production test tones
*/
sm_boot_mode_t appSmTestService_BootMode(void);
/*! \brief Write Test Step
*/

void appSmTestService_SetTestStep(uint8 step);

#else
#define appSmTestService_BootMode(void) FALSE
#define appSmTestService_SaveBootMode(mode) (UNUSED(mode))
#define appSmTestService_SetTestStep(step) (UNUSED(step))

#endif /*PRODUCTION_TEST_MODE*/

#endif /*EARBUD_PRODUCTION_TEST_H_*/
