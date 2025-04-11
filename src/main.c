#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <string.h>
#include <math.h>
#include "emulator.h"
#include "console_debug.h"
#include "sdl_audio_buffer.h"

//Remove quotes from filename
void RemoveQuotes(char* buffer, int max) {
    size_t len = strnlen(buffer, max);
    if (len >= 2 && buffer[0] == '\"' && buffer[len-1] == '\"') {
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

/*
void PrintDisassembly(Emulator* emu) {
    //Print last 4, next 5 instructions

    static char asm_buffer[64];
    uint16_t address = emu->cpu.state.pc;
    //Backtrack 4 instructions
    for (int i = 0; i < 4; i++) {
        if (Emu_DebugIterLastInstruction(emu, &address) == -1)
            break;
    }
    //Print last 4 + next 5 = 9 instructions
    for (int i = 0; i < 9; i++) {
        Emu_DebugDisassemble(emu, address, asm_buffer, sizeof(asm_buffer));
        printf("%c%c %s\n", 
            (address == emu->cpu.state.pc) ? '>' : ' ',                 //Display an arrow pointing to current instruction
            (Emu_DebugAddrHasCodeBreakpoint(emu, address) ? '*' : ' '), //Display an asterisk next to instructions with a code breakpoint
            asm_buffer
        );

        if (Emu_DebugIterNextInstruction(emu, &address) == -1)
            break;
    }
}

void PrintCPURegisters(Emulator* emu) {
    CPUState* state = &emu->cpu.state;
    printf("Cycle: %llu  PC: $%04X  A: $%02X  X: $%02X  Y: $%02X  S: $%02X  P: $%02X (%c%c-%c%c%c%c%c)\n", 
        state->cycles, state->pc, state->a, state->x, state->y, state->s, state->p,
    (state->p & CPU_FLAG_N) ? 'N' : 'n',
    (state->p & CPU_FLAG_V) ? 'V' : 'v',
    (state->p & CPU_FLAG_B) ? 'B' : 'b',
    (state->p & CPU_FLAG_D) ? 'D' : 'd',
    (state->p & CPU_FLAG_I) ? 'I' : 'i',
    (state->p & CPU_FLAG_Z) ? 'Z' : 'z',
    (state->p & CPU_FLAG_C) ? 'C' : 'c');
}

void PrintPPURegisters(Emulator* emu) {
    PPUState* state = &emu->ppu.state;
    printf("Cycle: %d  Scanline: %d  Frame: %llu  VRAM Addr: $%04X  T: $%04X  Fine X Scroll:  %d  Write Toggle: [%c]  OAM Addr: $%02X\n",
        state->cycle, state->scanline, state->frames, state->v, state->t, (int)state->x, state->w ? 'X' : ' ', state->oamaddr
    );
    printf("VRAM Increment: +%d  Sprite Table: $%04X  BG Table: $%04X  8x16 Sprite Mode: [%c]  NMI Enable: [%c]\n",
        (state->ppuctrl & PPUCTRL_INC) ? 32 : 1,
        (state->ppuctrl & PPUCTRL_SPRTABLE) ? 0x1000 : 0, (state->ppuctrl & PPUCTRL_BGTABLE) ? 0x1000 : 0,
        (state->ppuctrl & PPUCTRL_SPRSIZE) ? 'X' : ' ', (state->ppuctrl & PPUCTRL_NMI) ? 'X' : ' '
    );
    printf("Render BG: [%c]  Render Sprites: [%c]  Sprite Overflow: [%c]  Sprite 0 Hit: [%c]  VBlank: [%c]\n",
        (state->ppumask & PPUMASK_BG) ? 'X' : ' ', (state->ppumask & PPUMASK_SPR) ? 'X' : ' ',
        (state->ppustatus & PPUSTATUS_SPROVERFLOW) ? 'X' : ' ', (state->ppustatus & PPUSTATUS_SPR0HIT) ? 'X' : ' ', (state->ppustatus & PPUSTATUS_VBLANK) ? 'X' : ' '
    );
}

void PrintAPU(Emulator* emu) {
    APUState* state = &emu->apu.state;
    APUPulse* pulse1 = &state->ch_pulse1;
    APUEnvelope* p1_env = &pulse1->envelope;
    APUPulse* pulse2 = &state->ch_pulse2;
    APUEnvelope* p2_env = &pulse2->envelope;

    printf("Frame Counter -  5-step: [%c]  IRQ inhibit: [%c]  FC Cycle Counter: %.1f APU cycles\n",
        (state->fc_ctrl & FC_5STEP) ? 'X' : ' ',
        (state->fc_ctrl & FC_IRQ_INHIBIT) ? 'X' : ' ',
        (float)(state->fc_cycle_count / 2.0)
    );
    printf("Channel Status -  DMC: [%c]  Noise: [%c]  Triangle: [%c]  Pulse 2: [%c]  Pulse 1: [%c]\n",
        (state->status & APU_STATUS_D) ? 'X' : ' ',
        (state->status & APU_STATUS_N) ? 'X' : ' ',
        (state->status & APU_STATUS_T) ? 'X' : ' ',
        (state->status & APU_STATUS_2) ? 'X' : ' ',
        (state->status & APU_STATUS_1) ? 'X' : ' '
    );
    printf("Pulse 1 -  Period: %d  Timer: %d  Duty: %d  Duty Pos: %d\n",
        (int)pulse1->period, (int)pulse1->timer, (int)pulse1->duty, (int)pulse1->sequencer);
    printf(" Env Start: [%c]  Env Loop / Length Halt: [%c]  Env Const Volume: [%c]  Env Period/Volume: %d  Env Divider: %d  Env Decay: %d  Length Counter: %d\n",
        p1_env->start ? 'X' : ' ',
        p1_env->loop ? 'X' : ' ',
        p1_env->constant_volume ? 'X' : ' ',
        (int)p1_env->period,
        (int)p1_env->divider,
        (int)p1_env->decay,
        (int)pulse1->length
    );
    printf("Pulse 2 -  Period: %d  Timer: %d  Duty: %d  Duty Pos: %d\n",
        (int)pulse2->period, (int)pulse2->timer, (int)pulse2->duty, (int)pulse2->sequencer);
    printf(" Env Start: [%c]  Env Loop / Length Halt: [%c]  Env Const Volume: [%c]  Env Period/Volume: %d  Env Divider: %d  Env Decay: %d  Length Counter: %d\n",
        p2_env->start ? 'X' : ' ',
        p2_env->loop ? 'X' : ' ',
        p2_env->constant_volume ? 'X' : ' ',
        (int)p2_env->period,
        (int)p2_env->divider,
        (int)p2_env->decay,
        (int)pulse2->length
    );
}

DebugStepType ConsoleDebug(void* emulator) {
    Emulator* emu = (Emulator*)emulator;
    
    printf("\nDisassembly:\n");
    PrintDisassembly(emu);
    printf("\nCPU:\n");
    PrintCPURegisters(emu);
    printf("\nPPU:\n");
    PrintPPURegisters(emu);
    printf("\nAPU:\n");
    PrintAPU(emu);

    //Read commands

    static char input_buffer[1024];
    char* args[3];
    do {
        fgets(input_buffer, sizeof(input_buffer), stdin);

        //Trim newline
        for (int i = 0; i < sizeof(input_buffer); i++) {
            if (input_buffer[i] == '\n') {
                input_buffer[i] = '\0';
                break;
            }
        }

        args[0] = strtok(input_buffer, " ");
        args[1] = strtok(NULL, " ");
        args[2] = strtok(NULL, " ");
        
        if (args[0] == NULL) {
            printf("Command not entered.\n");
        } else if (!strcmp(args[0], "resume")) {
            //Resume execution

            return DEBUG_STEP_NONE;
        } else if (!strcmp(args[0], "step")) {
            //Step

            if (args[1] == NULL) {
                printf("Usage: step [into]\n");
            } else if (!strcmp(args[1], "into")) {
                return DEBUG_STEP_INTO;
            } else {
                printf("Usage: step [into]\n");
            }
        } else if (!strcmp(args[0], "bp")) {
            //Breakpoint

            if (args[1] == NULL || args[2] == NULL || strcmp(args[1], "set") != 0 || args[2][0] != '$') {
                printf("Usage: bp set $<address (hex)>\n");
            } else {
                Emu_DebugSetCodeBreakpoint(emu, strtoul(&args[2][1], NULL, 16));
            }
        } else {
            printf("Invalid command. Valid commands: resume, step, bp\n");
        }
    } while (true);
}
*/


int main(int argc, char** argv){
    char rompath[256];

    printf("Enter path to .nes ROM file: ");
    GetFilename(rompath, sizeof(rompath));
    
    // Initialize emulator
    Emulator* emulator = Emu_Create();
    if (Emu_LoadROM(emulator, rompath) != 0)
        return -1;
    Emu_SetDebugPauseCallback(emulator, &ConsoleDebugCallback, (void*)emulator);
    Emu_DebugEnable(emulator, true); //Enable emulator debugging

    // Initialize SDL2

    SDL_Window* window;
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

    Uint32 fps = 60;
    bool running = true;
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
                        //Other
                        case SDL_SCANCODE_GRAVE:    Emu_DebugPause(emulator); break; //~: Debug pause
                        case SDL_SCANCODE_ESCAPE:   running = false; break;
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
        SDLAudioBuffer_QueueAudio(audioBuffer, emuAudio, &len);
        Emu_ClearAudioBuffer(emulator);

        //Limit FPS
        Uint32 dt = SDL_GetTicks() - start;
        Uint32 msPerFrame = 1000 / fps;
        if (dt <= msPerFrame)
            SDL_Delay(msPerFrame - dt);
    }

    printf("Exiting emulator...\n");
    Emu_Free(emulator);
    SDLAudioBuffer_Free(audioBuffer);
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();

    return 0;
}
