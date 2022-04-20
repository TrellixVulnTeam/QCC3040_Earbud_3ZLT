/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\defgroup   focus_domains Focus
\ingroup    domains
\brief      Focus interface definition for instantiating a module which shall
            return the focussed remote device.
*/
#ifndef FOCUS_DEVICE_H
#define FOCUS_DEVICE_H

#include "focus_types.h"

#include <device.h>
#include <ui_inputs.h>
#include <bdaddr.h>

/*! \brief Focus interface callback used by Focus_GetDeviceForContext API */
typedef bool (*focus_device_for_context_t)(ui_providers_t provider, device_t* device);

/*! \brief Focus interface callback used by Focus_GetDeviceForUiInput API */
typedef bool (*focus_device_for_ui_input_t)(ui_input_t ui_input, device_t* device);

/*! \brief Focus interface callback used by Focus_GetFocusForDevice API */
typedef focus_t (*focus_for_device_t)(const device_t device);

/*! \brief Focus interface callback used by Focus_ExcludeDevice API */
typedef bool (*focus_device_add_to_excludelist_t)(device_t device);

/*! \brief Focus interface callback used by Focus_IncludeDevice API */
typedef bool (*focus_device_remove_from_excludelist_t)(device_t device);

/*! \brief Focus interface callback used by Focus_ResetExcludedDevices API */
typedef void (*focus_device_reset_excludelist_t)(void);

/*! \brief Structure used to configure the focus interface callbacks to be used
           to access the focussed device. And also to add/remove device to/from Exclude List. */
typedef struct
{
    focus_device_for_context_t for_context;
    focus_device_for_ui_input_t for_ui_input;
    focus_for_device_t focus;
    focus_device_add_to_excludelist_t add_to_excludelist;
    focus_device_remove_from_excludelist_t remove_from_excludelist;
    focus_device_reset_excludelist_t reset_excludelist;
} focus_device_t;

/*! \brief Configure a set of function pointers to use for retrieving the focussed device

    \param a structure containing the functions implementing the focus interface for retrieving
           the focussed device.
*/
void Focus_ConfigureDevice(focus_device_t const * focus_device);

/*! \brief Get the focussed device for which to query the context of the specified UI Provider

    \param provider - a UI Provider
    \param device - a pointer to the focussed device_t handle
    \return a bool indicating whether or not a focussed device was returned in the
            device parameter
*/
bool Focus_GetDeviceForContext(ui_providers_t provider, device_t* device);

/*! \brief Get the focussed device that should consume the specified UI Input

    \param ui_input - the UI Input that shall be consumed
    \param device - a pointer to the focussed device_t handle
    \return a bool indicating whether or not a focussed device was returned in the
            device parameter
*/
bool Focus_GetDeviceForUiInput(ui_input_t ui_input, device_t* device);

/*! \brief Get the current focus status for the specified device

    \param device - the device_t handle
    \return the focus status of the specified device
*/
focus_t Focus_GetFocusForDevice(const device_t device);


/*! \brief Set the BT device as excluded once ACL connection fails or connected.
    \note  if NULL device_t handle supplied then it is ignored and device is
           not added to excludelist. device_t handle can be NULL for LE AG or
           when device_t handle has been removed from database.

    \param device - a pointer to the focussed device_t handle to be excluded.

    \return a bool indicating whether or not a device is added to excludelist.
*/
bool Focus_ExcludeDevice(device_t device);

/*! \brief Remove the Bluetooth address from excludelist.
    \note  if NULL device_t handle supplied then it is ignored and device is
           not removed from excludelist. device_t handle can be NULL for LE AG
           or when device_t handle has been removed from database.

    \param device - a pointer to the focussed device_t handle to be removed from excludelist.

    \return a bool indicating whether or not a device is removed from excludelist.
*/
bool Focus_IncludeDevice(device_t device);

/*! \brief Reset the excludelist.

    \note  Connected(ACL) device will not be removed from excludelist.
*/
void Focus_ResetExcludedDevices(void);

#endif /* FOCUS_DEVICE_H */
