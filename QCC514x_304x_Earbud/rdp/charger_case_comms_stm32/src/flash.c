/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Flash
*/

/*-----------------------------------------------------------------------------
------------------ INCLUDES ---------------------------------------------------
-----------------------------------------------------------------------------*/

#include <stdio.h>
#include "stm32f0xx_flash.h"
#include "main.h"
#include "flash.h"

/*-----------------------------------------------------------------------------
------------------ PREPROCESSOR DEFINITIONS -----------------------------------
-----------------------------------------------------------------------------*/

#ifdef STM32F070x6
#define FLASH_NO_OF_SECTORS 8
#define FLASH_NO_OF_PAGES   32
#define FLASH_PAGE_SIZE     1024
#define FLASH_SECTOR_SIZE   4096
#endif

#ifdef STM32F070xB
#define FLASH_NO_OF_SECTORS 32
#define FLASH_NO_OF_PAGES   64
#define FLASH_PAGE_SIZE     2048
#define FLASH_SECTOR_SIZE   4096
#endif

/*-----------------------------------------------------------------------------
------------------ PROTOTYPES -------------------------------------------------
-----------------------------------------------------------------------------*/

static CLI_RESULT flash_cmd_status(uint8_t cmd_source);
static CLI_RESULT flash_cmd_wr(uint8_t cmd_source);
static CLI_RESULT flash_cmd_pe(uint8_t cmd_source);

/*-----------------------------------------------------------------------------
------------------ VARIABLES --------------------------------------------------
-----------------------------------------------------------------------------*/

static const CLI_COMMAND flash_command[] =
{
    { "",   flash_cmd_status, 2 },
    { "wr", flash_cmd_wr,     2 },
    { "pe", flash_cmd_pe,     2 },
    { NULL }
};

/*-----------------------------------------------------------------------------
------------------ FUNCTIONS --------------------------------------------------
-----------------------------------------------------------------------------*/

void flash_init(void)
{
}

static CLI_RESULT flash_cmd_status(uint8_t cmd_source)
{
    PRINTF("ACR  = 0x%08x", FLASH->ACR);
    PRINTF("SR   = 0x%08x", FLASH->SR);
    PRINTF("CR   = 0x%08x", FLASH->CR);
    PRINTF("AR   = 0x%08x", FLASH->AR);
    PRINTF("OBR  = 0x%08x", FLASH->OBR);
    PRINTF("WRPR = 0x%08x", FLASH->WRPR);

    return CLI_OK;
}

static void flash_wait_for_not_busy(void)
{
    while (FLASH->SR & FLASH_FLAG_BSY);
}

bool flash_write(uint32_t address, uint32_t data)
{
    /*
    * Wait until not busy.
    */
    flash_wait_for_not_busy();

    /*
    * Set programming bit
    */
    FLASH->CR |= FLASH_CR_PG;

    /*
    * Write the first half of the data.
    */
    *((__IO uint16_t *)address) = (uint16_t)data;

    /*
    * Wait until not busy.
    */
    flash_wait_for_not_busy();

    /*
    * Write the second half of the data.
    */
    *((__IO uint16_t *)(address + 2)) = data >> 16;

    /*
    * Wait until not busy.
    */
    flash_wait_for_not_busy();

    /*
    * Clear programming bit.
    */
    FLASH->CR &= ~FLASH_CR_PG;

    return (*((__IO uint32_t *)address) == data) ? true:false;
}

void flash_lock(void)
{
    FLASH_Lock();
}

void flash_unlock(void)
{
    FLASH_Unlock();
}

bool flash_erase_page(uint32_t page_address)
{
    /*
    * Wait until not busy.
    */
    flash_wait_for_not_busy();

    /*
    * Erase the page.
    */
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR = page_address;
    FLASH->CR |= FLASH_CR_STRT;

    /*
    * Wait until not busy.
    */
    flash_wait_for_not_busy();

    /*
    * Disable the PER bit.
    */
    FLASH->CR &= ~FLASH_CR_PER;

    return true;
}

static CLI_RESULT flash_cmd_pe(uint8_t cmd_source)
{
    CLI_RESULT ret = CLI_ERROR;
    long int page;

    if (cli_get_next_parameter(&page, 10))
    {
        FLASH_Unlock();
        if (flash_erase_page(FLASH_BASE + (page * FLASH_PAGE_SIZE)))
        {
            PRINT("Erased");
            ret = CLI_OK;
        }
        FLASH_Lock();
    }
    return ret;
}

static CLI_RESULT flash_cmd_wr(uint8_t cmd_source)
{
    long int start;
    uint8_t count = 0;

    if (cli_get_next_parameter(&start, 16))
    {
        long int data;

        FLASH_Unlock();
        while (cli_get_next_parameter(&data, 16))
        {
            if (!flash_write(start, data))
            {
                break;
            }

            start+=2;
            count++;
        }
        FLASH_Lock();
    }

    PRINTF("%d word%s written", count, (count==1) ? "":"s");

    return CLI_OK;
}

CLI_RESULT flash_cmd(uint8_t cmd_source)
{
    return cli_process_sub_cmd(flash_command, cmd_source);
}
