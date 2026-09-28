//
// Created by meirelles on 9/19/26.
//

#include "lib/string.h"

#include <stdlib.h>
#include <string.h>

#include "lib/linked_list.h"

CharLinkedList explode(char* haystack, const char* needle)
{
    int count = 0;
    char* limit = {nullptr};
    const size_t needle_len = strlen(needle);

    CharLinkedList parts = {0};

    CharLinkedList* item = &parts;

    do
    {
        count++;

        limit = strstr(haystack, needle);

        if (limit != nullptr)
        {
            *limit = '\0';

            item->data = haystack;

            haystack = limit + needle_len;
        }
        else
        {
            item->data = haystack;
            break;
        }

        item->next = (CharLinkedList *) malloc(sizeof(CharLinkedList));
        item = item->next;
    }
    while (1);

    parts.count = count;

    return parts;
}
