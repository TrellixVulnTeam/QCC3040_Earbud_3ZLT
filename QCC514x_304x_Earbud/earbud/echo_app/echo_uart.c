#include <message.h>
#include <pio.h>
#include <stream.h>
#include <sink.h>
#include <sink_.h>
#include <panic.h>

#include "echo_private.h"
#include "echo_common.h"
#include "echo_uart.h"

#include "echo_debug.h"

UARTStreamTaskData theUARTTask;
static Sink sUartSink = 0;
void Echo_UART_Handler(Task t, MessageId id, Message payload)
{
    UNUSED(t);
    UNUSED(payload);
    switch(id)
    {
    case MESSAGE_MORE_DATA:
    {
    }
        break;

    default:
        break;
    }
}
Sink Echo_Sink_UART_Init(Task task)
{
    sUartSink = StreamUartSink();
    if(!sUartSink)
        return 0;
    /* Configure sink not to send MESSAGE_MORE_SPACE */
    PanicFalse(SinkConfigure(sUartSink, VM_SINK_MESSAGES, VM_MESSAGES_NONE));
    StreamConfigure(VM_STREAM_UART_CONFIG, VM_STREAM_UART_LATENCY);
    StreamUartConfigure(VM_UART_RATE_115K2, VM_UART_STOP_ONE, VM_UART_PARITY_NONE);
    //MessageSinkTask(StreamUartSink(), task);
    MessageStreamTaskFromSink(StreamUartSink(),task);
    return sUartSink;
}

bool Echo_UART_Transmit(uint8 *data, uint16 size)
{
    if(!sUartSink)
    {
        return FALSE;
    }
    if (!data || size == 0)
    {
        return FALSE;
    }
    if(SinkClaim(sUartSink, size) != 0xFFFF)
    {
        memmove(SinkMap(sUartSink), data, size);
        (void) PanicZero(SinkFlush(sUartSink, size));
        return TRUE;
    }
    return FALSE;
}

void AppUartInit(Task client)
{
    theUARTTask.task.handler = Echo_UART_Handler;
    theUARTTask.client = client;
    theUARTTask.uart_sink = Echo_Sink_UART_Init(&theUARTTask.task);
    //printVmLogsInTestSystem("uartinit\r\n");
}
