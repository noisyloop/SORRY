#include "Game.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

struct MoodPalette {
    SDL_Color floor, wall, grass, door, background;
};

// Room tint follows the music: Vegas neon over desert dusk.
MoodPalette paletteFor(const std::string& mood) {
    if (mood == "melancholy_a_minor")
        return {{58, 52, 82, 255}, {28, 22, 46, 255}, {52, 74, 66, 255}, {214, 62, 106, 255}, {14, 10, 24, 255}};
    if (mood == "tense_chromatic")
        return {{70, 46, 46, 255}, {34, 16, 20, 255}, {84, 62, 40, 255}, {255, 179, 71, 255}, {18, 8, 10, 255}};
    if (mood == "dread_diminished")
        return {{40, 40, 48, 255}, {14, 12, 20, 255}, {36, 48, 44, 255}, {120, 220, 232, 255}, {6, 6, 10, 255}};
    if (mood == "playful_major")
        return {{88, 70, 100, 255}, {40, 26, 56, 255}, {66, 96, 70, 255}, {255, 214, 92, 255}, {24, 14, 34, 255}};
    if (mood == "dream_lydian")
        return {{62, 58, 96, 255}, {26, 24, 52, 255}, {58, 66, 96, 255}, {170, 120, 232, 255}, {12, 10, 28, 255}};
    // calm_pentatonic + default
    return {{66, 60, 88, 255}, {30, 24, 50, 255}, {56, 80, 70, 255}, {214, 62, 106, 255}, {16, 12, 26, 255}};
}

constexpr int kTile = Room::kTile;

}  // namespace

Game::Game()
    : renderer_(),
      assets_(renderer_),
      audio_(assets_),
      roomLoader_(assets_.rootDir()) {
    if (auto data = saveGame_.read()) {
        std::cout << "[save] resuming from " << saveGame_.path() << "\n";
        inventory_.setItems(data->inventory);
        flags_ = data->flags;
        enterRoom(data->room, nullptr, /*autosave=*/false);
        if (!room_)  // saved room's map file was deleted; fall back
            enterRoom("bedroom", nullptr, false);
        else
            player_.setPixelPosition(data->playerX, data->playerY);
    } else {
        enterRoom("bedroom", nullptr, /*autosave=*/false);
    }
    if (!room_)
        throw std::runtime_error("Could not load starting room 'bedroom' "
                                 "(missing assets/maps/bedroom.txt?)");
}

void Game::enterRoom(const std::string& name, const Direction* fromDir, bool autosave) {
    auto next = roomLoader_.load(name);
    if (!next) {
        std::cerr << "[game] staying put: room '" << name << "' failed to load\n";
        return;
    }
    room_ = std::move(next);

    // Collected items don't respawn.
    for (const auto& id : inventory_.items())
        room_->removeEntity(id);

    if (fromDir) {
        // Entering heading north => appear at the room's south door.
        if (const auto* door = room_->doorFacing(oppositeOf(*fromDir))) {
            int tx = door->tx, ty = door->ty;
            switch (door->dir) {  // one tile inward so we don't re-trigger
            case Direction::North: ++ty; break;
            case Direction::South: --ty; break;
            case Direction::West:  ++tx; break;
            case Direction::East:  --tx; break;
            }
            player_.setTilePosition(tx, ty);
        } else {
            int tx, ty;
            room_->defaultSpawn(tx, ty);
            player_.setTilePosition(tx, ty);
        }
    } else {
        int tx, ty;
        room_->defaultSpawn(tx, ty);
        player_.setTilePosition(tx, ty);
    }

    audio_.playMood(room_->mood());
    if (!room_->onEnterFlag().empty())
        setFlag(room_->onEnterFlag());
    if (autosave)
        save();
}

void Game::save() {
    SaveData data;
    data.room = room_->name();
    data.playerX = player_.x();
    data.playerY = player_.y();
    data.inventory = inventory_.items();
    data.flags = flags_;
    if (!saveGame_.write(data))
        std::cerr << "[save] autosave failed\n";
}

void Game::run() {
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running_) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = std::min((now - prev) / freq, 0.05);  // clamp hitches
        prev = now;
        worldTime_ += dt;

        input_.beginFrame();
        SDL_Event e;
        while (SDL_PollEvent(&e))
            input_.handleEvent(e);
        if (input_.quitRequested())
            running_ = false;

        update(dt);
        render();
    }
    save();  // be kind on quit too
}

