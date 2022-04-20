/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Memory
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include "stm32f0xx.h"
#include "core_cm0.h"
#include "main.h"
#include "cmsis.h"
#include "memory.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* MEM_CFG_CODE: Arbitrary number to indicate that we have stored configuration
* data.
*/
#define MEM_CFG_CODE 0x6A2EB0ED

#ifdef STM32F070x6
#define MEM_RAM_SIZE 6128
#else
#define MEM_RAM_SIZE 16368
#endif

#ifdef TEST
#undef MEM_RAM_START
#define MEM_RAM_START ((uint32_t)test_ram)
static uint8_t test_ram[MEM_RAM_SIZE];
#endif

#define MEM_STACK_SIZE 1024

#define MEM_RAM_END (MEM_RAM_START + MEM_RAM_SIZE - 1)
#define MEM_STACK_END (MEM_RAM_END - MEM_STACK_SIZE + 1)

/*
* MAX_STACK_DUMP: Maximum number of (32 bit) words of the stack that we will
* output in mem_stack_dump().
*/
#define MAX_STACK_DUMP 40

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT mem_cmd_status(uint8_t cmd_source);
static CLI_RESULT mem_cmd_rd(uint8_t cmd_source);
static CLI_RESULT mem_cmd_wr(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint8_t mem_rd_len;
static uint32_t mem_rd_start;

static const CLI_COMMAND mem_command[] =
{
    { "",   mem_cmd_status, 2 },
    { "rd", mem_cmd_rd,     2 },
    { "wr", mem_cmd_wr,     2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct
{
    uint32_t code;
    struct
    {
        /** Request to enter standby mode on boot. This takes precedence over
         *  entering stop mode. */
        bool enter_standby : 1;

        /** Request to enter stop mode on boot. */
        bool enter_stop : 1;

        /*
        * Disable wake sources when entering standby or stop mode.
        */
        bool disable_wake_lid : 1;
        bool disable_wake_chg : 1;

    } data;
}
RAM_CFG;

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static RAM_CFG ram_cfg = {0};
RAM_CFG saved_cfg __attribute__((section("CFGRAM")));

/* Ensure we have space in our preserved RAM_CFG. */
_Static_assert(sizeof(ram_cfg) <= 16, "Preserved memory data too large for CFGRAM section");

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void mem_init(void)
{
    uint32_t msp = GET_MSP();
    uint32_t m;

    /*
    * Read RAM config, if there is one.
    */
    if (saved_cfg.code==MEM_CFG_CODE)
    {
        memcpy(&ram_cfg, &saved_cfg, sizeof(ram_cfg));
    }

    /*
    * For now, just wipe the entire RAM config space. All we have in there at
    * the moment is the standby mode selection, and that needs to be cleared.
    */
    memset(&saved_cfg, 0, sizeof(saved_cfg));

    /*
    * Fill unused stack memory with 0xFF, so that we can detect if it has
    * been overwritten later.
    */
    for (m=MEM_STACK_END; m<msp; m++)
    {
        *((uint8_t *)m) = 0xFF;
    }
}

static void mem_cfg_update(void)
{
    ram_cfg.code = MEM_CFG_CODE;
    memcpy(&saved_cfg, &ram_cfg, sizeof(saved_cfg));
}

void mem_cfg_standby_set(bool dw_lid, bool dw_chg)
{
    ram_cfg.data.enter_standby = true;
    ram_cfg.data.disable_wake_lid = dw_lid;
    ram_cfg.data.disable_wake_chg = dw_chg;
    mem_cfg_update();
}

bool mem_cfg_standby(void)
{
    return ram_cfg.data.enter_standby;
}

void mem_cfg_stop_set(bool dw_lid, bool dw_chg)
{
    ram_cfg.data.enter_stop = true;
    ram_cfg.data.disable_wake_lid = dw_lid;
    ram_cfg.data.disable_wake_chg = dw_chg;
    mem_cfg_update();
}

bool mem_cfg_stop(void)
{
    return ram_cfg.data.enter_stop;
}

bool mem_cfg_disable_wake_lid(void)
{
    return ram_cfg.data.disable_wake_lid;
}

bool mem_cfg_disable_wake_chg(void)
{
    return ram_cfg.data.disable_wake_chg;
}

void mem_display(uint8_t cmd_source, uint8_t *start, uint8_t length)
{
    while (length)
    {
        uint8_t x = (length < 8) ? length:8;
        uint8_t n;

        PRINTF_U("%08x  ", (uint32_t)start);

        for (n=0; n<8; n++)
        {
            if (n >= x)
            {
               PRINT_U("   ");
            }
            else
            {
               PRINTF_U("%02x ", start[n]);
            }
        }

        PUTCHAR(' ');

        for (n=0; n<8; n++)
        {
            if (n >= x)
            {
                PRINT_U(" ");
            }
            else
            {
                if ((start[n] >= 0x20) && (start[n] < 0x7E))
                {
                    PUTCHAR((char)start[n]);
                }
                else
                {
                    PUTCHAR('.');
                }
            }
        }

        PRINT("");

        length -= x;
        start += x;
    }
}

void mem_stack_dump(uint8_t cmd_source)
{
    uint8_t n;
    uint32_t *msp = (uint32_t *)GET_MSP();

    for (n=0; (n<MAX_STACK_DUMP) && (msp < (uint32_t *)MEM_RAM_END); n++)
    {
        PRINTF_U("%08x ", *msp++);
        if (!((n+1) & 0x7))
        {
            PRINT("");
        }
    }
}

static CLI_RESULT mem_cmd_status(uint8_t cmd_source)
{
    uint32_t m;
    uint32_t msp = GET_MSP();

    /*
    * Starting at the end of the stack memory, look for the first non-FF byte,
    * which we will interpret as being the stack high water mark.
    */
    for (m=MEM_STACK_END; m<msp; m++)
    {
        if (*((uint8_t *)m) != 0xFF)
        {
            break;
        }
    }

    /*
    * Display the peak stack usage as a percentage.
    */
    PRINTF("%d%%",
        ((100 * (MEM_RAM_END - m + 1)) + MEM_STACK_SIZE / 2) / MEM_STACK_SIZE);

    return CLI_OK;
}

static CLI_RESULT mem_cmd_rd(uint8_t cmd_source)
{
    long int start, len;

    if (cli_get_next_parameter(&start, 16))
    {
        mem_rd_start = (uint32_t)start;
        if (cli_get_next_parameter(&len, 16))
        {
            mem_rd_len = (uint8_t)len;
        }
        else
        {
            mem_rd_len = 1;
        }
    }
    else
    {
        mem_rd_start += mem_rd_len;
    }

    if (mem_rd_len > 1)
    {
        mem_display(
            cmd_source, (uint8_t *)mem_rd_start, mem_rd_len);
    }
    else
    {
        PRINTF("%02x", *((uint8_t *)mem_rd_start));
    }

    return CLI_OK;
}

static CLI_RESULT mem_cmd_wr(uint8_t cmd_source)
{
    long int start;
    uint8_t count = 0;

    if (cli_get_next_parameter(&start, 16))
    {
        long int data;

        while (cli_get_next_parameter(&data, 16))
        {
            *((uint8_t *)start) = (uint8_t)data;
            start++;
            count++;
        }
    }

    PRINTF("%d byte%s written", count, (count==1) ? "":"s");

    return CLI_OK;
}

CLI_RESULT mem_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(mem_command, cmd_source);
}
