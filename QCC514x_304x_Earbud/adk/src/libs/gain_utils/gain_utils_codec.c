/****************************************************************************
Copyright (c) 2004 - 2016 Qualcomm Technologies International, Ltd.

FILE NAME
    gain_utils.c
    
DESCRIPTION

*/

#include <stdlib.h>
#include <stream.h>
#include <source.h>
#include <sink.h>
#include <panic.h>

#include "gain_utils.h"

#define db_to_db_60(db)     (db * 60)
#define db_60_to_db(db_60)  (db_60 / 60)
#define MAX_ANALOGUE_RAW_GAIN   42

uint16 gainUtilsCalculateRawAdcGainAnalogueComponent(uint16 dB_60)
{
    uint16 analogue_component = 0;

    if(dB_60 <= db_to_db_60(MAX_ANALOGUE_RAW_GAIN) && (db_60_to_db(dB_60) % 3) == 0)
    {
        analogue_component = (db_60_to_db(dB_60) / 3);
    }
    return analogue_component;
}

