#ifndef GAIA_TRANSPORT_H
#define GAIA_TRANSPORT_H

#include <gaia.h>

/* TODO: Move into gaia_transport_private.h */
#define HIGH(x) (x >> 8)
#define LOW(x) (x & 0xFF)
#define W16(x) (((*(x)) << 8) | (*((x) + 1)))


void GaiaTransport_TestInit(void);
void GaiaTransport_RfcommInit(void);
void GaiaTransport_GattInit(void);
void GaiaTransport_Iap2Init(void);

#endif // GAIA_TRANSPORT_H
