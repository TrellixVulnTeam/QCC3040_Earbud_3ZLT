#ifndef CONNECTION_ABSTRACTION_H
#define CONNECTION_ABSTRACTION_H
#include <connection.h>
#include <connection_no_ble.h>
#define isCLMessageId(id) ((id>=CL_MESSAGE_BASE) && (id <= CL_MESSAGE_TOP))
#endif // CONNECTION_ABSTRACTION_H
