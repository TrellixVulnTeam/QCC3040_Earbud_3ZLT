/* Copyright (c) 2016 Qualcomm Technologies International, Ltd. */
/*   %%version */
/**
 * \file
 * Intrusive Singly Linked List (implementation)
 */
#include "utils/utils_private.h"

/*****************************************************************************
 * Private Function Definitions
 ****************************************************************************/

utils_SLLMember **utils_SLL_findLink(utils_SLL *s, const utils_SLLMember *m)
{
    utils_SLLMember **link = &s->first;
    while (*link)
    {
       if (*link == m) 
           return link;

       link = &(*link)->next;
    }
    return NULL;
}

/**
 * Find last (NULL) link.
 *
 * Used when appending member to end (e.g. fifo)
 */
static utils_SLLMember **utils_SLL_findLastLink(utils_SLL *s)
{
    utils_SLLMember **link = &s->first;
    while (*link)
    {
       link = &(*link)->next;
    }
    return link;
}

/*****************************************************************************
 * Public Function Definitions
 ****************************************************************************/

void utils_SLL_append(utils_SLL *s, utils_SLLMember *m)
{
    utils_SLLMember **last_link;

    assert(!utils_SLL_contains(s,m));

    last_link = utils_SLL_findLastLink(s);
    m->next = NULL;
    *last_link = m;

    assert(utils_SLL_contains(s,m));
}

void utils_SLL_prepend(utils_SLL *s, utils_SLLMember *m)
{
    assert(!utils_SLL_contains(s,m));
    m->next = s->first;
    s->first = m;
    assert(utils_SLL_contains(s,m));
}

utils_SLLMember *utils_SLL_removeHead(utils_SLL *s)
{
    utils_SLLMember *head = s->first;
    if (head)
    {
        s->first = head->next;
    }
    return head;
}

void utils_SLL_remove(utils_SLL *s, utils_SLLMember *m)
{
    utils_SLLMember **link = utils_SLL_findLink(s, m);
    assert(NULL != link);
    *link = m->next;
    assert(!utils_SLL_contains(s,m));
}

void utils_SLL_forEach(const utils_SLL *s, utils_SLLFunction op)
{
    utils_SLLMember *m = s->first;
    while (m)
    {
        op(m);
        m = m->next;
    }
}

void utils_SLL_visitEach(const utils_SLL *s, utils_SLLFunctor *v)
{
    utils_SLLMember *m = s->first;
    while (m)
    {
        v->visitMember(v,m);
        m = m->next;
    }
}
