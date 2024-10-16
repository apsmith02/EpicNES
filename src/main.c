#include <stdio.h>
#include <SDL.h>
#include "ppu.h"

int main(int argc, char** argv){
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* screen_texture;
    SDL_Rect screen_rect = {
        .x = 0,
        .y = 0,
        .w = NES_SCREEN_W,
        .h = NES_SCREEN_H
    };

    // Initialize SDL2
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_Log("Failed to initialize SDL2: %s", SDL_GetError());
        return -1;
    }

    // Create window and renderer

    window = SDL_CreateWindow(
        "EpicNES", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
        screen_rect.w, screen_rect.h, 0);
    if (window == NULL) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (renderer == NULL) {
        SDL_Log("Failed to create renderer: %s", SDL_GetError());
        return -1;
    }

    // Check pixel formats

    SDL_RendererInfo info;
    SDL_GetRendererInfo(renderer, &info);
    printf("Renderer name: %s\n", info.name);
    printf("Texture formats:\n");
    for (Uint32 i = 0; i < info.num_texture_formats; i++) {
        printf("%s\n", SDL_GetPixelFormatName(info.texture_formats[i]));
    }

    // Create screen texture to copy emulator pixel buffer to for rendering
    
    screen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 256, 240);
    if (screen_texture == NULL) {
        SDL_Log("Failed to create screen texture: %s", SDL_GetError());
        return -1;
    }

    // Test pixel array

    void* pixels;
    int pitch;
    if (SDL_LockTexture(screen_texture, &screen_rect, &pixels, &pitch) != 0) {
        SDL_Log("SDL_LockTexture failed: %s", SDL_GetError());
        return -1;
    }
    for (int y = 0; y < screen_rect.h; y++) {
        for (int x = 0; x < screen_rect.w; x++) {
            RGBAPixel pixel = {
                .r = x,
                .g = x,
                .b = 255,
                .a = 255
            };
            ((RGBAPixel*)pixels)[y * screen_rect.w + x] = pixel;
        }
    }
    SDL_UnlockTexture(screen_texture);

    // Main loop

    if (SDL_RenderCopy(renderer, screen_texture, &screen_rect, &screen_rect) != 0) {
        SDL_Log("SDL_RenderCopy failed: %s", SDL_GetError());
        return -1;
    }
    SDL_RenderPresent(renderer);

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE)
                        running = 0;
                    break;
            }
        }
    }

    SDL_DestroyTexture(screen_texture);
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return 0;
}
