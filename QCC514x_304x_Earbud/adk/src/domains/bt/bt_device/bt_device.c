/*!
\copyright  Copyright (c) 2015 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version    
\file       bt_device.c
\brief      Device Management.
*/

#include "bt_device_marshal_typedef.h"
#include "bt_device_marshal_table.h"
#include "bt_device_typedef.h"
#include "device_properties.h"

#include <panic.h>
#include "connection_abstraction.h"
#include <device.h>
#include <device_list.h>
#include <marshal.h>
#include <ps.h>
#include <string.h>
#include <stdlib.h>
#include <region.h>
#include <service.h>

#include "av.h"
#include "device_db_serialiser.h"
#include "adk_log.h"
#include "a2dp_profile.h"

#include <connection_manager.h>
#include <connection_manager_config.h>
#include <hfp_profile.h>
#include "mirror_profile.h"
#include "ui.h"
#include <local_addr.h>

LOGGING_PRESERVE_MESSAGE_TYPE(bt_device_messages_t)
ASSERT_MESSAGE_GROUP_NOT_OVERFLOWED(BT_DEVICE, BT_DEVICE_MESSAGE_END)

/*! \brief Macro for simplifying creating messages */
#define MAKE_DEVICE_MESSAGE(TYPE) \
    TYPE##_T *message = PanicUnlessNew(TYPE##_T);
/*! \brief Macro for simplying copying message content */
#define COPY_DEVICE_MESSAGE(src, dst) *(dst) = *(src);

/*! \brief BT device internal messages */
enum
{
    BT_INTERNAL_MSG_STORE_PS_DATA,            /*!< Store device data in PS */
};

/*! \brief Delay before storing the device data in ps */
#define BT_DEVICE_STORE_PS_DATA_DELAY D_SEC(1)

/*!< App device management task */
deviceTaskData  app_device;

static unsigned Device_GetCurrentContxt(void);

static bdaddr btDevice_SanitiseBdAddr(const bdaddr *bd_addr)
{
    bdaddr sanitised_bdaddr = {0};
    sanitised_bdaddr.uap = bd_addr->uap;
    sanitised_bdaddr.lap = bd_addr->lap;
    sanitised_bdaddr.nap = bd_addr->nap;
    return sanitised_bdaddr;
}

static void btDevice_SetLinkBehaviorByDevice(device_t device, void *data)
{
    bdaddr *addr = NULL;
    size_t size = 0;
    UNUSED(data);

    Device_GetProperty(device, device_property_bdaddr, (void *)&addr, &size);

    BtDevice_SetLinkBehavior(addr);
}


static void btDevice_PrintDeviceInfo(device_t device, void *data)
{
    size_t size = 0;
    bdaddr *addr = NULL;
    deviceType *type = NULL;
    uint16 flags = 0;
    audio_source_t source = audio_source_none;
    avTaskData * av_inst = NULL;
    uint8 volume = 0;

    UNUSED(data);

    DEBUG_LOG("btDevice_PrintDeviceInfo");

    DEBUG_LOG("device %08x", device);

    Device_GetProperty(device, device_property_bdaddr, (void *)&addr, &size);
    DEBUG_LOG("bd addr %04x:%02x:%06x", addr->nap, addr->uap, addr->lap);

    Device_GetProperty(device, device_property_type, (void *)&type, &size);

    switch(*type)
    {
        case DEVICE_TYPE_UNKNOWN:
            DEBUG_LOG("type is unknown");
            break;

        case DEVICE_TYPE_EARBUD:
            DEBUG_LOG("type is earbud");
            break;

        case DEVICE_TYPE_HANDSET:
            DEBUG_LOG("type is handset");
            break;

        case DEVICE_TYPE_SINK:
            DEBUG_LOG("type is sink");
            break;

        case DEVICE_TYPE_SELF:
            DEBUG_LOG("type is self");
            break;

        default:
            DEBUG_LOG("type is INVALID!!!");
    }

    Device_GetPropertyU16(device, device_property_flags, (void *)&flags);

    if(flags & DEVICE_FLAGS_PRIMARY_ADDR)
    {
        DEBUG_LOG("has flag DEVICE_FLAGS_PRIMARY_ADDR");
    }

    if(flags & DEVICE_FLAGS_SECONDARY_ADDR)
    {
        DEBUG_LOG("has flag DEVICE_FLAGS_SECONDARY_ADDR");
    }

    if(flags & DEVICE_FLAGS_MIRRORING_C_ROLE)
    {
        DEBUG_LOG("has flag DEVICE_FLAGS_MIRRORING_C_ROLE");
    }

    if(flags & DEVICE_FLAGS_QHS_CONNECTED)
    {
        DEBUG_LOG("has flag DEVICE_FLAGS_QHS_CONNECTED");
    }

    if(flags & DEVICE_FLAGS_FIRST_CONNECT_AFTER_DFU)
    {
        DEBUG_LOG("has flag DEVICE_FLAGS_FIRST_CONNECT_AFTER_DFU");
    }

    if(flags & DEVICE_FLAGS_SWB_NOT_SUPPORTED)
    {
        DEBUG_LOG("has flag DEVICE_FLAGS_SWB_NOT_SUPPORTED");
    }

    if (Device_GetProperty(device, device_property_av_instance, (void *)&av_inst, &size))
    {
        DEBUG_LOG("av instance %08x", type);
    }

    if (Device_GetPropertyU8(device, device_property_audio_source, (void *)&source))
    {
        DEBUG_LOG("audio source %u", source);
    }

    if (Device_GetPropertyU8(device, device_property_audio_volume, (void *)&volume))
    {
        DEBUG_LOG("audio volume %d", volume);
    }

    if (Device_GetPropertyU8(device, device_property_voice_volume, (void *)&volume))
    {
        DEBUG_LOG("voice volume %d", volume);
    }
}

static device_t btDevice_CreateDevice(const bdaddr *bd_addr, deviceType type)
{
    deviceLinkMode link_mode = DEVICE_LINK_MODE_UNKNOWN;
    device_t device = Device_Create();

    bdaddr sanitised_bdaddr = btDevice_SanitiseBdAddr(bd_addr);
    Device_SetProperty(device, device_property_bdaddr, &sanitised_bdaddr, sizeof(bdaddr));
    Device_SetProperty(device, device_property_type, &type, sizeof(deviceType));
    Device_SetProperty(device, device_property_link_mode, &link_mode, sizeof(deviceLinkMode));
    Device_SetPropertyU32(device, device_property_supported_profiles, 0x0);
    Device_SetPropertyU16(device, device_property_flags, 0x0);

    return device;
}

