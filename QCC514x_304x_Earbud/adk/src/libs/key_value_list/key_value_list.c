/*!
\copyright  Copyright (c) 2018-2020 Qualcomm Technologies International, Ltd.\n
            All Rights Reserved.\n
            Qualcomm Technologies International, Ltd. Confidential and Proprietary.
\file
\brief      Source file for a data structure with a list of { key, value } elements.

*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <panic.h>
#include <vmtypes.h>

#include "key_value_list.h"

/*! The linked list of elements that do not fit into 32 bits */
typedef struct large_kv_element_t
{
    struct large_kv_element_t *next;
    uint16 len;
    uint16 key;
    uint8 data[1];
} large_kv_element_t;

/*! Structure to store key/value data. Types <= 32 bits are stored in dynamic
    fixed-type arrays. Types > 32 bits are stored in a linked list. */
struct key_value_list_tag
{
    /*! Length of dynamic keys array */
    uint8 len_keys;
    /*! Length of dynamic values8 array */
    uint8 len8;
    /*! Length of dynamic values16 array */
    uint8 len16;
    /*! Length of dynamic values32 array */
    uint8 len32;

    /* Pointer to arrays of keys. */
    uint16 *keys;
    /* Pointer to array of uint8s */
    uint8 *values8;
    /* Pointer to array of uint16s */
    uint16 *values16;
    /* Pointers to arrays of uint32/ptrs */
    uint32 *values32;

    /* Linked list of larger elements */
    large_kv_element_t *head;
};

/*****************************************************************************/

static bool keyValueList_addKeyValuePair(key_value_list_t list, key_value_key_t key, const void * value, size_t size)
{
    void *dest = NULL;
    unsigned key_index = 0;
    unsigned new_len;
    large_kv_element_t *ele;

    switch (size)
    {
        case sizeof(uint8):
            key_index = list->len8;
            new_len = list->len8 + 1;
            list->values8 = PanicNull(realloc(list->values8, sizeof(uint8) * new_len));
            dest = list->values8 + list->len8;
            list->len8 = new_len;
            break;

        case sizeof(uint16):
            key_index = list->len8 + list->len16;
            new_len = list->len16 + 1;
            list->values16 = PanicNull(realloc(list->values16, sizeof(uint16) * new_len));
            dest = list->values16 + list->len16;
            list->len16 = new_len;
            break;

        case sizeof(uint32):
            key_index = list->len8 + list->len16 + list->len32;
            new_len = list->len32 + 1;
            list->values32 = PanicNull(realloc(list->values32, sizeof(uint32) * new_len));
            dest = list->values32 + list->len32;
            list->len32 = new_len;
            break;

        default:
            ele = PanicUnlessMalloc(sizeof(*ele) + size - 1);
            ele->next = list->head;
            ele->len = size;
            ele->key = key;
            list->head = ele;
            dest = ele->data;
            break;
    }

    memmove(dest, value, size);

    if (size <= sizeof(uint32))
    {
        uint16 *key_p;
        new_len = list->len_keys + 1;
        list->keys = PanicNull(realloc(list->keys, sizeof(uint16) * new_len));
        key_p = list->keys + key_index;
        memmove(key_p + 1, key_p, sizeof(uint16) * (list->len_keys - key_index));
        list->len_keys = new_len;
        *key_p = key;
    }

    return TRUE;
}

/*****************************************************************************/
key_value_list_t KeyValueList_Create(void)
{
    size_t size = sizeof(struct key_value_list_tag);
    key_value_list_t list = PanicUnlessMalloc(size);
    memset(list, 0, size);
    return list;
}

void KeyValueList_Destroy(key_value_list_t* list)
{
    PanicNull(list);
    KeyValueList_RemoveAll(*list);
    free(*list);
    *list = NULL;
}

bool KeyValueList_Add(key_value_list_t list, key_value_key_t key, const void *value, size_t size)
{
    bool success = FALSE;

    PanicNull(list);

    if (!KeyValueList_IsSet(list, key))
    {
        success = keyValueList_addKeyValuePair(list, key, value, size);
    }

    return success;
}

bool KeyValueList_Get(key_value_list_t list, key_value_key_t key, void **value_p, size_t *size_p)
{
    unsigned index;
    uint16 *keys;
    large_kv_element_t *ele;

    PanicNull(list);
    PanicNull(value_p);
    PanicNull(size_p);

    for (ele = list->head; ele != NULL; ele = ele->next)
    {
        if (ele->key == key)
        {
            *size_p = ele->len;
            *value_p = ele->data;
            return TRUE;
        }
    }

    keys = list->keys;

    for (index = 0; index < list->len_keys; index++)
    {
        if (*keys++ == key)
        {
            if (index < list->len8)
            {
                *size_p = sizeof(uint8);
                *value_p = (void*)(list->values8 + index);
                return TRUE;
            }

            index -= list->len8;
            if (index < list->len16)
            {
                *size_p = sizeof(uint16);
                *value_p = (void*)(list->values16 + index);
                return TRUE;
            }

            index -= list->len16;
            *size_p = sizeof(uint32);
            *value_p = (void*)(list->values32 + index);
            return TRUE;
        }
    }
    return FALSE;
}

