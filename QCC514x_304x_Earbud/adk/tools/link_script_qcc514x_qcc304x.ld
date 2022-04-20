/* Copyright (c) 2018 Qualcomm Technologies International, Ltd. */
/* Link script for P1 */
OUTPUT_ARCH(QCC514x_apps)

$DATA_RAM_SIZE_BYTES    = 40 * 1024;
$STACK_SIZE_BYTES       = 2 * 1024; /* If you change this, change 
                                     * block_the_stack_away() in excep_test.c 
                                     * accordingly (needs to work for p0 and p1)
                                     */

$SHARED_RAM_SIZE_BYTES  = 32 * 1024;
$DATA_RAM_START_ADDRESS = 0x00010000;
$DATA_RAM_END_ADDRESS   = $DATA_RAM_START_ADDRESS + $DATA_RAM_SIZE_BYTES;

INCLUDE link_script_common.ld