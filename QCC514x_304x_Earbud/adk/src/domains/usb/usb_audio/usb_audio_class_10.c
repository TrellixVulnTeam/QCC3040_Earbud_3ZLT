/*!
\copyright  Copyright (c) 2020-2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      USB Audio Class 1.0 Driver
*/

#include <message.h>
#include <panic.h>
#include <sink.h>
#include <source.h>
#include <string.h>
#include <stream.h>
#include <usb.h>
#include <print.h>
#include <vmal.h>
#include <stdlib.h>
#include "logging.h"

#include "usb_device_utils.h"
#include "usb_audio_class_10.h"
#include "usb_audio_class_10_descriptors.h"

/* Audio Device Class Request Types */
#define SET       0x00
#define GET       0x80

/* Audio Device Class Request Codes */
#define REQ_CUR             0x01
#define REQ_MIN             0x02
#define REQ_MAX             0x03
#define REQ_RES             0x04
#define REQ_MEM             0x05

#define SET_CUR (SET | REQ_CUR)
#define SET_MIN (SET | REQ_MIN)
#define SET_MAX (SET | REQ_MAX)
#define SET_RES (SET | REQ_RES)
#define SET_MEM (SET | REQ_MEM)
#define GET_CUR (GET | REQ_CUR)
#define GET_MIN (GET | REQ_MIN)
#define GET_MAX (GET | REQ_MAX)
#define GET_RES (GET | REQ_RES)
#define GET_MEM (GET | REQ_MEM)

/* Helpers */
#define REQ_CS(req)         (req.wValue >> 8)
#define REQ_CN(req)         (req.wValue & 0xFF)
#define REQ_CODE(req)       (req.bRequest & 0x07)
#define REQ_UNIT(req)       (req.wIndex >> 8)
#define REQ_INTERFACE(req)  (req.wIndex & 0xFF)
#define REQ_IS_GET(req)     (req.bRequest & GET)

#define USB_AUDIO_PACKET_RATE_HZ                    1000

/* Round up _value to the nearest _multiple. */
#define ROUNDUP(_value, _multiple) (((_value) + (_multiple) - 1) / (_multiple))

#define UAC_MAX_PACKET_SIZE(sample_rate_hz, channels, sample_size)         \
    (ROUNDUP(sample_rate_hz, USB_AUDIO_PACKET_RATE_HZ) *   \
    (sample_size) * (channels))

#define CONVERT_FROM_3BYTES_LE(x) (((x)[0]) | ((uint32)((x)[1]) << 8)  | ((uint32)((x)[2]) << 16))

#define CONVERT_TO_3BYTES_LE(x,y) ((x)[0] = (uint8)((y) & 0xFF), \
                                     (x)[1] = (uint8)((y) >> 8),  \
                                     (x)[2] = (uint8)((y) >> 16))


/*! Structure to hold USB Audio Control Class-Specific Header descriptor
    This structure is taken from USB Device Class Definition for Audio Devices
    This is only used for accessing descriptor values not for allocating memory */
typedef struct
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint8 bDescriptorSubType;
    uint8 bcdADC_lo;
    uint8 bcdADC_hi;
    uint8 wTotalLength_lo;
    uint8 wTotalLength_hi;
    uint8 bInCollection;
    /*! Actual size of baInterfaceNr is depends on value of bInCollection*/
    uint8 baInterfaceNr[1];
} uac_cs_header_desc_t;


/*! Structure to hold USB Audio format descriptor
    This structure is taken from UUSB Device Class Definition for Audio Data Format
    This is only used for accessing descriptor values not for allocating memory */
typedef struct
{
    uint8 bLength;
    uint8 bDescriptorType;
    uint8 bDescriptorSubtype;
    uint8 bFormatType;
    uint8 bNrChannels;
    uint8 bSubframeSize;
    uint8 bBitResolution;
    /*! Number of supported sampling rate  */
    uint8 bSamFreqType;
    /*! Actual size of tSamFreq is depends on value of bSamFreqType*/
    uint8 tSamFreq[1][3];
} uac_format_descriptor_t;

/*! USB Audio Class 1.0: 4.3.2.5 Feature Unit Descriptor */
typedef struct
{
    uint8 bLength;
    uint8 bDescriptorType; /* UAC_CS_DESC_INTERFACE */
    uint8 bDescriptorSubtype; /* UAC_AC_DESC_FEATURE_UNIT */
    uint8 bUnitID;
    uint8 bSourceID;
    uint8 bControlSize;
    uint8 bmaControls[1];
    uint8 iFeature;
} uac_feature_unit_descriptor_t;