void Game::update(double dt) {
    if (input_.escapePressed())
        running_ = false;

    switch (state_) {
    case State::Exploring:
        if (input_.inventoryPressed()) {
            audio_.playSfx("confirm");
            state_ = State::InventoryOpen;
            return;
        }
        updateExploring(dt);
        break;
    case State::InDialogue:
        dialogue_.update(dt, input_);
        room_->update(dt);
        if (!dialogue_.active())
            state_ = State::Exploring;
        break;
    case State::InventoryOpen:
        if (input_.inventoryPressed() || input_.interactPressed())
            state_ = State::Exploring;
        break;
    }
}

void Game::updateExploring(double dt) {
    player_.update(dt, input_, *room_);
    room_->update(dt);
    pickupItemsUnderPlayer();

    // Standing on a door tile fires its transition.
    if (const auto* door = room_->doorAt(player_.tileX(), player_.tileY())) {
        std::string target = room_->transitionTo(door->dir);
        if (!target.empty()) {
            Direction travel = door->dir;
            enterRoom(target, &travel, /*autosave=*/true);
            return;
        }
    }

    if (input_.interactPressed())
        tryInteract();
}

void Game::tryInteract() {
    int tx, ty;
    player_.facingTile(tx, ty);
    Entity* e = room_->entityAt(tx, ty);
    if (!e)
        e = room_->entityAt(player_.tileX(), player_.tileY());
    if (!e) {
        audio_.playSfx("bump");
        return;
    }

    if (e->kind() == EntityKind::Npc) {
        const DialogueTree* tree = dialogueForNpc(e->id());
        if (!tree) {
            std::cerr << "[dialogue] no tree registered for npc '" << e->id() << "'\n";
            return;
        }
        DialogueBox::Effects fx;
        fx.hasFlag = [this](const std::string& f) { return hasFlag(f); };
        fx.setFlag = [this](const std::string& f) { setFlag(f); };
        fx.giveItem = [this](const std::string& id) {
            inventory_.add(id);
            setFlag("got_" + id);
            audio_.playSfx("pickup");
        };
        fx.onCharTyped = [this] { audio_.playSfx("blip"); };
        dialogue_.start(*tree, std::move(fx));
        state_ = State::InDialogue;
    }
}

void Game::pickupItemsUnderPlayer() {
    Entity* e = room_->entityAt(player_.tileX(), player_.tileY());
    if (e && e->kind() == EntityKind::Item) {
        std::string id = e->id();
        inventory_.add(id);
        setFlag("got_" + id);
        room_->removeEntity(id);
        audio_.playSfx("pickup");
        std::cout << "[game] picked up " << Inventory::displayName(id) << "\n";
    }
}

void Game::render() {
    const MoodPalette pal = paletteFor(room_->mood());
    renderer_.clear(pal.background);

    // Camera centered on the player, clamped to the room.
    int camX = static_cast<int>(player_.x()) + kTile / 2 - Renderer::kWindowW / 2;
    int camY = static_cast<int>(player_.y()) + kTile / 2 - Renderer::kViewH / 2;
    camX = std::clamp(camX, 0, std::max(0, room_->pixelWidth() - Renderer::kWindowW));
    camY = std::clamp(camY, 0, std::max(0, room_->pixelHeight() - Renderer::kViewH));
    if (room_->pixelWidth() < Renderer::kWindowW)
        camX = (room_->pixelWidth() - Renderer::kWindowW) / 2;
    if (room_->pixelHeight() < Renderer::kViewH)
        camY = (room_->pixelHeight() - Renderer::kViewH) / 2;
    renderer_.setCamera(camX, camY);

    renderWorld();

    if (state_ == State::InventoryOpen)
        inventory_.render(renderer_, assets_);
    if (dialogue_.active())
        dialogue_.render(renderer_, assets_);
    renderStatusBar();

    renderer_.present();
}

