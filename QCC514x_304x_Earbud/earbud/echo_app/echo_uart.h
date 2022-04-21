#ifndef ECHO_UART_H
#define ECHO_UART_H

#include <message.h>
typedef struct
{
    TaskData task;
    Task client;
    Sink uart_sink;
    unsigned               	uart_src_need_drop:1;
    uint8*					pUartSrcStart;
    uint8*					pUartSrcEnd;
    uint16					send_packet_length;
}UARTStreamTaskData;
extern UARTStreamTaskData theUARTTask;
Sink Echo_Sink_UART_Init(Task task);
bool Echo_UART_Transmit(uint8 *data, uint16 size);
void Echo_UART_Handler(Task t, MessageId id, Message msg);
void Echo_UART_Init(Task client);

#endif // ECHO_UART_H
