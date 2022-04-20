/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      DFU
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f0xx_crc.h"
#include "stm32f0xx_rcc.h"
#include "main.h"
#include "flash.h"
#include "power.h"
#include "timer.h"
#include "case.h"
#include "cmsis.h"
#include "dfu.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* DFU_MAX_S_REC_DATA: Defines the maximum amount of S-record data we can
* cope with, including the length and the checksum.
*/
#define DFU_MAX_SREC_DATA 200

/*
* DFU_MAX_NAME_LEN: Maximum length of variant name as contained in the S0
* record.
*/
#define DFU_MAX_NAME_LEN 7

#define IMAGE_A_START 0x8001000
#define IMAGE_A_END   0x80107FF
#define IMAGE_B_START 0x8010800
#define IMAGE_B_END   0x801FFFF

#define PAGE_SIZE 0x800

#define DFU_OPT_ACK_NACK        0x01
#define DFU_OPT_DRY_RUN         0x02
#define DFU_OPT_NO_TIMEOUT      0x04
#define DFU_OPT_SPURIOUS_NACK   0x08
#define DFU_OPT_NO_NAME_CHECK   0x10

#define DFU_TIMEOUT_SECONDS 300
#define DFU_ACTIVITY_TIMEOUT_SECONDS 10
#define DFU_RESET_TIMEOUT_SECONDS 1
#define DFU_WAIT_TIMEOUT_SECONDS 10

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef enum
{
    DFU_IDLE,
    DFU_WAITING,
    DFU_READY,
    DFU_CHECKSUM,
    DFU_WRITE_COUNT,
    DFU_RESET
}
DFU_STATE;

typedef struct
{
    uint32_t csum_a;
    uint32_t csum_b;
    char name[DFU_MAX_NAME_LEN + 1];
}
DFU_S0;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static DFU_STATE dfu_state = DFU_IDLE;
static uint8_t srec_data[DFU_MAX_SREC_DATA];
static uint8_t srec_data_len;
static bool dfu_running_image_a;
static uint32_t dfu_image_start;
static uint32_t dfu_image_end;
static uint8_t dfu_opts;
static uint32_t dfu_start_time;
static uint32_t dfu_activity_time;
static uint32_t dfu_reset_time;
static uint8_t dfu_cmd_source;
static uint8_t spurious_nack_ctr;
static uint32_t dfu_image_count;
static uint32_t dfu_next_addr;
static uint32_t dfu_image_count_data;
static uint32_t dfu_image_count_address;
static DFU_S0 dfu_s0;

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static uint32_t hex_str_to_int(char *str, uint8_t len)
{
    char tmp[9] = {0};

    memcpy(tmp, str, len);
    return strtoul(tmp, NULL, 16);
}

void dfu_init(void)
{
    /*
    * Work out if we are in image A or image B at run-time. It might be more
    * efficient to do this with the preprocessor, but this way leaves the door
    * a bit more open to having a single position-independent image in future.
    */
    if ((uint32_t)dfu_init > IMAGE_B_START)
    {
        dfu_running_image_a = false;
        dfu_image_start = IMAGE_A_START;
        dfu_image_end = IMAGE_A_END;
        dfu_image_count = *((uint32_t *)(IMAGE_B_START + 0x1C));
    }
    else
    {
        dfu_running_image_a = true;
        dfu_image_start = IMAGE_B_START;
        dfu_image_end = IMAGE_B_END;
        dfu_image_count = *((uint32_t *)(IMAGE_A_START + 0x1C));
    }

    dfu_image_count_address = dfu_image_start + 0x1C;
}

char *dfu_srec(char *str)
{
    char *e = NULL;
    int len = strlen(str);

    if ((dfu_opts & DFU_OPT_SPURIOUS_NACK) && (!(spurious_nack_ctr++ & 0x07)))
    {
        e = "spurious nack";
    }
    else if (len > (DFU_MAX_SREC_DATA << 1))
    {
        e = "record too big";
    }
    else if (len < 6)
    {
        e = "record too small";
    }
    else
    {
        uint16_t n;
        uint8_t csum = 0;
        srec_data_len = 0;

        for (n=0; n<len; n+=2)
        {
            srec_data[srec_data_len] = (uint8_t)hex_str_to_int(&str[n], 2);
            csum += srec_data[srec_data_len];
            srec_data_len++;
        }

        if (csum != 0xFF)
        {
            e = "record checksum";
        }
        else if (srec_data[0] != ((len>>1) - 1))
        {
            e = "length inconsistent";
        }
    }

    return e;
}

