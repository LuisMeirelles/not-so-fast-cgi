//
// Created by meirelles on 9/24/26.
//

#pragma once

#include <stddef.h>

typedef struct CharLinkedList
{
    char* data;
    struct CharLinkedList* next;
    size_t count;
} CharLinkedList;
