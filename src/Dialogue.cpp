#include "Dialogue.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

#include "Assets.h"
#include "Input.h"
#include "Renderer.h"

// --- Dialogue content (extract to files later) -------------------------------

namespace {

const std::unordered_map<std::string, DialogueTree>& dialogueRegistry() {
    static const std::unordered_map<std::string, DialogueTree> trees = [] {
        std::unordered_map<std::string, DialogueTree> t;

        t["npc_mother"] = DialogueTree{
            {
                {"MOM", "You slept through the whole afternoon, sweetheart. "
                        "The Strip is already lighting up out there.", 1, {}, "talked_mother", ""},
                {"MOM", "Your father used to say the neon looks like the city "
                        "apologizing for the desert. Do you miss him too?", -1,
                 {{"Every day.", 2}, {"I don't want to talk about it.", 3}}, "", ""},
                {"MOM", "Me too. Every single day. Go on, get some air -- "
                        "just be back before the lights go out.", -1, {}, "misses_father", ""},
                {"MOM", "That's alright. The feeling waits for you either way. "
                        "Go on, get some air.", -1, {}, "", ""},
            },
            0, "talked_mother", 4,
        };
        t["npc_mother"].nodes.push_back(
            {"MOM", "Still here? The casino floor never closes, and neither "
                    "does a mother's worrying.", -1, {}, "", ""});

        t["npc_dealer"] = DialogueTree{
            {
                {"DEALER", "This hallway is longer than it looks, kid. "
                           "Everything in this town is longer than it looks.", 1, {}, "met_dealer", ""},
                {"DEALER", "House always wins, they say. But the house never "
                           "says sorry. Remember that when you reach the garden.", -1, {}, "", ""},
            },
            0, "met_dealer", 2,
        };
        t["npc_dealer"].nodes.push_back(
            {"DEALER", "Cards down, kid. The garden is waiting east of nowhere.", -1, {}, "", ""});

        t["npc_shadow"] = DialogueTree{
            {
                {"???", "We can't stop here. This is bat country.", 1, {}, "met_shadow", ""},
                {"???", "...but you already stopped, didn't you. Something in "
                        "the grass is waiting for you. It has your handwriting.", -1, {}, "", ""},
            },
            0, "got_apology_note", 2,
        };
        t["npc_shadow"].nodes.push_back(
            {"???", "You found it. Some words weigh more than the whole "
                    "desert. Carry it gently.", -1, {}, "", ""});

        return t;
    }();
    return trees;
}

// Greedy word wrap against a pixel width.
std::vector<std::string> wrapText(Assets& assets, const std::string& text, int maxWidth) {
    std::vector<std::string> lines;
    std::istringstream ss(text);
    std::string word, current;
    while (ss >> word) {
        std::string candidate = current.empty() ? word : current + " " + word;
        if (!current.empty() && assets.measureText(candidate) > maxWidth) {
            lines.push_back(current);
            current = word;
        } else {
            current = std::move(candidate);
        }
    }
    if (!current.empty())
        lines.push_back(current);
    return lines;
}

}  // namespace

const DialogueTree* dialogueForNpc(const std::string& npcId) {
    const auto& reg = dialogueRegistry();
    auto it = reg.find(npcId);
    return it != reg.end() ? &it->second : nullptr;
}

// --- DialogueBox -------------------------------------------------------------

void DialogueBox::start(const DialogueTree& tree, Effects effects) {
    tree_ = &tree;
    effects_ = std::move(effects);
    int startNode = tree.start;
    if (!tree.altFlag.empty() && tree.altStart >= 0 && effects_.hasFlag
        && effects_.hasFlag(tree.altFlag))
        startNode = tree.altStart;
    enterNode(startNode);
}

void DialogueBox::enterNode(int index) {
    if (!tree_ || index < 0 || index >= static_cast<int>(tree_->nodes.size())) {
        tree_ = nullptr;
        return;
    }
    nodeIndex_ = index;
    shownChars_ = 0;
    lastBlipAt_ = 0;
    choiceIndex_ = 0;
    const auto& n = tree_->nodes[index];
    if (!n.setFlag.empty() && effects_.setFlag)
        effects_.setFlag(n.setFlag);
    if (!n.giveItem.empty() && effects_.giveItem)
        effects_.giveItem(n.giveItem);
}

void DialogueBox::update(double dt, const Input& input) {
    if (!tree_) return;
    const auto& n = node();
    const int total = static_cast<int>(n.text.size());
    const bool typing = shownChars_ < total;

    if (typing) {
        shownChars_ = std::min<double>(total, shownChars_ + dt * kCharsPerSec);
        int typed = static_cast<int>(shownChars_);
        if (typed / 3 > lastBlipAt_ / 3 && effects_.onCharTyped)
            effects_.onCharTyped();
        lastBlipAt_ = typed;
        if (input.interactPressed())
            shownChars_ = total;  // skip
        return;
    }

    if (!n.choices.empty()) {
        if (input.upPressed())
            choiceIndex_ = (choiceIndex_ + static_cast<int>(n.choices.size()) - 1)
                           % static_cast<int>(n.choices.size());
        if (input.downPressed())
            choiceIndex_ = (choiceIndex_ + 1) % static_cast<int>(n.choices.size());
        if (input.interactPressed())
            enterNode(n.choices[choiceIndex_].next);
    } else if (input.interactPressed()) {
        enterNode(n.next);
    }
}

void DialogueBox::render(Renderer& r, Assets& assets) const {
    if (!tree_) return;
    const auto& n = node();

    const int boxH = 140;
    const SDL_Rect box{12, Renderer::kViewH - boxH - 12, Renderer::kWindowW - 24, boxH};
    r.fillRect(box, SDL_Color{16, 10, 28, 235});
    r.outlineRect(box, SDL_Color{214, 62, 106, 255});  // neon pink trim

    int y = box.y + 10;
    if (!n.speaker.empty()) {
        r.drawText(assets, n.speaker, box.x + 14, y, SDL_Color{255, 179, 71, 255});
        y += assets.fontHeight() + 4;
    }

    std::string visible = n.text.substr(0, static_cast<size_t>(shownChars_));
    for (const auto& line : wrapText(assets, visible, box.w - 28)) {
        r.drawText(assets, line, box.x + 14, y, SDL_Color{235, 231, 240, 255});
        y += assets.fontHeight() + 2;
    }

    const bool done = shownChars_ >= static_cast<double>(n.text.size());
    if (done && !n.choices.empty()) {
        for (size_t i = 0; i < n.choices.size(); ++i) {
            bool sel = static_cast<int>(i) == choiceIndex_;
            std::string label = (sel ? "> " : "  ") + n.choices[i].label;
            r.drawText(assets, label, box.x + 28, y,
                       sel ? SDL_Color{255, 179, 71, 255} : SDL_Color{160, 150, 175, 255});
            y += assets.fontHeight() + 2;
        }
    } else if (done) {
        r.drawText(assets, "[E]", box.x + box.w - 44, box.y + box.h - assets.fontHeight() - 8,
                   SDL_Color{214, 62, 106, 255});
    }
}