/*! USB audio device class state. */
typedef struct uac_data_t
{
    const usb_audio_config_params_t *config;
    usb_audio_streaming_info_t      *streaming_info;

    uac_cs_header_desc_t       *cs_header_desc;
    Source                   control_source;
    UsbInterface             control_interface;
    uint8                    num_interfaces;

    uac_event_handler_t      evt_handler;
    struct uac_data_t *      next;
} uac_data_t;

#define I_INTERFACE_INDEX 0x00

/*! 
    Audio Gain Limits - We support analogue gain from -45dB to 0dB. 
    Digital gain goes up to +21.5 but distorts audio so don't report it. 
    Firmware settings for analogue gain are:
    0   1     2   3     4   5     6   7     8   9   10   11 12 12 14 15
    Which correspond to gains (in dB) of:
    -45 -41.5 -39 -35.5 -33 -29.5 -27 -23.5 -21 -18 -15 -12 -9 -6 -3 0
    We report resolution of 3dB so we're always within .5dB of the truth:
    -45 -42   -39 -36   -33 -30   -27 -24   -21 -18 -15 -12 -9 -6 -3 0
*/
#define UAC_VOLUME_MAX                0
#define UAC_VOLUME_MIN              -45
#define UAC_VOLUME_RESOLUTION         3
#define UAC_VOLUME_DEFAULT          -45

/* usb_codes_ac & usb_codes_as can be removed if P0 firmware not expecting us to reserve memory */

static const UsbCodes usb_codes_ac = {
    UAC_IF_CLASS_AUDIO, /* bInterfaceClass */
    UAC_IF_SUBCLASS_AUDIOCONTROL, /* bInterfaceSubClass */
    UAC_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
    I_INTERFACE_INDEX /* iInterface */ /* Index of a string descriptor that describes this interface. */
};

static const UsbCodes usb_codes_as = {
    UAC_IF_CLASS_AUDIO, /* bInterfaceClass */
    UAC_IF_SUBCLASS_AUDIOSTREAMING, /* bInterfaceSubClass */
    UAC_PROTOCOL_UNDEFINED, /* bInterfaceProtocol */
    I_INTERFACE_INDEX /* iInterface */
};

TaskData audio_control_task;
TaskData audio_streaming_task;

static uac_data_t *audio_class_info = NULL;

/****************************************************************************
    To create an instance of audio class
*/
static uac_data_t * usbAudioClass10_GetNewInstance(void)
{
    uac_data_t *uac_info = (uac_data_t *)PanicUnlessMalloc(sizeof(uac_data_t));
    memset(uac_info, 0, sizeof(uac_data_t));

    uac_info->next = audio_class_info;
    audio_class_info = uac_info;
    return uac_info;
}

/****************************************************************************
    To get audio class context from control source
*/
static uac_data_t * usbAudioClass10_GetInfoFromControlSource(Source source)
{
    uac_data_t *uac_info = audio_class_info;
    while(uac_info)
    {
        if(uac_info->control_source == source)
        {
            return uac_info;
        }
        uac_info = uac_info->next;
    }
    return NULL;
}

/****************************************************************************
    To get audio stream context from streaming source, also returns interface index
*/
static uac_data_t * usbAudioClass10_GetInfoFromStreamingSource(Source source, uint8 *intf_index_ptr)
{
    uac_data_t *uac_info = audio_class_info;
    while (uac_info)
    {
        for (uint8 i=0; i < uac_info->num_interfaces; i++)
        {
            if (uac_info->streaming_info[i].source == source)
            {
                *intf_index_ptr = i;
                return uac_info;
            }
        }
        uac_info = uac_info->next;
    }
    return NULL;
}

/****************************************************************************
    To get audio stream context from Feature Unit ID
*/
static usb_audio_streaming_info_t *usbAudioClass10_GetStreamingInfoFromUnitID(uac_data_t *uac_info,
                                                                              uint8 feature_id)
{
    for (uint8 i=0; i < uac_info->num_interfaces; i++)
    {
        if (uac_info->streaming_info[i].feature_unit_id == feature_id)
        {
            return &uac_info->streaming_info[i];
        }
    }

    return NULL;
}

static uint8 usbAudioClass10_GetIntfIndexFromUnitID(uac_data_t * uac_info, uint8 feature_id)
{
    uint8 i;

    for (i=0; i < uac_info->num_interfaces; i++)
    {
        if (uac_info->streaming_info[i].feature_unit_id == feature_id)
        {
            break;
        }
    }

    return i;
}

/*
 * Parse AudioControl descriptors and find feature unit id
 */
