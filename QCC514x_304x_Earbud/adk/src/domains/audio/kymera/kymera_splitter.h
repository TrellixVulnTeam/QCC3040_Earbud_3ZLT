/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Kymera module to manage creation of splitter chains with multiple streams

        Chain example:
                                                        /------------------\
                            /--------------------\      | STREAM (INDEX 0) |
                            | SPLITTER (INDEX 0) |----->| output (index 0) |
          Input (index 0) ->|                    |----->| output (index 1) |
          Input (index 1) ->|                    |----  \------------------/
                            |                    |-- |                              /------------------\
                            \--------------------/ | |  /--------------------\      | STREAM (INDEX 1) |
                                                   | |  | SPLITTER (INDEX 1) |----->| output (index 0) |
                                                   | -->|                    |----->| output (index 1) |
                                                   ---->|                    |----  \------------------/
                                                        |                    |-- |
                                                        \--------------------/ | |  /------------------\
                                                                               | |  | STREAM (INDEX 2) |
                                                                               | -->| output (index 0) |
                                                                               ---->| output (index 1) |
                                                                                    \------------------/
*/

#ifndef KYMERA_MIC_SPLITTER_H_
#define KYMERA_MIC_SPLITTER_H_

#include <operators.h>
#include "cap_id_prim.h"

typedef struct splitter_tag * splitter_handle_t;

/*!
    Values used to configure specific parameters/behaviour of the splitter
*/
typedef struct
{
    /*! If zero it will be ignored */
    uint16 transform_size_in_words;
    /*! Data format to be used, if config is not used it will default to PCM */
    operator_data_format_t data_format;
} splitter_config_t;

/*! \brief Create splitter chain
    \param num_of_streams The maximum amount of streams the chain needs to support
    \param num_of_inputs The number of inputs to the chain, which is also the size of all the streams
    \param splitter_config_t If NULL default values will be used for the splitter configuration
           The pointer MUST REMAIN VALID until the splitter is destroyed, since the contents are not stored internally
    \return A handle to be used for subsequent calls to this interface
*/
splitter_handle_t Kymera_SplitterCreate(uint8 num_of_streams, uint8 num_of_inputs, const splitter_config_t *config);

/*! \brief Destroy splitter chain
    \param handle Reference to the handle of the chain
*/
void Kymera_SplitterDestroy(splitter_handle_t *handle);

/*! \brief Return the splitters respective input sink
    \param handle Reference to the handle of the chain
    \param input_index The index for the input terminal
    \return The sink for that input terminal
*/
Sink Kymera_SplitterGetInput(splitter_handle_t *handle, uint8 input_index);

/*! \brief Connect to one of the chains output streams
    \param handle Reference to the handle of the chain
    \param stream_index The index of the stream to connect to (based on num_of_streams at creation)
    \param input An array of sinks that the stream output will be connected to
                 Must have length equal to that of num_of_streams used at creation
*/
void Kymera_SplitterConnectToOutputStream(splitter_handle_t *handle, uint8 stream_index, const Sink *input);

/*! \brief Disconnect from one of the chains output streams
    \param handle Reference to the handle of the chain
    \param stream_index The index of the stream to disconnect connect from (based on num_of_streams at creation)
*/
void Kymera_SplitterDisconnectFromOutputStream(splitter_handle_t *handle, uint8 stream_index);

/*! \brief Start one of the chains output streams
    \param handle Reference to the handle of the chain
    \param stream_index The index of the stream (has to be previously connected) to start
*/
void Kymera_SplitterStartOutputStream(splitter_handle_t *handle, uint8 stream_index);

/*! \brief Facilitate transition to low power mode for splitter chain
    \param handle Reference to the handle of the chain
*/
void Kymera_SplitterSleep(splitter_handle_t *handle);

/*! \brief Facilitate transition to exit low power mode for splitter chain
    \param handle Reference to the handle of the chain
*/
void Kymera_SplitterWake(splitter_handle_t *handle);

#ifdef HOSTED_TEST_ENVIRONMENT
/*! \brief When testing, sink connections get lost. An array is filled instead to be able to check the correct routing */
void kymera_SplitterCollectSinks(splitter_handle_t handle, uint8 stream_index, const Sink *input);
/*! \brief Provides access for the test environment: Reads out the content of the stored sink array */
Sink Kymera_SplitterGetSink(splitter_handle_t handle, uint8 stream_index, uint8 channel);
/*! \brief Provides number of inputs to the splitter */
uint8 Kymera_SplitterGetNumOfInputs(splitter_handle_t handle);
#endif   //HOSTED_TEST_ENVIRONMENT

#endif // KYMERA_MIC_SPLITTER_H_
