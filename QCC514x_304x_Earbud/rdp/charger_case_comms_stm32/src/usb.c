/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include "stm32f0xx_hal.h"
#include "usb_device.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "main.h"
#include "power.h"
#include "clock.h"
#include "cmsis.h"
#include "config.h"
#include "usb.h"
#include "stm32f0xx_hal_pcd.h"
#include "charger_detect.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* These should always be powers of 2 in order to simplify calculations and
* save memory.
*/
#define USB_RX_BUFFER_SIZE 1024
#define USB_TX_BUFFER_SIZE 512

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static bool usb_is_ready = false;
static bool usb_data_to_send = false;

static uint8_t usb_rx_buf[USB_RX_BUFFER_SIZE] = {0};
static uint8_t usb_tx_buf[USB_TX_BUFFER_SIZE] = {0};

static uint16_t usb_rx_buf_head = 0;
static uint16_t usb_rx_buf_tail = 0;
static uint16_t usb_tx_buf_head = 0;
static uint16_t usb_tx_buf_tail = 0;

USBD_HandleTypeDef hUsbDeviceFS;
extern PCD_HandleTypeDef hpcd_USB_FS;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void SystemClock_Config(void)
{
    /* Configure the USB clock source */
    RCC->CFGR3 |= RCC_CFGR3_USBSW;
}

void usb_init(void)
{
    SystemClock_Config();

    if (USBD_Init(&hUsbDeviceFS, (USBD_DescriptorsTypeDef *)&FS_Desc, DEVICE_FS) == USBD_OK)
    {
        if (USBD_RegisterClass(&hUsbDeviceFS, (USBD_ClassTypeDef *)&USBD_CDC) == USBD_OK)
        {
            if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) == USBD_OK)
            {
                PRINT_B("USB initialised");
            }
        }
    }
}

void usb_start(void)
{
    if (USBD_Start(&hUsbDeviceFS) == USBD_OK)
    {
        PRINT_B("USB started");
    }
}

void usb_stop(void)
{
    if (USBD_Stop(&hUsbDeviceFS) == USBD_OK)
    {
        if (USBD_DeInit(&hUsbDeviceFS) == USBD_OK)
        {
            usb_not_ready();
            PRINT_B("USB stopped");
        }
    }
    RCC->CFGR3 &= ~RCC_CFGR3_USBSW;
}

void usb_tx(uint8_t *data, uint16_t len)
{
    usb_data_to_send = true;

    /*
    * Put all the data in the buffer to be sent.
    */
    while (len--)
    {
        uint16_t next_head = (uint16_t)((usb_tx_buf_head + 1) & (USB_TX_BUFFER_SIZE-1));

        if (next_head == usb_tx_buf_tail)
        {
            /*
            * Not enough room in the buffer, so give up. Data output will be
            * truncated.
            */
            break;
        }

        usb_tx_buf[usb_tx_buf_head] = *data++;
        usb_tx_buf_head = next_head;
    }

    if (usb_is_ready)
    {
        power_set_run_reason(POWER_RUN_USB_TX);
    }
}

void usb_rx(uint8_t *data, uint32_t len)
{
    uint32_t n;

    power_set_run_reason(POWER_RUN_USB_RX);
    for (n=0; n<len; n++)
    {
        uint16_t next_head = (usb_rx_buf_head + 1) & (USB_RX_BUFFER_SIZE - 1);

        if (next_head == usb_rx_buf_tail)
        {
            /*
            * Not enough room in the buffer, so give up.
            */
            break;
        }
        else
        {
            usb_rx_buf[usb_rx_buf_head] = data[n];
            usb_rx_buf_head = next_head;
        }
    }
}

void usb_tx_complete(void)
{
    usb_data_to_send = false;
    power_clear_run_reason(POWER_RUN_USB_TX);
}

