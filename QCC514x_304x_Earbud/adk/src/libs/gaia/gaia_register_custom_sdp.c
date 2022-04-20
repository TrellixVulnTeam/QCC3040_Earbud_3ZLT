#include <connection.h>
#include <ps.h>
#include "gaia_private.h"
#include "gaia_register_custom_sdp.h"

/*************************************************************************
NAME
    register_custom_sdp

DESCRIPTION
    Register a custom SDP record retrieved from PS

    The first word in the PS data is the length after unpacking.  This
    allows us to retrieve an odd number of bytes and allows some sanity
    checking.
*/
void register_custom_sdp(uint16 pskey)
{
    UNUSED(pskey);
    GAIA_DEBUG(("INFO: register_custom_sdp not implemented"));
}
