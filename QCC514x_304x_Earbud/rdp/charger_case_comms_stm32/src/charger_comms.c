#include <stdint.h>
#include "main.h"
#include "wire.h"
#include "charger_comms.h"
#include "cli.h"
#include "wire.h"
#include "timer.h"

#define VCHG_WAKE_IDX 0
#define VCHG_OFF_IDX 10
#define VCHG_HIGH_TIME 5
#define VCHG_LOW_TIME 4

#define VCHG_HIGH_IDX (VCHG_OFF_IDX + VCHG_HIGH_TIME)
#define VCHG_LOW_IDX (VCHG_HIGH_IDX + VCHG_LOW_TIME)

typedef enum
{
    CHARGER_COMMS_NOT_ACTIVE = 0,
    CHARGER_COMMS_START_SEQUENCE,
    CHARGER_COMMS_TRANSMITTING,
    CHARGER_COMMS_READING
} charger_comms_states;

typedef enum
{
    CHARGER_COMMS_VREG_LOW = 0,
    CHARGER_COMMS_VREG_HIGH,
    CHARGER_COMMS_VREG_WAKE
} charger_comms_vreg_states;


static const charger_comms_cfg *cfg;
static charger_comms_states charger_comm_state = CHARGER_COMMS_NOT_ACTIVE;
static charger_comms_vreg_states charger_comm_vreg_state = CHARGER_COMMS_VREG_HIGH;
static uint8_t start_idx;
static uint32_t write_idx;
static uint32_t bit_idx;
static uint32_t timer_idx;
static uint16_t num_tx_bits;
static uint8_t tx_buf[CHARGER_COMMS_MAX_MSG_LEN];
static bool expect_reply;

extern bool received_charger_comm_packet;

void charger_comms_init(const charger_comms_cfg *new_cfg)
{
    cfg = new_cfg;
}

static void vreg_high(void)
{
    charger_comm_vreg_state = CHARGER_COMMS_VREG_HIGH;
    charger_comms_vreg_high();
}

static void vreg_low(void)
{
    charger_comm_vreg_state = CHARGER_COMMS_VREG_LOW;
    charger_comms_vreg_low();
}

static void vreg_reset(void)
{
    charger_comm_vreg_state = CHARGER_COMMS_VREG_WAKE;
    charger_comms_vreg_reset();
}

/* TODO: Remove these and use PRINT style functions instead */
static void print_adc_uint(uint32_t print)
{
    PRINTF_B("%lu", print);
}

static void print_string(char* s)
{
    PRINT_B(s);
}

static void print_earbud_debug(earbud_channel *earbud)
{
    print_string("adc_val:");
    print_adc_uint((uint32_t)earbud->current_adc_val);
    print_string("all_idx:");
    print_adc_uint((uint32_t)earbud->all_idx);
    print_string("read_idx:");
    print_adc_uint((uint32_t)earbud->read_idx);
    print_string("reading:");
    print_adc_uint((uint32_t)earbud->start_reading);
    print_string("prev:");
    print_adc_uint((uint32_t)earbud->previous);
    print_string("adc_buf:");
    print_adc_uint((uint32_t)earbud->adc_buf);
    print_string("valid:");
    print_adc_uint((uint32_t)earbud->data_valid);
    print_string("wait:");
    print_adc_uint((uint32_t)earbud->wait);

    print_string("adc_thres:");
    print_adc_uint((uint32_t)earbud->adc_thres);
}

void print_charger_comms_debug(void)
{
    print_string("state:");
    print_adc_uint((uint32_t)charger_comm_state);
    print_string("start_idx:");
    print_adc_uint((uint32_t)start_idx);
    print_string("write_idx:");
    print_adc_uint((uint32_t)write_idx);
    print_string("bit_idx:");
    print_adc_uint((uint32_t)bit_idx);
    print_string("time:");
    print_adc_uint((uint32_t)timer_idx);
    print_string("\nleft\n");
    print_earbud_debug(&left_earbud);
    print_string("\nright\n");
    print_earbud_debug(&right_earbud);
}