static bool usbAudioClass10_GetFeatureUnitID(const uac_control_config_t *control_desc,
                                             uint8 *feature_unit_id_ptr)
{
    const uint8 *ptr = control_desc->descriptor;
    const uint8 *end = ptr + control_desc->size_descriptor;

    while (ptr < end)
    {
        const uac_feature_unit_descriptor_t *fu_desc =
                (const uac_feature_unit_descriptor_t *)ptr;

        if (fu_desc->bDescriptorType == UAC_CS_DESC_INTERFACE &&
            fu_desc->bDescriptorSubtype == UAC_AC_DESC_FEATURE_UNIT)
        {
            *feature_unit_id_ptr = fu_desc->bUnitID;
            return TRUE;
        }
        ptr += fu_desc->bLength;
    }
    return FALSE;
}

/*
 * Parse AudioStreaming descriptors and return format descriptor
 */
static uac_format_descriptor_t *usbAudioClass10_GetStreamingFormatDescriptor(const uac_streaming_config_t *streaming_config)
{
    const uint8 *ptr = streaming_config->descriptor;
    const uint8 *end = ptr + streaming_config->size_descriptor;

    while (ptr < end)
    {
        uac_format_descriptor_t* format = (uac_format_descriptor_t*) ptr;

        if (format->bDescriptorType == UAC_CS_DESC_INTERFACE &&
            format->bDescriptorSubtype == UAC_AS_DESC_FORMAT_TYPE &&
            format->bFormatType == UAC_AS_DESC_FORMAT_TYPE_I)
        {
            return format;
        }
        ptr += format->bLength;
    }
    return NULL;
}


/****************************************************************************
    To round requested sampling rate to the closest rate supported by the interface.
*/
static uint32 usbAudioClass10_RoundSamplingRate(const uac_streaming_config_t *streaming_config,
                                                uint32 sampling_rate)
{
    uac_format_descriptor_t *format;
    uint32 closest_rate = 0;

    format = usbAudioClass10_GetStreamingFormatDescriptor(streaming_config);
    if (format)
    {
        uint8* freq_p = &(format->tSamFreq[0][0]);
        closest_rate = CONVERT_FROM_3BYTES_LE(freq_p);

        for (uint8 i = 1; i < format->bSamFreqType; i++)
        {
            uint32 rate;

            freq_p = &(format->tSamFreq[i][0]);
            rate = CONVERT_FROM_3BYTES_LE(freq_p);
            if(abs(closest_rate - sampling_rate) > abs(rate - sampling_rate))
            {
                closest_rate = rate;
            }
        }
    }

    return closest_rate;
}


/****************************************************************************
    To get maximum sampling rate supported by the streaming interface endpoint
*/
static uint32 usbAudioClass10_GetMaxSamplingRate(const uac_streaming_config_t *streaming_config)
{
    uac_format_descriptor_t *format;
    uint32 max_rate = 0;

    format = usbAudioClass10_GetStreamingFormatDescriptor(streaming_config);
    if (format)
    {
        for (uint8 i=0; i < format->bSamFreqType; i++)
        {
            uint8* freq_p;
            freq_p = &(format->tSamFreq[i][0]);
            uint32 samp_rate = CONVERT_FROM_3BYTES_LE(freq_p);
            if(max_rate <= samp_rate)
            {
                max_rate = samp_rate;
            }
        }
    }

    return max_rate;
}


/****************************************************************************
    To get maximum packet size need by the streaming interface endpoint
*/
static uint16 usbAudioClass10_GetMaxPacketSize(const uac_streaming_config_t *streaming_config)
{
    uac_format_descriptor_t *format;
    uint16 max_packet_size = 0;

    format = usbAudioClass10_GetStreamingFormatDescriptor(streaming_config);
    if (format)
    {
        uint32 max_rate = 0;
        for (uint8 i = 0; i < format->bSamFreqType; i++)
        {
            uint8* freq_p;
            freq_p = &(format->tSamFreq[i][0]);
            uint32 samp_rate = CONVERT_FROM_3BYTES_LE(freq_p);
            if(max_rate <= samp_rate)
            {
                max_rate = samp_rate;
            }
        }

        max_packet_size = UAC_MAX_PACKET_SIZE(max_rate, format->bNrChannels, format->bSubframeSize);
    }
    return max_packet_size;
}

