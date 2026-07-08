#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include "Assets.h"
#include "Audio.h"
#include "Dialogue.h"
#include "Input.h"
#include "Inventory.h"
#include "Player.h"
#include "Renderer.h"
#include "Room.h"
#include "RoomLoader.h"
#include "SaveGame.h"

// Top-level game: owns every subsystem, runs the loop, switches rooms.
class Game {
public:
    Game();  // throws std::runtime_error if a critical asset is missing
    void run();

private:
    enum class State { Title, Exploring, InDialogue, InventoryOpen };

    // fromDir is the direction the player travelled to get here (spawns them
    // at the matching door); nullptr = use explicit/default position.
    void enterRoom(const std::string& name, const Direction* fromDir,
                   bool autosave);
    void save();
    void update(double dt);
    void updateTitle(double dt);
    void updateExploring(double dt);
    void tryInteract();
    void pickupItemsUnderPlayer();
    void render();
    void renderTitle();
    void renderWorld();
    void renderStatusBar();

    bool hasFlag(const std::string& f) const { return flags_.contains(f); }
    void setFlag(const std::string& f) { flags_.insert(f); }

    Renderer renderer_;
    Assets assets_;
    Audio audio_;
    Input input_;
    RoomLoader roomLoader_;
    SaveGame saveGame_;

    Player player_;
    Inventory inventory_;
    DialogueBox dialogue_;
    std::unique_ptr<Room> room_;
    std::unordered_set<std::string> flags_;

    State state_ = State::Title;
    bool running_ = true;
    double worldTime_ = 0;   // drives ambient animation
    double titleHold_ = 0;   // seconds any key has been held on the title screen

    static constexpr double kTitleHoldSeconds = 0.8;
};
