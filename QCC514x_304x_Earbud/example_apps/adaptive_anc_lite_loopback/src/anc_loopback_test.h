/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file       anc_loopback_test.h
\brief      anc loopback test header file
*/

#ifndef ANC_LOOPBACK_TEST_H_
#define ANC_LOOPBACK_TEST_H_

enum anc_loopback
{
    APP_SETUP_ANC,
    APP_ENABLE_ADAPTIVITY,
    APP_DISABLE_ADAPTIVITY,
    APP_RESTART_ADAPTIVITY
};

/*! \brief Initialises different microphones required for ANC loopback application.
    \param     void.
    \return    void.
*/
void Ancloopback_Setup(void);


/*! \brief configures and starts the AANC lite operator.Once started feedforward fine gain of ANC
           is ramped from 0 to 255 independent of mic data.
    \param     void.
    \return    void.
*/
void Ancloopback_EnableAdaptivity(void);

/*! \brief Deconfigures and destroys the AANC lite operator.
    \param     void.
    \return    void.
*/
void Ancloopback_DisableAdaptivity(void);


/*! \brief Updates UCID for AANC lite operator. The updating of UCID restarts ramp of feedforward fine gain
          from 0 to 255.
    \param     void.
    \return    void.
*/
void Ancloopback_RestartAdaptivity(void);

/*! \brief Gets the microphone bias voltage
    \param     microphone bias id
    \return    bias voltage.
*/
unsigned Ancloopback_GetMicrophoneBiasVoltage(mic_bias_id id);

#endif