void Game::renderWorld() {
    const MoodPalette pal = paletteFor(room_->mood());

    // Tiles: real PNG if provided (tile_wall.png etc.), colored rects otherwise.
    for (int ty = 0; ty < room_->height(); ++ty) {
        for (int tx = 0; tx < room_->width(); ++tx) {
            Tile t = room_->tileAt(tx, ty);
            const char* spriteName = nullptr;
            SDL_Color c = pal.floor;
            switch (t) {
            case Tile::Wall:  spriteName = "tile_wall";  c = pal.wall; break;
            case Tile::Floor: spriteName = "tile_floor"; c = pal.floor; break;
            case Tile::Grass: spriteName = "tile_grass"; c = pal.grass; break;
            case Tile::Door:  spriteName = "tile_door";  c = pal.door; break;
            }
            if (!assets_.spriteIsPlaceholder(spriteName)) {
                renderer_.drawSprite(assets_.sprite(spriteName), tx * kTile, ty * kTile,
                                     kTile, kTile);
            } else {
                renderer_.fillRectWorld(tx * kTile, ty * kTile, kTile, kTile, c);
                if (t == Tile::Grass) {
                    // Swaying blades: three thin bars with a phase per column.
                    double sway = std::sin(worldTime_ * 2.0 + tx * 1.7 + ty * 0.9) * 2.0;
                    SDL_Color blade{static_cast<Uint8>(std::min(255, pal.grass.r + 30)),
                                    static_cast<Uint8>(std::min(255, pal.grass.g + 40)),
                                    static_cast<Uint8>(std::min(255, pal.grass.b + 30)), 255};
                    for (int b = 0; b < 3; ++b)
                        renderer_.fillRectWorld(tx * kTile + 6 + b * 9 + static_cast<int>(sway),
                                                ty * kTile + 8, 3, 18, blade);
                } else if (t == Tile::Door) {
                    double pulse = (std::sin(worldTime_ * 3.0) + 1.0) * 0.5;
                    SDL_Color glow{c.r, c.g, c.b, static_cast<Uint8>(90 + pulse * 100)};
                    renderer_.fillRectWorld(tx * kTile + 6, ty * kTile + 6,
                                            kTile - 12, kTile - 12, glow);
                }
            }
        }
    }

    // Entities.
    for (const auto& e : room_->entities()) {
        int px = e->tileX() * kTile;
        int py = e->tileY() * kTile;
        if (e->kind() == EntityKind::Npc) {
            double bob = std::sin(worldTime_ * 2.2 + e->tileX()) * 2.0;
            renderer_.drawSprite(assets_.sprite(e->sprite()), px,
                                 py + static_cast<int>(bob), kTile, kTile);
        } else {
            // Items hover and glimmer so they read as collectible.
            double hover = std::sin(worldTime_ * 3.0) * 3.0;
            renderer_.drawSprite(assets_.sprite(e->sprite()), px + 4,
                                 py + 4 + static_cast<int>(hover), kTile - 8, kTile - 8);
        }
    }

    // Player: directional frames if real art exists, bobbing fallback if not.
    std::string frame = player_.spriteName();
    int px = static_cast<int>(player_.x());
    int py = static_cast<int>(player_.y());
    if (!assets_.spriteIsPlaceholder(frame)) {
        renderer_.drawSprite(assets_.sprite(frame), px, py, kTile, kTile);
    } else {
        double bob = player_.moving() ? std::sin(player_.animTime() * 18.0) * 2.0 : 0.0;
        renderer_.drawSprite(assets_.sprite("player"), px,
                             py + static_cast<int>(bob), kTile, kTile);
        // Tiny facing tick so direction is readable on the plain square.
        int fx = px + kTile / 2 - 3, fy = py + kTile / 2 - 3;
        switch (player_.facing()) {
        case Facing::Down:  fy = py + kTile - 8; break;
        case Facing::Up:    fy = py + 2; break;
        case Facing::Left:  fx = px + 2; break;
        case Facing::Right: fx = px + kTile - 8; break;
        }
        renderer_.fillRectWorld(fx, fy + static_cast<int>(bob), 6, 6,
                                SDL_Color{255, 255, 255, 230});
    }
}

void Game::renderStatusBar() {
    const SDL_Rect bar{0, Renderer::kViewH, Renderer::kWindowW, Renderer::kStatusBarH};
    renderer_.fillRect(bar, SDL_Color{10, 6, 18, 255});
    renderer_.outlineRect(bar, SDL_Color{60, 40, 80, 255});

    int textY = bar.y + (Renderer::kStatusBarH - assets_.fontHeight()) / 2;
    renderer_.drawText(assets_, room_->name(), 14, textY, SDL_Color{255, 179, 71, 255});
    renderer_.drawText(assets_, "~ " + audio_.currentMood(),
                       14 + renderer_.textWidth(assets_, room_->name()) + 18, textY,
                       SDL_Color{140, 120, 170, 255});

    std::string hint;
    switch (state_) {
    case State::InDialogue:    hint = "E: continue"; break;
    case State::InventoryOpen: hint = "I: close"; break;
    case State::Exploring: {
        int tx, ty;
        player_.facingTile(tx, ty);
        const Entity* e = room_->entityAt(tx, ty);
        hint = (e && e->kind() == EntityKind::Npc) ? "E: talk" : "I: inventory";
        break;
    }
    }
    std::string items = std::to_string(inventory_.items().size()) + " item"
                        + (inventory_.items().size() == 1 ? "" : "s");
    std::string right = items + "   " + hint;
    renderer_.drawText(assets_, right,
                       Renderer::kWindowW - renderer_.textWidth(assets_, right) - 14,
                       textY, SDL_Color{200, 190, 215, 255});
}
