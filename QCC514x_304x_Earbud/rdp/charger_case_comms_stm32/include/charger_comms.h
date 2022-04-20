/**
 * @file    charger_comms.h
 * @brief   Functionality required to perform 2-wire charger comms.
 */

#ifndef __CHARGER_COMMS_H
#define __CHARGER_COMMS_H

#include "charger_comms_if.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/** The maximum length of a charger comms message (including header) in octets */
#define CHARGER_COMMS_MAX_MSG_LEN 16

/** The maximum length of a charger comms payload in octets. */
#define CHARGER_COMMS_MAX_PAYLOAD_LEN (CHARGER_COMMS_MAX_MSG_LEN - 1)

/** Number of ADC samples to take per bit period to detect edges. */
#define OVERSAMPLES 10

/** Size of buffer used for each charger comm ADC buffer */
#define CHARGER_COMMS_ADC_BUFFER_SIZE 512

typedef struct earbud_channel
{
    volatile uint16_t* current_adc_val;
    uint16_t all_idx;
    uint16_t end_idx;
    uint16_t read_idx;
    bool start_reading;
    bool data_valid;
    int16_t previous;
    uint16_t *adc_buf;
    uint16_t wait;

    bool is_reading_header;
    uint8_t num_rx_octets;

    /** ADC readings above this threshold are considered logical high */
    uint16_t adc_thres;

    /** Number of calibration readings made to decide */
    uint16_t adc_cal_cnt;

    /** Cumulative ADC readings for calibration. */
    uint32_t adc_calibrate;
} earbud_channel;

typedef struct charger_comms_cfg
{
    /* Callback function when a charger comm transaction has completed, either
     * with or without having received data. */
    void (*on_complete)(void);

    /* Callback function for tasks to perform before initiating a charger comms
     * transaction. */
    void (*on_tx_start)(uint8_t *buf, uint8_t num_tx_octets);
    
    /* The time to wait for a reply from the earbuds after sending a charger
     * comm packet. */
    uint16_t packet_reply_timeout_ms;

    /* Number of ADC units that are required to detect an edge from the current
     * senses. This should ideally be in between I_low and I_high. */
    uint16_t adc_threshold;

} charger_comms_cfg;

/**
 * @brief Initialise the charger comms module
 * @param cfg: Configuration to use for charger comms.
 * @retval None
 */
void charger_comms_init(const charger_comms_cfg *cfg);

/**
 * @brief Submit a request to transmit a charger comms packet
 * @param buf: The data transmit
 * @param num_tx_octets: The total length of the message. Maximum value MAX_MSG_LEN.
 * @retval None
 */
void charger_comms_transmit(uint8_t dest, uint8_t *buf, uint16_t num_tx_octets);

/**
 * @brief Demodulate a received message from an earbud into a buffer.
 * @param earbud: The earbud channel that produced the received message.
 * @param buf: The buffer to write the fetched data to. Must have a capacity
 *              of at least MAX_MSG_LEN octets.
 * @retval None
 */
void charger_comms_fetch_rx_data(earbud_channel *earbud, uint8_t *buf);

/**
 * @brief Main function that drives charger comms. This must be executed at a
 *         regular interval while either transmitting or receiving.
 * @retval None
 */
extern void charger_comms_tick(void);

/**
 * @brief Return whether a charger comms is either transmitting or receiving
 * @retval true while either transmitting or receiving a charger comm packet
 */
bool charger_comms_is_active(void);

/**
 * @brief Read the header from any incoming charger comm packets.
 * @retval None
 */
void charger_comms_read_header(void);

/**
 * @brief Return whether incoming charger comm packets need their headers read.
 * @retval non-zero when headers require reading.
 */
bool charger_comms_should_read_header(void);

void print_charger_comms_debug(void);

/**
 * @brief Receive a single octet of charger comm data
 * @param data: The next octet in this packet.
 * @retval None
 */
void charger_comms_receive(uint8_t data);

/**
 * @brief Actions to perform when a charger comms transmit completes
 * @retval None
 */
void charger_comms_transmit_done(void);

/* TODO: don't expose this all. */
extern void (*on_rx_complete)(void);
extern void (*on_tx_start)(uint8_t *buf, uint8_t num_tx_octets);
extern earbud_channel left_earbud;
extern earbud_channel right_earbud;

#endif /* __CHARGER_COMMS_H */