void *KeyValueList_GetSized(key_value_list_t list, key_value_key_t key, size_t size)
{
    unsigned index;
    uint16 *keys;
    large_kv_element_t *ele;

    PanicNull(list);

    keys = list->keys;

    switch (size)
    {
        case sizeof(uint8):
            for (index = 0; index < list->len8; index++)
            {
                if (*keys++ == key)
                {
                    return list->values8 + index;
                }
            }
            break;

        case sizeof(uint16):
            keys += list->len8;
            for (index = 0; index < list->len16; index++)
            {
                if (*keys++ == key)
                {
                    return list->values16 + index;
                }
            }
            break;

        case sizeof(uint32):
            keys += list->len8;
            keys += list->len16;
            for (index = 0; index < list->len32; index++)
            {
                if (*keys++ == key)
                {
                    return list->values32 + index;
                }
            }
            break;

        default:
            /* Key not found in fixed type list, now search in dynamic list */
            for (ele = list->head; ele != NULL; ele = ele->next)
            {
                if (ele->key == key)
                {
                    size = ele->len;
                    return ele->data;
                }
            }
            break;
    }

    /* Key not found based on size, logical error if the key exists with an
       unexpected size */
    PanicFalse(!KeyValueList_IsSet(list, key));
    return NULL;
}

void KeyValueList_Remove(key_value_list_t list, key_value_key_t key)
{
    unsigned key_index;
    unsigned index;
    uint16 *keys;
    large_kv_element_t **elpp, *elp;

    PanicNull(list);

    for (elpp = &list->head; (elp = *elpp) != NULL; )
    {
        if (elp->key == key)
        {
            *elpp = elp->next;
            free(elp);
            return;
        }
        else
        {
            elpp = &elp->next;
        }
    }

    keys = list->keys;

    for (key_index = 0; key_index < list->len_keys; key_index++)
    {
        if (keys[key_index] == key)
        {
            index = key_index;
            if (index < list->len8)
            {
                uint8 *dest = list->values8 + index;
                memmove(dest, dest + 1, sizeof(uint8) * (list->len8 - index));
                list->len8 -= 1;
                if (list->len8)
                {
                    list->values8 = PanicNull(realloc(list->values8, sizeof(uint8) * list->len8));
                }
                else
                {
                    free(list->values8);
                    list->values8 = NULL;
                }
            }
            else
            {
                index -= list->len8;
                if (index < list->len16)
                {
                    uint16 *dest = list->values16 + index;
                    memmove(dest, dest + 1, sizeof(uint16) * (list->len16 - index));
                    list->len16 -= 1;
                    if (list->len16)
                    {
                        list->values16 = PanicNull(realloc(list->values16, sizeof(uint16) * list->len16));
                    }
                    else
                    {
                        free(list->values16);
                        list->values16 = NULL;
                    }
                }
                else
                {
                    index -= list->len16;
                    if (index < list->len32)
                    {
                        uint32 *dest = list->values32 + index;
                        memmove(dest, dest + 1, sizeof(uint32) * (list->len32 - index));
                        list->len32 -= 1;
                        if (list->len32)
                        {
                            list->values32 = PanicNull(realloc(list->values32, sizeof(uint32) * list->len32));
                        }
                        else
                        {
                            free(list->values32);
                            list->values32 = NULL;
                        }
                    }
                    else
                    {
                        /* Invalid lengths */
                        Panic();
                    }
                }
            }
            break;
        }
    }

    if (key_index < list->len_keys)
    {
        uint16 *dest = list->keys + key_index;
        memmove(dest, dest + 1, sizeof(uint16) * (list->len_keys - key_index));
        list->len_keys -= 1;
        if (list->len_keys)
        {
            list->keys = PanicNull(realloc(list->keys, sizeof(uint16) * list->len_keys));
        }
        else
        {
            free(list->keys);
            list->keys = NULL;
        }
    }
}

void KeyValueList_RemoveAll(key_value_list_t list)
{
    large_kv_element_t *head = list->head;
    while (head)
    {
        large_kv_element_t *tmp = head;
        head = head->next;
        free(tmp);
    }
    free(list->keys);
    free(list->values8);
    free(list->values16);
    free(list->values32);
    memset(list, 0, sizeof(*list));
}

bool KeyValueList_IsSet(key_value_list_t list, key_value_key_t key)
{
    size_t size;
    void *addr;
    return KeyValueList_Get(list, key, &addr, &size);
}
