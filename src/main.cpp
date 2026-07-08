#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <exception>
#include <iostream>

#include "Game.h"

namespace {

// RAII for the SDL global init/quit pairs so early throws still clean up.
struct SdlRuntime {
    SdlRuntime() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
            throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
        if (TTF_Init() != 0) {
            SDL_Quit();
            throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
        }
        IMG_Init(IMG_INIT_PNG);  // optional; missing PNGs fall back to rects
    }
    ~SdlRuntime() {
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
    }
};

}  // namespace

int main() {
    try {
        SdlRuntime sdl;
        Game game;
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
