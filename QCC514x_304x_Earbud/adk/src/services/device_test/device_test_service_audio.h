/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    device_test_service
\brief      Local functions related to audio used in the Device Test Service.
*/
/*! \addtogroup device_test_service
 @{
*/

#include <kymera.h>

/*! Check the validity of speaker requested

    The function checks if the value given for the speaker is valid.

    The valid speakers may be
    \li restricted by the chip hardware (can be detected here)
    \li restricted by the board hardware (can probably be detected here)
    \li restricted by the physical board configuration (not detectable here)

    Initially the speaker parameter was encoded as follows

            ...HHHIICC
        3 bits for the hardware
        2 bits for the instance
        2 bits for the channel

    The ranges can be found by looking at the values defined 
    in app\audio\audio_if.h

    \param speaker The speaker defined in the command. This encodes the 
                    speaker hardware and can vary based on platform.
    \param[out] output A populated output type, IF the speaker was valid

    \return TRUE if the requested speaker is valid, FALSE otherwise.
 */
bool DeviceTestService_SpeakerExists(uint16 speaker, appKymeraHardwareOutput *output);

/*! Check the validity of the microphone requested

    The function checks if the value given for the microphone is valid.

    The valid microphone may be
    \li restricted by the chip hardware (can be detected here)
    \li restricted by the board hardware (can probably be detected here)
    \li restricted by the physical board configuration (not detectable here)

    \param microphone The microphone defined in the command. This encodes the 
                    microphone hardware and can vary based on platform.

    \return TRUE if the requested microphone is valid, FALSE otherwise.
 */
bool DeviceTestService_MicrophoneExists(uint16 microphone);


/*! @} End of group documentation */
