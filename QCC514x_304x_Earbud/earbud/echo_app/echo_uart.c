#include <message.h>
#include <pio.h>
#include "sink.h"
#include "sink_.h"
#include "echo_uart.h"
#include "stream.h"
#include "panic.h"
#include "logging.h"
#include "source.h"
#include "echo_common.h"
#include "echo_private.h"

UARTStreamTaskData uartData;
//static Sink sUartSink = 0;
/*
static void Echo_Uart_Start()
{
    MessageCancelAll(ECHO_Get_Task_State(), ECHO_UART_START);
    MessageSendLater(ECHO_Get_Task_State(), ECHO_UART_START, NULL, 1000);
}
*/

// APP UART HANDLER
void Echo_UartHandler(Task task, MessageId id, Message msg)
{
    UNUSED(task);
    UNUSED(msg);
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

// APP UART Transmit
void Echo_UartSendToSink(const char *cmd)
{
    uint16 cmd_len = strlen(cmd);

    if(uartData.sink && (SinkSlack(uartData.sink)) >= cmd_len)
    {
        /* Claim space in the sink, getting the offset to it. */
        uint16 offset = SinkClaim(uartData.sink, cmd_len);

        /* Check we have a valid offset */
        if (offset != 0xFFFF)
        {
            /* Map the sink into memory space. */
            uint8 *snk = SinkMap(uartData.sink);

            if(snk == NULL)
            {
                //DEBUG_LOG("Invalid Sink pointer");
            }
            (void) PanicNull(snk);

            /* Copy the string into the claimed space. */
            memcpy(snk+offset, cmd, cmd_len);

            /* Flush the data out to the UART. */
            PanicZero(SinkFlush(uartData.sink, cmd_len));
        }
        else
        {
            DEBUG_LOG("Invalid sink offset");
        }
    }
    else
    {
        DEBUG_LOG("Invalid UART Sink or Insufficient space in Sink");
    }
}


bool Echo_UartInit(void) // UartInit
{
    /* Assign the appropriate PIOs to be used by UART. */
    bool status = FALSE;
    uint32 bank = 0;
    uint32 mask = (1<<BOARD_UART_RTS) | (1<<BOARD_UART_CTS) | (1<<BOARD_UART_TX) | (1<<BOARD_UART_RX);

    if (!uartData.initialised)
    {
        //DEBUG_LOG("appUartInitConnection start");
        status = PioSetMapPins32Bank(bank, mask, 0);
        PanicNotZero(status);

        PioSetFunction(BOARD_UART_RTS, UART_RTS);
        PioSetFunction(BOARD_UART_CTS, UART_CTS);
        PioSetFunction(BOARD_UART_TX, UART_TX);
        PioSetFunction(BOARD_UART_RX, UART_RX);

        StreamUartConfigure(VM_UART_RATE_115K2, VM_UART_STOP_ONE, VM_UART_PARITY_NONE);

        uartData.sink = StreamUartSink();
        PanicNull(uartData.sink);
        /* Configure the sink such that messages are not received */
        SinkConfigure(uartData.sink, VM_SINK_MESSAGES, VM_MESSAGES_NONE);

        uartData.source = StreamUartSource();
        PanicNull(uartData.source);
        /* Configure the source for getting all the messages */
        SourceConfigure(uartData.source, VM_SOURCE_MESSAGES, VM_MESSAGES_ALL);

        /* Associate the task with the stream source */
        uartData.task.handler = Echo_UartHandler;
        MessageStreamTaskFromSink(StreamSinkFromSource(uartData.source), (Task) &uartData.task);
        //DEBUG_LOG("appUartInitConnection end");

        uartData.initialised=TRUE;
    }
    else
    {
        //DEBUG_LOG("UART Already Initialised");
    }

    return uartData.initialised;
}

/*
void AppUartInit(Task client)
{
    theUARTTask.task.handler = Echo_UartHandler;
    theUARTTask.client = client;
    //theUARTTask.sink = Echo_UartInit(&theUARTTask.task);

    DEBUG_LOG("Invalid UART Sink or Insufficient space in Sink");
}
*/
