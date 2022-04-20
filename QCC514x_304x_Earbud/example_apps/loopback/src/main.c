/*!
\copyright  Copyright (c) 2008 - 2021 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\version
\file       main.c
\brief      Main application task for loopback example application
*/

#include <os.h>
#include <panic.h>
#include <sink.h>
#include <source.h>
#include <stream.h>
#include <vmal.h>
#include <operator.h>
#include <operators.h>
#include <pio.h>
#include <psu.h>
#include <cap_id_prim.h>
#include <opmsg_prim.h>
#include <pmalloc.h>

#ifdef __QCC516X__
#include <ultra_quiet_dac.h>
#endif

/*! Sample rate to use for the DAC */
#define DAC_SAMPLE_RATE         48000

/* Create defines for volume manipulation.
 * The passthrough gain is the log2 of the required linear gain in Q6N format.
 * Convert a dB gain to Q6N as follows: 2^(32-6) * gain_db / 20log(2)
 * This can be simplified to a scaling of 2^26 / 20log2 = 67108864 / 6.0206
 */
/* Operator applies unity gain (0dB) */
#define GAIN_DB_TO_Q6N_SF       (11146541)
#define GAIN_DB(x)              ((int32)(GAIN_DB_TO_Q6N_SF * (x)))

/*! Initial gain set for the passthrough operator */
#define INITIAL_OPERATOR_GAIN    GAIN_DB(0)

/*! Audio core to load bundle in */
#define AUDIO_CORE 0
#define AMP_PIO 32
#define AMP_PIO_BANK        1
#define AMP_PIO_ENABLE      1<<0

#ifdef __QCC516X__
_Pragma ("unitsuppress Unused")
_Pragma("datasection apppool")
static const pmalloc_pool_config app_pools[] =
{
    {   84, 1 },
    {   100, 4 }
};
#endif

/*! Define to use downloadable passthrough capability
 *  @note To use the downloadable capability you will need to add the download_passthrough.dkcs found at
 *  ...\kalimba\kymera\output_bundles\<chiptype>\download_passthrough\download_passthrough.dkcs within
 *   the ro_fw project.
 */
//#define USE_DOWNLOADABLE

/*! Passthrough operator to load into the DSP */
static Operator passthrough;

#ifdef USE_DOWNLOADABLE
/*! Bundle ID for the downloadable capability file to load*/
static BundleID bID;

/*! file index of the downloadable capability */
static FILE_INDEX index=FILE_NONE;

/*! Name of the downloadable capability file to load */
static const char operator_file[] = "download_passthrough.dkcs";
#endif

/*! \brief Setup and connect audio inputs to outputs

    This function sets up the audio loopback by connecting
    the audio inputs to the audio outputs
*/
static void appSetupLoopback(void)
{
    /* Get the input endpoints */
    Source source_line_in_left  = PanicNull(StreamAudioSource(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    Source source_line_in_right = PanicNull(StreamAudioSource(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B));

    /* ...and configure the sample rate and gain for each input channel */
    PanicFalse(SourceConfigure(source_line_in_left,  STREAM_CODEC_INPUT_RATE, DAC_SAMPLE_RATE));
    PanicFalse(SourceConfigure(source_line_in_right, STREAM_CODEC_INPUT_RATE, DAC_SAMPLE_RATE));
    PanicFalse(SourceConfigure(source_line_in_left,  STREAM_CODEC_INPUT_GAIN, 9));
    PanicFalse(SourceConfigure(source_line_in_right, STREAM_CODEC_INPUT_GAIN, 9));

    /* Synchronize the inputs together */
    PanicFalse(SourceSynchronise(source_line_in_left,source_line_in_right));

    /* Get the output endpoints */
    Sink sink_line_out_left  = PanicNull(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_A));
    Sink sink_line_out_right = PanicNull(StreamAudioSink(AUDIO_HARDWARE_CODEC, AUDIO_INSTANCE_0, AUDIO_CHANNEL_B));

    /* ...and configure the sample rate and gain for each outupt channel */
    PanicFalse(SinkConfigure(sink_line_out_left,  STREAM_CODEC_OUTPUT_RATE, DAC_SAMPLE_RATE));
    PanicFalse(SinkConfigure(sink_line_out_right, STREAM_CODEC_OUTPUT_RATE, DAC_SAMPLE_RATE));
    PanicFalse(SinkConfigure(sink_line_out_left,  STREAM_CODEC_OUTPUT_GAIN, 15));
    PanicFalse(SinkConfigure(sink_line_out_right, STREAM_CODEC_OUTPUT_GAIN, 15));

    /* Get the passthrough operator to load */
#ifdef USE_DOWNLOADABLE
    /* Check whether the bundle file actually exists */
    index = FileFind(FILE_ROOT, operator_file, strlen(operator_file));
    if(index == FILE_NONE)Panic();
    bID = PanicZero(OperatorBundleLoad(index,AUDIO_CORE));
    passthrough = PanicZero(VmalOperatorCreate( CAP_ID_DOWNLOAD_PASSTHROUGH));
#else
    passthrough = PanicZero(VmalOperatorCreate( CAP_ID_BASIC_PASS ));
#endif
    uint16 set_gain[] = { OPMSG_COMMON_ID_SET_PARAMS, 1, 1, 1,
                          UINT32_MSW(INITIAL_OPERATOR_GAIN),
                          UINT32_LSW(INITIAL_OPERATOR_GAIN) };
    PanicZero(VmalOperatorMessage(passthrough, set_gain,
                                  sizeof(set_gain)/sizeof(set_gain[0]),
                                  NULL, 0));

    /* Connect the inputs to the passthrough operator */
    PanicNull(StreamConnect(source_line_in_left,  StreamSinkFromOperatorTerminal(passthrough, 0)));
    PanicNull(StreamConnect(source_line_in_right, StreamSinkFromOperatorTerminal(passthrough, 1)));


    /* Connect the passthrough operator to the outputs */
    PanicNull(StreamConnect(StreamSourceFromOperatorTerminal(passthrough, 0), sink_line_out_left));
    PanicNull(StreamConnect(StreamSourceFromOperatorTerminal(passthrough, 1), sink_line_out_right));

    /* Start the passthrough operator */
    Operator op_list[] = {
        passthrough,
    };
    PanicFalse(OperatorStartMultiple(sizeof(op_list)/sizeof(op_list[0]),op_list,NULL));
}

/*! \brief Application entry point

    This function is the entry point for the application.

    \returns Nothing. Only exits by powering down.
*/
int main(void)
{
#ifdef __QCC516X__
    UNUSED(app_pools);
#endif
    OsInit();

    /* Wait for VgregEn to go high ...*/
    while (!PsuGetVregEn());

    /* Enable the main audio processor */
    PanicFalse(VmalOperatorFrameworkEnableMainProcessor(TRUE));

    /* connect the inputs and outputs together in loopback */
    appSetupLoopback();

    /* Enable external audio amp via PIO */
    PioSetFunction(AMP_PIO, PIO);
    PioSetDir32Bank(AMP_PIO_BANK, AMP_PIO_ENABLE, AMP_PIO_ENABLE);
    PioSet32Bank(AMP_PIO_BANK, AMP_PIO_ENABLE, 0);

#ifdef __QCC516X__
    /*ultra-quiet mode initialization*/
    setupUltraQuietDac();
#endif
    /* Start the message scheduler loop */
    MessageLoop();

    /* We should never get here, keep compiler happy */
    return 0;
}