device_t BtDevice_GetDeviceCreateIfNew(const bdaddr *bd_addr, deviceType type)
{
    device_t device = NULL;

    DEBUG_LOG("BtDevice_GetDeviceCreateIfNew: %04x %02x %06x type %u",
        bd_addr->nap, bd_addr->uap, bd_addr->lap, type);

    bdaddr sanitised_bdaddr = btDevice_SanitiseBdAddr(bd_addr);
    device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, &sanitised_bdaddr, sizeof(bdaddr));
    if (!device)
    {
        DEBUG_LOG("- new");
        device = btDevice_CreateDevice(bd_addr, type);
        if (!DeviceList_AddDevice(device))
        {
            Device_Destroy(&device);

            /* As can't add the device to the device list so no point going forward */
            DEBUG_LOG("BtDevice_GetDeviceCreateIfNew can't add device to the device list");
            Panic();
        }
        else if(type == DEVICE_TYPE_SELF)
        {
            MAKE_DEVICE_MESSAGE(BT_DEVICE_SELF_CREATED_IND);
            message->device = device;

            DEBUG_LOG_VERBOSE("BtDevice_GetDeviceCreateIfNew SELF device has been created");

            TaskList_MessageSendWithSize(DeviceGetTaskData()->listeners, BT_DEVICE_SELF_CREATED_IND, message, sizeof(BT_DEVICE_SELF_CREATED_IND_T));
        }
    }
    else
    {
        deviceType *existing_type = NULL;
        size_t size = 0;

        PanicFalse(Device_GetProperty(device, device_property_type, (void *)&existing_type, &size));
        DEBUG_LOG_ERROR("- existing type %u", *existing_type);
        PanicFalse(*existing_type == type);
    }

    return device;
}

static bool btDevice_DeviceIsValid_flag;

static void btDevice_Matches(device_t device, void *sought_device)
{
    if ((void*)device == sought_device)
    {
        btDevice_DeviceIsValid_flag = TRUE;
    }
}

bool BtDevice_DeviceIsValid(device_t device)
{
    btDevice_DeviceIsValid_flag = FALSE;

    DeviceList_Iterate(btDevice_Matches, (void*)device);

    DEBUG_LOG_V_VERBOSE("BtDevice_DeviceIsValid %p=%d", device, btDevice_DeviceIsValid_flag);

    return btDevice_DeviceIsValid_flag;
}

bool BtDevice_isKnownBdAddr(const bdaddr *bd_addr)
{
    bdaddr sanitised_bdaddr = btDevice_SanitiseBdAddr(bd_addr);
    if (DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, &sanitised_bdaddr, sizeof(bdaddr)) != NULL)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

device_t BtDevice_GetDeviceForBdAddr(const bdaddr *bd_addr)
{
    device_t dev = NULL;

    bdaddr sanitised_bdaddr = btDevice_SanitiseBdAddr(bd_addr);

    dev = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, &sanitised_bdaddr, sizeof(bdaddr));

    DEBUG_LOG_VERBOSE("BtDevice_GetDeviceForBdAddr [%04x,%02x,%06lx]  device 0x%p", bd_addr->nap, bd_addr->uap, bd_addr->lap, dev);

    return dev;
}

device_t BtDevice_GetDeviceForTpbdaddr(const tp_bdaddr *tpbdaddr)
{
    typed_bdaddr resolved_typed_addr;

    if(!BtDevice_GetPublicAddress(&tpbdaddr->taddr, &resolved_typed_addr))
    {
        resolved_typed_addr = tpbdaddr->taddr;
    }

    return BtDevice_GetDeviceForBdAddr(&resolved_typed_addr.addr);
}

static bool btDevice_GetDeviceBdAddr(deviceType type, bdaddr *bd_addr)
{
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
    if (device)
    {
        *bd_addr = DeviceProperties_GetBdAddr(device);
        return TRUE;
    }
    else
    {
        BdaddrSetZero(bd_addr);
        return FALSE;
    }
}

static void btDevice_StoreDeviceDataInPs(void)
{
    bdaddr handset_address = {0,0,0};
    appDeviceGetHandsetBdAddr(&handset_address);

    /* Update mru device in ps */
    appDeviceUpdateMruDevice(&handset_address);

    /* Store device data in ps */
    DeviceDbSerialiser_Serialise();

}

bool appDeviceGetPeerBdAddr(bdaddr *bd_addr)
{
//    return btDevice_GetDeviceBdAddr(DEVICE_TYPE_EARBUD, bd_addr);
    bool rc = FALSE;
    rc = btDevice_GetDeviceBdAddr(DEVICE_TYPE_EARBUD, bd_addr);
    DEBUG_LOG("appDeviceGetPeerBdAddr %04x,%02x,%06lx", bd_addr->nap, bd_addr->uap, bd_addr->lap);
    return rc;
}

bool appDeviceGetHandsetBdAddr(bdaddr *bd_addr)
{
    uint8 is_mru_handset = TRUE;
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &is_mru_handset, sizeof(uint8));
    if (device)
    {
        // Get MRU handset device
        *bd_addr = DeviceProperties_GetBdAddr(device);
        return TRUE;
    }
    else
    {
        // Get first Handset in Device Database
        return btDevice_GetDeviceBdAddr(DEVICE_TYPE_HANDSET, bd_addr);
    }
}

bool BtDevice_GetAllHandsetBdAddr(bdaddr **bd_addr, unsigned *num_addresses)
{
    device_t* devices = NULL;
    unsigned num_devices = 0;
    *bd_addr = NULL;
    deviceType type = DEVICE_TYPE_HANDSET;

    DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);

    if(num_devices)
    {
        unsigned index;
        *bd_addr = PanicUnlessMalloc(sizeof(bdaddr) * num_devices);
        for(index = 0 ; index < num_devices ; index++)
        {
             (*bd_addr)[index] = DeviceProperties_GetBdAddr(devices[index]);
        }
    }

    free(devices);
    devices = NULL;
    *num_addresses = num_devices;
    return (num_devices > 0);
}

bool BtDevice_IsPairedWithHandset(void)
{
    bdaddr bd_addr;
    BdaddrSetZero(&bd_addr);
    return btDevice_GetDeviceBdAddr(DEVICE_TYPE_HANDSET, &bd_addr);
}

bool BtDevice_IsPairedWithPeer(void)
{
    bdaddr bd_addr;
    BdaddrSetZero(&bd_addr);
    return btDevice_GetDeviceBdAddr(DEVICE_TYPE_EARBUD, &bd_addr);
}

bool BtDevice_IsPairedWithSink(void)
{
    bdaddr bd_addr;
    BdaddrSetZero(&bd_addr);
    return btDevice_GetDeviceBdAddr(DEVICE_TYPE_SINK, &bd_addr);
}

bool appDeviceGetFlags(bdaddr *bd_addr, uint16 *flags)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);

    if (device)
    {
        return Device_GetPropertyU16(device, device_property_flags, flags);
    }
    else
    {
        *flags = 0;
        return FALSE;
    }
}

bool appDeviceGetMyBdAddr(bdaddr *bd_addr)
{
    bool succeeded = FALSE;
    if (bd_addr && btDevice_GetDeviceBdAddr(DEVICE_TYPE_SELF, bd_addr))
    {
        succeeded = TRUE;
    }
    return succeeded;
}