static uint16_t process_bits(uint16_t *pmu_int_times, uint16_t pmu_int_idx, uint16_t pmu_start_idx,
                      uint16_t stop_start_time, uint16_t ignore_idx, uint8_t *buf)
{
    uint16_t i;
    size_t j;
    uint32_t diff;
    uint16_t conseq_bits;
    uint16_t current_bit_state = 0;
    uint16_t bit_pos = 0;
    uint16_t ignore_bits = 0;
    uint16_t round_value = OVERSAMPLES/2;
    uint16_t sample_interval = OVERSAMPLES;
    uint32_t zero_bits_detected = 0;

    current_bit_state = 0x80;

    for(i = pmu_start_idx; i < pmu_int_idx; i++)
    {

        /* Calculate how many bit periods have elapsed since last sample.
         * Rounded to nearest bit period so assumes that error < round_value */
        diff = (uint32_t) abs( (int32_t)pmu_int_times[i] - (int32_t)stop_start_time);
        conseq_bits = (uint16_t)((diff + round_value )/
                                sample_interval)
                                & 0xffff;

        /* If there has been 8 bits since the last start bit,
         * we must reset the start bit time and ignore the first
         * bit of the next sample.*/
        if (conseq_bits == 9)
        {
            stop_start_time = pmu_int_times[i];
            /* We will ignore the first bit of this sample. */
            ignore_bits = (i == ignore_idx);
            /* Ignore the first bit of the next sample*/
            ignore_idx = (uint16_t)(i + 1);
        }

        /* Get time delta between edges. */
        diff = (uint32_t) abs((int32_t)pmu_int_times[i] - (int32_t)pmu_int_times[i-1]);

        /* Round to nearest bit period. Converts to clock units. */
        conseq_bits = (uint16_t)((diff + round_value)/
                                sample_interval)
                                & 0xffff;

        current_bit_state = current_bit_state ? 0x0 : 0x80;

        if(conseq_bits == 0) 
        {
            zero_bits_detected++;
            continue;
        }

        /* Ignore start/stop bit by decrementing consecutive bits. */
        if (i == ignore_idx || ignore_bits)
        {
            conseq_bits--;
            ignore_bits = 0;
        }

        if(conseq_bits > 8) {
            print_string("Missing start bit\n");
            return bit_pos;
        }

        /* Write a number of consecutive bits to the receive buffer. */
        for(j = 0; j < conseq_bits; j++)
        {
            if(bit_pos == CHARGER_COMMS_MAX_MSG_LEN * 8)
            {
                print_string("Unexpected number of bits\n");
                return bit_pos;
            }

            buf[bit_pos / 8] =
                (uint8_t) ((buf[bit_pos / 8] >> 1) | current_bit_state);
            bit_pos++;
        }
    }

    current_bit_state = current_bit_state ? 0x0 : 0x80;
    for (; bit_pos < CHARGER_COMMS_MAX_MSG_LEN * 8; bit_pos++)
    {
        buf[bit_pos / 8] =
                (uint8_t) ((buf[bit_pos / 8] >> 1) | current_bit_state);
    }

    if (zero_bits_detected)
    {
        PRINTF_B("%u Zero bits", zero_bits_detected);
    }

    return bit_pos;
}

static void reset_earbud_channel(earbud_channel *earbud)
{
    earbud->all_idx = 0;
    earbud->end_idx = 0;
    earbud->read_idx = 0;
    earbud->data_valid = false;
    earbud->previous = 0;
    earbud->wait = 0;
    earbud->num_rx_octets = 0;
    earbud->start_reading = false;
    earbud->is_reading_header = false;

    earbud->adc_thres = 400;
    earbud->adc_calibrate = 0;
    earbud->adc_cal_cnt = 0;
}

