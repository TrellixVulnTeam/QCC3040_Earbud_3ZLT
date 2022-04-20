// *****************************************************************************
// Copyright (c) 2020 Qualcomm Technologies International, Ltd.
// %%version
//
// *****************************************************************************

// *****************************************************************************
// NOTICE TO ANYONE CHANGING THIS FILE:
// IF YOU UPDATE THE SVAD STRUCTURE WITH NEW FIELD(S) THEN
// REMEMBER TO CHANGE THE CORRESPONDING ASM HEADER FILE 'filter_bank_library.h'
// WITH THE NEW FIELD(S) AS WELL
// *****************************************************************************

#ifndef CVCLIB_C_H
#define CVCLIB_C_H

//  WARNING!! DO NOT EDIT WITHOUT EDITING cvclib.h
typedef struct
{  
   int *real_ptr;
   int *imag_ptr;
   int *exp_ptr;
} t_filter_bank_channel_object;

#endif