bool appDeviceDelete(const bdaddr *bd_addr)
{
    DEBUG_LOG("appDeviceDelete addr = %04x,%02x,%06lx",bd_addr->nap, bd_addr->uap, bd_addr->lap);

    if (!ConManagerIsConnected(bd_addr))
    {
        ConnectionAuthSetPriorityDevice(bd_addr, FALSE);
        ConnectionSmDeleteAuthDevice(bd_addr);

        device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
        if (device)
        {
            DeviceList_RemoveDevice(device);
            Device_Destroy(&device);
            DeviceDbSerialiser_Serialise();

            BtDevice_PrintAllDevices();
        }

        return TRUE;
    }
    else
    {
        DEBUG_LOG("appDeviceDelete, Failed to delete device as connected");
        return FALSE;
    }
}

void BtDevice_DeleteAllDevicesOfType(deviceType type)
{
    device_t* devices = NULL;
    unsigned num_devices = 0;

    DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);
    if (devices && num_devices)
    {
        for (unsigned i=0; i< num_devices; i++)
        {
            bdaddr bd_addr = DeviceProperties_GetBdAddr(devices[i]);
            appDeviceDelete(&bd_addr);
        }
    }
    free(devices);
    devices = NULL;
}

static void appDeviceHandleSetLinkBehaviorCfm(CL_DM_SET_LINK_BEHAVIOR_CFM_T * message)
{
    DEBUG_LOG_INFO("appDeviceHandleSetLinkBehaviorCfm, status %d, addr %04x,%02x,%06lx",
              message->status,
              message->taddr.addr.nap,
              message->taddr.addr.uap,
              message->taddr.addr.lap);
}

bool BtDevice_IsFull(void)
{
    return DeviceList_GetMaxTrustedDevices() == DeviceList_GetNumOfDevices();
}

static inline void btDevice_LocalBdAddrCfm(bool status, bdaddr* device_addr)
{
    if(status)
    {
        deviceType type = DEVICE_TYPE_SELF;
        device_t my_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_type, &type, sizeof(deviceType));
        
        if (my_device)
        {
            bdaddr sanitised_bdaddr = btDevice_SanitiseBdAddr(device_addr);
            BtDevice_SetMyAddress(&sanitised_bdaddr);
            DEBUG_LOG("local device bd addr set to lap: 0x%x", device_addr->lap);
        }
        LocalAddr_SetProgrammedBtAddress(device_addr);
    }
    else
    {
        DEBUG_LOG("Failed to read local BDADDR");
        Panic();
    }
}

bool appDeviceHandleClDmLocalBdAddrCfm(Message message)
{
    DEBUG_LOG("appDeviceHandleClDmLocalBdAddrCfm");
    CL_DM_LOCAL_BD_ADDR_CFM_T *cfm = (CL_DM_LOCAL_BD_ADDR_CFM_T *)message;
    btDevice_LocalBdAddrCfm(cfm->status == hci_success, &cfm->bd_addr);

    return TRUE;
}

static void btDevice_HandleDeviceDeleteInd (typed_bdaddr *tbdaddr)
{
    device_t device;

    DEBUG_LOG_INFO("btDevice_HandleDeviceDeleteInd: 0x%x lap 0x%x", tbdaddr->type, tbdaddr->addr.lap);

    device = BtDevice_GetDeviceForBdAddr(&tbdaddr->addr);
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);
        if ((flags & DEVICE_FLAGS_KEY_SYNC_PDL_UPDATE_IN_PROGRESS) == 0)
        {
            DeviceList_RemoveDevice(device);
            Device_Destroy(&device);
            DeviceDbSerialiser_Serialise();
            DEBUG_LOG_VERBOSE("btDevice_HandleDeviceDeleteInd device removed");
        }
    }
}


/*! @brief BT device task message handler.
 */
static void appDeviceHandleMessage(Task task, MessageId id, Message message)
{
    UNUSED(task);

    switch (id)
    {
        /* Bt device handover message */
        case BT_INTERNAL_MSG_STORE_PS_DATA:
            btDevice_StoreDeviceDataInPs();
            break;

        case CL_DM_SET_LINK_BEHAVIOR_CFM:
            appDeviceHandleSetLinkBehaviorCfm((CL_DM_SET_LINK_BEHAVIOR_CFM_T*)message);
            break;

        default:
            break;
    }
}

bool BtDevice_HandleConnectionLibraryMessages(MessageId id, Message message, bool already_handled)
{
    bool handled = FALSE;
    UNUSED(already_handled);

    switch(id)
    {
        case CL_SM_AUTH_DEVICE_DELETED_IND:
            {
                CL_SM_AUTH_DEVICE_DELETED_IND_T *ind = (CL_SM_AUTH_DEVICE_DELETED_IND_T *)message;
                btDevice_HandleDeviceDeleteInd(&ind->taddr);
                handled = TRUE;
            }
            break;

        default:
            break;
    }

    return handled;
}


bool appDeviceInit(Task init_task)
{
    deviceTaskData *theDevice = DeviceGetTaskData();

    DEBUG_LOG("appDeviceInit");

    theDevice->task.handler = appDeviceHandleMessage;
    theDevice->listeners = TaskList_CreateWithCapacity(1);

    Ui_RegisterUiProvider(ui_provider_device, Device_GetCurrentContxt);

    ConnectionReadLocalAddr(init_task);

    DeviceList_Iterate(btDevice_SetLinkBehaviorByDevice, NULL);

    BtDevice_PrintAllDevices();

    return TRUE;
}

void BtDevice_RegisterListener(Task listener)
{
    TaskList_AddTask(DeviceGetTaskData()->listeners, listener);
}

deviceType BtDevice_GetDeviceType(device_t device)
{
    deviceType type = DEVICE_TYPE_UNKNOWN;
    void *value = NULL;
    size_t size = sizeof(deviceType);
    if (Device_GetProperty(device, device_property_type, &value, &size))
    {
        type = *(deviceType *)value;
    }
    return type;
}

bool appDeviceIsPeer(const bdaddr *bd_addr)
{
    bool isPeer = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        if ((BtDevice_GetDeviceType(device) == DEVICE_TYPE_EARBUD) ||
            (BtDevice_GetDeviceType(device) == DEVICE_TYPE_SELF))
        {
            isPeer = TRUE;
        }
    }
    return isPeer;
}

bool BtDevice_LeDeviceIsPeer(const tp_bdaddr *tpaddr)
{
    bool device_is_peer;

    if (tpaddr->taddr.type == TYPED_BDADDR_RANDOM)
    {
        tp_bdaddr remote;

        if (VmGetPublicAddress(tpaddr, &remote))
        {
            device_is_peer = appDeviceIsPeer(&remote.taddr.addr);
        }
        else
        {
        /*  Assume no IRK => not bonded => not our peer  */
            device_is_peer = FALSE;
        }
    }
    else
    {
        device_is_peer = appDeviceIsPeer(&tpaddr->taddr.addr);
    }

    return device_is_peer;
}

bool appDeviceIsHandset(const bdaddr *bd_addr)
{
    return appDeviceTypeIsHandset(bd_addr);
}

bool appDeviceTypeIsHandset(const bdaddr *bd_addr)
{
    bool is_handset = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        if (BtDevice_GetDeviceType(device) == DEVICE_TYPE_HANDSET)
        {
            is_handset = TRUE;
        }
    }
    return is_handset;
}

