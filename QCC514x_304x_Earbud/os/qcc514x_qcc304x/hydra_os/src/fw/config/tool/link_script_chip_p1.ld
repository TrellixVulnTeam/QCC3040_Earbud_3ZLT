/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */

/* Link script for QCC514x/5x and QCC304x/5x Processor 1 */

ENTRY($reset)

$DATA_RAM_SIZE_BYTES    = 40 * 1024;
$DATA_ROM_WINDOW        = 0x78000000;
$DATA_RAM_START_ADDRESS = 0x00010000;

INCLUDE link_script_chip_common.ld  /* common to P0 and P1 */
INCLUDE link_script_common.ld           /* common across chips */
