/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    kymera
\brief      Handles music processing chain

*/

#include "kymera_music_processing.h"
#include "kymera.h"
#include "kymera_ucid.h"
#include "kymera_internal_msg_ids.h"
#include "kymera_data.h"
#include "kymera_setup.h"
#include <chain.h>
#include <panic.h>
#include <operators.h>
#include <ps.h>
#include <ps_key_map.h>

#define PS_KEY_USER_EQ_PRESET_INDEX    0
#define PS_KEY_USER_EQ_START_GAINS_INDEX    1

#if defined(INCLUDE_MUSIC_PROCESSING)
static void kymera_GetUserEqParamsFromPsStorage(void);
#endif /* INCLUDE_MUSIC_PROCESSING */

void Kymera_InitMusicProcessing(void)
{
#if defined(INCLUDE_MUSIC_PROCESSING)
    kymeraTaskData *theKymera = KymeraGetTaskData();
    memset(&theKymera->eq, 0, sizeof(theKymera->eq));

    kymera_GetUserEqParamsFromPsStorage();

#endif /* INCLUDE_MUSIC_PROCESSING */
}

bool Kymera_IsMusicProcessingPresent(void)
{
    return Kymera_GetChainConfigs()->chain_music_processing_config ? TRUE : FALSE;
}

void Kymera_CreateMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        theKymera->chain_music_processing_handle = PanicNull(ChainCreate(Kymera_GetChainConfigs()->chain_music_processing_config));
    }
}

void Kymera_ConfigureMusicProcessing(uint32 sample_rate)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymera_chain_handle_t chain = KymeraGetTaskData()->chain_music_processing_handle;
        Operator eq;
        Operator user_eq;

        PanicNull(chain);

        PanicFalse(Kymera_SetOperatorUcid(chain, OPR_ADD_HEADROOM, UCID_PASS_ADD_HEADROOM));
        PanicFalse(Kymera_SetOperatorUcid(chain, OPR_SPEAKER_EQ, UCID_SPEAKER_EQ));
        PanicFalse(Kymera_SetOperatorUcid(chain, OPR_REMOVE_HEADROOM, UCID_PASS_REMOVE_HEADROOM));


        eq = PanicZero(ChainGetOperatorByRole(chain, OPR_SPEAKER_EQ));
        OperatorsStandardSetSampleRate(eq, sample_rate);

        user_eq = ChainGetOperatorByRole(chain, OPR_USER_EQ);
        if(user_eq)
        {
            OperatorsStandardSetSampleRate(user_eq, sample_rate);
#if defined(INCLUDE_MUSIC_PROCESSING)
            Kymera_SelectEqBankNow(KymeraGetTaskData()->eq.selected_eq_bank);
#endif
        }

        if(KymeraGetTaskData()->chain_config_callbacks && KymeraGetTaskData()->chain_config_callbacks->ConfigureMusicProcessingChain)
        {
            kymera_music_processing_config_params_t params = {0};
            params.sample_rate = sample_rate;
            KymeraGetTaskData()->chain_config_callbacks->ConfigureMusicProcessingChain(chain, &params);
        }

        ChainConnect(chain);
    }
}

void Kymera_StartMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        PanicNull(theKymera->chain_music_processing_handle);

        ChainStart(theKymera->chain_music_processing_handle);
    }
}

void Kymera_StopMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

#if defined(INCLUDE_MUSIC_PROCESSING) && defined(INCLUDE_MUSIC_PROCESSING_PEER)
        MessageCancelAll(KymeraGetTask(), KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK);
        MessageCancelAll(KymeraGetTask(), KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS);
#endif /* INCLUDE_MUSIC_PROCESSING && INCLUDE_MUSIC_PROCESSING_PEER */

        PanicNull(theKymera->chain_music_processing_handle);

        ChainStop(theKymera->chain_music_processing_handle);
    }
}

void Kymera_DestroyMusicProcessingChain(void)
{
    if(Kymera_IsMusicProcessingPresent())
    {
        kymeraTaskData *theKymera = KymeraGetTaskData();

        PanicNull(theKymera->chain_music_processing_handle);

        ChainDestroy(theKymera->chain_music_processing_handle);
        theKymera->chain_music_processing_handle = NULL;
    }
}

#ifdef INCLUDE_MUSIC_PROCESSING
#define UINT32_SIZE (32)

uint8 Kymera_UserEqActive(void)
{
    kymera_chain_handle_t chain = KymeraGetTaskData()->chain_music_processing_handle;

    return ((ChainGetOperatorByRole(chain, OPR_USER_EQ)) ? (1) : (0));
}


