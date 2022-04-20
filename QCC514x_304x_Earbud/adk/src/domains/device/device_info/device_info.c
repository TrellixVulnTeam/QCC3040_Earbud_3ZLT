/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief
*/

#include "device_info.h"
#include "device_info_config.h"

#include <panic.h>
#include <ps.h>
#include "local_name.h"

static const char device_hardware_version[] = DEVICE_HARDWARE_VERSION;
static const char device_firmware_version[] = DEVICE_FIRMWARE_VERSION  _DIC_STRINGIFY_(DEVICE_BUILDID);
static const char device_current_language[] = DEVICE_CURRENT_LANGUAGE;
static const char device_software_version[] = DEVICE_SOFTWARE_VERSION;

#define PSKEY_USB_MANUF_STRING          705
#define PSKEY_USB_PRODUCT_STRING        706
#define PSKEY_USB_SERIAL_NUMBER_STRING  707

static struct
{
    const char *manufacturer;
    const char *model_id;
    const char *serial_number;
} device = {NULL};

static char * deviceInfo_ReadPsData(uint16 pskey)
{
    uint16 length = 0;
    char* buffer = NULL;
    length = PsFullRetrieve(pskey, NULL, 0)*sizeof(uint16);
    buffer = PanicUnlessMalloc(length + 1);
    PsFullRetrieve(pskey, buffer, length);
    buffer[length] = 0;
    return buffer;
}

static const char * deviceInfo_GetLoadedPsData(const char **loaded_data, uint16 pskey)
{
    if (*loaded_data == NULL)
        *loaded_data = deviceInfo_ReadPsData(pskey);

    return *loaded_data;
}

const char * DeviceInfo_GetName(void)
{
    uint16 name_length;
    return (const char *)LocalName_GetName(&name_length);
}

const char * DeviceInfo_GetManufacturer(void)
{
    return deviceInfo_GetLoadedPsData(&device.manufacturer, PSKEY_USB_MANUF_STRING);
}

const char * DeviceInfo_GetModelId(void)
{
    return deviceInfo_GetLoadedPsData(&device.model_id, PSKEY_USB_PRODUCT_STRING);
}

const char * DeviceInfo_GetHardwareVersion(void)
{
    return device_hardware_version;
}

const char * DeviceInfo_GetFirmwareVersion(void)
{
    return device_firmware_version;
}

const char * DeviceInfo_GetSerialNumber(void)
{
    return deviceInfo_GetLoadedPsData(&device.serial_number, PSKEY_USB_SERIAL_NUMBER_STRING);
}

const char * DeviceInfo_GetCurrentLanguage(void)
{
    return device_current_language;
}

const char * DeviceInfo_GetSoftwareVersion(void)
{
    return device_software_version;
}
