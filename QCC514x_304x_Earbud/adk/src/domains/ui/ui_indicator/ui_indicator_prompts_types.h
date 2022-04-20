#ifndef UI_PROMPTS_TYPES_H
#define UI_PROMPTS_TYPES_H

#include <csrtypes.h>
#include <kymera.h>

/*! Audio prompt configuration */
typedef struct
{
    const char *filename;                       /*!< Prompt filename */
    uint32 rate;                                /*!< Prompt sample rate */
    promptFormat format;                        /*!< Prompt format */
    unsigned interruptible:1;                   /*!< Determines whether the audio prompt can be interrupted during playback */
    unsigned queueable:1;                       /*!< Determines whether the audio prompt can be queued prior to playback */
    unsigned requires_repeat_delay:1;           /*!< Determines whether the audio prompt can be repeated within the minumum delay timeframe */
    unsigned local_feedback:1;                  /*!< The prompt will only be rendered on the local device */

} ui_prompt_data_t;

#endif // UI_PROMPTS_TYPES_H
