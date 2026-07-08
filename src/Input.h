#pragma once

#include <SDL.h>
#include <array>

// Keyboard state with held vs edge-triggered queries.
// Call beginFrame() once per frame before pumping events,
// then feed every SDL_Event through handleEvent().
class Input {
public:
    void beginFrame();
    void handleEvent(const SDL_Event& e);

    bool isDown(SDL_Scancode sc) const { return down_[sc]; }
    bool wasPressed(SDL_Scancode sc) const { return pressed_[sc]; }

    bool quitRequested() const { return quit_; }

    // Semantic helpers so callers don't repeat scancode lists.
    bool moveLeft() const  { return isDown(SDL_SCANCODE_LEFT)  || isDown(SDL_SCANCODE_A); }
    bool moveRight() const { return isDown(SDL_SCANCODE_RIGHT) || isDown(SDL_SCANCODE_D); }
    bool moveUp() const    { return isDown(SDL_SCANCODE_UP)    || isDown(SDL_SCANCODE_W); }
    bool moveDown() const  { return isDown(SDL_SCANCODE_DOWN)  || isDown(SDL_SCANCODE_S); }
    bool interactPressed() const { return wasPressed(SDL_SCANCODE_E) || wasPressed(SDL_SCANCODE_SPACE); }
    bool upPressed() const   { return wasPressed(SDL_SCANCODE_UP)   || wasPressed(SDL_SCANCODE_W); }
    bool downPressed() const { return wasPressed(SDL_SCANCODE_DOWN) || wasPressed(SDL_SCANCODE_S); }
    bool inventoryPressed() const { return wasPressed(SDL_SCANCODE_I); }
    bool escapePressed() const { return wasPressed(SDL_SCANCODE_ESCAPE); }

private:
    std::array<bool, SDL_NUM_SCANCODES> down_{};
    std::array<bool, SDL_NUM_SCANCODES> pressed_{};
    bool quit_ = false;
};
