/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd.


This code allows the user to read the dkcs file header.
This code may be removed once a suitable trap is added to perform this functionality.
*/

#ifndef LIBS_CUSTOM_OPERATOR_DKCS_READER_H_
#define LIBS_CUSTOM_OPERATOR_DKCS_READER_H_

#include <csrtypes.h>
#include <operators.h>
#include <file.h>

typedef struct
{
    /* The header chip ID field */
    uint16 chip_id;
    /* The header build ID field */
    uint32 build_id;
    /* The number of dkcs in the file */
    uint16 num_dkcs;
    /* The number of capability IDs in all dkcs defined in the header */
    uint32 num_cap_ids;
    /* A list of capability IDs in all dkcs defined in the header */
    uint16 capability_ids[1];
} dkcs_header_t;

typedef struct {
    /* The overall length of all the KDCs, as recorded in the KCS header */
    uint32  kcs_length;
    /* The PM space needed for KDC[0] */
    uint32  kdc_pm_length;
    /* The DM1 space needed for KDC[0] */
    uint32  kdc_dm1_length;
    /* The DM2 space needed for KDC[0] */
    uint32  kdc_dm2_length;
} dkcs_info_t;

/* Read and return the dkcs file's header. Panics if there are any exceptions. */
dkcs_header_t *dkcsHeaderRead(FILE_INDEX file_index);

/* Free the dkcs header. */
void dkcsHeaderFree(dkcs_header_t *header);

/* Read the memory information from a given KCS file index */
/* The user supplied info pointer will be populated with the extracted information */
/* return TRUE if all is well, or FALSE if the file couldn't be accessed */
bool dkcsReadInfo(capability_id_t cap_id, FILE_INDEX file_index, dkcs_info_t *info);

#endif /* LIBS_CUSTOM_OPERATOR_DKCS_READER_H_ */
