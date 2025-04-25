#include "sdl_audio_buffer.h"
#include <stdlib.h>
#include <stdio.h>

int SDLAudioBuffer_Create(SDLAudioBuffer **buffer, SDL_AudioSpec audioSpec, unsigned ringBufferScale)
{
    SDLAudioBuffer* buf = malloc(sizeof(SDLAudioBuffer));
    *buffer = buf;
    buf->audiospec = audioSpec;
    buf->audiospec.callback = &_SDLAudioBufferCallback;
    buf->audiospec.userdata = buf;

    if (SDL_OpenAudio(&buf->audiospec, NULL) != 0) {
        return -1;
    }
    SDL_PauseAudio(0);

    RingBuffer_Init(&buf->buffer, buf->audiospec.size * ringBufferScale);

    return 0;
}

void SDLAudioBuffer_Free(SDLAudioBuffer *buffer)
{
    SDL_CloseAudio();
    RingBuffer_Destroy(&buffer->buffer);
    free(buffer);
}

void SDLAudioBuffer_QueueAudio(SDLAudioBuffer *buffer, Uint8 *src, size_t* len)
{
    SDL_LockAudio();
    RingBuffer_Queue(&buffer->buffer, src, len);
    SDL_UnlockAudio();
}

void _SDLAudioBufferCallback(void *userdata, Uint8 *stream, int len)
{
    size_t l = len;
    RingBuffer_Consume(&((SDLAudioBuffer*)userdata)->buffer, stream, &l);
    SDL_memset(stream + l, 0, len - l);
}