/****************************************************************************
    To write the value of required attribute to sink
*/
static uint16 usbAudioClass10_GetLevel(usb_audio_streaming_info_t *streaming_info,
                                       const usb_audio_volume_config_t *volume_config,
                                       Sink sink, uint8 control, uint8 code, uint8 channel)
{
    uint8*  p_snk;

    if (channel == UAC_CHANNEL_MASTER)
    {
        if (control == UAC_FU_CONTROL_MUTE)
        {
            if (code == REQ_CUR)
            {
                DEBUG_LOG("audioGetLevel:MUTE: intf - %d, VAL - %d ",
                          streaming_info->interface,
                          streaming_info->volume_status.mute_status);

                p_snk = SinkMapClaim(sink, 1);
                if (p_snk)
                {
                    p_snk[0] = streaming_info->volume_status.mute_status;
                    return 1;
                }
                else
                {
                    DEBUG_LOG_WARN("audioGetLevel: SinkClaim Failed");
                    return 0;
                }
            }
        }
        else if (control == UAC_FU_CONTROL_VOLUME)
        {
            uint8 value = 0;
            bool success = TRUE;

            switch (code)
            {
            case REQ_CUR:
                value = (uint8)streaming_info->volume_status.volume_db;
                break;

            case REQ_MIN:
                value = (uint8)volume_config->min_db;
                break;

            case REQ_MAX:
                value = (uint8)volume_config->max_db;
                break;

            case REQ_RES:
                value = volume_config->res_db;
                break;

            default:
                success = FALSE;
            }

            if (success)
            {
                DEBUG_LOG("audioGetLevel: VOL: intf - %d, Code - %d, VAL - %d",
                           streaming_info->interface, code, value);

                /* Volume is 2 bytes.
                 * The low byte is 1/256 db fractions, since we support
                 * only integer values it is set to "0". */
                p_snk    = SinkMapClaim(sink, 2);
                if (p_snk)
                {
                    p_snk[0] = 0;
                    p_snk[1] = value;
                    return 2;
                }
                else
                {
                    DEBUG_LOG_WARN("audioGetLevel: SinkClaim Failed");
                    return 0;
                }
            }
        }
    }
    /* Unsupported request */
    DEBUG_LOG_WARN("audioGetLevel: Unsupported request: intf - %d, "
                   "Control - %d, Code - %d, Channel - %d",
                   streaming_info->interface, control, code, channel);
    return 0;
}

/****************************************************************************
    To round the gain based on volume configuration.
*/
static int8 usbAudioClass10_RoundVolume(const usb_audio_volume_config_t *volume_config,
                                        int8 volume)
{
    int8 max_db = volume_config->max_db;
    int8 min_db = volume_config->min_db;
    uint8 res_db = volume_config->res_db;

    /* Don't round up to maximum or round down to minimum to avoid
     * max/min prompts when host volume is at 99% / 1%. */

    if (volume >= max_db)
    {
        return max_db;
    }
    else if (volume <= min_db)
    {
        return min_db;
    }
    /* round up volumes in the range (min_db, min_db + res_db] */
    else if (volume <= min_db + res_db)
    {
        return (int8)(min_db + res_db);
    }

    /* this code rounds down, so values [max_db - res_db, max_db) are fine. */
    return (int8)(min_db + (volume - min_db) / res_db * res_db);
}

/****************************************************************************
    Read the level value from the source, store it in the volume control and
    notify Application with the USB_DEVICE_CLASS_MSG_AUDIO_LEVELS_IND message.
*/
static bool usbAudioClass10_SetLevel(usb_audio_streaming_info_t * streaming_info,
                                     const usb_audio_volume_config_t *volume_config,
                                     Source source, uint8 control, uint8 code, uint8 channel)
{
    uint16 data_len = SourceBoundary(source);

    /* Only allow setting current levels */
    if (code == REQ_CUR &&
            channel == UAC_CHANNEL_MASTER)
    {
        const uint8* p_src = SourceMap(source);

        if (control == UAC_FU_CONTROL_MUTE &&
                data_len == 1)
        {
            streaming_info->volume_status.mute_status = p_src[0] ? 1 : 0;
            DEBUG_LOG_STATE("audioSetLevel: MUTE: intf - %d,  VAL - %d",
                            streaming_info->interface,
                            streaming_info->volume_status.mute_status);
            /* Respond with success */
            return TRUE;
        }
        else if (control == UAC_FU_CONTROL_VOLUME &&
                data_len == 2)
        {
            /* ignore the low byte with 1/256 db fractions as we only support
             * integer values. */
            int8 volume = p_src[1];
            streaming_info->volume_status.volume_db =
                    usbAudioClass10_RoundVolume(volume_config, volume);

            DEBUG_LOG_STATE("audioSetLevel: VOL: intf - %d, VAL - %d",
                                     streaming_info->interface,
                                     streaming_info->volume_status.volume_db);
            /* Respond with success */
            return TRUE;
        }
    }

    /* Unsupported request */
    DEBUG_LOG_WARN("audioSetLevel: Unsupported request: intf - %d, "
                    "Control - %d, Code - %d, Channel - %d, data len %d",
                    streaming_info->interface, control, code, channel, data_len);
    return FALSE;
}

