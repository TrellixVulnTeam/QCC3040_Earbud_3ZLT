/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      AV instance management
*/

#ifndef AV_INSTANCE_H_
#define AV_INSTANCE_H_

#include "av_typedef.h"
#include "device.h"

/*! Maximum number of AV connections for TWS */
#define AV_MAX_NUM_TWS (1)

/*! Maximum number of AV connections for audio Sinks */
#define AV_MAX_NUM_SNK (2)

/*! \brief Maximum number of AV connections

    Defines the maximum number of simultaneous AV connections that are allowed.

    This is based on the number of links we want as TWS or as a standard Sink.
*/
#define AV_MAX_NUM_INSTANCES (AV_MAX_NUM_TWS + AV_MAX_NUM_SNK)

#define for_all_av_instances(av_instance, iterator) for(av_instance = AvInstance_GetFirst(iterator); av_instance != NULL; av_instance = AvInstance_GetNext(iterator))

typedef struct
{
    avInstanceTaskData* instances[AV_MAX_NUM_INSTANCES];
    unsigned index;
} av_instance_iterator_t;

/*! \brief Initialise the iterator and return the first AV instance

    \note This function will populate the iterator with a list of all non-NULL
    AV instances associated with devices in the device list. 

    \warning Once the iterator has been populated the device list may change, 
    using iterators for anything other than atomic access may result in missing
    new AV instances or attempting to access invalid AV instances

    \param  iterator    The iterator to initialise and use

    \return The first avInstanceTaskData* found or NULL if there are none.
*/
avInstanceTaskData* AvInstance_GetFirst(av_instance_iterator_t* iterator);

/*! \brief Progress the iterator to the next AV instance and return

    \note This function will progress the iterator to the next AV instance

    \param  iterator    The iterator to use

    \return The next avInstanceTaskData* found or NULL if there are none remaining
*/
avInstanceTaskData* AvInstance_GetNext(av_instance_iterator_t* iterator);

/*! \brief Get the AV instance associated with a device

    \param  device The device

    \return The avInstanceTaskData* associated with the device or NULL if
    there is no instance associated with the device
*/
avInstanceTaskData* AvInstance_GetInstanceForDevice(device_t device);

/*! \brief Set the AV instance associated with a device

    \param  device The device
    \param  av_instance The avInstanceTaskData* to associate with the device
*/
void AvInstance_SetInstanceForDevice(device_t device, avInstanceTaskData* av_instance);

/*! \brief Find AV instance for the focus handset

    \note This function returns the AV. It does not check for an
            active connection, or whether A2DP/AVRCP exists.

    \return Pointer to AV data that matches the handset device currently
            in focus. NULL if none was found.
*/
avInstanceTaskData *AvInstance_FindFromFocusHandset(void);

/*! \brief Obtain the A2DP sink av_instance associated with a source

    \param  source The source to find the instance for.

    \return pointer to the A2DP sink instance associated with source. NULL if none
*/
avInstanceTaskData* AvInstance_GetSinkInstanceForAudioSource(audio_source_t source);

/*! \brief Register media control interface for an AV instance.
    \param The AV instance.
*/
void AvInstance_RegisterMediaControlInterfaceForInstance(avInstanceTaskData *av_inst);


#endif /* AV_INSTANCE_H_ */