uint8 Kymera_GetNumberOfEqBands(void)
{
    return KymeraGetTaskData()->eq.user.number_of_bands;
}

uint8 Kymera_GetNumberOfEqBanks(void)
{
    return KymeraGetTaskData()->eq.number_of_presets + 2;
}

uint8 Kymera_GetSelectedEqBank(void)
{
    return KymeraGetTaskData()->eq.selected_eq_bank;
}

bool Kymera_SelectEqBank(uint32 delay_ms, uint8 bank)
{
    KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK_T *message = PanicUnlessMalloc(sizeof(KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK_T));

    message->preset = bank;

    MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_USER_EQ_SELECT_EQ_BANK, message, delay_ms);

    return TRUE;
}

static void kymera_StoreBankToPsStore(void)
{
    uint16 i;
    uint16 num_of_words = KymeraGetTaskData()->eq.user.number_of_bands + 1;
    uint16 *data = PanicUnlessMalloc(num_of_words * sizeof(uint16));

    data[0] = KymeraGetTaskData()->eq.selected_eq_bank;

    for(i = 0; i < KymeraGetTaskData()->eq.user.number_of_bands; ++i)
    {
        data[i + 1] = KymeraGetTaskData()->eq.user.params[i].gain;
    }

    PsStore(PS_KEY_USER_EQ, data, num_of_words);

    free(data);

}

#define NUMBER_OF_CORE_BAND_PARAMS 4
#define NUMBER_OF_PARAMS_PER_BAND 4

typedef enum
{
    peq_filter_param_filter_type = 0,
    peq_filter_param_cut_off_frequency,
    peq_filter_param_gain,
    peq_filter_param_q
} eq_param_t;

static uint32 kymera_ConvertTo32bitQFormat(const uint8 N, const uint32 value)
{
    uint8 M = 0;
    if (N <= UINT32_SIZE)
    {
        M = (uint8)((uint8)UINT32_SIZE - N);
    }
    return value << M;
}

static uint16 kymera_GetParamId(const uint16 bandm, eq_param_t peq_filter_param)
{
    return (uint16)(NUMBER_OF_CORE_BAND_PARAMS + (uint16)(NUMBER_OF_PARAMS_PER_BAND * bandm) + peq_filter_param);
}

bool Kymera_ApplyGains(uint8 start_band, uint8 end_band)
{
    bool user_eq_bands_set = FALSE;
    kymera_chain_handle_t chain = KymeraGetTaskData()->chain_music_processing_handle;
    const Operator peq_op = chain ? ChainGetOperatorByRole(chain, OPR_USER_EQ) : 0;
    uint8 i;
    uint8 gain_index;

    if (peq_op)
    {
        set_params_data_t* set_params_data = OperatorsCreateSetParamsData((end_band - start_band) + 1);

        DEBUG_LOG_VERBOSE("Kymera_ApplyGains start_band %d, end_band %d", start_band, end_band);

        gain_index = 0;
        for (i = start_band; i <= end_band; i++)
        {
            set_params_data->standard_params[gain_index].id = kymera_GetParamId(i, peq_filter_param_gain);
            /* value is Gain * 60, required format is Q 12.N (int32) */
            int32 gain = KymeraGetTaskData()->eq.user.params[i].gain;
            set_params_data->standard_params[gain_index].value = kymera_ConvertTo32bitQFormat(17, (uint32)((int32)gain * 0.5333));
            ++gain_index;
        }

        OperatorsStandardSetParameters(peq_op, set_params_data);
        free(set_params_data);

        user_eq_bands_set = TRUE;
    }

    return user_eq_bands_set;
}

uint32 eq_gain_apply_delay = 10;

bool Kymera_SelectEqBankNow(uint8 bank)
{
    bool eq_set = FALSE;
    uint8 eq_bank = KymeraGetTaskData()->eq.selected_eq_bank;

    DEBUG_LOG_VERBOSE("Kymera_SelectEqBankNow %d", bank);

    if(Kymera_IsMusicProcessingPresent())
    {
        if (bank == EQ_BANK_USER)
        {
            eq_bank = bank;
            eq_set = TRUE;
        }
        else if (bank < EQ_BANK_USER)
        {
            eq_bank = bank + 1;
            eq_set = TRUE;
        }
    }

    if (eq_set)
    {
        kymera_chain_handle_t chain = KymeraGetTaskData()->chain_music_processing_handle;

        DEBUG_LOG_VERBOSE("Kymera_SelectEqBankNow selecting %d", bank);

        if(chain)
        {
            Kymera_SetOperatorUcid(chain, OPR_USER_EQ, eq_bank);
            KymeraGetTaskData()->eq.selected_eq_bank = bank;

            if(bank == EQ_BANK_USER)
            {
                MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_USER_EQ_APPLY_GAINS, NULL, eq_gain_apply_delay);
            }
        }
        kymera_StoreBankToPsStore();
    }

    return eq_set;
}

