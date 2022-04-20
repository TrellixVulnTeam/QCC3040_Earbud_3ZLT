/* Copyright (c) 2020 Qualcomm Technologies International, Ltd. */
/*   %%version */

/* Link script for QCC514x/5x and QCC304x/5x Processor P1 Oxygen */

$STACK_SIZE_BYTES = 2 * 1024; /* If you change this, change
                               * blow_the_stack_away() in excep_test.c
                               * accordingly (needs to work for p0 and p1)
                               */

INCLUDE link_script_chip_p1.ld  /* No revision specific defines required */