void dfu_failed(char *err, char *rec)
{
    uint8_t cmd_source = dfu_cmd_source;

    cli_intercept_line(cmd_source, NULL);
    power_clear_run_reason(POWER_RUN_DFU);
    case_dfu_finished();
    flash_lock();
    dfu_state = DFU_IDLE;

    PRINTF("DFU: Error (%s)", err);

    /*
    * Display the S-record that caused the problem.
    */
    if (rec)
    {
        PRINT_U("DFU: ");
        PRINT(rec);
    }
}

void dfu_rx(uint8_t cmd_source, char *str)
{
    dfu_activity_time = ticks;

    if (str[0]=='S')
    {
        char *e = dfu_srec(&str[2]);

        if (e)
        {
            /*
            * Something is wrong with the received S-record. Depending on the
            * option, either NACK it or give up altogether.
            */
            if (dfu_opts & DFU_OPT_ACK_NACK)
            {
                PRINTF("DFU: NACK");
            }
            else
            {
                dfu_failed(e, str);
            }
        }
        else
        {
            /*
            * The received S-record was OK. Send an ACK if configured to do so.
            */
            if (dfu_opts & DFU_OPT_ACK_NACK)
            {
                PRINT("DFU: ACK");
            }

            /*
            * The second byte of the string tells us what sort of S-record this
            * is, we are interested in S0, S3 and S7 records.
            */
            switch (str[1])
            {
                case '0':
                    /*
                    * S0 record (header). We use it to convey the checksums,
                    * variant name and possibly other information in future.
                    */
                    {
                        /*
                        * Determine the payload length. We already know we have
                        * at least six bytes of data because of the earlier
                        * check in dfu_srec().
                        */
                        uint8_t s0_payload_len = srec_data_len - 4;

                        /*
                        * Reduce the payload length if it's too big to fit into
                        * dfu_s0. This is to allow older firmware to cope with
                        * extra information being added to the S0 record.
                        */
                        if (s0_payload_len > sizeof(dfu_s0))
                        {
                            s0_payload_len = sizeof(dfu_s0);
                        }

                        /*
                        * Copy as much data as will fit into dfu_s0.
                        */
                        memcpy(&dfu_s0, &srec_data[3], s0_payload_len);

                        /*
                        * Enforce a null termination of 'name', just in case.
                        */
                        dfu_s0.name[DFU_MAX_NAME_LEN] = 0;

                        /*
                        * Check the variant name in order to make sure that
                        * this firmware is for this variant.
                        */
                        if ((dfu_opts & DFU_OPT_NO_NAME_CHECK) ||
                            !strcmp(dfu_s0.name, VARIANT_NAME))
                        {
                            PRINTF("DFU: Start (%s)", dfu_s0.name);
                        }
                        else
                        {
                            dfu_failed("incompatible", str);
                        }
                    }
                    break;

                case '3':
                    /*
                    * S3 record. This contains the actual image data. If its
                    * address is within the range we are programming, then we
                    * write its data to flash.
                    */
                    {
                        uint32_t addr;

                        addr = (srec_data[1] << 24) + (srec_data[2] << 16) +
                            (srec_data[3] << 8) + srec_data[4];

                        if ((addr >= dfu_image_start) && (addr <= dfu_image_end))
                        {
                            if (!(dfu_opts & DFU_OPT_DRY_RUN))
                            {
                                uint8_t n;

                                for (n=5; n<srec_data_len-1; n+=4)
                                {
                                    uint32_t data = srec_data[n] +
                                        (srec_data[n+1] << 8) +
                                        (srec_data[n+2] << 16) +
                                        (srec_data[n+3] << 24);

                                    if (addr == dfu_image_count_address)
                                    {
                                        /*
                                        * Don't write to the image count
                                        * location, but make a note of what
                                        * came from the S-record for checksum
                                        * purposes.
                                        */
                                        dfu_image_count_data = data;
                                    }
                                    else
                                    {
                                        /*
                                        * Write the data to flash.
                                        */
                                        if (!flash_write(addr, data))
                                        {
                                            dfu_failed("flash failed", str);
                                            break;
                                        }
                                    }
                                    addr += 4;
                                    dfu_next_addr = addr;
                                }
                            }
                        }
                    }
                    break;

                case '7':
                    /*
                    * S7 record (termination). We just take this to mean that
                    * there are no more records, we don't care about its
                    * contents.
                    */
                    dfu_state = DFU_CHECKSUM;
                    break;

                default:
                    break;
            }
        }
    }
}

