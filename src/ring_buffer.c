#include "ring_buffer.h"
#include <stdlib.h>

void RingBuffer_Init(RingBuffer *buffer, size_t size)
{
    buffer->buffer = malloc(size);
    buffer->size = size;
    buffer->head = buffer->tail = 0;
    buffer->space = size;
}

void RingBuffer_Destroy(RingBuffer *buffer)
{
    if (buffer->buffer != NULL)
        free(buffer->buffer);
}

void RingBuffer_Queue(RingBuffer* buffer, const void* src, size_t* len)
{
    if (*len > buffer->space) {
        *len = buffer->space;
    }
    char* dst = buffer->buffer;
    for (size_t i = 0; i < *len; i++) {
        dst[buffer->head] = ((char*)src)[i];
        buffer->head = (buffer->head + 1) % buffer->size;
    }
    buffer->space -= *len;
}

void RingBuffer_Consume(RingBuffer *buffer, void *dst, size_t *len)
{
    if (*len > (buffer->size - buffer->space)) {
        *len = buffer->size - buffer->space;
    }
    char* src = buffer->buffer;
    for (size_t i = 0; i < *len; i++) {
        ((char*)dst)[i] = src[buffer->tail];
        buffer->tail = (buffer->tail + 1) % buffer->size;
    }
    buffer->space += *len;
}
