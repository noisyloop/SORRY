#include "Inventory.h"

#include <algorithm>
#include <unordered_map>

#include "Assets.h"
#include "Renderer.h"

std::string Inventory::displayName(const std::string& itemId) {
    static const std::unordered_map<std::string, std::string> names = {
        {"apology_note", "Apology Note"},
    };
    auto it = names.find(itemId);
    if (it != names.end()) return it->second;
    // Fallback: "some_item_id" -> "some item id"
    std::string pretty = itemId;
    std::replace(pretty.begin(), pretty.end(), '_', ' ');
    return pretty;
}

bool Inventory::has(const std::string& itemId) const {
    return std::find(items_.begin(), items_.end(), itemId) != items_.end();
}

void Inventory::add(const std::string& itemId) {
    if (!has(itemId))
        items_.push_back(itemId);
}

void Inventory::render(Renderer& r, Assets& assets) const {
    const SDL_Rect panel{Renderer::kWindowW / 2 - 180, 80, 360, 320};
    r.fillRect(panel, SDL_Color{16, 10, 28, 240});
    r.outlineRect(panel, SDL_Color{255, 179, 71, 255});

    r.drawText(assets, "INVENTORY", panel.x + 16, panel.y + 12, SDL_Color{255, 179, 71, 255});
    int y = panel.y + 12 + assets.fontHeight() + 12;

    if (items_.empty()) {
        r.drawText(assets, "(empty pockets, heavy heart)", panel.x + 16, y,
                   SDL_Color{160, 150, 175, 255});
        return;
    }
    for (const auto& id : items_) {
        SDL_Texture* icon = assets.sprite(id);
        SDL_Rect dst{panel.x + 16, y, 24, 24};
        SDL_RenderCopy(r.sdl(), icon, nullptr, &dst);
        r.drawText(assets, displayName(id), panel.x + 50, y + 2, SDL_Color{235, 231, 240, 255});
        y += 32;
    }
}
