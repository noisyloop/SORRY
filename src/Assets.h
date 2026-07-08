#pragma once

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "Renderer.h"

struct TtfFontDeleter   { void operator()(TTF_Font* f) const   { if (f) TTF_CloseFont(f); } };
struct MixChunkDeleter  { void operator()(Mix_Chunk* c) const  { if (c) Mix_FreeChunk(c); } };

using FontPtr  = std::unique_ptr<TTF_Font, TtfFontDeleter>;
using ChunkPtr = std::unique_ptr<Mix_Chunk, MixChunkDeleter>;

// Sprite / font / sfx cache.
//
// Sprites: looks for assets/sprites/<name>.png. If missing, generates a
// labeled colored rectangle (color hashed from the name, first letter of the
// last word as a glyph) so the game is playable before any art exists.
class Assets {
public:
    // Locates the assets/ directory (cwd, parent, or next to the executable).
    // Throws std::runtime_error if no usable font can be opened.
    explicit Assets(Renderer& renderer);

    const std::string& rootDir() const { return root_; }

    // Never returns null: falls back to a generated placeholder.
    SDL_Texture* sprite(const std::string& name);
    bool spriteIsPlaceholder(const std::string& name);

    // Cached text rendering (UI strings repeat a lot frame-to-frame).
    SDL_Texture* textTexture(const std::string& text, SDL_Color c);
    int measureText(const std::string& text);
    int fontHeight() const { return TTF_FontHeight(font_.get()); }

    // Null if the file doesn't exist; callers treat sfx as optional.
    Mix_Chunk* sfx(const std::string& name);

private:
    SDL_Texture* makePlaceholder(const std::string& name);

    Renderer& renderer_;
    std::string root_;
    FontPtr font_;
    std::unordered_map<std::string, TexturePtr> sprites_;
    std::unordered_map<std::string, bool> placeholder_;
    std::unordered_map<std::string, TexturePtr> textCache_;
    std::unordered_map<std::string, ChunkPtr> sfx_;
};
