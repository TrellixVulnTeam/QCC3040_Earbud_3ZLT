/*!
\copyright  Copyright (c) 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      SCSI support for USB mass storage class
*/

#include "usb_msc_common.h"

typedef enum
{
    BYTES_SCSI6,
    BYTES_SCSI10,
    BYTES_SCSI12
} bytes_scsi;

/* Standard SCSI INQUIRY response */
static const usb_msc_inquiry_response_t inquiry_response = {
    {0x00}, /* peripheral direct access device */
    {0x80}, /* removable media */
    {0x04}, /* version */
    {0x02}, /* response data format */
    {0x20}, /* additional length */
    {0x00},
    {0x00},
    {0x00},
    {' ',' ',' ',' ',' ',' ',' ',' '}, /* vendor ID */
    {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '}, /* product ID */
    {'0','0','0','1'} /* product revision */
};

static uint16 scsi_inquiry(uint8 *data, uint32 dataXferLen)
{
    uint16 allocation_length = (data[2] << 8) | data[3];

    DEBUG_LOG_INFO("UsbMsc: SCSI_INQUIRY alloc_len: %d", allocation_length);

    uint16 data_length = MIN(allocation_length, dataXferLen);
    data_length = MIN(data_length, SIZE_INQUIRY_RESPONSE);

    UsbMsc_SendBulkData((uint8 *)&inquiry_response, data_length);

    return CSW_STATUS_PASSED;
}

static uint16 scsi_request_response(uint8 *data, uint32 dataXferLen)
{
    uint16 allocation_length = data[3];

    DEBUG_LOG_INFO("UsbMsc: SCSI_REQUEST_SENSE alloc_len: %d",
            allocation_length);

    uint16 data_length = MIN(allocation_length, dataXferLen);
    data_length = MIN(data_length, SIZE_REQUEST_SENSE_RESPONSE);

    UsbMsc_SendBulkData((uint8 *)&msc_class_data->req_sense_rsp, data_length);

    return CSW_STATUS_PASSED;
}

static uint16 scsi_read(uint8 *data, uint32 dataXferLen, bytes_scsi version)
{
    uint32 lba = (((uint32)data[1] << 24) & 0xff000000ul) |
                 (((uint32)data[2] << 16) & 0xff0000) |
                 (((uint32)data[3] << 8) & 0xff00) |
                          (data[4] & 0xff);
    uint32 transfer_length;
    uint16 status = CSW_STATUS_PASSED;

    if (version == BYTES_SCSI12)
        transfer_length = (((uint32)data[5] << 24) & 0xff000000ul) |
                          (((uint32)data[6] << 16) & 0xff0000) |
                          (((uint32)data[7] << 8) & 0xff00) |
                                   (data[8] & 0xff);
    else
        transfer_length = (((uint16)data[6] << 8) & 0xff00) |
                                   (data[7] & 0xff);

    DEBUG_LOG_INFO("UsbMsc: SCSI_READ(%d) lba %ld len %ld",
            version, lba, transfer_length);

    if ((transfer_length * UsbMsc_Fat16_GetBlockSize()) > dataXferLen)
    {
        /* the amount of data that needs to be sent to the host exceeds
         * the amount the host wants to receive, so clip the total read blocks
         * to fit the hosts requirements
        */
        transfer_length = dataXferLen / UsbMsc_Fat16_GetBlockSize();
        status = CSW_STATUS_PHASE_ERROR;
    }

    if (transfer_length)
    {
        UsbMsc_Fat16_Read(msc_class_data, lba, transfer_length);
    }

    return status;
}

static uint16 scsi_read_capacity10(uint32 dataXferLen)
{
    usb_msc_read_capacity10_response_t response;
    uint32 block_size = UsbMsc_Fat16_GetBlockSize();
    uint32 last_lba = UsbMsc_Fat16_GetTotalBlocks() - 1;

    DEBUG_LOG_INFO("UsbMsc: SCSI_READ_CAPACITY10, "
            "returned block_size %ld last_lba %ld",
            block_size, last_lba);

    response.LBA[0] = (last_lba & 0xff000000ul) >> 24;
    response.LBA[1] = (last_lba & 0xff0000) >> 16;
    response.LBA[2] = (last_lba & 0xff00) >> 8;
    response.LBA[3] = last_lba & 0xff;

    response.BlockLength[0] = (block_size & 0xff000000ul) >> 24;
    response.BlockLength[1] = (block_size & 0xff0000) >> 16;
    response.BlockLength[2] = (block_size & 0xff00) >> 8;
    response.BlockLength[3] = block_size & 0xff;

    UsbMsc_SendBulkData((uint8 *)&response,
            MIN(dataXferLen, SIZE_READ_CAPACITY10_RESPONSE));

    return CSW_STATUS_PASSED;
}

static uint16 scsi_read_capacity16(uint32 dataXferLen)
{
    usb_msc_read_capacity16_response_t response;
    uint32 block_size = UsbMsc_Fat16_GetBlockSize();
    uint32 last_lba = UsbMsc_Fat16_GetTotalBlocks() - 1;

    response.LBA[0] = 0x00;
    response.LBA[1] = 0x00;
    response.LBA[2] = 0x00;
    response.LBA[3] = 0x00;
    response.LBA[4] = (last_lba & 0xff000000ul) >> 24;
    response.LBA[5] = (last_lba & 0xff0000) >> 16;
    response.LBA[6] = (last_lba & 0xff00) >> 8;
    response.LBA[7] = last_lba & 0xff;

    response.BlockLength[0] = (block_size & 0xff000000ul) >> 24;
    response.BlockLength[1] = (block_size & 0xff0000) >> 16;
    response.BlockLength[2] = (block_size & 0xff00) >> 8;
    response.BlockLength[3] = block_size & 0xff;

    response.ProtPType[0] = 0x00;   /* no protection */

    DEBUG_LOG_INFO("UsbMsc: SCSI_READ_CAPACITY16, "
            "returned block_size %ld last_lba %ld",
            block_size, last_lba);

    memset(&response.Reserved, 0, sizeof(response.Reserved));

    if (dataXferLen < SIZE_READ_CAPACITY16_RESPONSE)
        UsbMsc_SendBulkData((uint8 *)&response, dataXferLen);
    else
        UsbMsc_SendBulkData((uint8 *)&response, SIZE_READ_CAPACITY16_RESPONSE);

    return CSW_STATUS_PASSED;
}

static uint16 scsi_read_format_capacities(uint32 dataXferLen)
{
    usb_msc_read_format_capacities_response_t response;
    uint32 block_size = UsbMsc_Fat16_GetBlockSize();
    uint32 total_blocks = UsbMsc_Fat16_GetTotalBlocks();

    DEBUG_LOG_INFO("UsbMsc: SCSI_READ_FORMAT_CAPACITIES, "
            "returned block_size %ld total_blocks %ld",
            block_size, total_blocks);

    /* Capacity List Header */
    response.CapacityListHeader[0] = 0x00;
    response.CapacityListHeader[1] = 0x00;
    response.CapacityListHeader[2] = 0x00;
    response.CapacityListHeader[3] = 0x10; /* capacity list length */

    /* Current Capacity Header */
    /* Number of Blocks - 4 bytes */
    response.CurrentMaximumCapacityHeader[0] = (total_blocks & 0xff000000ul) >> 24;
    response.CurrentMaximumCapacityHeader[1] = (total_blocks & 0xff0000) >> 16;
    response.CurrentMaximumCapacityHeader[2] = (total_blocks & 0xff00) >> 8;
    response.CurrentMaximumCapacityHeader[3] = total_blocks & 0xff;
    /* Descriptor Code */
    response.CurrentMaximumCapacityHeader[4] = 0x02; /* Formatted Media - Current media capacity */
    /* Block Length */
    response.CurrentMaximumCapacityHeader[5] = (block_size & 0xff0000) >> 16;
    response.CurrentMaximumCapacityHeader[6] = (block_size & 0xff00) >> 8;
    response.CurrentMaximumCapacityHeader[7] = block_size & 0xff;

    /* Formattable Capacity Descriptor */
    /* Number of Blocks - 4 bytes */
    response.FormattableCapacityDescriptor[0] = (total_blocks & 0xff000000ul) >> 24;
    response.FormattableCapacityDescriptor[1] = (total_blocks & 0xff0000) >> 16;
    response.FormattableCapacityDescriptor[2] = (total_blocks & 0xff00) >> 8;
    response.FormattableCapacityDescriptor[3] = total_blocks & 0xff;
    /* Reserved */
    response.FormattableCapacityDescriptor[4] = 0x00;
    /* Block Length */
    response.FormattableCapacityDescriptor[5] = (block_size & 0xff0000) >> 16;
    response.FormattableCapacityDescriptor[6] = (block_size & 0xff00) >> 8;
    response.FormattableCapacityDescriptor[7] = block_size & 0xff;

    UsbMsc_SendBulkData((uint8 *)&response,
            MIN(dataXferLen, SIZE_READ_FORMAT_CAPACITIES_RESPONSE));

    return CSW_STATUS_PASSED;
}

static uint16 scsi_mode_sense(uint8 *data, uint32 dataXferLen, bytes_scsi version)
{
    uint8 page_code = data[1] & 0x3f;
    uint8 response[12];
    uint16 data_length = 0;
    uint16 size_header = SIZE_MODE_PARAM_HEADER;
    usb_msc_mode_parameter_header_t header;
    uint16 allocation_length;

    if (version == BYTES_SCSI10)
        allocation_length = (data[6] << 8) | data[7];
    else
        allocation_length = data[3];

    DEBUG_LOG_INFO("UsbMsc: SCSI_MODE_SENSE(%d) alloc_len %d page %d",
            version, allocation_length, page_code);

    switch (page_code)
    {
        case PAGE_CODE_TIMER_AND_PROTECT_PAGE: /* Timer And Protect Page */
        {
            usb_msc_page_timer_protect_response_t page;

            header.ModeDataLength[0] = SIZE_MODE_PARAM_HEADER + SIZE_PAGE_TIMER_PROTECT_RESPONSE - 1 ;
            header.MediumType[0] = 0x00;
            header.DeviceSpecificParam[0] = 0x80; /* bit 7 marks the media as un-writable */
            header.BlockDescriptorLength[0] = 0x00;

            page.PageCode[0] = PAGE_CODE_TIMER_AND_PROTECT_PAGE;
            page.PageLength[0] = 0x06;
            page.Reserved1[0] = 0x00;
            page.InactivityTimeMult[0] = 0x05;
            page.Reserved2[0] = 0x00;
            page.Reserved2[1] = 0x00;
            page.Reserved2[2] = 0x00;
            page.Reserved2[3] = 0x00;
            data_length = SIZE_PAGE_TIMER_PROTECT_RESPONSE;

            memcpy(response, (uint8 *)&header, SIZE_MODE_PARAM_HEADER);
            memcpy(response + SIZE_MODE_PARAM_HEADER, (uint8 *)&page, SIZE_PAGE_TIMER_PROTECT_RESPONSE);
            break;
        }
        case PAGE_CODE_CACHING:
        case PAGE_CODE_ALL_PAGES: /* Return all pages */
        {
            header.ModeDataLength[0] = SIZE_MODE_PARAM_HEADER - 1;
            header.MediumType[0] = 0x00;
            header.DeviceSpecificParam[0] = 0x80; /* bit 7 marks the media as un-writable */
            header.BlockDescriptorLength[0] = 0x00;

            data_length = 0;

            memcpy(response, (uint8 *)&header, SIZE_MODE_PARAM_HEADER);

            break;
        }
        default:
        {
            return CSW_STATUS_FAILED;
        }
    }

    uint16 data_len = MIN(allocation_length, dataXferLen);
    data_len = MIN(data_len, size_header + data_length);

    UsbMsc_SendBulkData((uint8 *)&response, data_len);

    return CSW_STATUS_PASSED;
}


static uint16 scsi_write(uint8 *data, uint32 dataXferLen, bytes_scsi version)
{
    uint32 transfer_length;
    uint16 status = CSW_STATUS_PASSED;

    if (version == BYTES_SCSI12)
        transfer_length = (((uint32)data[5] << 24) & 0xff000000ul) |
                          (((uint32)data[6] << 16) & 0xff0000) |
                          (((uint32)data[7] << 8) & 0xff00) |
                                   (data[8] & 0xff);
    else
        transfer_length = (((uint16)data[6] << 8) & 0xff00) |
                                   (data[7] & 0xff);

    if ((transfer_length * UsbMsc_Fat16_GetBlockSize()) > dataXferLen)
        status = CSW_STATUS_FAILED;

    return status;
}

static uint16 scsi_read_command(scsi_command_t cmd, uint8 *data, uint32 xfer_length)
{
    uint16 status_code = CSW_STATUS_PASSED;

    switch (cmd)
    {
    case SCSI_TEST_UNIT_READY:
        DEBUG_LOG_INFO("UsbMsc: SCSI_TEST_UNIT_READY");
        /* accept this command */
        break;

    case SCSI_REQUEST_SENSE:
        status_code = scsi_request_response(data, xfer_length);
        break;

    case SCSI_INQUIRY:
        status_code = scsi_inquiry(data, xfer_length);
        break;

    case SCSI_READ10:
        status_code = scsi_read(data, xfer_length, BYTES_SCSI10);
        break;

    case SCSI_READ12:
        status_code = scsi_read(data, xfer_length, BYTES_SCSI12);
        break;

    case SCSI_READ_CAPACITY10:
        status_code = scsi_read_capacity10(xfer_length);
        break;

    case SCSI_READ_CAPACITY16:
        status_code = scsi_read_capacity16(xfer_length);
        break;

    case SCSI_READ_FORMAT_CAPACITIES:
        status_code = scsi_read_format_capacities(xfer_length);
        break;

    case SCSI_MODE_SENSE6:
        status_code = scsi_mode_sense(data, xfer_length, BYTES_SCSI6);
        break;

    case SCSI_MODE_SENSE10:
        status_code = scsi_mode_sense(data, xfer_length, BYTES_SCSI10);
        break;

    case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
        DEBUG_LOG_INFO("UsbMsc: SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL");
        /* accept this command */
        break;

    default:
        DEBUG_LOG_WARN("UsbMsc: Unhandled SCSI 0x%x", cmd);
        status_code = CSW_STATUS_FAILED;
        break;
    }
    return status_code;
}

static uint16 scsi_write_command(scsi_command_t cmd, uint8 *data, uint32 xfer_length)
{
    uint16 status_code = CSW_STATUS_PASSED;

    switch (cmd)
    {
    case SCSI_WRITE10:
        status_code = scsi_write(data, xfer_length, BYTES_SCSI10);
        break;

    case SCSI_WRITE12:
        status_code = scsi_write(data, xfer_length, BYTES_SCSI12);
        break;

    default:
        status_code = CSW_STATUS_FAILED;
        break;
    }

    return status_code;
}

uint16 UsbMsc_ScsiCommand(bool is_to_host, scsi_command_t cmd, uint8 *data, uint32 xfer_length)
{
    uint16 status_code = CSW_STATUS_PASSED;

    if (xfer_length == 0)
    {
        /* the data transfer length is zero, so the device should only
         * send back the CSW with the correct status code */
        switch (cmd)
        {
            case SCSI_REQUEST_SENSE:
            case SCSI_INQUIRY:
            case SCSI_READ10:
            case SCSI_READ12:
            case SCSI_READ_CAPACITY10:
            case SCSI_READ_FORMAT_CAPACITIES:
            case SCSI_MODE_SENSE6:
            case SCSI_MODE_SENSE10:
            case SCSI_WRITE10:
            case SCSI_WRITE12:
            case SCSI_VERIFY10:
                status_code = CSW_STATUS_PHASE_ERROR;
                DEBUG_LOG_WARN("USB: Data tranfer length zero, Phase Error SCSI:[%d]\n", cmd);
                break;
            default:
                DEBUG_LOG_INFO("USB: Data tranfer length zero, SCSI:[%d]\n", cmd);
                break;
         }
    }
    else
    {
        status_code = (is_to_host) ?
                scsi_read_command(cmd, data, xfer_length) :
                scsi_write_command(cmd, data, xfer_length);
    }

    if (status_code == CSW_STATUS_FAILED)
    {
        usb_msc_request_sense_response_t *rsp = &msc_class_data->req_sense_rsp;

        UsbMsc_ScsiInit();

        DEBUG_LOG_WARN("UsbMsc: Unhandled SCSI 0x%x", cmd);
        rsp->SenseKey[0] = SENSE_ERROR_ILLEGAL_REQUEST;
        rsp->ASC[0] = SENSE_ASC_INVALID_FIELD_IN_PARAMETER_LIST;
        rsp->ASCQ[0] = SENSE_ASCQ_INVALID_FIELD_IN_PARAMETER_LIST;

    }

    return status_code;
}

void UsbMsc_ScsiInit(void)
{
    usb_msc_request_sense_response_t *rsp = &msc_class_data->req_sense_rsp;

    memset(rsp, 0, SIZE_REQUEST_SENSE_RESPONSE);

    rsp->Valid_ResponseCode[0] = SENSE_RESPONSE_CURRENT;
    rsp->SenseKey[0] = SENSE_ERROR_NO_SENSE;
    rsp->AddSenseLen[0] = 0xa; /* n-7 = SIZE_REQUEST_SENSE_RESPONSE - 1 - 7 */
}