void charger_comms_transmit(
    uint8_t earbud,
    uint8_t *buf,
    uint16_t num_tx_octets)
{
    if(num_tx_octets > CHARGER_COMMS_MAX_MSG_LEN)
    {
        return;
    }

    //num_tx_octets = total length including header.
    num_tx_bits = (uint16_t) (num_tx_octets * 8U);
    memcpy(tx_buf, buf, sizeof(buf[0]) * num_tx_octets);

    expect_reply = (earbud==WIRE_DEST_BROADCAST) ? false:true;
    write_idx = 0;
    bit_idx = 0;
    timer_idx = 0;
    start_idx = 0;
    charger_comm_state = CHARGER_COMMS_START_SEQUENCE;
    reset_earbud_channel(&left_earbud);
    reset_earbud_channel(&right_earbud);

    /* Call the hook so device specific actions can occur to setup and start
     * transmit/receive */
    cfg->on_tx_start(buf, (uint8_t)num_tx_octets);

#ifdef CHARGER_COMMS_FAKE
    charger_comm_state = CHARGER_COMMS_NOT_ACTIVE;
#endif
}

void charger_comms_fetch_rx_data(earbud_channel *earbud, uint8_t *buf)
{
    memset(buf, 0, sizeof(buf[0]) * CHARGER_COMMS_MAX_MSG_LEN);
    (void)process_bits(earbud->adc_buf, earbud->read_idx, 2, earbud->adc_buf[1], 2, buf);
}

static uint8_t read_earbud_channel_header(earbud_channel *earbud)
{
    uint8_t rx_buf[CHARGER_COMMS_MAX_MSG_LEN];

    /* Demodulate the first octet of the incoming packet (the header)
     * and extract its length. */
    charger_comms_fetch_rx_data(earbud, rx_buf);
    earbud->num_rx_octets = wire_get_payload_length(rx_buf) + 1;

    /* Update the time we expect this packet to end based on the length
     * denoted in header */
    earbud->end_idx = (uint16_t) ((earbud->num_rx_octets*9 + 15) * OVERSAMPLES);

    /* We have completed the read. */
    earbud->is_reading_header = false;

    return rx_buf[0];
}

bool charger_comms_should_read_header(void)
{
    return left_earbud.is_reading_header || right_earbud.is_reading_header;
}

void charger_comms_read_header(void)
{
    if(left_earbud.is_reading_header)
    {
        read_earbud_channel_header(&left_earbud);
    }

    if(right_earbud.is_reading_header)
    {
        read_earbud_channel_header(&right_earbud);
    }
}

static uint8_t process_earbud(earbud_channel *earbud)
{
    /* If we're at the end of the message, return immediately */
     if (earbud->all_idx >= earbud->end_idx) 
     {
         return 0;
     }

    if(earbud->all_idx == 10 * OVERSAMPLES)
    {
        earbud->is_reading_header = true;
    }

    uint32_t adc_val = (uint32_t) *(earbud->current_adc_val);

    if(earbud->start_reading)
    {
        int16_t this = (int16_t)adc_val;
        int16_t previous = earbud->previous;
        uint16_t adc_thres = earbud->adc_thres;

        if((this > adc_thres && previous <= adc_thres) ||
           (this < adc_thres && previous >= adc_thres))
        {
            if (earbud->read_idx >= CHARGER_COMMS_ADC_BUFFER_SIZE)
            {
                return 0;
            }
            earbud->adc_buf[earbud->read_idx] = earbud->all_idx;
            earbud->read_idx++;
        }

        earbud->all_idx++;
        earbud->previous = this;
    }
    else
    {
        if(adc_val > earbud->adc_thres)
        {
            earbud->read_idx = 0;
            earbud->start_reading = true;
            earbud->previous = 0;
        }
        earbud->wait++;
    }
    return 1;
}