/****************************************************************************
    To handle audio control request
*/
static void usbAudioClass10_HandleControlClassRequest(uac_data_t * uac_info)
{
    const usb_audio_volume_config_t *volume_config = &uac_info->config->volume_config;
    usb_audio_streaming_info_t *streaming_info;
    uint16 packet_size;
    Source req = uac_info->control_source;
    Sink sink = StreamSinkFromSource(req);
    uint8 channel;
    uint8 unit_id;
    uint8 code;
    uint8 control;
    
    /* Check for outstanding Class requests */
    while ((packet_size = SourceBoundary(req)) != 0)
    {
        /*
            Build the response. It must contain the original request,
            so copy from the source header.
        */
        UsbResponse usbresp;
        memcpy(&usbresp.original_request, SourceMapHeader(req), sizeof(UsbRequest));

        /* Reject by default */
        usbresp.data_length = 0;
        usbresp.success = FALSE;
        
        /* Get audio specific request info */
        channel = REQ_CN(usbresp.original_request);
        control = REQ_CS(usbresp.original_request);
        code    = REQ_CODE(usbresp.original_request);
        unit_id = REQ_UNIT(usbresp.original_request);
        
        streaming_info = usbAudioClass10_GetStreamingInfoFromUnitID(uac_info, unit_id);

        if (streaming_info != NULL)
        {
            if(REQ_IS_GET(usbresp.original_request))
            {
                /* Return value to USB host */
                DEBUG_LOG("AudioControlClassRequest: GET");
                usbresp.data_length = usbAudioClass10_GetLevel(streaming_info,
                                                               volume_config,
                                                               sink, control, code, channel);
                usbresp.success = (usbresp.data_length != 0);
            }
            else
            {
                /* Update value sent by USB host */
                DEBUG_LOG("AudioControlClassRequest: SET");
                usbresp.success = usbAudioClass10_SetLevel(streaming_info,
                                                           volume_config,
                                                           req, control, code, channel);
                if(usbresp.success)
                {
                    uint8 intf_index = usbAudioClass10_GetIntfIndexFromUnitID(uac_info, unit_id);
                    /* Level has changed, notify application */
                    uac_info->evt_handler(uac_info,
                                          intf_index,
                                          USB_AUDIO_CLASS_MSG_LEVELS);
                }
            }
        }
        else
        {
            usbresp.success = FALSE;
        }

        if(!usbresp.success)
        {
            DEBUG_LOG_WARN("AudioControlClassRequest: Failed \n");
        }
        
        /* Send response */
        if (usbresp.data_length)
        {
            (void)SinkFlushHeader(sink, usbresp.data_length, (void *)&usbresp, sizeof(UsbResponse));
        }
        else
        {
            /* Sink packets can never be zero-length, so flush a dummy byte */
            (void)SinkClaim(sink, 1);
            (void)SinkFlushHeader(sink, 1, (void *)&usbresp, sizeof(UsbResponse));
        }

        /* Discard the original request */
        SourceDrop(req, packet_size);
    }
}

/****************************************************************************
    To handle audio streaming request
*/
static void usbAudioClass10_HandleStreamingClassRequest(uac_data_t *uac_info, uint8 intf_index)
{
    usb_audio_streaming_info_t *streaming_info = &uac_info->streaming_info[intf_index];
    const usb_audio_interface_config_t *intf_config = &uac_info->config->intf_list->intf[intf_index];

    Source req = streaming_info->source;
    Sink sink = StreamSinkFromSource(req);
    uint16 packet_size;

    /* Check for outstanding Class requests */
    while ((packet_size = SourceBoundary(req)) != 0)
    {
        /*
            Build the response. It must contain the original request,
            so copy from the source header.
        */
        UsbResponse usbresp;
        memcpy(&usbresp.original_request, SourceMapHeader(req), sizeof(UsbRequest));

        /* Set the response fields to default values to make the code below simpler */
        usbresp.success = FALSE;
        usbresp.data_length = 0;

        /* Endpoint only allows SET_/GET_ of sampling frequency */
        if ((REQ_CS(usbresp.original_request) == UAC_EP_CONTROL_SAMPLING_FREQ) &&
                 (usbresp.original_request.wLength == 3))
        {
            /* Check this is for a valid audio end point */
            if(usbresp.original_request.wIndex == streaming_info->ep_address)
            {
                if (usbresp.original_request.bRequest == SET_CUR)
                {
                    const uint8* rate = SourceMap(req);
                    uint32 new_rate  = (uint32)CONVERT_FROM_3BYTES_LE(rate);

                    new_rate = usbAudioClass10_RoundSamplingRate(intf_config->streaming_desc,
                                                                 new_rate);

                    usbresp.success = TRUE;
                    /* store sample rate */
                    streaming_info->current_sampling_rate = new_rate;
                    DEBUG_LOG_STATE("AudioStreaming: Set intf - %d Rate %lu",
                                    streaming_info->interface, new_rate);

                    /* notify change in sample rate */
                    uac_info->evt_handler(uac_info,
                                          intf_index,
                                          USB_AUDIO_CLASS_MSG_SAMPLE_RATE);
                }
                else if (usbresp.original_request.bRequest == GET_CUR)
                {
                    /* Return current value */
                    uint8 *ptr = SinkMapClaim(sink, 3);

                    if (ptr != NULL)
                    {
                        /* get sample rate */
                        uint32 sampling_rate = streaming_info->current_sampling_rate;
                        CONVERT_TO_3BYTES_LE(ptr,sampling_rate);
                        DEBUG_LOG_STATE("AudioStreaming: Get intf - %d Rate %lu ",
                                streaming_info->interface, sampling_rate);

                        usbresp.data_length = 3;
                        usbresp.success = TRUE;
                    }
                }
            }
        }

        /* Send response */
        if (usbresp.data_length)
        {
            (void)SinkFlushHeader(sink, usbresp.data_length, (void *)&usbresp, sizeof(UsbResponse));
        }
        else
        {
            /* Sink packets can never be zero-length, so flush a dummy byte */
            (void)SinkClaim(sink, 1);
            (void)SinkFlushHeader(sink, 1, (void *)&usbresp, sizeof(UsbResponse));
        }

        /* Discard the original request */
        SourceDrop(req, packet_size);
    }

}


