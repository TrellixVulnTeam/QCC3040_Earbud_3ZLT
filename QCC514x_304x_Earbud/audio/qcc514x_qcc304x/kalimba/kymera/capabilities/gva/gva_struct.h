/**
 * \file  va_pryon_lite_struct.h
 * \ingroup capabilities
 *
 *
 */

#ifndef VA_GOOGLE_STRUCT_H
#define VA_GOOGLE_STRUCT_H

#include "capabilities.h"
#include "op_msg_utilities.h"
#include "wwe/wwe_struct.h"

enum
{
    memory_bank_dm = 0,
    memory_bank_ro,
    memory_bank_dm1,
    memory_bank_dm2,
    num_memory_banks
};

typedef enum {
  /**
   * An mmap of this type is Read-Only data
   */
  GSOUND_HOTWORD_MMAP_TEXT,
  /*
   * An mmap of this type is preinitialized Read-Write data
   */
  GSOUND_HOTWORD_MMAP_DATA,
  /*
   * An mmap of this type is uninitialized Read-Write data
   */
  GSOUND_HOTWORD_MMAP_BSS
} GSoundHotwordMmapType;

/****************************************************************************
Public Type Definitions
*/

typedef struct VA_GOOGLE_OP_DATA{
    WWE_CLASS_DATA wwe_class;

    void *memory_handle;
    void *memory_banks_array[num_memory_banks];
    
    void *f_handle;
    int   hotword_lib_version;
} VA_GOOGLE_OP_DATA;

#endif /* VA_GOOGLE_H */
