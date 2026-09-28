//
// Created by meirelles on 9/16/26.
//

#include "process_stdout.h"

#include <stdlib.h>

#include "lib/string.h"

int process_stdout(char* content, StdoutReponse* stdout_buf)
{
    int i = 0;
    char* http_headers = content;

    CharLinkedList parts = explode(http_headers, "\r\n\r\n");

    http_headers = parts.data;
    stdout_buf->body = parts.next->data;

    const CharLinkedList headers = explode(http_headers, "\r\n");

    const size_t headers_count = headers.count;

    const CharLinkedList* header = &headers;

    for (i = 0; i < headers_count; i++)
    {
        parts = explode(header->data, ":");

        char* key = parts.data;
        char* value = parts.next->data;

        while (*value == ' ') value++;

        stdout_buf->headers[i].key = key;
        stdout_buf->headers[i].value = value;

        header = header->next;
    }

    return 0;
}
