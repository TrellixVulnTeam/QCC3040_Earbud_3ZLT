/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Programming by contract (PBC) support macros.
 *
 * If you #include this in individual source files you can
 * override -D NDEBUG just before the #include to have localised
 * control over PBC.
*/
#ifndef PBC_H
#define PBC_H

#include "assert.h"
#include "hydra_log/hydra_log.h" /* fw assert.h includes this, desktop doesn't */

#define assert_precondition(c)  assert(c)
#define assert_postcondition(c) assert(c)
#define assert_invariant(c)     assert(c)
#define assert_not_reached()    /*lint --e{774} */ assert(FALSE)

#endif /* !PBC_H */
