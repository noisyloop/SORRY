#pragma once

#include <functional>
#include <string>
#include <vector>

class Renderer;
class Assets;
class Input;

// --- Data model -------------------------------------------------------------

struct DialogueChoice {
    std::string label;
    int next = -1;  // node index, -1 ends the conversation
};

struct DialogueNode {
    std::string speaker;
    std::string text;
    int next = -1;                        // ignored when choices are present
    std::vector<DialogueChoice> choices;  // shown once the text finishes
    std::string setFlag;                  // story flag set when node is shown
    std::string giveItem;                 // item id granted when node is shown
};

struct DialogueTree {
    std::vector<DialogueNode> nodes;
    int start = 0;
    // If this flag is already set, start at altStart instead (repeat visits).
    std::string altFlag;
    int altStart = -1;
};

// Trees live in code for now; looked up by NPC id. Null if unknown.
const DialogueTree* dialogueForNpc(const std::string& npcId);

// --- Runtime box ------------------------------------------------------------

// Typewriter dialogue box at the bottom of the screen.
// Interact advances (or skips the typewriter); up/down pick a choice.
class DialogueBox {
public:
    struct Effects {
        std::function<bool(const std::string&)> hasFlag;
        std::function<void(const std::string&)> setFlag;
        std::function<void(const std::string&)> giveItem;
        std::function<void()> onCharTyped;  // e.g. play a blip every few chars
    };

    void start(const DialogueTree& tree, Effects effects);
    bool active() const { return tree_ != nullptr; }

    void update(double dt, const Input& input);
    void render(Renderer& r, Assets& assets) const;

private:
    void enterNode(int index);
    const DialogueNode& node() const { return tree_->nodes[nodeIndex_]; }

    const DialogueTree* tree_ = nullptr;
    Effects effects_;
    int nodeIndex_ = 0;
    double shownChars_ = 0;
    int lastBlipAt_ = 0;
    int choiceIndex_ = 0;

    static constexpr double kCharsPerSec = 40.0;
};