void usb_tx_periodic(void)
{
    uint16_t head = usb_tx_buf_head;
    uint16_t tail = usb_tx_buf_tail;

    /*
    * Kick off any data to be sent.
    */
    if (usb_is_ready && (head != tail))
    {
        uint16_t len;

        if (tail > head)
        {
            len = USB_TX_BUFFER_SIZE - tail;
        }
        else
        {
            len = head - tail;
        }

        if (CDC_Transmit_FS((uint8_t *)&usb_tx_buf[tail], len)==USBD_OK)
        {
            usb_tx_buf_tail = (usb_tx_buf_tail + len) & (USB_TX_BUFFER_SIZE - 1);
        }
    }
}

void usb_rx_periodic(void)
{
    /*
    * Handle received data.
    */
    while (usb_rx_buf_head != usb_rx_buf_tail)
    {
        cli_rx(CLI_SOURCE_USB, (char)usb_rx_buf[usb_rx_buf_tail]);
        usb_rx_buf_tail = (usb_rx_buf_tail + 1) & (USB_RX_BUFFER_SIZE - 1);
    }
    power_clear_run_reason(POWER_RUN_USB_RX);
}

void Error_Handler(void)
{
}

void USB_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&hpcd_USB_FS);
}

void usb_ready(void)
{
    if (!usb_is_ready)
    {
        PRINT_B("USB ready");
        usb_is_ready = true;

        if (usb_data_to_send)
        {
            power_set_run_reason(POWER_RUN_USB_TX);
        }
    }
}

void usb_not_ready(void)
{
    if (usb_is_ready)
    {
        PRINT_B("USB not ready");
        power_clear_run_reason(POWER_RUN_USB_TX);
        usb_is_ready = false;
    }
}

bool usb_has_enumerated(void)
{
    return hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED;
}

void usb_chg_detected(void)
{
    usb_connected();
    /* Start USB charger detection */
    charger_detect_start();
}

void usb_activate_bcd(void)
{
    USB->BCDR = 0;
    (void)HAL_PCDEx_ActivateBCD(&hpcd_USB_FS);
}

void usb_deactivate_bcd(void)
{
    (void)HAL_PCDEx_DeActivateBCD(&hpcd_USB_FS);
}

/*
* Returns true if the USB data line made contact and false otherwise.
*/
bool usb_dcd(void)
{
    return (USB->BCDR & USB_BCDR_DCDET) ? true:false;
}

/*
* Returns true if D- > V_DAT_REF, false otherwise.
*/
bool usb_pdet(void)
{
    return (USB->BCDR & USB_BCDR_PDET) ? true:false;
}

/*
* Returns true if D+ > V_DAT_REF, false otherwise.
*/
bool usb_sdet(void)
{
    return (USB->BCDR & USB_BCDR_SDET) ? true:false;
}

/*
* Disable Data Contact Detection mode.
*/
void usb_dcd_disable(void)
{
    USB->BCDR &= ~USB_BCDR_DCDEN;
}

/*
* Enable Primary Detection mode.
*/
void usb_primary_detection_enable(void)
{
    USB->BCDR |= USB_BCDR_PDEN;
}

/*
* Disable Primary Detection mode.
*/
void usb_primary_detection_disable(void)
{
    USB->BCDR &= ~USB_BCDR_PDEN;
}

/*
* Enable Secondary Detection mode.
*/
void usb_secondary_detection_enable(void)
{
    USB->BCDR |= USB_BCDR_SDEN;
}

/*
* Disable Secondary Detection mode.
*/
void usb_secondary_detection_disable(void)
{
    USB->BCDR &= ~USB_BCDR_SDEN;
}

void usb_connected(void)
{
#if !defined(FORCE_48MHZ_CLOCK)
    DISABLE_IRQ();
    clock_change(CLOCK_48MHZ);
    ENABLE_IRQ();
#endif
    usb_init();
}

void usb_disconnected(void)
{
    usb_stop();
#if !defined(FORCE_48MHZ_CLOCK)
    DISABLE_IRQ();
    clock_change(CLOCK_8MHZ);
    ENABLE_IRQ();
#endif
}

uint64_t usb_serial_num(void)
{
    return config_get_serial();
}
