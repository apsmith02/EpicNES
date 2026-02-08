#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL.h>
#include <string.h>
#include <math.h>
#include "emulator.h"
#include "sdl_audio_buffer.h"

#ifdef _WIN32
    #include <direct.h> // For _mkdir on Windows
    #define MKDIR(path) _mkdir(path)
#else
    #include <sys/stat.h> // For mkdir on POSIX systems
    #define MKDIR(path) mkdir(path, 0777) // 0777 for permissions
#endif

//Remove quotes from filename
void RemoveQuotes(char* buffer, int max) {
    size_t len = strnlen(buffer, max);
    if (len >= 2 && 
    (buffer[0] == '\"' || buffer[0] == '\'') && 
    (buffer[len-1] == '\"' || buffer[len-1] == '\'')) {
        buffer[--len] = '\0';
        for (size_t i = 1; i <= len; i++) {
            buffer[i-1] = buffer[i];
        }
    }
}

//Input filename. Removes trailing newline and quotes.
void GetFilename(char* buffer, int max) {
    fgets(buffer, max, stdin);
    
    size_t len = strnlen(buffer, max);
    //remove trailing newline
    if (len > 0 && buffer[len-1] == '\n')
        buffer[len-1] = '\0';
    //remove quotes
    RemoveQuotes(buffer, max);
}