void charger_comms_tick(void)
{
    timer_idx++;

    /* Reading occurs OVERSAMPLES times more often than writing.
     * We read on every tick, but should only write every OVERSAMPLES tick.
     */
    if (charger_comm_state != CHARGER_COMMS_READING)
    {
        if (timer_idx % OVERSAMPLES !=0)
        {
            return;
        }
    }

    switch(charger_comm_state)
    {
        case CHARGER_COMMS_NOT_ACTIVE:
        {
           return; 
        }
        case CHARGER_COMMS_START_SEQUENCE:
        {
            switch(start_idx)
            {
                case VCHG_WAKE_IDX:
                    /* Drive VCHG low so no charger is present */
                    vreg_reset();
                    break;

                case VCHG_OFF_IDX:
                    /* Drive VCHG HIGH to start */
                    vreg_high();
                    break;

                case VCHG_HIGH_IDX:
                    /* Start index is a sync signal. Signal for 4ms before data start.*/
                    vreg_low();
                    break;

                case VCHG_LOW_IDX:
                    charger_comm_state = CHARGER_COMMS_TRANSMITTING;
                    break;

                default:
                    break;
            }

            start_idx++;
            break;
        }
        case CHARGER_COMMS_TRANSMITTING:
        {
            if (bit_idx >= 5)
            {
                if(*left_earbud.current_adc_val)
                {
                    left_earbud.adc_calibrate += *(left_earbud.current_adc_val);
                    left_earbud.adc_cal_cnt++;
                }

                if(*right_earbud.current_adc_val)
                {
                    right_earbud.adc_calibrate += *(right_earbud.current_adc_val);
                    right_earbud.adc_cal_cnt++;
                }
            }

            if (write_idx == num_tx_bits)
            {
                /* Calculate the threshold for a HIGH bit based on the current drawn while transmitting. */
                left_earbud.adc_thres = (uint16_t) (left_earbud.adc_calibrate/left_earbud.adc_cal_cnt + cfg->adc_threshold);
                right_earbud.adc_thres = (uint16_t) (right_earbud.adc_calibrate/right_earbud.adc_cal_cnt + cfg->adc_threshold);

                /* After sending the packet, just drive normal.*/
                vreg_high();
                write_idx++;

                /* Assume the packet is "maximum length" for now. We will adjust
                 * the length after we've read the packet length from the header.
                 */
                left_earbud.end_idx =  (CHARGER_COMMS_MAX_MSG_LEN*9 * OVERSAMPLES)
                                       + (OVERSAMPLES * 15U);
                right_earbud.end_idx = (CHARGER_COMMS_MAX_MSG_LEN*9 * OVERSAMPLES)
                                       + (OVERSAMPLES * 15U);

                if (expect_reply)
                {
                    charger_comm_state = CHARGER_COMMS_READING;
                }
                else
                {
                    charger_comm_state = CHARGER_COMMS_NOT_ACTIVE;
                    cfg->on_complete();
                }
                return;
            }

            /* Force a start transition every 9-bits i.e before every octet. */
            if (bit_idx % 9 == 0)
            {
                charger_comm_vreg_state == CHARGER_COMMS_VREG_HIGH ?
                                           vreg_low() : vreg_high();
            }
            else
            {
                /* Modulate a bit from the message.
                 * Each octet is transmitted with the LSB first and the MSB last.
                 * The encoding is NRZ-L so a high bit is a high voltage and a low
                 * bit is a low voltage. */
                (tx_buf[write_idx /8] >> ((write_idx)%8)) & 0x1 ? vreg_high() : vreg_low();
                write_idx++;
            }

            bit_idx++;
            break;
        }
        case CHARGER_COMMS_READING:
        {
            uint8_t right_active = process_earbud(&right_earbud);
            uint8_t left_active = process_earbud(&left_earbud);
            uint16_t timeout_in_ticks = cfg->packet_reply_timeout_ms * OVERSAMPLES;

            /* Check if we've got responses or have timed out. */
            if ((!left_active || left_earbud.wait > timeout_in_ticks) &&
                (!right_active || right_earbud.wait > timeout_in_ticks))
            {
                right_earbud.data_valid = right_earbud.read_idx > 0;
                left_earbud.data_valid = left_earbud.read_idx > 0;
                charger_comm_state = CHARGER_COMMS_NOT_ACTIVE;
                cfg->on_complete();
            }
            break;
        }
    }
}

bool charger_comms_is_active(void)
{
    return (received_charger_comm_packet ||
           (charger_comm_state != CHARGER_COMMS_NOT_ACTIVE) ||
           ticks < 5);
}
