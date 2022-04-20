/*!
\copyright  Copyright (c) 2020 Qualcomm Technologies International, Ltd.
            All Rights Reserved.
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\ingroup    media_player
\brief      Marshalling definitions.
 
*/

#ifndef MUSIC_PROCESSING_MARSHAL_DESC_H_
#define MUSIC_PROCESSING_MARSHAL_DESC_H_

#include <marshal_common.h>
#include <marshal.h>

/*@{*/

/*! Music processing information sent from Primary to Secondary */
typedef struct
{
    /*! Absolute time in microseconds when the EQ should be handled in secondary EB */
    marshal_rtime_t timestamp;
    /*! EQ change type */
    uint8 eq_change_type;
    /*! Payload length */
    uint8 payload_length;
    /*! payload */
    uint8 payload[1];
} music_processing_eq_info_t;

/* Create base list of marshal types the key sync will use. */
#define MARSHAL_TYPES_TABLE(ENTRY) \
    ENTRY(music_processing_eq_info_t)

/* X-Macro generate enumeration of all marshal types */
#define EXPAND_AS_ENUMERATION(type) MARSHAL_TYPE(type),
enum MARSHAL_TYPES
{
    /* common types must be placed at the start of the enum */
    DUMMY = NUMBER_OF_COMMON_MARSHAL_OBJECT_TYPES-1,
    /* now expand the marshal types specific to this component. */
    MARSHAL_TYPES_TABLE(EXPAND_AS_ENUMERATION)
    NUMBER_OF_MARSHAL_OBJECT_TYPES
};
#undef EXPAND_AS_ENUMERATION

/* Make the array of all message marshal descriptors available. */
extern const marshal_type_descriptor_t * const music_processessing_marshal_type_descriptors[];

/*@}*/

#endif /* MUSIC_PROCESSING_MARSHAL_DESC_H_ */
