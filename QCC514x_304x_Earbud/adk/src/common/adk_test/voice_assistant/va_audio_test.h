/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    adk_test_common
\brief      Interface for voice assistant audio testing functions.
            These override the VA protocols and are not meant to be used to drive VA feature (they are audio specific test APIs).
            They should not be used while an assistant is connected/used (they will override the audio control from the VA protocol and vice versa).
*/

/*! @{ */

#ifndef VA_AUDIO_TEST_H_
#define VA_AUDIO_TEST_H_

#include "va_audio_types.h"

/*! \brief Start VA audio capture (same type of capture used in PTT or TTT use cases).
    \param encoder The encoding format for the captured data.
    \num_of_mics The number of microphones to attempt to use (the closest number supported will be used).
    \return FALSE if a capture is already ongoing or if the configuration is not supported, TRUE otherwise.
*/
bool appTestStartVaCapture(va_audio_codec_t encoder, unsigned num_of_mics);

/*! \brief Stop any existing VA audio capture.
    \return TRUE if a capture was stopped, FALSE otherwise.
*/
bool appTestStopVaCapture(void);

#ifdef INCLUDE_WUW
/*! \brief Start VA WuW detection.
    \param wuw_engine The WuW engine to be used for the detection.
    \param num_of_mics The number of microphones to attempt to use (the closest number supported will be used).
    \param start_capture_on_detection If TRUE a VA audio capture will start (same type of capture used in WuW use cases).
           If FALSE the detection will be ignored and the engine reset so that it can be triggered again and again.
    \param encoder The encoding format for the captured data in case of an audio capture.
    \return FALSE if a capture or detection is already ongoing or if the configuration is not supported, TRUE otherwise.
*/
bool appTestStartVaWakeUpWordDetection(va_wuw_engine_t wuw_engine, unsigned num_of_mics, bool start_capture_on_detection, va_audio_codec_t encoder);

/*! \brief Stop any existing VA WuW detection.
    \return TRUE if a detection was stopped, FALSE otherwise.
*/
bool appTestStopVaWakeUpWordDetection(void);
#endif

#endif /* VA_AUDIO_TEST_H_ */

/*! @} */
