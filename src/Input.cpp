#include "Input.h"

void Input::beginFrame() {
    pressed_.fill(false);
}

void Input::handleEvent(const SDL_Event& e) {
    switch (e.type) {
    case SDL_QUIT:
        quit_ = true;
        break;
    case SDL_KEYDOWN:
        if (!e.key.repeat) {
            down_[e.key.keysym.scancode] = true;
            pressed_[e.key.keysym.scancode] = true;
        }
        break;
    case SDL_KEYUP:
        down_[e.key.keysym.scancode] = false;
        break;
    default:
        break;
    }
}
