#include <iostream>
#include <string>
#include <math.h>

#include <SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "widgets/FileDialog.h"

#include "sdl_audio_buffer.h"

#include "emulator.h"

Emulator* emulator = nullptr;
bool paused = false;

int mainMenuHeight;
FileDialog fileDialog;
struct VolumeMixer {
    bool active = false;
    int chVolPercent[APU_NUM_VOL_SETTINGS] = { 100, 100, 100, 100, 100, 100 };
    bool chMute[APU_NUM_VOL_SETTINGS] = { false };
};
VolumeMixer volumeMixer;

SDL_Window* window;
std::string windowTitle = "EpicNES";
SDL_Renderer* renderer;
SDL_Texture* emuVideo;
SDL_AudioSpec audioSpec;
SDLAudioBuffer* audioBuffer = nullptr;

void UIAction_Open();
void UIAction_Close();
void UIAction_Pause();
void UIAction_PowerCycle();

void OpenROM(const char* path);

void FitRectToRegion(SDL_Rect& rect, const SDL_Rect& region);

int main(int argc, char* argv[]) {
    // Initialize SDL2
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        std::cout << "Error initializing SDL2: " << SDL_GetError() << std::endl;
        return -1;
    }
    if (SDL_CreateWindowAndRenderer(NES_SCREEN_W * 2, NES_SCREEN_H * 2, SDL_WINDOW_RESIZABLE, &window, &renderer) != 0) {
        std::cout << "Error creating SDL window and renderer: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Set up ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    // Set up ImGui SDL2 renderer backend
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Create emulator
    emulator = Emu_Create();

    // 16-bit 44.1khz audio
    audioSpec.freq = 44100;
    audioSpec.format = AUDIO_S16;
    audioSpec.channels = 1;
    audioSpec.samples = 1024;
    if (SDLAudioBuffer_Create(&audioBuffer, audioSpec, 4) != 0) {
        std::cout << "Error opening SDL audio: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    // Create video output texture
    emuVideo = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, NES_SCREEN_W, NES_SCREEN_H);
    if (emuVideo == NULL) {
        SDL_Log("Failed to create emulator video output texture: %s", SDL_GetError());
        return -1;
    }
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    // Set window size to NES screen dimensions * 2, plus main menu height
    {
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        if (ImGui::BeginMainMenuBar()) {
            mainMenuHeight = ImGui::GetWindowSize().y;
        }
        ImGui::EndMainMenuBar();
        ImGui::EndFrame();
    }
    SDL_SetWindowSize(window, NES_SCREEN_W * 2, NES_SCREEN_H * 2 + mainMenuHeight);

    // Set up widgets
    fileDialog = FileDialog("Open ROM File");

    // Main loop
    Uint32 max_fps = 60;
    bool running = true;
    while (running) {
        Uint32 start = SDL_GetTicks();

        // Input events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_DROPFILE:
                    std::cout << "Dropped file " << event.drop.file << std::endl;
                    OpenROM(event.drop.file);
                    SDL_free(event.drop.file);
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_CLOSE)
                        running = false;
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        //Controller input
                        case SDL_SCANCODE_X:        Emu_PressButton(emulator, BUTTON_A);        break;
                        case SDL_SCANCODE_Z:        Emu_PressButton(emulator, BUTTON_B);        break;
                        case SDL_SCANCODE_RETURN:   Emu_PressButton(emulator, BUTTON_START);    break;
                        case SDL_SCANCODE_RSHIFT:   Emu_PressButton(emulator, BUTTON_SELECT);   break;
                        case SDL_SCANCODE_UP:       Emu_PressButton(emulator, BUTTON_UP);       break;
                        case SDL_SCANCODE_DOWN:     Emu_PressButton(emulator, BUTTON_DOWN);     break;
                        case SDL_SCANCODE_LEFT:     Emu_PressButton(emulator, BUTTON_LEFT);     break;
                        case SDL_SCANCODE_RIGHT:    Emu_PressButton(emulator, BUTTON_RIGHT);    break;

                        // UI actions
                        case SDL_SCANCODE_R:
                            if (event.key.keysym.mod & KMOD_CTRL) //CTRL+R: Reset
                                UIAction_PowerCycle();
                            break;
                        case SDL_SCANCODE_O:
                        if (event.key.keysym.mod & KMOD_CTRL) //CTRL+O: Open ROM
                                UIAction_Open();
                            break;
                        case SDL_SCANCODE_W:
                            if (event.key.keysym.mod & KMOD_CTRL) //CTRL+W: Close ROM
                                UIAction_Close();
                            break;
                        case SDL_SCANCODE_ESCAPE: //Esc: Pause/resume
                            UIAction_Pause();
                            break;
                        default: break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.sym) {
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

            ImGui_ImplSDL2_ProcessEvent(&event);
        }

        if (Emu_IsROMLoaded(emulator) && !paused) {
            //Run emulator
            if (Emu_RunFrame(emulator) != 0)
                return -1;
            //Queue samples from audio output
            size_t len;
            Uint8* emuAudio = (Uint8*)Emu_GetAudioBuffer(emulator, &len);
            size_t iLen = len;
            SDLAudioBuffer_QueueAudio(audioBuffer, emuAudio, &len);
            Emu_ClearAudioBuffer(emulator);
        }

        // ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        {
            // Main menu bar
            if (ImGui::BeginMainMenuBar()) {
                // File
                if (ImGui::BeginMenu("File")) {
                    if (ImGui::MenuItem("Open ROM", "Ctrl+O"))      UIAction_Open();
                    if (ImGui::MenuItem("Close ROM", "Ctrl+W"))     UIAction_Close();
                    if (ImGui::MenuItem("Exit"))                    running = false;

                    ImGui::EndMenu();
                }
                // Game
                if (ImGui::BeginMenu("Game")) {
                    if (ImGui::MenuItem("Pause", "Esc"))            UIAction_Pause();
                    if (ImGui::MenuItem("Reset", "Ctrl+R"))         UIAction_PowerCycle();

                    ImGui::EndMenu();
                }
                // Options
                if (ImGui::BeginMenu("Options")) {
                    if (ImGui::MenuItem("Volume Mixer"))            volumeMixer.active = true;

                    ImGui::EndMenu();
                }
                ImGui::EndMainMenuBar();
            }

            // File dialog
            if (fileDialog.Update()) {
                // File confirmed, open
                OpenROM(fileDialog.GetFilePath().u8string().c_str());
            }

            // Volume mixer
            if (volumeMixer.active) {
                if (ImGui::Begin("Volume Mixer", &volumeMixer.active)) {
                    if (ImGui::SliderInt("Master Volume", &volumeMixer.chVolPercent[APU_CH_MASTER], 0, 100)) {
                        Emu_SetAudioChannelVolume(emulator, APU_CH_MASTER, volumeMixer.chVolPercent[APU_CH_MASTER] / 100.0);
                    }
                }
                ImGui::End();
            }
        }
        ImGui::EndFrame();
        
        // Rendering
        ImGui::Render();

        SDL_RenderClear(renderer);
        if (Emu_IsROMLoaded(emulator)) {
            SDL_Rect screenSrc = {0, 0, 0, 0};
            RGBAPixel* buffer = Emu_GetPixelBuffer(emulator, &screenSrc.w, &screenSrc.h);
            SDL_UpdateTexture(emuVideo, &screenSrc, buffer, sizeof(*buffer) * screenSrc.w);
            
            SDL_Rect screenDest;
            SDL_QueryTexture(emuVideo, NULL, NULL, &screenDest.w, &screenDest.h);
            SDL_Rect screenRegion = { 0, mainMenuHeight, 0, 0 };
            SDL_GetWindowSize(window, &screenRegion.w, &screenRegion.h);
            screenRegion.h -= screenRegion.y;
            FitRectToRegion(screenDest, screenRegion);

            SDL_RenderCopy(renderer, emuVideo, NULL, &screenDest);
        }
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
        
        //Limit FPS
        Uint32 fps;
        Uint32 msPerFrame = 1000 / max_fps;
        Uint32 dt = SDL_GetTicks() - start;
        if (dt <= msPerFrame) {
            SDL_Delay(msPerFrame - dt);
            fps = max_fps;
        } else
            fps = (Uint32)(1.0 / ((double)dt / 1000.0));
        
        windowTitle = "EpicNES";
        windowTitle += " (";
        windowTitle += std::to_string(fps);
        windowTitle += " FPS)";

        if (paused)
            windowTitle += " (Paused)";

        SDL_SetWindowTitle(window, windowTitle.c_str());
    }

    Emu_Free(emulator);

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}

void UIAction_Open()
{
    fileDialog.Show();
}

void UIAction_Close()
{
    Emu_CloseROM(emulator);
}

void UIAction_Pause()
{
    paused = !paused;
}

void UIAction_PowerCycle()
{
    if (Emu_IsROMLoaded(emulator))
        Emu_PowerOn(emulator);
}

void OpenROM(const char *path)
{
    if (Emu_LoadROM(emulator, path) == 0)
        Emu_PowerOn(emulator);
}

void FitRectToRegion(SDL_Rect &rect, const SDL_Rect &region)
{
    float wScale = (float)region.w / rect.w;
    float hScale = (float)region.h / rect.h;
    float scale = std::min(wScale, hScale);

    rect.w *= scale;
    rect.h *= scale;

    rect.x = region.x + ((region.w - rect.w) / 2);
    rect.y = region.y + ((region.h - rect.h) / 2);
}
