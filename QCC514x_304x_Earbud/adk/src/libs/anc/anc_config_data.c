/*******************************************************************************
Copyright (c) 2017-2020 Qualcomm Technologies International, Ltd.


FILE NAME
    anc_data.c

DESCRIPTION
    Encapsulation of the ANC VM Library data.
*/

#include "anc_config_data.h"
#include "anc_data.h"

/******************************************************************************/

#define TABLE_SIZE      (sizeof(look_up_table)/sizeof(look_up_table[0]))

/* Table to store pre defined values of gains (from 1 to 255) in fixed-point format (Q6.9) */
const int16 look_up_table[] = {

    -21577,  -18495,  -16692,  -15412,  -14420,  -13609,  -12924,  -12330,  -11806,  -11337,
    -10913,  -10527,  -10171,   -9841,   -9534,   -9247,   -8978,   -8723,   -8483,   -8255,
     -8038,   -7831,   -7633,   -7444,   -7262,   -7088,   -6920,   -6758,   -6602,   -6452,
     -6306,   -6165,   -6028,   -5895,   -5766,   -5641,   -5519,   -5400,   -5285,   -5172,
     -5062,   -4955,   -4851,   -4748,   -4648,   -4551,   -4455,   -4361,   -4270,   -4180,
     -4092,   -4005,   -3921,   -3838,   -3756,   -3676,   -3597,   -3520,   -3444,   -3369,
     -3296,   -3223,   -3152,   -3082,   -3013,   -2945,   -2878,   -2812,   -2748,   -2684,
     -2620,   -2558,   -2497,   -2436,   -2377,   -2318,   -2260,   -2202,   -2146,   -2090,
     -2034,   -1980,   -1926,   -1873,   -1820,   -1768,   -1717,   -1666,   -1616,   -1566,
     -1517,   -1468,   -1420,   -1373,   -1325,   -1279,   -1233,   -1187,   -1142,   -1097,
     -1053,   -1009,    -966,    -923,    -880,    -838,    -796,    -755,    -714,    -673,
      -633,    -593,    -554,    -515,    -476,    -437,    -399,    -361,    -324,    -287,
      -250,    -213,    -177,    -141,    -105,     -70,     -34,       0,      35,      69,
       103,     137,     170,     204,     237,     270,     302,     335,     367,     399,
       430,     462,     493,     524,     555,     585,     615,     646,     676,     705,
       735,     764,     793,     822,     851,     880,     908,     936,     964,     992,
      1020,    1048,    1075,    1102,    1129,    1156,    1183,    1209,    1236,    1262,
      1288,    1314,    1340,    1365,    1391,    1416,    1441,    1466,    1491,    1516,
      1541,    1565,    1590,    1614,    1638,    1662,    1686,    1710,    1733,    1757,
      1780,    1803,    1826,    1849,    1872,    1895,    1918,    1940,    1962,    1985,
      2007,    2029,    2051,    2073,    2095,    2116,    2138,    2159,    2180,    2202,
      2223,    2244,    2265,    2286,    2306,    2327,    2348,    2368,    2388,    2409,
      2429,    2449,    2469,    2489,    2509,    2528,    2548,    2567,    2587,    2606,
      2626,    2645,    2664,    2683,    2702,    2721,    2740,    2758,    2777,    2796,
      2814,    2832,    2851,    2869,    2887,    2905,    2923,    2941,    2959,    2977,
      2995,    3013,    3030,    3048,    3065
};

static int16 getClosestValue(int16 prev, int16 middle, int16 q_format)
{
    if (q_format - prev >= middle - q_format)
    {
        return middle;
    }
    else
        return prev;
}

static uint8 getGainFromIndex(uint8 index)
{
    return (index+1);
}

static uint16 getGainIndexFromGain(uint16 gain)
{
    return (gain-1);
}

bool ancConfigDataUpdateOnModeChange(anc_mode_t mode)
{
    return ancDataRetrieveAndPopulateTuningData(mode);
}

int16 convertGainTo16BitQFormat(uint16 gain)
{
    if(gain == 0)
        return gain;
    else
        return look_up_table[getGainIndexFromGain(gain)];
}

uint16 convert16BitQFormatToGain(int16 q_format)
{
    if(q_format == 0)
    {
        return 128;
    }
    uint8 middle_index = 0, lower_index = 0, upper_index = TABLE_SIZE - 1;

    while(lower_index <= upper_index)
    {
        middle_index = (lower_index + upper_index) / 2;

        if(q_format == look_up_table[middle_index])
        {
            return getGainFromIndex(middle_index);
        }
        if(q_format < look_up_table[middle_index])
        {
            if((middle_index > 0) && (q_format > look_up_table[middle_index-1]))
            {
                if(getClosestValue(look_up_table[middle_index-1], look_up_table[middle_index], q_format) == look_up_table[middle_index-1])
                    return getGainFromIndex(middle_index-1);
                else return getGainFromIndex(middle_index);
            }
            upper_index = middle_index;
        }
        else
        {
            if((middle_index < TABLE_SIZE-1) && (q_format < look_up_table[middle_index+1]))
            {
                if(getClosestValue(look_up_table[middle_index], look_up_table[middle_index+1], q_format) == look_up_table[middle_index])
                    return getGainFromIndex(middle_index);
                else return getGainFromIndex(middle_index+1);
            }
            lower_index = middle_index+1;
        }
    }
    return 0;
}
