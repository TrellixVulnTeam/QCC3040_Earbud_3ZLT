/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Header file for USB application framework
*/

#ifndef USB_APPLICATION_H_
#define USB_APPLICATION_H_

#include "usb_device.h"

/*! USB Application interface */
typedef struct
{
    /*! Register USB classes, configuration and event handlers */
    void (*Create)(usb_device_index_t dev_index);
    /*! Attach to the host */
    void (*Attach)(usb_device_index_t dev_index);
    /*! Detach from the host */
    void (*Detach)(usb_device_index_t dev_index);
    /*! Destroy application and free any allocated memory */
    void (*Destroy)(usb_device_index_t dev_index);
} usb_app_interface_t;

/*! Open application referenced by a pointer to application interface
 *
 * If there is currently active application it is detached and destroyed first.
 * Then the new application is created and attached using application interface
 * callbacks.
 *
 * If the application provided is already active, function does nothing.
 *
 * \param app pointer to USB application interface
 */
void UsbApplication_Open(const usb_app_interface_t *app);

/*! Detach and destroy currently active application */
void UsbApplication_Close(void);

/*! \brief Attaches USB application device to the internal hub
 *  Once attached device becomes visible to the host and can now be enumerated.
*/
void UsbApplication_Attach(void);

/*! \brief Detaches USB application device from the internal hub*/
void UsbApplication_Detach(void);

/*! \brief To check whether USB application device is attached to internal hub*/
bool UsbApplication_IsAttachedToHub(void);

/*! \brief To check whether USB application device is connected to host*/
bool UsbApplication_IsConnectedToHost(void);

/*! \brief To get latest USB application asked to open*/
const usb_app_interface_t *UsbApplication_GetActiveApp(void);

#endif /* USB_APPLICATION_H_ */