bool appDeviceTypeIsSink(const bdaddr *bd_addr)
{
    bool is_sink = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        if (BtDevice_GetDeviceType(device) == DEVICE_TYPE_SINK)
        {
            is_sink = TRUE;
        }
    }
    return is_sink;
}

bool BtDevice_IsProfileSupported(const bdaddr *bd_addr, uint32 profile_to_check)
{
    bool is_supported = FALSE;
    uint32 supported_profiles = 0;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device && Device_GetPropertyU32(device, device_property_supported_profiles, &supported_profiles))
    {
        is_supported = !!(supported_profiles & profile_to_check);
    }
    return is_supported;
}

void BtDevice_AddSupportedProfilesToDevice(device_t device, uint32 profile_mask)
{
    PanicNull(device);
    uint32 supported_profiles = 0;
    Device_GetPropertyU32(device, device_property_supported_profiles, &supported_profiles);

    DEBUG_LOG("BtDevice_SetSupportedProfilesToDevice, device 0x%p supported_profiles %08x profile_mask %08x",
                device, supported_profiles, profile_mask);

    supported_profiles |= profile_mask;

    Device_SetPropertyU32(device, device_property_supported_profiles, supported_profiles);
}

device_t BtDevice_AddSupportedProfiles(const bdaddr *bd_addr, uint32 profile_mask)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        BtDevice_AddSupportedProfilesToDevice(device, profile_mask);
    }
    return device;
}

void BtDevice_RemoveSupportedProfiles(const bdaddr *bd_addr, uint32 profile_mask)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint32 supported_profiles = 0;
        Device_GetPropertyU32(device, device_property_supported_profiles, &supported_profiles);

        DEBUG_LOG("BtDevice_RemoveSupportedProfiles, device 0x%p supported_profiles %08x profile_mask %08x",
                  device, supported_profiles, profile_mask);

        supported_profiles &= ~profile_mask;

        Device_SetPropertyU32(device, device_property_supported_profiles, supported_profiles);
    }
}

uint32 BtDevice_GetSupportedProfilesForDevice(device_t device)
{
    uint32 supported_profiles = 0;
    if (device)
    {
        Device_GetPropertyU32(device, device_property_supported_profiles, &supported_profiles);
        DEBUG_LOG("BtDevice_GetSupportedProfilesForDevice, device 0x%p supported_profiles %08x", device, supported_profiles);
    }
    return supported_profiles;
}

uint32 BtDevice_GetSupportedProfiles(const bdaddr *bd_addr)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    return BtDevice_GetSupportedProfilesForDevice(device);
}

void BtDevice_SetConnectedProfiles(device_t device, uint32 connected_profiles_mask )
{
    PanicNull(device);

    DEBUG_LOG("BtDevice_SetConnectedProfiles, connected_profiles %08x", connected_profiles_mask);
    Device_SetPropertyU32(device, device_property_connected_profiles, connected_profiles_mask);
}

uint32 BtDevice_GetConnectedProfiles(device_t device)
{
    uint32 connected_profiles_mask = 0;
    PanicNull(device);
    Device_GetPropertyU32(device, device_property_connected_profiles, &connected_profiles_mask);
    return connected_profiles_mask;
}

void appDeviceSetLinkMode(const bdaddr *bd_addr, deviceLinkMode link_mode)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        Device_SetProperty(device, device_property_link_mode, (void *)&link_mode, sizeof(deviceLinkMode));
    }
}

static bool btDevice_IsDeviceConnectedOverBredr(device_t device)
{
    bdaddr handset_addr = DeviceProperties_GetBdAddr(device);
    return ConManagerIsConnected(&handset_addr);
}

static bool btDevice_IsDeviceConnectedOverLe(device_t device)
{
    bool is_connected = FALSE;
    tp_bdaddr handset_addr;

    handset_addr.transport = TRANSPORT_BLE_ACL;
    handset_addr.taddr.type = TYPED_BDADDR_PUBLIC;
    handset_addr.taddr.addr = DeviceProperties_GetBdAddr(device);

    is_connected = ConManagerIsTpConnected(&handset_addr);

    if(!is_connected)
    {
        handset_addr.taddr.type = TYPED_BDADDR_RANDOM;
        is_connected = ConManagerIsTpConnected(&handset_addr);
    }
    return is_connected;
}

static bool btDevice_IsDeviceConnectedOverBredrOrLe(device_t device)
{
    return (btDevice_IsDeviceConnectedOverBredr(device) || btDevice_IsDeviceConnectedOverLe(device));
}

typedef bool (*TEST_CONNECTION_FN_T)(device_t device);

static bool btDevice_IsHandsetConnected(TEST_CONNECTION_FN_T connected)
{
    bool is_handset_connected = FALSE;
    device_t* devices = NULL;
    unsigned num_devices = 0;
    deviceType type = DEVICE_TYPE_HANDSET;

    DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);

    if (devices && num_devices)
    {
        for (unsigned i=0; i< num_devices; i++)
        {
            is_handset_connected = connected(devices[i]);

            if(is_handset_connected)
            {
                break;
            }
        }
    }
    free(devices);
    devices = NULL;

   return is_handset_connected;
}

bool appDeviceIsBredrHandsetConnected(void)
{
    return btDevice_IsHandsetConnected(btDevice_IsDeviceConnectedOverBredr);
}

bool appDeviceIsLeHandsetConnected(void)
{
    return btDevice_IsHandsetConnected(btDevice_IsDeviceConnectedOverLe);
}

bool appDeviceIsHandsetConnected(void)
{
    return btDevice_IsHandsetConnected(btDevice_IsDeviceConnectedOverBredrOrLe);
}

typedef bool (*HANDSET_FILTER_FN_T)(device_t device);

static unsigned btDevice_GetFilteredConnectedHandset(device_t** devices, HANDSET_FILTER_FN_T filter_function)
{
    unsigned num_devices = 0;
    unsigned num_handsets_connected = 0;
    deviceType type = DEVICE_TYPE_HANDSET;

    PanicNull(devices);

    DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), devices, &num_devices);

    if (*devices)
    {
        for(unsigned i=0; i< num_devices; i++)
        {
            if(filter_function((*devices)[i]))
            {
                (*devices)[num_handsets_connected] = (*devices)[i];
                num_handsets_connected++;
            }
        }
    }
    return num_handsets_connected;
}

unsigned BtDevice_GetConnectedBredrHandsets(device_t** devices)
{
    return btDevice_GetFilteredConnectedHandset(devices, btDevice_IsDeviceConnectedOverBredr);
}

unsigned BtDevice_GetConnectedLeHandsets(device_t** devices)
{
    return btDevice_GetFilteredConnectedHandset(devices, btDevice_IsDeviceConnectedOverLe);
}

unsigned BtDevice_GetConnectedHandsets(device_t** devices)
{
    return btDevice_GetFilteredConnectedHandset(devices, btDevice_IsDeviceConnectedOverBredrOrLe);
}

unsigned BtDevice_GetNumberOfHandsetsConnectedOverBredr(void)
{
    device_t* devices = NULL;
    unsigned num_connected_handsets = BtDevice_GetConnectedBredrHandsets(&devices);
    free(devices);
    return num_connected_handsets;
}