/****************************************************************************
    To handle audio cotrol request from all device indexes
*/
static void usbAudioClass10_ControlHandle(Task task, MessageId id, Message message) 
{ 
    UNUSED(task);
    if (id == MESSAGE_MORE_DATA)
    {
        Source class_source = ((MessageMoreData *)message)->source;
        uac_data_t *uac_info = usbAudioClass10_GetInfoFromControlSource(class_source);
        if (uac_info != NULL)
        {
            usbAudioClass10_HandleControlClassRequest(uac_info);
        }
    }
} 

/****************************************************************************
    To handle audio streaming request
*/
static void usbAudioClass10_StreamingHandler(Task task, MessageId id, Message message) 
{ 
    UNUSED(task);
    if (id == MESSAGE_MORE_DATA)
    {
        Source class_source = ((MessageMoreData *)message)->source;
        uint8 intf_index;
        uac_data_t *uac_info = usbAudioClass10_GetInfoFromStreamingSource(class_source, &intf_index);
        if (uac_info != NULL)
        {
            usbAudioClass10_HandleStreamingClassRequest(uac_info, intf_index);
        }
    }
}

/** Audio endpoint data, provides bRefresh and bSyncAddress */
static const uint8 audio_endpoint_user_data[] =
{
    0, /* bRefresh */
    0  /* bSyncAddress */
};

/****************************************************************************
    To add streaming interface for enumeration
*/
static void usbAudioClass10_EnumerateStreaming(usb_device_index_t usb_device_index,
        usb_audio_streaming_info_t *streaming_info,
        const usb_audio_interface_config_t *intf_config)
{
    uint8 ep_address;
    EndPointInfo ep_info;
    uac_format_descriptor_t *format_descriptor;

    if (!usbAudioClass10_GetFeatureUnitID(intf_config->control_desc,
                                          &streaming_info->feature_unit_id))
    {
        DEBUG_LOG_ERROR("EnumerateAudioStreaming: Feature unit_id not found");
        Panic();
    }

    format_descriptor = usbAudioClass10_GetStreamingFormatDescriptor(intf_config->streaming_desc);
    PanicNull(format_descriptor);
    streaming_info->channels = format_descriptor->bNrChannels;
    streaming_info->frame_size = format_descriptor->bSubframeSize;

    /* Add the speaker Audio Streaming Interface */
    streaming_info->interface = UsbAddInterface(&usb_codes_as, UAC_CS_DESC_INTERFACE,
                                                intf_config->streaming_desc->descriptor,
                                                intf_config->streaming_desc->size_descriptor);
    
    if (streaming_info->interface == usb_interface_error)
    {
        DEBUG_LOG_ERROR("EnumerateAudioStreaming: UsbAddInterface Failed");
        Panic();
    }

    /* USB endpoint information */
    ep_address = UsbDevice_AllocateEndpointAddress(usb_device_index,
                                                   intf_config->endpoint->is_to_host);
    ep_info.bEndpointAddress = ep_address;
    ep_info.bmAttributes = end_point_attr_iso;
    ep_info.wMaxPacketSize = intf_config->endpoint->wMaxPacketSize ?
                  intf_config->endpoint->wMaxPacketSize :
                  usbAudioClass10_GetMaxPacketSize(intf_config->streaming_desc);
    ep_info.bInterval = intf_config->endpoint->bInterval;
    ep_info.extended = audio_endpoint_user_data;
    ep_info.extended_length = sizeof(audio_endpoint_user_data);

    /* Add the speaker endpoint */
    if (!UsbAddEndPoints(streaming_info->interface, 1, &ep_info))
    {
        DEBUG_LOG_ERROR("EnumerateAudioStreaming: UsbAddEndPoints Failed");
        Panic();
    }
    streaming_info->ep_address = ep_address;
}

