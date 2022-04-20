/****************************************************************************
Copyright (c) 2018 Qualcomm Technologies International, Ltd.

FILE NAME
    custom_operator_bundle_files.h

DESCRIPTION
    Custom operator bundle management implementation.
*/

#ifndef LIBS_CUSTOM_OPERATOR_BUNDLE_FILES_H_
#define LIBS_CUSTOM_OPERATOR_BUNDLE_FILES_H_

#include "operators.h"
#include <file.h>

/*
 * Used to load the bundle file with this capability.
 * Will load the file only if such a bundle file is found and it has not been loaded previously.
 * Returns the file_index of the bundle file with this capability or else it returns FILE_NONE.
 */
FILE_INDEX customOperatorLoadBundle(capability_id_t cap_id);

/*
 * After loading a bundle file and creating an operator that uses it,
 * call this function to add the operator to the list of users of that bundle file.
 * Keeps track of users to unload the bundle appropriately.
 */
void customOperatorAddOperatorToBundleFile(Operator op, FILE_INDEX bundle_file_index);

/*
 * If this operator is in the list of users of a bundle file it will remove it from that list.
 * If that bundle file no longer has any other users and is not protected, it is unloaded.
 */
void customOperatorUnloadBundle(Operator operator);

/*
 * The bundle file with the assoicated file index will be marked as protected from unloading.
 */
void customOperatorProtectBundle(FILE_INDEX bundle_file_index);

/*
 * The bundle file with the associated capability ID has its protected flag cleared.
 * If that bundle file has no users, it is unloaded.
 */
void customOperatorUnloadProtectedBundleFromCapId(capability_id_t cap_id);

/*
 * Get the dkcs bundle size for a given capability.
 */
uint32 customOperatorBundleSize(capability_id_t cap_id);

#endif /* LIBS_CUSTOM_OPERATOR_BUNDLE_FILES_H_ */
