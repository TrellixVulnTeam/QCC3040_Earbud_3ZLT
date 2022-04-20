/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   source_prediction Audio Sources
\ingroup    audio_domain
\brief      The source prediction component provides generic API to predict audio sources and its parameters.
*/

#ifndef SOURCE_PREDICTION_H_
#define SOURCE_PREDICTION_H_

/*! \brief Predict the possible A2DP parameters
    \param rate sample rate
    \param seid stream endpoint identifier
    \return bool TRUE if parameters are valid
 */
bool SourcePrediction_GetA2dpParametersPrediction(uint32 *rate, uint8 *seid);

#endif /* SOURCE_PREDICTION_H_ */