bool Kymera_SetUserEqBands(uint32 delay_ms, uint8 start_band, uint8 end_band, int16 * gains)
{
    KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS_T *message = PanicUnlessMalloc(sizeof(KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS_T));

    DEBUG_LOG_VERBOSE("Kymera_SetUserEqBands start band %d, end band %d, first gain %d",
                start_band, end_band, gains[0]);

    size_t size_of_gains = (end_band-start_band+1)*sizeof(int16);

    message->start_band = start_band;
    message->end_band = end_band;
    message->gain = PanicUnlessMalloc(size_of_gains);
    memcpy(message->gain, gains, size_of_gains);

    MessageSendLater(KymeraGetTask(), KYMERA_INTERNAL_USER_EQ_SET_USER_GAINS, message, delay_ms);

    return TRUE;
}

bool Kymera_SetUserEqBandsNow(uint8 start_band, uint8 end_band, int16 * gains)
{
    bool user_eq_bands_set = FALSE;
    uint8 i;
    uint8 gain_index;

    DEBUG_LOG_VERBOSE("Kymera_SetUserEqBandsNow start band %d, end band %d, first gain %d",
                start_band, end_band, gains[0]);

    gain_index = 0;
    for(i = start_band; i <= end_band; i++)
    {
        KymeraGetTaskData()->eq.user.params[i].gain = gains[gain_index];
        DEBUG_LOG_VERBOSE("Kymera_SetUserEqBandsNow gain %d set to %d", i, gains[gain_index]);
        ++gain_index;
    }

    user_eq_bands_set = Kymera_ApplyGains(start_band, end_band);

    kymera_StoreBankToPsStore();

    return user_eq_bands_set;
}

void Kymera_GetEqBandInformation(uint8 band, kymera_eq_paramter_set_t *param_set)
{
    if(band >= KymeraGetTaskData()->eq.user.number_of_bands)
    {
        Panic();
    }

    *param_set = KymeraGetTaskData()->eq.user.params[band];
}

static uint32 convertFromQFormatTo32bitNumber(const uint8 N, const uint32 value)
{
    uint8 M = 0;
    if (N <= UINT32_SIZE)
    {
        M = (uint8)((uint8)UINT32_SIZE - N);
    }
    return value >> M;
}

typedef struct
{
    uint32 type;
    uint32 freq;
    uint32 gain;
    uint32 q;
} dsp_eq_params_t;

static void convertParams(dsp_eq_params_t *dsp_params, kymera_eq_paramter_set_t *param_set)
{
    uint32 freq = convertFromQFormatTo32bitNumber(22, (uint32)(dsp_params->freq * 0.75));
    uint32 gain = convertFromQFormatTo32bitNumber(18, (uint32)((int32)dsp_params->gain * 0.9375));
    uint32 q = convertFromQFormatTo32bitNumber(20, dsp_params->q);

    DEBUG_LOG_ALWAYS("kymera_GetEqParams stage type %d", dsp_params->type);
    DEBUG_LOG_ALWAYS("kymera_GetEqParams freq %d %d", freq, freq/3);
    DEBUG_LOG_ALWAYS("kymera_GetEqParams gain %d %d", gain, gain/60);
    DEBUG_LOG_ALWAYS("kymera_GetEqParams q %d", q);

    if(param_set)
    {
        param_set->filter_type = dsp_params->type;
        param_set->cut_off_freq = freq/3;
        param_set->gain = gain;
        param_set->q = q;
    }
}