CLI_RESULT dfu_cmd(uint8_t cmd_source)
{
    if (dfu_state==DFU_IDLE)
    {
        uint32_t page_addr;
        long int opts;

        if (cli_get_next_parameter(&opts, 16))
        {
            dfu_opts = (uint8_t)opts;
        }
        else
        {
            dfu_opts = 0;
        }

        power_set_run_reason(POWER_RUN_DFU);
        cli_intercept_line(cmd_source, dfu_rx);
        flash_unlock();

        if (!(dfu_opts & DFU_OPT_DRY_RUN))
        {
            /*
            * Erase the other image.
            */
            for (page_addr=dfu_image_start; page_addr<dfu_image_end; page_addr+=PAGE_SIZE)
            {
                flash_erase_page(page_addr);
            }
        }

        PRINT("DFU: Wait");
        dfu_state = DFU_WAITING;
        dfu_start_time = ticks;
        dfu_cmd_source = cmd_source;
        dfu_image_count_data = 0;
    }
    else
    {
        PRINT("DFU: Busy");
    }

    return CLI_OK;
}

void dfu_periodic(void)
{
    uint8_t cmd_source = dfu_cmd_source;

    switch (dfu_state)
    {
        case DFU_IDLE:
            break;

        case DFU_WAITING:
            /*
            * Waiting for the case to finish any pre-existing charger comms
            * activity - but if we wait too long, go ahead anyway.
            */
            if (case_allow_dfu() ||
                ((ticks - dfu_start_time) >
                (DFU_WAIT_TIMEOUT_SECONDS * TIMER_FREQUENCY_HZ)))
            {
                PRINTF("DFU: Ready (%c)", (dfu_running_image_a) ? 'A':'B');
                dfu_state = DFU_READY;
                dfu_activity_time = ticks;
            }
            break;

        case DFU_CHECKSUM:
            {
                uint32_t csum_expected =
                    (dfu_running_image_a) ? dfu_s0.csum_b : dfu_s0.csum_a;
                uint32_t csum;
                uint32_t addr;

                RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
                CRC->CR |= CRC_CR_RESET;

                for (addr=dfu_image_start; addr<dfu_next_addr; addr+=4)
                {
                    if (addr == dfu_image_count_address)
                    {
                        /*
                        * Image count address. At this point the contents of
                        * flash will be 0xFFFFFFFF, but the build script will
                        * have calculated the checksum based on the value
                        * that we read earlier on, so we must use that.
                        */
                        CRC->DR = dfu_image_count_data;
                    }
                    else
                    {
                        CRC->DR = *((uint32_t *)(addr));
                    }
                }

                csum = CRC->DR;
                RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, DISABLE);

                PRINTF("DFU: Checksum = 0x%08X, expected 0x%08X",
                    csum, csum_expected);

                if (csum != csum_expected)
                {
                    dfu_failed("checksum", NULL);
                }
                else
                {
                    dfu_state = DFU_WRITE_COUNT;
                }
            }
            break;

        case DFU_WRITE_COUNT:
            if (flash_write(dfu_image_count_address,
                (dfu_image_count==0xFFFFFFFE) ? 0 : dfu_image_count + 1))
            {
                PRINT("DFU: Complete");

                dfu_reset_time = ticks;
                dfu_state = DFU_RESET;
            }
            else
            {
                dfu_failed("write count failed", NULL);
            }
            break;

        case DFU_RESET:
            /*
            * Small delay before resetting to allow any buffered CLI
            * responses to be sent.
            */
            if ((ticks - dfu_reset_time) >
                (DFU_RESET_TIMEOUT_SECONDS * TIMER_FREQUENCY_HZ))
            {
                NVIC_SYSTEM_RESET();
            }
            break;

        default:
            if (!(dfu_opts & DFU_OPT_NO_TIMEOUT))
            {
                if ((ticks - dfu_start_time) >
                    (DFU_TIMEOUT_SECONDS * TIMER_FREQUENCY_HZ))
                {
                    dfu_failed("timeout", NULL);
                }
                else if ((ticks - dfu_activity_time) >
                    (DFU_ACTIVITY_TIMEOUT_SECONDS * TIMER_FREQUENCY_HZ))
                {
                    dfu_failed("activity timeout", NULL);
                }
            }
            break;
    }
}

