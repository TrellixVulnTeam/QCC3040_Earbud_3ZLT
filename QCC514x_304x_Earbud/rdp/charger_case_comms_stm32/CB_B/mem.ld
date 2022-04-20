/*
 * Memory Spaces Definitions.
 *
 * Need modifying for a specific board.
 *   FLASH.ORIGIN: starting address of flash
 *   FLASH.LENGTH: length of flash
 *   RAM.ORIGIN: starting address of RAM bank 0
 *   RAM.LENGTH: length of RAM bank 0
 *
 * The values below can be addressed in further linker scripts
 * using functions like 'ORIGIN(RAM)' or 'LENGTH(RAM)'.
 */

MEMORY
{
  VECTOR_TABLE (xrw) : ORIGIN = 0x20000000, LENGTH = 192
  RAM (xrw) : ORIGIN = 0x200000C0, LENGTH = 16176
  CFGRAM (rw) : ORIGIN = 0x20003FF0, LENGTH = 16
  CCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 0
  FLASH_CFG (rx) : ORIGIN = 0x08000800, LENGTH = 2K
  FLASH (rx) : ORIGIN = 0x08010800, LENGTH = 62K
  FLASHB1 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB0 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB1 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB2 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  EXTMEMB3 (rx) : ORIGIN = 0x00000000, LENGTH = 0
  MEMORY_ARRAY (xrw)  : ORIGIN = 0x00000000, LENGTH = 0
}

/*
 * For external ram use something like:

  RAM (xrw) : ORIGIN = 0x68000000, LENGTH = 16K

 */
