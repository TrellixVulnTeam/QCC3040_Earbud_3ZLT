#ifndef ECHO_UART_H
#define ECHO_UART_H

#include <message.h>
typedef struct
{
    TaskData task;
    Task client;
    bool initialised;
    Source source;
    Sink sink;
}UARTStreamTaskData;

extern UARTStreamTaskData theUARTTask;

void Echo_UartSendToSink(const char *cmd);
bool Echo_UartInit(void);
void Echo_UartHandler(Task task, MessageId id, Message msg);
void AppUartInit(Task client);

bool UartSendData(uint8 *data, uint16 size);


// UART PIO PIN SETTING
#define BOARD_UART_TX  18
#define BOARD_UART_RX  19
#define BOARD_UART_RTS 0x0
#define BOARD_UART_CTS 0x0

#endif // ECHO_UART_H
