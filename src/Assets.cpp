#include "Assets.h"

#include <SDL_image.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace {

std::string findAssetRoot() {
    for (const char* candidate : {"assets", "../assets"}) {
        if (fs::is_directory(candidate))
            return fs::absolute(candidate).string();
    }
    char* base = SDL_GetBasePath();
    if (base) {
        fs::path p = fs::path(base) / "assets";
        SDL_free(base);
        if (fs::is_directory(p))
            return p.string();
    }
    return "";
}

FontPtr openAnyFont(const std::string& root, int ptSize) {
    // Any TTF the user dropped into assets/fonts wins.
    if (!root.empty() && fs::is_directory(fs::path(root) / "fonts")) {
        for (const auto& entry : fs::directory_iterator(fs::path(root) / "fonts")) {
            auto ext = entry.path().extension().string();
            if (ext == ".ttf" || ext == ".TTF" || ext == ".otf") {
                if (TTF_Font* f = TTF_OpenFont(entry.path().string().c_str(), ptSize))
                    return FontPtr(f);
                std::cerr << "[assets] could not open font " << entry.path()
                          << ": " << TTF_GetError() << "\n";
            }
        }
    }
    // System fallbacks: macOS first (the target platform), then common Linux
    // paths so the game also runs in CI containers.
    for (const char* path : {
             "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
             "/System/Library/Fonts/Monaco.ttf",
             "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
             "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
         }) {
        if (TTF_Font* f = TTF_OpenFont(path, ptSize))
            return FontPtr(f);
    }
    return nullptr;
}

// Stable color from a sprite name so placeholders are consistent run-to-run.
SDL_Color colorForName(const std::string& name) {
    uint32_t h = 2166136261u;
    for (char c : name) { h ^= static_cast<uint8_t>(c); h *= 16777619u; }
    auto channel = [&](int shift) {
        return static_cast<Uint8>(96 + ((h >> shift) & 0x7F));  // keep it bright-ish
    };
    return SDL_Color{channel(0), channel(8), channel(16), 255};
}

char glyphForName(const std::string& name) {
    // "npc_mother" -> 'M', "apology_note" -> 'N', "player" -> 'P'
    auto pos = name.find_last_of('_');
    char c = (pos != std::string::npos && pos + 1 < name.size()) ? name[pos + 1] : name.empty() ? '?' : name[0];
    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

}  // namespace

Assets::Assets(Renderer& renderer) : renderer_(renderer), root_(findAssetRoot()) {
    if (root_.empty())
        std::cerr << "[assets] WARNING: no assets/ directory found; everything will be a placeholder\n";
    font_ = openAnyFont(root_, 18);
    if (!font_)
        throw std::runtime_error(
            "No usable font. Drop a .ttf into assets/fonts/ "
            "(fallback /System/Library/Fonts/Supplemental/Andale Mono.ttf was also unavailable).");
}

SDL_Texture* Assets::sprite(const std::string& name) {
    if (auto it = sprites_.find(name); it != sprites_.end())
        return it->second.get();

    SDL_Texture* tex = nullptr;
    if (!root_.empty()) {
        fs::path png = fs::path(root_) / "sprites" / (name + ".png");
        if (fs::exists(png)) {
            tex = IMG_LoadTexture(renderer_.sdl(), png.string().c_str());
            if (!tex)
                std::cerr << "[assets] failed to load " << png << ": " << IMG_GetError() << "\n";
        }
    }
    bool isPlaceholder = (tex == nullptr);
    if (!tex)
        tex = makePlaceholder(name);

    placeholder_[name] = isPlaceholder;
    sprites_[name] = TexturePtr(tex);
    return tex;
}

bool Assets::spriteIsPlaceholder(const std::string& name) {
    sprite(name);  // ensure loaded
    return placeholder_[name];
}

SDL_Texture* Assets::makePlaceholder(const std::string& name) {
    constexpr int kSize = 32;
    SurfacePtr surf(SDL_CreateRGBSurfaceWithFormat(0, kSize, kSize, 32, SDL_PIXELFORMAT_RGBA32));
    SDL_Color body = colorForName(name);
    SDL_FillRect(surf.get(), nullptr,
                 SDL_MapRGBA(surf->format, body.r, body.g, body.b, 255));
    SDL_Rect border{0, 0, kSize, kSize};
    SDL_Color edge{static_cast<Uint8>(body.r / 2), static_cast<Uint8>(body.g / 2),
                   static_cast<Uint8>(body.b / 2), 255};
    // 2px darker border, drawn as four bars.
    Uint32 edgePx = SDL_MapRGBA(surf->format, edge.r, edge.g, edge.b, 255);
    SDL_Rect bars[] = {{0, 0, kSize, 2}, {0, kSize - 2, kSize, 2}, {0, 0, 2, kSize}, {kSize - 2, 0, 2, kSize}};
    for (auto& b : bars) SDL_FillRect(surf.get(), &b, edgePx);
    (void)border;

    char glyph[2] = {glyphForName(name), '\0'};
    SDL_Color ink{255, 255, 255, 255};
    if (SDL_Surface* g = TTF_RenderUTF8_Blended(font_.get(), glyph, ink)) {
        SurfacePtr glyphSurf(g);
        SDL_Rect dst{(kSize - g->w) / 2, (kSize - g->h) / 2, g->w, g->h};
        SDL_BlitSurface(glyphSurf.get(), nullptr, surf.get(), &dst);
    }
    return SDL_CreateTextureFromSurface(renderer_.sdl(), surf.get());
}

SDL_Texture* Assets::textTexture(const std::string& text, SDL_Color c) {
    std::string key = text + "\x1f" + std::to_string(c.r) + "," + std::to_string(c.g) + ","
                      + std::to_string(c.b) + "," + std::to_string(c.a);
    if (auto it = textCache_.find(key); it != textCache_.end())
        return it->second.get();

    // Unbounded caches leak in long sessions; UI text churns (typewriter).
    if (textCache_.size() > 512)
        textCache_.clear();

    SurfacePtr surf(TTF_RenderUTF8_Blended(font_.get(), text.c_str(), c));
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_.sdl(), surf.get());
    textCache_[key] = TexturePtr(tex);
    return tex;
}

int Assets::measureText(const std::string& text) {
    int w = 0, h = 0;
    TTF_SizeUTF8(font_.get(), text.c_str(), &w, &h);
    return w;
}

Mix_Chunk* Assets::sfx(const std::string& name) {
    if (auto it = sfx_.find(name); it != sfx_.end())
        return it->second.get();
    Mix_Chunk* chunk = nullptr;
    if (!root_.empty()) {
        fs::path wav = fs::path(root_) / "sfx" / (name + ".wav");
        if (fs::exists(wav))
            chunk = Mix_LoadWAV(wav.string().c_str());
    }
    sfx_[name] = ChunkPtr(chunk);
    return chunk;
}
