/****************************************************************************
Copyright (c) 2004 - 2016 Qualcomm Technologies International, Ltd.

*/
/*!
    @file   gain_utils.h

    @brief Header file for the gain utility library. This library implements several utility volume
    functions
 
 */

/*@{*/

#ifndef GAIN_UTILS_H_
#define GAIN_UTILS_H_

#include <csrtypes.h>

/* number of DAC steps */
#define CODEC_STEPS             15

/* DSP gain values are set in dB/60 */ 
#define DB_DSP_SCALING_FACTOR 60

/* Macro to convert dB to dB/60 */
#define GainIn60thdB(gain_db) ((gain_db) * DB_DSP_SCALING_FACTOR)

/* DAC steps of 3 dB * scaling factor of dsp volume control which is 60 */
#define DB_TO_DAC                           GainIn60thdB(3)

/* mute is -120dB */
#define DIGITAL_VOLUME_MUTE                 GainIn60thdB(-120)

#define RAW_GAIN_DIGITAL_COMPONENT_0_GAIN    0x8020

/*!
    @brief Calculate the analogue component of a raw codec gain value.

    @param dB_60 gain in dB/60.

    @return db_60 expressed as a analogue component of a raw gain value.
*/
uint16 gainUtilsCalculateRawAdcGainAnalogueComponent(uint16 dB_60);


#endif /* GAIN_UTILS_H_ */
/*@}*/