unsigned BtDevice_GetNumberOfHandsetsConnectedOverLe(void)
{
    device_t* devices = NULL;
    unsigned num_connected_handsets = BtDevice_GetConnectedLeHandsets(&devices);
    free(devices);
    return num_connected_handsets;
}

unsigned BtDevice_GetNumberOfHandsetsConnected(void)
{
    device_t* devices = NULL;
    unsigned num_connected_handsets = BtDevice_GetConnectedHandsets(&devices);
    free(devices);
    return num_connected_handsets;
}

static avInstanceTaskData* btDevice_GetAvInstanceForHandset(void)
{
    bdaddr bd_addr;
    avInstanceTaskData* av_instance = NULL;

    if(appDeviceGetHandsetBdAddr(&bd_addr))
    {
        device_t device = BtDevice_GetDeviceForBdAddr(&bd_addr);
        av_instance = Av_InstanceFindFromDevice(device);
    }

    return av_instance;
}

bool appDeviceIsHandsetA2dpDisconnected(void)
{
    bool is_disconnected = TRUE;
    avInstanceTaskData *inst = btDevice_GetAvInstanceForHandset();
    if (inst)
    {
        if (!appA2dpIsDisconnected(inst))
            is_disconnected = FALSE;
    }
    return is_disconnected;
}

bool appDeviceIsHandsetA2dpConnected(void)
{
    bool is_connected = FALSE;
    avInstanceTaskData *inst = btDevice_GetAvInstanceForHandset();
    if (inst)
    {
        if (appA2dpIsConnected(inst))
            is_connected = TRUE;
    }
    return is_connected;
}

bool appDeviceIsHandsetA2dpStreaming(void)
{
    bool is_streaming = FALSE;
    avInstanceTaskData *inst = btDevice_GetAvInstanceForHandset();
    if (inst)
    {
        if (appA2dpIsStreaming(inst))
            is_streaming = TRUE;
    }
    return is_streaming;
}

bool appDeviceIsHandsetAvrcpDisconnected(void)
{
    bool is_disconnected = TRUE;
    avInstanceTaskData *inst = btDevice_GetAvInstanceForHandset();
    if (inst)
    {
        if (!appAvrcpIsDisconnected(inst))
            is_disconnected = FALSE;
    }
    return is_disconnected;
}

bool appDeviceIsHandsetAvrcpConnected(void)
{
    bool is_connected = FALSE;
    avInstanceTaskData *inst = btDevice_GetAvInstanceForHandset();
    if (inst)
    {
        if (appAvrcpIsConnected(inst))
            is_connected = TRUE;
    }
    return is_connected;
}

bool appDeviceIsPeerConnected(void)
{
    bool is_peer_connected = FALSE;
    bdaddr peer_addr;
    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        is_peer_connected = ConManagerIsConnected(&peer_addr);
    }
    return is_peer_connected;
}

bool appDeviceIsPeerA2dpConnected(void)
{
    bdaddr peer_addr;
    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        avInstanceTaskData* inst = appAvInstanceFindFromBdAddr(&peer_addr);
        if (inst)
        {
            if (!appA2dpIsDisconnected(inst))
                return TRUE;
        }
    }
    return FALSE;
}

bool appDeviceIsPeerAvrcpConnected(void)
{
    bdaddr peer_addr;
    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        avInstanceTaskData* inst = appAvInstanceFindFromBdAddr(&peer_addr);
        if (inst)
        {
            if (!appAvrcpIsDisconnected(inst))
                return TRUE;
        }
    }
    return FALSE;
}

bool appDeviceIsPeerAvrcpConnectedForAv(void)
{
    bdaddr peer_addr;
    if (appDeviceGetPeerBdAddr(&peer_addr))
    {
        avInstanceTaskData* inst = appAvInstanceFindFromBdAddr(&peer_addr);
        if (inst)
        {
            return appAvIsAvrcpConnected(inst);
        }
    }
    return FALSE;
}

bool appDeviceIsPeerMirrorConnected(void)
{
    return MirrorProfile_IsConnected();
}

/*! \brief Set flag for handset device indicating if address needs to be sent to peer earbud.

    \param handset_bd_addr BT address of handset device.
    \param reqd  TRUE Flag is set, link key is required to be sent to peer earbud.
                 FALSE Flag is clear, link key does not need to be sent to peer earbud.
    \return bool TRUE Success, FALSE failure device not known.
*/
bool appDeviceSetHandsetAddressForwardReq(const bdaddr *handset_bd_addr, bool reqd)
{
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, handset_bd_addr, sizeof(bdaddr));
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);

        if (reqd)
            flags |= DEVICE_FLAGS_HANDSET_ADDRESS_FORWARD_REQD;
        else
            flags &= ~DEVICE_FLAGS_HANDSET_ADDRESS_FORWARD_REQD;

        Device_SetPropertyU16(device, device_property_flags, flags);

        return TRUE;
    }

    return FALSE;
}

/*! \brief Set flag device indicating QHS has been used

    \param bd_addr address of the device.
    \param suspported TRUE QHS Flag is set indicating it has been connected
           FALSE QHS Flag is cleared indicating it isn't supported

    \note This flag is used to indicate the QHS has been conected, and not that
          it is connected

    \return bool TRUE Success, FALSE failure device not known.
*/
bool appDeviceSetQhsConnected(const bdaddr *bd_addr, bool supported)
{
    device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, bd_addr, sizeof(bdaddr));
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);

        if (supported)
            flags |= DEVICE_FLAGS_QHS_CONNECTED;
        else
            flags &= ~DEVICE_FLAGS_QHS_CONNECTED;

        Device_SetPropertyU16(device, device_property_flags, flags);

        return TRUE;
    }

    return FALSE;
}

/*! \brief Set flag device indicating first connect post DFU

    \device handle to a device instance.
    \param set TRUE If first connect post flag is set indicating next connect
           will be first connect post DFU; else FALSE.

    \return bool TRUE Success, FALSE failure device not known.
*/
bool appDeviceSetFirstConnectAfterDFU(device_t device, bool set)
{
    DEBUG_LOG("appDeviceSetFirstConnectAfterDFU device 0x%x set %d", device, set);
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);

        if (set)
            flags |= DEVICE_FLAGS_FIRST_CONNECT_AFTER_DFU;
        else
            flags &= ~DEVICE_FLAGS_FIRST_CONNECT_AFTER_DFU;

        Device_SetPropertyU16(device, device_property_flags, flags);

        return TRUE;
    }

    return FALSE;
}

bool appDeviceIsTwsPlusHandset(const bdaddr *handset_bd_addr)
{
    UNUSED(handset_bd_addr);
    return FALSE;
}

bool appDeviceIsHandsetAnyProfileConnected(void)
{
    return appHfpIsConnected() ||
           appDeviceIsHandsetA2dpConnected() ||
           appDeviceIsHandsetAvrcpConnected();
}

