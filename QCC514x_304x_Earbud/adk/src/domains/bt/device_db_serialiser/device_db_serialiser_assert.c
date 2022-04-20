/*!
\copyright  Copyright (c) 2020 - 2021 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Validation of device db.

*/

#include "device_db_serialiser_assert.h"

#include <device_list.h>
#include <connection_no_ble.h>
#include <logging.h>
#include <panic.h>
#include <ps.h>
#include "device_types.h"


#include <stdlib.h>

#define DB_INDEX 0
#define FRAME_LEN_INDEX 1
#define TYPE_INDEX 7
#define FLAGS_HI_INDEX 13
#define FLAGS_LOW_INDEX 14
#define FF_INDEX 34
#define ODD_BYTE_INDEX 35

bool DeviceDbSerialiser_IsPsStoreValid(void)
{
    unsigned type_cnt[4] = {0};

    for (uint8 pdl_index = 0; pdl_index < DeviceList_GetMaxTrustedDevices(); pdl_index++)
    {
        typed_bdaddr taddr = {0};
        uint16 pdd_frame_length = ConnectionSmGetIndexedAttributeSizeNowReq(pdl_index);
        if(pdd_frame_length)
        {
            uint8 *pdd_frame = (uint8 *)PanicUnlessMalloc(pdd_frame_length);
            if(ConnectionSmGetIndexedAttributeNowReq(0, pdl_index, pdd_frame_length, pdd_frame, &taddr))
            {
                DEBUG_LOG_VERBOSE("DeviceDbSerialiser_IsPsStoreValid lap 0x%x, db: 0x%x, frame_len: 0x%x, type: 0x%x, flags: 0x%02x%02x, ff: 0x%x, odd byte: 0x%x",
                        taddr.addr.lap, pdd_frame[DB_INDEX], pdd_frame[FRAME_LEN_INDEX], pdd_frame[TYPE_INDEX],
                        pdd_frame[FLAGS_HI_INDEX], pdd_frame[FLAGS_LOW_INDEX], pdd_frame[FF_INDEX], pdd_frame[ODD_BYTE_INDEX]);

                if(pdd_frame[7] < DEVICE_TYPE_MAX)
                {
                    ++type_cnt[pdd_frame[7]];
                }
                else
                {
                    free(pdd_frame);
                    return FALSE;
                }
            }
            free(pdd_frame);
        }
    }

    DEBUG_LOG_VERBOSE("DeviceDbSerialiser_IsPsStoreValid device type occurrences 0: %d, 1: %d, 2: %d, 3: %d",
            type_cnt[0], type_cnt[1], type_cnt[2], type_cnt[3]);

    if(type_cnt[1] > 1 || type_cnt[3] > 1)
    {
        return FALSE;
    }

    return TRUE;
}

#define ATTRIBUTE_BASE_PSKEY_INDEX  100
#define GATT_ATTRIBUTE_BASE_PSKEY_INDEX  110
#define TDL_BASE_PSKEY_INDEX        142

#define TDL_INDEX_PSKEY             141
#define PSKEY_SM_KEY_STATE_IR_ER_INDEX 140

void DeviceDbSerialiser_HandleCorruption(void)
{
    DEBUG_LOG_ERROR("DeviceDbSerialiser_HandleCorruption");


    for (int i=0; i < DeviceList_GetMaxTrustedDevices(); i++)
    {
        PsStore(ATTRIBUTE_BASE_PSKEY_INDEX+i, NULL, 0);
        PsStore(TDL_BASE_PSKEY_INDEX+i, NULL, 0);
        PsStore(GATT_ATTRIBUTE_BASE_PSKEY_INDEX+i, NULL, 0);
    }

    PsStore(TDL_INDEX_PSKEY, NULL, 0);
    PsStore(PSKEY_SM_KEY_STATE_IR_ER_INDEX, NULL, 0);

    Panic();
}
