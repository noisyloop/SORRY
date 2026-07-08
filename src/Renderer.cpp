#include "Renderer.h"
#include "Assets.h"

#include <stdexcept>

Renderer::Renderer() {
    window_.reset(SDL_CreateWindow("SORRY",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowW, kWindowH, SDL_WINDOW_ALLOW_HIGHDPI));
    if (!window_)
        throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());

    renderer_.reset(SDL_CreateRenderer(window_.get(), -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    if (!renderer_)  // headless / odd drivers: fall back to software
        renderer_.reset(SDL_CreateRenderer(window_.get(), -1, SDL_RENDERER_SOFTWARE));
    if (!renderer_)
        throw std::runtime_error(std::string("SDL_CreateRenderer failed: ") + SDL_GetError());

    SDL_RenderSetLogicalSize(renderer_.get(), kWindowW, kWindowH);
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);
}

void Renderer::clear(SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_.get(), c.r, c.g, c.b, c.a);
    SDL_RenderClear(renderer_.get());
}

void Renderer::present() {
    SDL_RenderPresent(renderer_.get());
}

void Renderer::drawSprite(SDL_Texture* tex, int worldX, int worldY, int w, int h,
                          const SDL_Rect* src) {
    SDL_Rect dst{worldX - camX_, worldY - camY_, w, h};
    SDL_RenderCopy(renderer_.get(), tex, src, &dst);
}

void Renderer::fillRectWorld(int worldX, int worldY, int w, int h, SDL_Color c) {
    fillRect(SDL_Rect{worldX - camX_, worldY - camY_, w, h}, c);
}

void Renderer::fillRect(const SDL_Rect& r, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_.get(), c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer_.get(), &r);
}

void Renderer::outlineRect(const SDL_Rect& r, SDL_Color c) {
    SDL_SetRenderDrawColor(renderer_.get(), c.r, c.g, c.b, c.a);
    SDL_RenderDrawRect(renderer_.get(), &r);
}

void Renderer::drawText(Assets& assets, const std::string& text, int x, int y, SDL_Color c) {
    if (text.empty()) return;
    SDL_Texture* tex = assets.textTexture(text, c);
    if (!tex) return;
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{x, y, w, h};
    SDL_RenderCopy(renderer_.get(), tex, nullptr, &dst);
}

void Renderer::drawTextCentered(Assets& assets, const std::string& text, int cx, int cy,
                                SDL_Color c, int scale) {
    if (text.empty() || scale < 1) return;
    SDL_Texture* tex = assets.textTexture(text, c);
    if (!tex) return;
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    SDL_Rect dst{cx - w * scale / 2, cy - h * scale / 2, w * scale, h * scale};
    SDL_RenderCopy(renderer_.get(), tex, nullptr, &dst);
}

int Renderer::textWidth(Assets& assets, const std::string& text) {
    return assets.measureText(text);
}