int main(int argc, char** argv){
    char rompath[256];
    bool logcpu = false;
    int path_arg_index = 1;

    //Read command line arguments
    if (argc > 1 && strcmp(argv[1], "--logcpu") == 0) {
        logcpu = true;
        path_arg_index = 2;
    }

    if (argc > path_arg_index) {
        strncpy(rompath, argv[path_arg_index], sizeof(rompath) - 1);
        rompath[sizeof(rompath) - 1] = '\0';
    } else {
        printf("Enter path to .nes ROM file: ");
        GetFilename(rompath, sizeof(rompath));
    }
    
    // Initialize emulator
    Emulator* emulator = Emu_Create();

    //Log CPU?
    if (logcpu) {
        FILE* logfile = fopen("cpu.log", "w");
        if (logfile) {
            CPU_SetLogFile(&emulator->cpu, logfile);
        }
    }

    //Set default save directory to saves/
    Emu_SetSavePath(emulator, "saves/");
    MKDIR("saves/");
    
    //Configure volume
    APU_SetChannelVolume(&emulator->apu, APU_CH_MASTER, 0.25);

    //Load ROM
    if (Emu_LoadROM(emulator, rompath) != 0)
        return -1;

    // Initialize SDL2
    SDL_Window* window;
    char window_title[32];
    SDL_Renderer* renderer;
    SDL_Texture* screen_texture;
    SDL_Rect screen_rect = {
        .x = 0,
        .y = 0,
        .w = NES_SCREEN_W * 2,
        .h = NES_SCREEN_H * 2
    };
    SDL_AudioSpec audioSpec = { //16-bit 44.1khz audio
        .freq = 44100,
        .format = AUDIO_S16,
        .channels = 1,
        .samples = 1024
    };
    SDLAudioBuffer* audioBuffer;


    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_Log("Failed to initialize SDL2: %s", SDL_GetError());
        return -1;
    }

    // Create window and renderer
    snprintf(window_title, sizeof(window_title), "EpicNES");
    window = SDL_CreateWindow(
        window_title, 
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

    // Create audio buffer

    if (SDLAudioBuffer_Create(&audioBuffer, audioSpec, 8) != 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
        return -1;
    }

    // Create screen texture to copy emulator pixel buffer to for rendering
    
    screen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, NES_SCREEN_W, NES_SCREEN_H);
    if (screen_texture == NULL) {
        SDL_Log("Failed to create screen texture: %s", SDL_GetError());
        return -1;
    }

    // Main loop

    if (SDL_RenderCopy(renderer, screen_texture, &screen_rect, &screen_rect) != 0) {
        SDL_Log("SDL_RenderCopy failed: %s", SDL_GetError());
        return -1;
    }
    SDL_RenderPresent(renderer);

    Uint32 max_fps = 60;
    bool running = true;
    bool paused = false;
    while (running) {
        Uint32 start = SDL_GetTicks();
        
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_WINDOWEVENT:
                    if (e.window.event == SDL_WINDOWEVENT_CLOSE)
                        running = false;
                    break;
                case SDL_KEYDOWN:
                    switch (e.key.keysym.scancode) {
                        //Controller input
                        case SDL_SCANCODE_X:        Emu_PressButton(emulator, BUTTON_A);        break;
                        case SDL_SCANCODE_Z:        Emu_PressButton(emulator, BUTTON_B);        break;
                        case SDL_SCANCODE_RETURN:   Emu_PressButton(emulator, BUTTON_START);    break;
                        case SDL_SCANCODE_RSHIFT:   Emu_PressButton(emulator, BUTTON_SELECT);   break;
                        case SDL_SCANCODE_UP:       Emu_PressButton(emulator, BUTTON_UP);       break;
                        case SDL_SCANCODE_DOWN:     Emu_PressButton(emulator, BUTTON_DOWN);     break;
                        case SDL_SCANCODE_LEFT:     Emu_PressButton(emulator, BUTTON_LEFT);     break;
                        case SDL_SCANCODE_RIGHT:    Emu_PressButton(emulator, BUTTON_RIGHT);    break;
                        case SDL_SCANCODE_R:
                            if (e.key.keysym.mod & KMOD_CTRL) //CTRL+R: Reset
                                Emu_PowerOn(emulator);
                            break;
                        //Other
                        case SDL_SCANCODE_U:
                            if (e.key.keysym.mod & KMOD_CTRL) { //CTRL+U: Unlimited speed
                                if (max_fps == 60)
                                    max_fps = 1000;
                                else
                                    max_fps = 60;
                            }
                            break;
                        case SDL_SCANCODE_P:
                            if (e.key.keysym.mod & KMOD_CTRL) { //CTRL+P: Pause emulator
                                if (!paused) {
                                    paused = true;
                                    SDL_SetWindowTitle(window, "EpicNES (Paused)");
                                } else {
                                    paused = false;
                                    SDL_SetWindowTitle(window, "EpicNES");
                                }
                            }
                            break;
                        case SDL_SCANCODE_ESCAPE:   running = false; break;
                        
                        case SDL_SCANCODE_F6: //F6: Toggle Pulse 1 mute
                            APU_SetChannelMute(&emulator->apu, APU_CH_PULSE1, !APU_GetChannelMute(&emulator->apu, APU_CH_PULSE1));
                            printf("Pulse 1 %smuted\n", APU_GetChannelMute(&emulator->apu, APU_CH_PULSE1) ? "" : "un");
                            break;
                        case SDL_SCANCODE_F7: //F7: Toggle Pulse 2 mute
                            APU_SetChannelMute(&emulator->apu, APU_CH_PULSE2, !APU_GetChannelMute(&emulator->apu, APU_CH_PULSE2));
                            printf("Pulse 2 %smuted\n", APU_GetChannelMute(&emulator->apu, APU_CH_PULSE2) ? "" : "un");
                            break;
                        case SDL_SCANCODE_F8: //F8: Toggle Triangle mute
                            APU_SetChannelMute(&emulator->apu, APU_CH_TRIANGLE, !APU_GetChannelMute(&emulator->apu, APU_CH_TRIANGLE));
                            printf("Triangle %smuted\n", APU_GetChannelMute(&emulator->apu, APU_CH_TRIANGLE) ? "" : "un");
                            break;
                        case SDL_SCANCODE_F9: //F9: Toggle Noise mute
                            APU_SetChannelMute(&emulator->apu, APU_CH_NOISE, !APU_GetChannelMute(&emulator->apu, APU_CH_NOISE));
                            printf("Noise %smuted\n", APU_GetChannelMute(&emulator->apu, APU_CH_NOISE) ? "" : "un");
                            break;
                        case SDL_SCANCODE_F10: //F10: Toggle DMC mute
                            APU_SetChannelMute(&emulator->apu, APU_CH_DMC, !APU_GetChannelMute(&emulator->apu, APU_CH_DMC));
                            printf("DMC %smuted\n", APU_GetChannelMute(&emulator->apu, APU_CH_DMC) ? "" : "un");
                            break;
                            case SDL_SCANCODE_F11: //F11: Toggle Master mute
                            APU_SetChannelMute(&emulator->apu, APU_CH_MASTER, !APU_GetChannelMute(&emulator->apu, APU_CH_MASTER));
                            printf("Master volume %smuted\n", APU_GetChannelMute(&emulator->apu, APU_CH_MASTER) ? "" : "un");
                            break;
                        case SDL_SCANCODE_MINUS: //-: Master volume down
                            APU_SetChannelVolume(&emulator->apu, APU_CH_MASTER, APU_GetChannelVolume(&emulator->apu, APU_CH_MASTER) - 0.05);
                            printf("Master volume: %.0lf%%\n", APU_GetChannelVolume(&emulator->apu, APU_CH_MASTER) * 100.0);
                            break;
                        case SDL_SCANCODE_EQUALS: //= (+): Master volume up
                            APU_SetChannelVolume(&emulator->apu, APU_CH_MASTER, APU_GetChannelVolume(&emulator->apu, APU_CH_MASTER) + 0.05);
                            printf("Master volume: %.0lf%%\n", APU_GetChannelVolume(&emulator->apu, APU_CH_MASTER) * 100.0);
                            break;
                        default: break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (e.key.keysym.sym) {
                        case SDLK_x:        Emu_ReleaseButton(emulator, BUTTON_A);        break;
                        case SDLK_z:        Emu_ReleaseButton(emulator, BUTTON_B);        break;
                        case SDLK_RETURN:   Emu_ReleaseButton(emulator, BUTTON_START);    break;
                        case SDLK_RSHIFT:   Emu_ReleaseButton(emulator, BUTTON_SELECT);   break;
                        case SDLK_UP:       Emu_ReleaseButton(emulator, BUTTON_UP);       break;
                        case SDLK_DOWN:     Emu_ReleaseButton(emulator, BUTTON_DOWN);     break;
                        case SDLK_LEFT:     Emu_ReleaseButton(emulator, BUTTON_LEFT);     break;
                        case SDLK_RIGHT:    Emu_ReleaseButton(emulator, BUTTON_RIGHT);    break;
                        default: break;
                    }
            }
        }
        
        if (!paused) {
            //Run emulator
            if (Emu_RunFrame(emulator) != 0)
                return -1;
            
            //Render
            SDL_RenderClear(renderer);
            SDL_Rect rect = {0, 0, 0, 0};
            RGBAPixel* buffer = Emu_GetPixelBuffer(emulator, &rect.w, &rect.h);
            SDL_UpdateTexture(screen_texture, &rect, buffer, sizeof(*buffer) * rect.w);
            SDL_RenderCopy(renderer, screen_texture, NULL, &screen_rect);
            SDL_RenderPresent(renderer);
            
            //Queue samples from audio output
            size_t len;
            void* emuAudio = Emu_GetAudioBuffer(emulator, &len);
            size_t iLen = len;
            SDLAudioBuffer_QueueAudio(audioBuffer, emuAudio, &len);
            Emu_ClearAudioBuffer(emulator);
            
            //Limit FPS
            Uint32 fps;
            Uint32 msPerFrame = 1000 / max_fps;
            Uint32 dt = SDL_GetTicks() - start;
            if (dt <= msPerFrame) {
                SDL_Delay(msPerFrame - dt);
                fps = max_fps;
            } else
                fps = (Uint32)(1.0 / ((double)dt / 1000.0));
            snprintf(window_title, sizeof(window_title), "EpicNES (%u FPS)", fps);
            SDL_SetWindowTitle(window, window_title);
        }
    }

    printf("Exiting emulator...\n");
    if (logcpu) {
        if (emulator->cpu.log) {
            fclose(emulator->cpu.log);
        }
    }
    Emu_Free(emulator);
    SDLAudioBuffer_Free(audioBuffer);
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return 0;
}