inline static void btDevice_ClearPreviousMruDevice(void)
{
    uint8 mru = TRUE;
    device_t old_mru_device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &mru, sizeof(uint8));
    if (old_mru_device)
    {
        Device_SetPropertyU8(old_mru_device, device_property_mru, FALSE);
    }
}

void appDeviceUpdateMruDevice(const bdaddr *bd_addr)
{
    static bdaddr bd_addr_mru_cached = {0, 0, 0};

    if (!BdaddrIsSame(bd_addr, &bd_addr_mru_cached))
    {
        device_t new_mru_device = BtDevice_GetDeviceForBdAddr(bd_addr);
        if (new_mru_device)
        {
            if (BtDevice_GetDeviceType(new_mru_device)==DEVICE_TYPE_HANDSET ||
                BtDevice_GetDeviceType(new_mru_device)==DEVICE_TYPE_SINK)
            {
                btDevice_ClearPreviousMruDevice();

                Device_SetPropertyU8(new_mru_device, device_property_mru, TRUE);
            }
            ConnectionSmUpdateMruDevice(bd_addr);
            bd_addr_mru_cached = *bd_addr;
        }
        else
        {
            // Unexpectedly unable to find device address, reset mru cache
            memset(&bd_addr_mru_cached, 0, sizeof(bd_addr_mru_cached));
        }
    }
}

device_t BtDevice_GetMruDevice(void)
{
    uint8 mru_device = TRUE;
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_mru, &mru_device, sizeof(uint8));
}

static unsigned Device_GetCurrentContxt(void)
{
    dm_provider_context_t current_ctxt;

    if(appHfpIsConnected() || appDeviceIsHandsetA2dpConnected())
    {
        current_ctxt = context_handset_connected;
    }
    else
    {
        current_ctxt = context_handset_not_connected;
    }

    return (unsigned)current_ctxt;
}

static bool appDeviceGetBdAddrByFlag(bdaddr* bd_addr, uint16 desired_mask)
{
    uint16 flags;

    /*! \todo Would we do better with a database scan and check on flags.
        Or make the property of PRI/SEC a field  */
    if (appDeviceGetMyBdAddr(bd_addr))
    {
        if (appDeviceGetFlags(bd_addr, &flags))
        {
            if ((flags & desired_mask) == desired_mask)
            {
                return TRUE;
            }
        }
    }

    if (appDeviceGetPeerBdAddr(bd_addr))
    {
        if (appDeviceGetFlags(bd_addr, &flags))
        {
            if ((flags & desired_mask) == desired_mask)
            {
                return TRUE;
            }
        }
    }

    BdaddrSetZero(bd_addr);
    return FALSE;
}

bool appDeviceGetPrimaryBdAddr(bdaddr* bd_addr)
{
    return appDeviceGetBdAddrByFlag(bd_addr, DEVICE_FLAGS_PRIMARY_ADDR);
}

bool appDeviceGetSecondaryBdAddr(bdaddr* bd_addr)
{
    return appDeviceGetBdAddrByFlag(bd_addr, DEVICE_FLAGS_SECONDARY_ADDR);
}

bool appDeviceIsPrimary(const bdaddr* bd_addr)
{
    bdaddr primary_addr;
    return (appDeviceGetBdAddrByFlag(&primary_addr, DEVICE_FLAGS_PRIMARY_ADDR)
            && BdaddrIsSame(bd_addr, &primary_addr));
}

bool appDeviceIsSecondary(const bdaddr* bd_addr)
{
    bdaddr secondary_addr;
    return (appDeviceGetBdAddrByFlag(&secondary_addr, DEVICE_FLAGS_SECONDARY_ADDR)
            && BdaddrIsSame(bd_addr, &secondary_addr));
}

bool BtDevice_IsMyAddressPrimary(void)
{
    bdaddr self = {0}, primary = {0};
    bool is_primary = FALSE;
    if(appDeviceGetPrimaryBdAddr(&primary) && appDeviceGetMyBdAddr(&self))
    {
        is_primary = BdaddrIsSame(&primary, &self);
    }
    DEBUG_LOG("BtDevice_AmIPrimary =%d, primary %04x,%02x,%06lx, self %04x,%02x,%06lx", is_primary, primary.nap, primary.uap, primary.lap, self.nap, self.uap, self.lap );
    return is_primary;
}

/*! \brief Determine if a device had connected QHS.

    \param bd_addr Pointer to read-only BT device address.
    \return bool TRUE address device supports QHS and it has been connected, FALSE if not.
*/
bool BtDevice_WasQhsConnected(const bdaddr *bd_addr)
{
    bool qhs_connected = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);
        qhs_connected = !!(flags & DEVICE_FLAGS_QHS_CONNECTED);
    }
    return qhs_connected;
}

/*! \brief Determine if a device has connected first time post DFU.

    \device handle to a device instance.
    \return bool TRUE if a device has connected first time post DFU, FALSE if not.
*/
bool BtDevice_IsFirstConnectAfterDFU(device_t device)
{
    bool first_connect_after_dfu = FALSE;
    if (device)
    {
        uint16 flags = 0;
        Device_GetPropertyU16(device, device_property_flags, &flags);
        first_connect_after_dfu = !!(flags & DEVICE_FLAGS_FIRST_CONNECT_AFTER_DFU);
    }
    DEBUG_LOG("BtDevice_IsFirstConnectAfterDFU first_connect_after_dfu %d", first_connect_after_dfu);
    return first_connect_after_dfu;
}

/*! \brief Set flag for handset device indicating if link key needs to be sent to
           peer earbud.

    \param handset_bd_addr BT address of handset device.
    \param reqd  TRUE link key TX is required, FALSE link key TX not required.
    \return bool TRUE Success, FALSE failure.
 */
bool BtDevice_SetHandsetLinkKeyTxReqd(bdaddr *handset_bd_addr, bool reqd)
{
    if (appDeviceGetHandsetBdAddr(handset_bd_addr))
    {
        uint16 flags = 0;
        device_t device = DeviceList_GetFirstDeviceWithPropertyValue(device_property_bdaddr, handset_bd_addr, sizeof(bdaddr));
        PanicFalse(device);
        Device_GetPropertyU16(device, device_property_flags, &flags);
        if (reqd)
            flags |= DEVICE_FLAGS_HANDSET_LINK_KEY_TX_REQD;
        else
            flags &= ~DEVICE_FLAGS_HANDSET_LINK_KEY_TX_REQD;
        Device_SetPropertyU16(device, device_property_flags, flags);
        return TRUE;
    }
    return FALSE;
}

bool appDeviceSetBatterServerConfigLeft(const bdaddr *bd_addr, uint16 config)
{
    bool config_set = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 client_config = config;
        config_set = Device_GetPropertyU16(device, device_property_battery_server_config_l, &client_config);
        if (!config_set || (config != client_config))
        {
            Device_SetPropertyU16(device, device_property_battery_server_config_l, config);
        }
    }
    return config_set;
}

bool appDeviceGetBatterServerConfigLeft(const bdaddr *bd_addr, uint16* config)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    return (device) ? Device_GetPropertyU16(device, device_property_battery_server_config_l, config) : FALSE;
}

