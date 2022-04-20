/*******************************************************************************
Copyright (c) 2016 Qualcomm Technologies International, Ltd.

FILE NAME
    vmal_operator.c

DESCRIPTION
    Operator shim
*/

#include <vmal.h>
#include <operator.h>
#include <panic.h>
#include <stdlib.h>

#ifndef UNUSED
#define UNUSED(x) ((void)x)
#endif

#define MIN_RECEIVE_LEN 10



Operator VmalOperatorCreate(uint16 cap_id)
{
    return OperatorCreate(cap_id, 0, NULL);
}

Operator VmalOperatorCreateWithKeys(uint16 capability_id, vmal_operator_keys_t* keys, uint16 num_keys)
{
    return OperatorCreate(capability_id, num_keys, (OperatorCreateKeys *)keys);
}

bool VmalOperatorFrameworkEnableMainProcessor(bool enable)
{
    return OperatorFrameworkEnable(enable ? MAIN_PROCESSOR_ON : MAIN_PROCESSOR_OFF);
}

bool VmalOperatorFrameworkEnableSecondProcessor(bool enable)
{
    return OperatorFrameworkEnable(enable ? SECOND_PROCESSOR_ON : SECOND_PROCESSOR_OFF);
}