/****************************************************************************
    To Initialize audio control cs header descriptor based on streaming interface count
    and size of all Unit Descriptors and Terminal Descriptors
*/
static uac_cs_header_desc_t * usbAudioClass10_GetHeaderDesc(uint8 intf_count, uint16 unit_terminal_size)
{
    uint16 total_size = UAC_AC_IF_HEADER_DESC_SIZE(intf_count) + unit_terminal_size;
    uac_cs_header_desc_t * cs_header = (uac_cs_header_desc_t *)PanicUnlessMalloc(UAC_AC_IF_HEADER_DESC_SIZE(intf_count));

    cs_header->bLength = UAC_AC_IF_HEADER_DESC_SIZE(intf_count);
    cs_header->bDescriptorType = UAC_CS_DESC_INTERFACE;
    cs_header->bDescriptorSubType = UAC_AC_DESC_HEADER;
    cs_header->bcdADC_lo = UAC_BCD_ADC_1_0 & 0xFF;
    cs_header->bcdADC_hi = (UAC_BCD_ADC_1_0 >> 8) & 0xFF;
    cs_header->wTotalLength_lo = total_size & 0xFF;
    cs_header->wTotalLength_hi = (total_size >> 8) & 0xFF;
    cs_header->bInCollection = intf_count;

    for (uint8 i=0; i<intf_count; i++)
    {
        /* will be updated later with the actual streaming interface id */
        cs_header->baInterfaceNr[i] = 0;
    }

    return cs_header;
}

/****************************************************************************
    To add control interface for enumeration
*/
static void usbAudioClass10_EnumerateControl(uac_data_t * uac_info)
{
    uint16 unit_terminal_size = 0;
    const usb_audio_interface_config_list_t *intf_list = uac_info->config->intf_list;

    for (uint8 i=0; i < uac_info->num_interfaces; i++)
    {
        unit_terminal_size += intf_list->intf[i].control_desc->size_descriptor;
    }

    uac_info->cs_header_desc = usbAudioClass10_GetHeaderDesc(uac_info->num_interfaces,
                                                                 unit_terminal_size);
    /* Add an Audio Control Interface */
    uac_info->control_interface = UsbAddInterface(&usb_codes_ac,
                                              UAC_CS_DESC_INTERFACE,
                                              (uint8 *)uac_info->cs_header_desc,
                                              UAC_AC_IF_HEADER_DESC_SIZE(uac_info->num_interfaces));

    if (uac_info->control_interface == usb_interface_error)
    {
        DEBUG_LOG_ERROR("EnumerateAudioControl: usb_interface_error");
        Panic();
    }

    for (uint8 i=0; i < uac_info->num_interfaces; i++)
    {
        const usb_audio_interface_config_t *intf_config = &intf_list->intf[i];

        /* streaming interfaces will be added right after the control interface
         * and get consecutive numbers */
        uac_info->cs_header_desc->baInterfaceNr[i] = uac_info->control_interface+i+1;

        if (!UsbAddDescriptor(uac_info->control_interface, UAC_CS_DESC_INTERFACE,
                              intf_config->control_desc->descriptor,
                              intf_config->control_desc->size_descriptor))
        {
            DEBUG_LOG_ERROR("UsbAudio_Control::UsbAddDescriptor ERROR");
            Panic();
        }
    }
}

/****************************************************************************
    To manage stream connection of audio class
*/ 
static void usbAudioClass10_ManageStreamConnection(uac_data_t * uac_info)
{
    Sink sink;
    usb_audio_streaming_info_t *streaming_info;

    audio_control_task.handler = usbAudioClass10_ControlHandle;
    audio_streaming_task.handler = usbAudioClass10_StreamingHandler;

    sink = StreamUsbClassSink(uac_info->control_interface);
    uac_info->control_source = StreamSourceFromSink(sink);
    MessageStreamTaskFromSink(sink, &audio_control_task);

    for (uint8 i=0; i < uac_info->num_interfaces; i++)
    {
        streaming_info = &uac_info->streaming_info[i];
        sink = StreamUsbClassSink(streaming_info->interface);
        streaming_info->source = StreamSourceFromSink(sink);
        MessageStreamTaskFromSink(sink, &audio_streaming_task);
    }

    StreamConfigure(VM_STREAM_USB_ALT_IF_MSG_ENABLED, 1);
}