bool appDeviceSetBatterServerConfigRight(const bdaddr *bd_addr, uint16 config)
{
    bool config_set = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 client_config = config;
        config_set = Device_GetPropertyU16(device, device_property_battery_server_config_r, &client_config);
        if (!config_set || (config != client_config))
        {
            Device_SetPropertyU16(device, device_property_battery_server_config_r, config);
        }
    }
    return config_set;
}

bool appDeviceGetBatterServerConfigRight(const bdaddr *bd_addr, uint16* config)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    return (device) ? Device_GetPropertyU16(device, device_property_battery_server_config_r, config) : FALSE;
}

bool appDeviceSetGattServerConfig(const bdaddr *bd_addr, uint16 config)
{
    bool config_set = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint16 client_config = config;
        config_set = Device_GetPropertyU16(device, device_property_gatt_server_config, &client_config);
        if (!config_set || (config != client_config))
        {
            Device_SetPropertyU16(device, device_property_gatt_server_config, config);
        }
    }
    return config_set;
}

bool appDeviceGetGattServerConfig(const bdaddr *bd_addr, uint16* config)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    return (device) ? Device_GetPropertyU16(device, device_property_gatt_server_config, config) : FALSE;
}

bool appDeviceSetGattServerServicesChanged(const bdaddr *bd_addr, uint8 flag)
{
    bool config_set = FALSE;
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    if (device)
    {
        uint8 client_flag = flag;
        config_set = Device_GetPropertyU8(device, device_property_gatt_server_services_changed, &client_flag);
        if (!config_set || (flag != client_flag))
        {
            Device_SetPropertyU8(device, device_property_gatt_server_services_changed, flag);
        }
    }
    return config_set;
}

bool appDeviceGetGattServerServicesChanged(const bdaddr *bd_addr, uint8* flag)
{
    device_t device = BtDevice_GetDeviceForBdAddr(bd_addr);
    return (device) ? Device_GetPropertyU8(device, device_property_gatt_server_services_changed, flag) : FALSE;
}

static bool btDevice_ValidateAddressesForAddressSwap(const bdaddr *bd_addr_1, const bdaddr *bd_addr_2)
{
    if(!BtDevice_GetDeviceForBdAddr(bd_addr_1))
    {
        DEBUG_LOG("There is no device corresponding to address lap 0x%x", bd_addr_1->lap);
        return FALSE;
    }

    if(!BtDevice_GetDeviceForBdAddr(bd_addr_2))
    {
        DEBUG_LOG("There is no device corresponding to address lap 0x%x", bd_addr_2->lap);
        return FALSE;
    }

    if(BdaddrIsSame(bd_addr_1, bd_addr_2))
    {
        DEBUG_LOG("Addresses are the same, no point in swapping them");
        return FALSE;
    }

    if(!appDeviceIsPeer(bd_addr_1))
    {
        DEBUG_LOG("Address lap 0x%x doesn't belong to a peer device", bd_addr_1->lap);
        return FALSE;
    }

    if(!appDeviceIsPeer(bd_addr_2))
    {
        DEBUG_LOG("Address lap 0x%x doesn't belong to a peer device", bd_addr_2->lap);
        return FALSE;
    }

    return TRUE;
}

static void btDevice_SwapFlags(uint16 *flags_1, uint16 *flags_2, uint16 flags_to_swap)
{
    uint16 temp_1;
    uint16 temp_2;

    temp_1 = *flags_1 & flags_to_swap;
    *flags_1 &= ~flags_to_swap;
    temp_2 = *flags_2 & flags_to_swap;
    *flags_2 &= ~flags_to_swap;
    *flags_1 |= temp_2;
    *flags_2 |= temp_1;
}

bool BtDevice_SwapAddresses(const bdaddr *bd_addr_1, const bdaddr *bd_addr_2)
{
    device_t device_1;
    device_t device_2;

    uint16 flags_1;
    uint16 flags_2;

    PanicNull((bdaddr *)bd_addr_1);
    PanicNull((bdaddr *)bd_addr_2);

    DEBUG_LOG("BtDevice_SwapAddresses addr 1 lap 0x%x, addr 2 lap 0x%x", bd_addr_1->lap, bd_addr_2->lap);

    if(!btDevice_ValidateAddressesForAddressSwap(bd_addr_1, bd_addr_2))
    {
        return FALSE;
    }

    device_1 = BtDevice_GetDeviceForBdAddr(bd_addr_1);
    device_2 = BtDevice_GetDeviceForBdAddr(bd_addr_2);

    /* Swap BT addresses */

    Device_SetProperty(device_1, device_property_bdaddr, (void*)bd_addr_2, sizeof(bdaddr));
    Device_SetProperty(device_2, device_property_bdaddr, (void*)bd_addr_1, sizeof(bdaddr));

    /* Swap flags associated with the BT address */

    Device_GetPropertyU16(device_1, device_property_flags, &flags_1);
    Device_GetPropertyU16(device_2, device_property_flags, &flags_2);

    btDevice_SwapFlags(&flags_1, &flags_2,
            DEVICE_FLAGS_PRIMARY_ADDR | DEVICE_FLAGS_SECONDARY_ADDR);

    Device_SetPropertyU16(device_1, device_property_flags, flags_1);
    Device_SetPropertyU16(device_2, device_property_flags, flags_2);

    return TRUE;
}

bool BtDevice_SetMyAddress(const bdaddr *new_bd_addr)
{
    bdaddr my_bd_addr;

    DEBUG_LOG("BtDevice_SetMyAddressBySwapping new_bd_addr lap 0x%x", new_bd_addr->lap);

    if(!appDeviceGetMyBdAddr(&my_bd_addr))
    {
        return FALSE;
    }

    if(BdaddrIsSame(&my_bd_addr, new_bd_addr))
    {
        DEBUG_LOG("BtDevice_SetMyAddressBySwapping address is already new_bdaddr, no need to swap");
        BtDevice_PrintAllDevices();
        return TRUE;
    }
    else
    {
        bool ret = BtDevice_SwapAddresses(&my_bd_addr, new_bd_addr);
        BtDevice_PrintAllDevices();
        return ret;
    }


}

void BtDevice_PrintAllDevices(void)
{
    DEBUG_LOG("BtDevice_PrintAllDevices number of devices: %d", DeviceList_GetNumOfDevices());

    DeviceList_Iterate(btDevice_PrintDeviceInfo, NULL);
}



void BtDevice_StorePsDeviceDataWithDelay(void)
{
    MessageSendLater(&DeviceGetTaskData()->task, BT_INTERNAL_MSG_STORE_PS_DATA,
                     NULL, BT_DEVICE_STORE_PS_DATA_DELAY);
}

static tp_bdaddr btDevice_GetTpAddrFromTypedAddr(const typed_bdaddr *taddr)
{
    tp_bdaddr tpaddr;
    tp_bdaddr public_tpaddr;

    tpaddr.transport = TRANSPORT_BLE_ACL;
    tpaddr.taddr = *taddr;

    if(taddr->type == TYPED_BDADDR_RANDOM)
    {
        if(VmGetPublicAddress(&tpaddr, &public_tpaddr))
        {
            tpaddr = public_tpaddr;
        }
    }

    return tpaddr;
}

