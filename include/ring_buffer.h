#ifndef RING_BUFFER_H
#define RING_BUFFER_C
#include <stdint.h>
#include <stddef.h>

typedef struct {
    void* buffer;
    size_t size;
    size_t head;
    size_t tail;
    size_t space;
} RingBuffer;

void RingBuffer_Init(RingBuffer* buffer, size_t size);
void RingBuffer_Destroy(RingBuffer* buffer);

/**
* Queue up to len bytes to the buffer from src.
* The actual number of queued bytes will be written back to len.
*
* @param src The source buffer to copy up to len bytes from
* @param len Number of bytes to copy from src and queue to the buffer.
* Less bytes may be free in the buffer. The actual number of queued bytes is written back to len.
*/
void RingBuffer_Queue(RingBuffer* buffer, const void* src, size_t* len);
/**
* Consume up to len bytes from the buffer and write them to dst.
* The actual number of consumed bytes will be written back to len.
*
* @param dst The destination buffer to write up to len bytes to
* @param len Number of bytes to consume and write to dst.
* Less bytes may be available to consume. The actual number of consumed bytes is written back to len.
*/
void RingBuffer_Consume(RingBuffer* buffer, void* dst, size_t* len);


#endif