static void usbAudioClass10_Set_Defaults(uac_data_t *uac_info)
{
    usb_audio_streaming_info_t *streaming_info = uac_info->streaming_info;
    const usb_audio_config_params_t *config = uac_info->config;

    for (uint8 i=0; i < uac_info->num_interfaces; i++)
    {
        const usb_audio_interface_config_t *intf_config = &config->intf_list->intf[i];

        /* set default sampling Rate */
        streaming_info[i].current_sampling_rate =
                usbAudioClass10_GetMaxSamplingRate(intf_config->streaming_desc);
        PanicZero(streaming_info[i].current_sampling_rate);

        /* set speaker default volume */
        streaming_info[i].volume_status.mute_status = 0;
        streaming_info[i].volume_status.volume_db = config->volume_config.target_db;
    }
}

/****************************************************************************
    To add class 1.0 audio interface.
*/
static uac_ctx_t usbAudioClass10_Create(usb_device_index_t device_index,
                                     const usb_audio_config_params_t *config,
                                     usb_audio_streaming_info_t **streaming_info,
                                     uac_event_handler_t evt_handler)
{
    DEBUG_LOG_DEBUG("usbAudioClass10_Create");
    PanicZero(config);
    PanicNull(streaming_info);
    PanicZero(evt_handler);

    const usb_audio_interface_config_list_t *intf_list = config->intf_list;

    uac_data_t *uac_info = usbAudioClass10_GetNewInstance();

    uac_info->config = config;
    uac_info->num_interfaces = config->intf_list->num_interfaces;
    uac_info->evt_handler = evt_handler;

    uac_info->streaming_info = (usb_audio_streaming_info_t *)
            PanicUnlessMalloc(sizeof(usb_audio_streaming_info_t) *
                              uac_info->num_interfaces);
    memset(uac_info->streaming_info, 0,
           sizeof(usb_audio_streaming_info_t) * uac_info->num_interfaces);

    *streaming_info = uac_info->streaming_info;

    /* Attempt to register control interface */
    usbAudioClass10_EnumerateControl(uac_info);

    /* Set default volume and frequency */
    usbAudioClass10_Set_Defaults(uac_info);

    /* Attempt to register all interface */
    for (uint8 i=0; i < uac_info->num_interfaces; i++)
    {
        usbAudioClass10_EnumerateStreaming(device_index,
                                           &uac_info->streaming_info[i],
                                           &intf_list->intf[i]);
    }

    usbAudioClass10_ManageStreamConnection(uac_info);

    DEBUG_LOG_INFO("usbAudioClass10_Create success");
    return (uac_ctx_t) uac_info;
}

/****************************************************************************
    To delete usb audio class 1.0 instance which created using usbAudioClass10_Add.
*/
static bool usbAudioClass10_Delete(uac_ctx_t class_ctx)
{
    uac_data_t **uac_info_ptr = &audio_class_info;

    while (*uac_info_ptr)
    {
        if (*uac_info_ptr == (uac_data_t *)class_ctx)
        {
            uac_data_t *uac_info = *uac_info_ptr;

            *uac_info_ptr = (*uac_info_ptr)->next;

            free(uac_info->cs_header_desc);
            free(uac_info->streaming_info);
            free(uac_info);

            return TRUE;
        }

        uac_info_ptr = &((*uac_info_ptr)->next);
    }

    DEBUG_LOG_ERROR("usbAudioClass10_RemoveInstance Failed");

    return FALSE;
}

static bool usbAudioClass10_Reset(uac_ctx_t class_ctx)
{
    uac_data_t *uac_info = audio_class_info;

    while (uac_info)
    {
        if (uac_info == (uac_data_t *)class_ctx)
        {
            usbAudioClass10_Set_Defaults(uac_info);
            return TRUE;
        }

        uac_info = uac_info->next;
    }

    DEBUG_LOG_ERROR("usbAudioClass10_Reset Failed");

    return FALSE;
}

static usb_fn_tbl_uac_if uac10_fn_tbl = {
    .Create = usbAudioClass10_Create,
    .Reset = usbAudioClass10_Reset,
    .Delete = usbAudioClass10_Delete,
};

usb_fn_tbl_uac_if *UsbAudioClass10_GetFnTbl(void)
{
    return &uac10_fn_tbl;
}