bool BtDevice_GetPublicAddress(const typed_bdaddr *source_taddr, typed_bdaddr *public_taddr)
{
    bool status;

    if (source_taddr->type == TYPED_BDADDR_PUBLIC)
    {
        *public_taddr = *source_taddr;
        status = TRUE;
    }
    else
    {
        tp_bdaddr tpaddr;

        tpaddr.transport = TRANSPORT_BLE_ACL;
        tpaddr.taddr = *source_taddr;
        status = VmGetPublicAddress(&tpaddr, &tpaddr);

        if (status)
        {
            *public_taddr = tpaddr.taddr;
        }
    }

    DEBUG_LOG("BtDevice_GetPublicAddress: %02x %04x %02x %06x -> %02x %04x %02x %06x (%u)",
        source_taddr->type,
        source_taddr->addr.nap, source_taddr->addr.uap, source_taddr->addr.lap,
        public_taddr->type,
        public_taddr->addr.nap, public_taddr->addr.uap, public_taddr->addr.lap,
        status);

    return status;
}

bool BtDevice_ResolvedBdaddrIsSame(const bdaddr *public_addr, const typed_bdaddr *taddr)
{
    typed_bdaddr resolved_taddr;
    bool is_same = FALSE;

    if (BtDevice_GetPublicAddress(taddr, &resolved_taddr))
    {
        if (BdaddrIsSame(public_addr, &resolved_taddr.addr))
        {
            is_same = TRUE;
        }
    }

    return is_same;
}

bool BtDevice_BdaddrTypedIsSame(const typed_bdaddr *taddr1, const typed_bdaddr *taddr2)
{
    tp_bdaddr tpaddr1 = btDevice_GetTpAddrFromTypedAddr(taddr1);
    tp_bdaddr tpaddr2 = btDevice_GetTpAddrFromTypedAddr(taddr2);

    return BdaddrTpIsSame(&tpaddr1, &tpaddr2);
}

bool BtDevice_SetDefaultProperties(device_t device)
{
    if(!DeviceProperties_SetAudioVolume(device, A2dpProfile_GetDefaultVolume()))
    {
        return FALSE;
    }
    if(!DeviceProperties_SetVoiceVolume(device, HfpProfile_GetDefaultVolume()))
    {
        return FALSE;
    }
    if(!Device_SetPropertyU8(device, device_property_hfp_mic_gain, HfpProfile_GetDefaultMicGain()))
    {
        return FALSE;
    }
    if(!Device_SetPropertyU8(device, device_property_hfp_profile, hfp_handsfree_profile))
    {
        return FALSE;
    }        

    return TRUE;
}

bool BtDevice_SetFlags(device_t device, uint16 flags_to_modify, uint16 flags)
{
    uint16 old_flags;
    uint16 new_flags;

    DEBUG_LOG("BtDevice_SetFlags %04x %04x", flags_to_modify, flags);

    if(!Device_GetPropertyU16(device, device_property_flags, &old_flags))
    {
        /* No flags property has been set, default to 0 */
        old_flags = 0;
    }

    DEBUG_LOG("BtDevice_SetFlags old %04x", old_flags);

    new_flags = old_flags;

    new_flags &= ~(flags_to_modify & ~flags);
    new_flags |= (flags_to_modify & flags);

    DEBUG_LOG("BtDevice_SetFlags new %04x", new_flags);

    if(new_flags != old_flags)
    {
        if(!Device_SetPropertyU16(device, device_property_flags, new_flags))
        {
            return FALSE;
        }
    }
    return TRUE;
}

void BtDevice_Validate(void)
{
    DEBUG_LOG_VERBOSE("BtDevice_Validate");

    if(DeviceList_GetNumOfDevices() > 0)
    {
        device_t* devices = NULL;
        unsigned num_devices = 0;
        deviceType type = DEVICE_TYPE_SELF;

        DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);
        if(num_devices > 1)
        {
            DEBUG_LOG_ERROR("BtDevice_Validate: BAD STATE two self devices");
            Panic();
        }
        free(devices);
        devices = NULL;

        type = DEVICE_TYPE_EARBUD;
        DeviceList_GetAllDevicesWithPropertyValue(device_property_type, &type, sizeof(deviceType), &devices, &num_devices);
        if(num_devices > 1)
        {
            DEBUG_LOG_ERROR("BtDevice_Validate: BAD STATE two earbud devices");
            Panic();
        }
        free(devices);
    }
}

bool BtDevice_GetIndexedDevice(unsigned index, device_t* device)
{
    bool device_found = FALSE;
    typed_bdaddr taddr = {0};

    /* Get the BT address of the device from connection lib's Trusted Device List(TDL)*/
    if (ConnectionSmGetIndexedAttributeNowReq(0, index, 0, NULL, &taddr))
    {
        /* Get the device using the BT address.*/
        *device = BtDevice_GetDeviceForBdAddr(&taddr.addr);
        if (*device != NULL)
        {
            device_found = TRUE;
        }
        DEBUG_LOG("BtDevice_GetIndexedDevice addr [%04x,%02x,%06lx] device %p",
                                    taddr.addr.nap,
                                    taddr.addr.uap,
                                    taddr.addr.lap,
                                    *device);
    }

   return device_found;
}

bool BtDevice_GetTpBdaddrForDevice(device_t device, tp_bdaddr* tp_addr)
{
    bdaddr bd_addr;
    typed_bdaddr typ_addr;

    if (BtDevice_DeviceIsValid(device))
    {
        bd_addr = DeviceProperties_GetBdAddr(device);
        typ_addr.addr = bd_addr;
        typ_addr.type = TYPED_BDADDR_PUBLIC;

        tp_addr->taddr = typ_addr;
        tp_addr->transport = TRANSPORT_BREDR_ACL;

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void BtDevice_SetLinkBehavior(const bdaddr *addr)
{
    typed_bdaddr tpaddr;
    memcpy(&tpaddr.addr, (bdaddr*) addr, sizeof(bdaddr));
    tpaddr.type = TYPED_BDADDR_PUBLIC;

    ConnectionDmSetLinkBehaviorReq(&app_device.task, &tpaddr, FALSE);

    DEBUG_LOG_INFO("BtDevice_SetLinkBehavior addr %04x,%02x,%06lx",
              addr->nap,
              addr->uap,
              addr->lap);
}

bool BtDevice_SetUpgradeTransportConnected(device_t device, bool connected)
{
    bool successful = FALSE;

    if (device)
    {
        successful = Device_SetPropertyU8(device, device_property_upgrade_transport_connected, connected);
    }

    DEBUG_LOG("BtDevice_SetUpgradeTransportConnected device 0x%p connected %d",
              device,
              connected);

    return successful;
}

device_t BtDevice_GetUpgradeDevice(void)
{
    uint8 upgrade_transport_connected = TRUE;
    return DeviceList_GetFirstDeviceWithPropertyValue(device_property_upgrade_transport_connected, &upgrade_transport_connected, sizeof(uint8));
}
