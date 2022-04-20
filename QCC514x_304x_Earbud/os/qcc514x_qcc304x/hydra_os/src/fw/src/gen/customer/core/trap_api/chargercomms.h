#ifndef __CHARGERCOMMS_H__
#define __CHARGERCOMMS_H__
#include <app/charger_comms/charger_comms_if.h>

      
#if TRAPSET_CHARGERCOMMS

/**
 *  \brief Configure ChargerComms UART interface.
 * 	    Note: ChargerCommsUartConfigure doesn't support Case device_id at the
 *  moment.
 *         
 *  \param config_key Configuration key.
 *  \param value The value of the configuration key.
 * 
 * \ingroup trapset_chargercomms
 */
bool ChargerCommsUartConfigure(charger_comms_uart_config_key_t config_key, uint32 value);

/**
 *  \brief         Transmit a message to a charger using 2-wire Charger Comms.
 *         Every message must contain a one octet header, followed by up to 14
 *  octets extra payload.
 *         The header is primarily populated by the system, the structure is as
 *  follows:
 * \verbatim
 *         0: Reserved (set to 0)
 *         D: Destination (2 bits)
 *            msb    lsb
 *             |      |
 *             00DD0000
 *               ^
 *               |
 *               |
 *               |
 *             destination
 *        Destinations:
 *             0: Case
 *             1: Right Device
 *             2: Left Device
 *             3: Reserved
 * \endverbatim
 *         Every transmit request will be followed with a
 *  MESSAGE_CHARGERCOMMS_STATUS containing transmission status.
 *         
 *  \param length             The total length of the packet (including the header) to be
 *  transmitted in octets.
 *             The minimum length is therefore 1, to hold the header and the
 *  maximum is 15 octets
 *             (1 octet header, 14 octet payload).
 *             
 *  \param data             The data to be transmitted, including the header.
 *             
 *  \return           Boolean to indicate whether the request was accepted. If True a
 *  MESSAGE_CHARGERCOMMS_STATUS will follow
 *           containing the status of the transmission.
 *           
 * 
 * \ingroup trapset_chargercomms
 */
bool ChargerCommsTransmit(uint16 length, uint8 * data);

/**
 *  \brief         Reset the sequence numbers associated with a particular charger comms
 *  device.
 *         
 *  \param address                 Which device to reset the sequence numbers of.
 *                 A broadcast address will result in both the left and right
 *  earbuds sequence numbers being reset.
 *             
 *  \return           Boolean to indicate whether the request was accepted. Return true if
 *  the sequence numbers were reset, false otherwise.
 *           
 * 
 * \ingroup trapset_chargercomms
 */
bool ChargerCommsUartResetSeqNum(charger_comms_uart_address address);
#endif /* TRAPSET_CHARGERCOMMS */
#endif
