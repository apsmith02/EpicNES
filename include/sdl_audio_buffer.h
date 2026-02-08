#ifdef __cplusplus
extern "C" {
#endif

#ifndef SDL_AUDIO_BUFFER_H
#define SDL_AUDIO_BUFFER_H

#include <SDL.h>
#include "ring_buffer.h"

typedef struct {
    SDL_AudioSpec audiospec;
    RingBuffer buffer;
} SDLAudioBuffer;

/**
 * Create an SDL audio buffer. Opens audio with SDL_OpenAudio().
 * 
 * @param audioSpec The SDL_AudioSpec to open audio with.
 * @param ringBufferScale The ring buffer scale. The ring buffer will hold audioSpec.samples * ringBufferScale samples.
 * 
 * @return 0 on success, -1 on SDL error.
*/
int SDLAudioBuffer_Create(SDLAudioBuffer** buffer, SDL_AudioSpec audioSpec, unsigned ringBufferScale);

/**
 * Free an SDL audio buffer. Closes audio with SDL_CloseAudio().
*/
void SDLAudioBuffer_Free(SDLAudioBuffer* buffer);

void SDLAudioBuffer_QueueAudio(SDLAudioBuffer* buffer, Uint8* src, size_t* len);

void _SDLAudioBufferCallback(void* userdata, Uint8* stream, int len);

#endif //#ifndef SDL_AUDIO_BUFFER_H
#ifdef __cplusplus
}
#endif