/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Config
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "flash.h"
#include "crc.h"
#include "config.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

/*
* CONFIG_CODE: Arbitrary number to indicate that we have stored configuration
* data. Should be incremented whenever the config structure changes to the
* point that it is incompatible with the previous version.
*/
#define CONFIG_CODE 0xD06FACE2

/*-----------------------------------------------------------------------------
------------------ TYPE DEFINITIONS -------------------------------------------
-----------------------------------------------------------------------------*/

typedef struct __attribute__((__packed__))
{
    /*
    * The serial number must always be first in this structure and never have
    * its type changed. This is so that we can stop it getting reset when
    * CONFIG_CODE changes.
    */
    uint64_t serial;

    uint32_t status_time_closed;
    uint32_t status_time_open;
    bool shipping_mode;
    uint16_t battery_cutoff_mv;
    uint32_t board_id;
}
CONFIG_DATA;

typedef struct __attribute__((__packed__))
{
    uint32_t code;
    uint16_t data_size;
    uint8_t checksum;
    CONFIG_DATA data;
}
CONFIG;

typedef struct
{
    char *name;
    CLI_RESULT (*fn_set)(uint8_t, char *);
    void (*fn_get)(uint8_t);
}
CONFIG_COMMAND;

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT config_cmd_display(uint8_t cmd_source, bool verbose);
static CLI_RESULT config_cmd_battery_cutoff(uint8_t cmd_source, char *tok);
static CLI_RESULT config_cmd_serial(uint8_t cmd_source, char *tok);
static CLI_RESULT config_cmd_status_time_closed(uint8_t cmd_source, char *tok);
static CLI_RESULT config_cmd_status_time_open(uint8_t cmd_source, char *tok);
static CLI_RESULT config_cmd_board_id(uint8_t cmd_source, char *tok);
static void config_cmd_get_battery_cutoff(uint8_t cmd_source);
static void config_cmd_get_serial(uint8_t cmd_source);
static void config_cmd_get_status_time_closed(uint8_t cmd_source);
static void config_cmd_get_status_time_open(uint8_t cmd_source);
static void config_cmd_get_board_id(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static CONFIG config;
uint32_t config_page_start;

static const CONFIG_COMMAND config_command[] =
{
    { "serial", config_cmd_serial,             config_cmd_get_serial },
    { "stc",    config_cmd_status_time_closed, config_cmd_get_status_time_closed },
    { "sto",    config_cmd_status_time_open,   config_cmd_get_status_time_open },
    { "bco",    config_cmd_battery_cutoff,     config_cmd_get_battery_cutoff },
    { "id",     config_cmd_board_id,           config_cmd_get_board_id },
    { NULL }
};

static const CONFIG_DATA config_default =
{
    0xFFFFFFFFFFFFFFFF, /* serial */
    60,                 /* status_time_closed */ 
    0,                  /* status_time_open */  
    false,              /* shipping_mode */ 
    3200,               /* battery_cutoff_mv */
    0                   /* board_id */
};

#ifdef TEST
uint8_t _flash_cfg[2048];
#else
extern void *_flash_cfg;
#endif

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

static bool config_write(void)
{
    bool ret = false;

    config.checksum = crc_calculate_crc8(
        (uint8_t *)&config.data, sizeof(CONFIG_DATA));

    flash_unlock();

    if (flash_erase_page(config_page_start))
    {
        uint32_t n;
        uint32_t new_config[(sizeof(config) + 3) >> 2];

        /*
        * Because we will write to flash in 4-byte chunks, get the config in
        * a format to facilitate this. If the size of the config structure is
        * not divisible by 4, any 'spare' bytes need to be set to 0xFF so
        * that any future settings have the expected default.
        */
        memset(new_config, 0xFF, sizeof(new_config));
        memcpy(new_config, &config, sizeof(config));

        ret = true;

        for (n=0; n<sizeof(config); n+=4)
        {
            if (!flash_write(config_page_start + n, new_config[n>>2]))
            {
                ret = false;
                break;
            }
        }
    }

    flash_lock();

    return ret;
}

/*
* Serial number.
*/
uint64_t config_get_serial(void)
{
    return config.data.serial;
}

/*
* Time in seconds between status polls when the lid is closed.
*/
uint32_t config_get_status_time_closed(void)
{
    return config.data.status_time_closed;
}

/*
* Time in seconds between status polls when the lid is open.
*/
uint32_t config_get_status_time_open(void)
{
    return config.data.status_time_open;
}

/*
* Get shipping mode.
*/
bool config_get_shipping_mode(void)
{
    return config.data.shipping_mode;
}

/*
* Set shipping mode.
*/
bool config_set_shipping_mode(bool mode)
{
    config.data.shipping_mode = mode;
    return config_write();
}

/*
* Get battery cutoff level.
*/
uint16_t config_get_battery_cutoff_mv(void)
{
    return config.data.battery_cutoff_mv;
}

/*
* Get board ID.
*/
uint32_t config_get_board_id(void)
{
    return config.data.board_id;
}

void config_init(void)
{
    CONFIG *c;
    config_page_start = (uint32_t)&_flash_cfg;

    memset(&config, 0xFF, sizeof(config));
    config.code = CONFIG_CODE;
    config.data_size = sizeof(CONFIG_DATA);

    /*
    * Start with the default values.
    */
    memcpy(&config.data, &config_default, sizeof(CONFIG_DATA));

    c = (CONFIG *)config_page_start;

    /*
    * Overwrite some or all of the default values if there is a valid config
    * in flash.
    */
    if (c->code == CONFIG_CODE)
    {
        if (c->checksum == crc_calculate_crc8(
            (uint8_t *)&c->data, c->data_size))
        {
            if (c->data_size > sizeof(CONFIG_DATA))
            {
                /*
                * Config is valid, but there is more data stored than this
                * version of the firmware knows what to do with (presumably
                * saved by a different version of firmware with more settings).
                * Copy in as much data as we can handle.
                */
                memcpy(&config.data, &c->data, sizeof(CONFIG_DATA));
            }
            else
            {
                /*
                * Config is valid, and there is not too much data stored.
                * Copy in as much as is available.
                */
                memcpy(&config.data, &c->data, c->data_size);
            }
        }
    }
    else
    {
        /*
        * Retain the serial number even though the config code has changed and
        * consequently the rest of the saved config is being disregarded.
        */
        config.data.serial = c->data.serial;
    }
}

static CLI_RESULT config_cmd_display(uint8_t cmd_source, bool verbose)
{
    uint8_t n;

    for (n=0; config_command[n].name; n++)
    {
        if (verbose)
        {
            PRINTF_U("%-6s : ", config_command[n].name);
        }

        config_command[n].fn_get(cmd_source);

        if (verbose || !config_command[n+1].name)
        {
            PRINT("");
        }
        else
        {
            PRINT_U(",");
        }
    }

    return CLI_OK;
}

static CLI_RESULT config_cmd_set_uint32(char *tok, uint8_t *cptr, uint32_t min, uint32_t max)
{
    CLI_RESULT ret = CLI_ERROR;
    long long int x;

    x = strtoull(tok, NULL, 10);

    if ((x >= min) && (x <= max))
    {
        uint32_t x32 = (uint32_t)x;
        memcpy(cptr, &x32, sizeof(x32));

        if (config_write())
        {
            ret = CLI_OK;
        }
    }

    return ret;
}

/*
* Set the serial number.
*/
static CLI_RESULT config_cmd_serial(uint8_t cmd_source __attribute__((unused)), char *tok)
{
    CLI_RESULT ret = CLI_ERROR;

    config.data.serial = strtoull(tok, NULL, 16);
    if (config_write())
    {
        ret = CLI_OK;
    }

    return ret;
}

/*
* Set the time in seconds between status polls when the lid is closed.
*/
static CLI_RESULT config_cmd_status_time_closed(uint8_t cmd_source __attribute__((unused)), char *tok)
{
    return config_cmd_set_uint32(
        tok, (uint8_t *)&config.data.status_time_closed, 1, 0xFFFFFFFF);
}

/*
* Set the time in seconds between status polls when the lid is open.
*/
static CLI_RESULT config_cmd_status_time_open(uint8_t cmd_source __attribute__((unused)), char *tok)
{
    return config_cmd_set_uint32(
        tok, (uint8_t *)&config.data.status_time_open, 1, 0xFFFFFFFF);
}

/*
* Set the battery cutoff voltage.
*/
static CLI_RESULT config_cmd_battery_cutoff(
    uint8_t cmd_source __attribute__((unused)),
    char *tok)
{
    CLI_RESULT ret = CLI_ERROR;

    if (tok)
    {
        config.data.battery_cutoff_mv = (uint16_t)strtol(tok, NULL, 10);
        if (config_write())
        {
            ret = CLI_OK;
        }
    }

    return ret;
}

/*
* Set the board ID.
*/
static CLI_RESULT config_cmd_board_id(uint8_t cmd_source __attribute__((unused)), char *tok)
{
    return config_cmd_set_uint32(
        tok, (uint8_t *)&config.data.board_id, 0, 0xFFFFFFFF);
}

/*
* Display the serial number.
*/
static void config_cmd_get_serial(uint8_t cmd_source)
{
    PRINTF_U("%08X%08X",
        (uint32_t)(config.data.serial >> 32),
        (uint32_t)(config.data.serial & 0xFFFFFFFF));
}

/*
* Display the time in seconds between status polls when the lid is closed.
*/
static void config_cmd_get_status_time_closed(uint8_t cmd_source)
{
    PRINTF_U("%d", config.data.status_time_closed);
}

/*
* Display the time in seconds between status polls when the lid is open.
*/
static void config_cmd_get_status_time_open(uint8_t cmd_source)
{
    PRINTF_U("%d", config.data.status_time_open);
}

/*
* Display the battery cutoff level.
*/
static void config_cmd_get_battery_cutoff(uint8_t cmd_source)
{
    PRINTF_U("%d", config.data.battery_cutoff_mv);
}

/*
* Display the board ID.
*/
static void config_cmd_get_board_id(uint8_t cmd_source)
{
    PRINTF_U("%d", config.data.board_id);
}

CLI_RESULT config_cmd(uint8_t cmd_source)
{
    bool ret = CLI_ERROR;
    char *tok = cli_get_next_token();

    if (tok)
    {
        uint8_t n;

        for (n=0; config_command[n].name; n++)
        {
            if (!strcasecmp(tok, config_command[n].name))
            {
                char *tok = cli_get_next_token();

                if (tok)
                {
                    ret = config_command[n].fn_set(cmd_source, tok);
                }
                else
                {
                    config_command[n].fn_get(cmd_source);
                    PRINT("");
                    ret = CLI_OK;
                }
            }
        }
    }
    else
    {
        ret = config_cmd_display(cmd_source, true);
    }

    return ret;
}

CLI_RESULT ats_config(uint8_t cmd_source)
{
    return config_cmd(cmd_source);
}

CLI_RESULT atq_config(uint8_t cmd_source)
{
    return config_cmd_display(cmd_source, false);
}
