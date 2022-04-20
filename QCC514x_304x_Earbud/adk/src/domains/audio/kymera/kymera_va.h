/*!
\copyright  Copyright (c) 2019-2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to handle Voice Assistant related internal APIs

*/

#ifndef KYMERA_VA_H_
#define KYMERA_VA_H_

#include "kymera.h"
#include "va_audio_types.h"
#include <source.h>

/*! \brief To be called during components initialisation.
*/
void Kymera_VaInit(void);

/*! \brief Start capturing/encoding live mic data.
    \param params Parameters based on which the voice capture will be configured.
    \return The output of the capture chain as a mapped Source or NULL if it fails.
*/
Source Kymera_StartVaLiveCapture(const va_audio_voice_capture_params_t *params);

/*! \brief Stop capturing/encoding mic data.
    \return True on success.
*/
bool Kymera_StopVaCapture(void);

/*! \brief Start Wake-Up-Word detection.
    \param wuw_detection_handler Task to receive the Wake-Up-Word detection message from audio.
    \param params Parameters based on which the Wake-Up-Word detection will be configured.
    \return True on success.
*/
bool Kymera_StartVaWuwDetection(Task wuw_detection_handler, const va_audio_wuw_detection_params_t *params);

/*! \brief Stop Wake-Up-Word detection.
    \return True on success.
*/
bool Kymera_StopVaWuwDetection(void);

/*! \brief Must immediately be called after a Wake-Up-Word detection message from audio.
    \return True on success.
 */
bool Kymera_VaWuwDetected(void);

/*! \brief Start capturing/encoding wuw and live mic data.
           Must be called as a result of a Wake-Up-Word detection message from audio.
    \param params Parameters based on which the voice capture will be configured.
    \return The output of the capture chain as a mapped Source.
*/
Source Kymera_StartVaWuwCapture(const va_audio_wuw_capture_params_t *params);

/*! \brief Must be called when WuW is detected but ignored (capture is not started).
    \return True on success.
 */
bool Kymera_IgnoreDetectedVaWuw(void);

/*! \brief Check if capture is active.
    \return True on success.
*/
bool Kymera_IsVaCaptureActive(void);

/*! \brief Check if Wake-Up-Word detection is active.
*/
bool Kymera_IsVaWuwDetectionActive(void);

/*! \brief Check if Voice Assistant is active.
*/
bool Kymera_IsVaActive(void);

/*! \brief Get the minimum clock required for the DSP to run the currently active VA chain
 *  \return The minimum clock setting for the DSP
*/
audio_dsp_clock_type Kymera_VaGetMinDspClock(void);

/*! \brief Get the minimum low power clock speed in MHz required to run the currently active VA chain
 *  \return The minimum low power mode clock speed for VA in MHz
*/
uint8 Kymera_VaGetMinLpClockSpeedMhz(void);

#endif /* KYMERA_VA_H_ */