void Kymera_GetEqParams(uint8 band)
{
    uint8 i;
    uint8 first_index = band * 4 + 4;
    kymera_chain_handle_t chain = KymeraGetTaskData()->chain_music_processing_handle;
    const Operator peq_op = ChainGetOperatorByRole(chain, OPR_USER_EQ);

    get_params_data_t* get_params_data = OperatorsCreateGetParamsData(4);

    for(i = 0; i < 4; ++i)
    {
        get_params_data->standard_params[i].id = first_index + i;
    }

    OperatorsStandardGetParameters(peq_op, get_params_data);

    DEBUG_LOG_ALWAYS("kymera_GetEqParams band %d, num of param %d, staus enum:obpm_result_state_t:0x%x", band,
            get_params_data->number_of_params, get_params_data->result);

    for(i = 0; i < 4; ++i)
    {
        DEBUG_LOG_ALWAYS("kymera_GetEqParams i %d, id %d, val 0x%x", i,
                get_params_data->standard_params[i].id,
                get_params_data->standard_params[i].value);
    }

    dsp_eq_params_t dsp_params;
    dsp_params.type = get_params_data->standard_params[0].value;
    dsp_params.freq = get_params_data->standard_params[1].value;
    dsp_params.gain = get_params_data->standard_params[2].value;
    dsp_params.q = get_params_data->standard_params[3].value;

    convertParams(&dsp_params, NULL);
}

#define FIRST_PRESET_PSKEY 9348
#define NUM_OF_PRESETS 12
#define LAST_PRESET_PSKEY (FIRST_PRESET_PSKEY + NUM_OF_PRESETS * 2)
#define USER_EQ_PSKEY 9470
#define NUM_OF_BANDS_OFFSET 7

static uint32 kymera_GetUint32FromPskey(uint32 key, uint16 offset)
{
    uint16 audio_key_buffer[2] = {0};
    uint16 new_key_len = 0;
    uint16 result;
    uint32 value = 0;

    result = PsReadAudioKey(key, audio_key_buffer, 2, offset, &new_key_len);

    value = audio_key_buffer[0] << 16 | audio_key_buffer[1];
    DEBUG_LOG_ALWAYS("kymera_GetUint32FromPskey key %d, offset %d, new_key_len %d, result %d, value 0x%x", key, offset, new_key_len, result, value);

    return value;
}


static void kymera_GetEqParamsFromPsKey(uint32 key, uint8 band, kymera_eq_paramter_set_t *param_set)
{
    dsp_eq_params_t dsp_params;
    dsp_params.type = kymera_GetUint32FromPskey(key, 11 + band * 8);
    dsp_params.freq = kymera_GetUint32FromPskey(key, 13 + band * 8);
    dsp_params.gain = kymera_GetUint32FromPskey(key, 15 + band * 8);
    dsp_params.q = kymera_GetUint32FromPskey(key, 17 + band * 8);

    convertParams(&dsp_params, param_set);
}

uint8 Kymera_PopulatePresets(uint8 *presets)
{
    uint8 preset_index = 0;
    uint8 ucid = 1;
    uint32 key;

    for(key = FIRST_PRESET_PSKEY; key < LAST_PRESET_PSKEY; key += 2)
    {
        uint16 key_len = 0;
        PsReadAudioKey(key, NULL, 0, 0, &key_len);
        if(key_len)
        {
            if(presets)
            {
                presets[preset_index] = ucid;
            }
            ++preset_index;
        }
        ++ucid;
    }

    return preset_index;
}

static void kymera_GetUserEqParamsFromPsStorage(void)
{
    kymeraTaskData *theKymera = KymeraGetTaskData();
    uint8 i;

    theKymera->eq.number_of_presets = Kymera_PopulatePresets(NULL);

    theKymera->eq.user.number_of_bands = kymera_GetUint32FromPskey(USER_EQ_PSKEY, NUM_OF_BANDS_OFFSET);
    theKymera->eq.user.params = PanicUnlessMalloc(sizeof(kymera_eq_paramter_set_t) * theKymera->eq.user.number_of_bands);

    for(i = 0; i < theKymera->eq.user.number_of_bands; ++i)
    {
        kymera_GetEqParamsFromPsKey(USER_EQ_PSKEY, i, &theKymera->eq.user.params[i]);
    }

    uint16 ps_key_size = PsRetrieve(PS_KEY_USER_EQ, NULL, 0);

    if(ps_key_size)
    {
        uint16 *data = PanicUnlessMalloc(ps_key_size * sizeof(uint16));
        uint16 retrieved_data_size = 0;
        retrieved_data_size = PsRetrieve(PS_KEY_USER_EQ, data, ps_key_size);
        if(retrieved_data_size != ps_key_size)
        {
            Panic();
        }
        theKymera->eq.selected_eq_bank = data[PS_KEY_USER_EQ_PRESET_INDEX];

        uint8 gains_to_restore = MIN(theKymera->eq.user.number_of_bands, ps_key_size - 1);

        for(i = 0; i < gains_to_restore; ++i)
        {
            theKymera->eq.user.params[i].gain = data[i+1];
        }

        free(data);
    }
}


#endif /* INCLUDE_MUSIC_PROCESSING */
