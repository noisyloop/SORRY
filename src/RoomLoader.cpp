#include "RoomLoader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "NPC.h"

namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& s) {
    auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// Parses "0:npc_mother 1:npc_shadow" into slot -> id.
std::unordered_map<int, std::string> parseSlotList(const std::string& value) {
    std::unordered_map<int, std::string> out;
    std::istringstream ss(value);
    std::string tok;
    while (ss >> tok) {
        auto colon = tok.find(':');
        if (colon == std::string::npos) continue;
        try {
            out[std::stoi(tok.substr(0, colon))] = tok.substr(colon + 1);
        } catch (const std::exception&) {
            std::cerr << "[rooms] bad slot entry '" << tok << "'\n";
        }
    }
    return out;
}

bool parseDirection(const std::string& s, Direction& out) {
    if (s == "north") out = Direction::North;
    else if (s == "south") out = Direction::South;
    else if (s == "east") out = Direction::East;
    else if (s == "west") out = Direction::West;
    else return false;
    return true;
}

}  // namespace

std::unique_ptr<Room> RoomLoader::load(const std::string& name) const {
    fs::path mapPath = fs::path(root_) / "maps" / (name + ".txt");
    std::ifstream mapFile(mapPath);
    if (!mapFile) {
        std::cerr << "[rooms] missing map file " << mapPath << "\n";
        return nullptr;
    }

    std::vector<std::string> lines;
    std::string line;
    size_t width = 0;
    while (std::getline(mapFile, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        if (line.empty() && lines.empty()) continue;  // leading blank lines
        lines.push_back(line);
        width = std::max(width, line.size());
    }
    while (!lines.empty() && lines.back().empty())
        lines.pop_back();
    if (lines.empty() || width == 0) {
        std::cerr << "[rooms] map " << mapPath << " is empty\n";
        return nullptr;
    }

    const int w = static_cast<int>(width);
    const int h = static_cast<int>(lines.size());
    std::vector<Tile> tiles(static_cast<size_t>(w) * h, Tile::Floor);
    struct Slot { int tx, ty; };
    std::vector<Slot> npcSlots, itemSlots;

    for (int ty = 0; ty < h; ++ty) {
        for (int tx = 0; tx < w; ++tx) {
            char c = tx < static_cast<int>(lines[ty].size()) ? lines[ty][tx] : '#';
            Tile t = Tile::Floor;
            switch (c) {
            case '#': t = Tile::Wall; break;
            case '.': t = Tile::Floor; break;
            case '~': t = Tile::Grass; break;
            case '>': t = Tile::Door; break;
            case 'N': npcSlots.push_back({tx, ty}); break;
            case 'I': itemSlots.push_back({tx, ty}); break;
            case ' ': t = Tile::Wall; break;  // padding outside the room
            default:
                std::cerr << "[rooms] " << name << ": unknown tile '" << c
                          << "' at " << tx << "," << ty << ", treating as floor\n";
            }
            tiles[static_cast<size_t>(ty) * w + tx] = t;
        }
    }

    auto room = std::make_unique<Room>(name, w, h, std::move(tiles));

    // Meta file is optional but a room without one gets defaults only.
    fs::path metaPath = fs::path(root_) / "maps" / (name + ".meta");
    std::ifstream metaFile(metaPath);
    if (!metaFile) {
        std::cerr << "[rooms] note: no meta file for room '" << name << "'\n";
        return room;
    }

    std::unordered_map<int, std::string> npcIds, itemIds;
    while (std::getline(metaFile, line)) {
        std::string t = trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;
        auto eq = t.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(t.substr(0, eq));
        std::string value = trim(t.substr(eq + 1));

        if (key == "music_mood") {
            room->setMood(value);
        } else if (key == "on_enter_flag") {
            room->setOnEnterFlag(value);
        } else if (key == "transitions") {
            std::istringstream ss(value);
            std::string tok;
            while (ss >> tok) {
                auto colon = tok.find(':');
                Direction dir;
                if (colon != std::string::npos && parseDirection(tok.substr(0, colon), dir))
                    room->setTransition(dir, tok.substr(colon + 1));
                else
                    std::cerr << "[rooms] " << name << ": bad transition '" << tok << "'\n";
            }
        } else if (key == "npcs") {
            npcIds = parseSlotList(value);
        } else if (key == "items") {
            itemIds = parseSlotList(value);
        } else {
            std::cerr << "[rooms] " << name << ": unknown meta key '" << key << "'\n";
        }
    }

    for (size_t i = 0; i < npcSlots.size(); ++i) {
        auto it = npcIds.find(static_cast<int>(i));
        if (it == npcIds.end()) {
            std::cerr << "[rooms] " << name << ": npc slot " << i << " has no id in meta\n";
            continue;
        }
        room->addEntity(std::make_unique<NPC>(it->second, npcSlots[i].tx, npcSlots[i].ty));
    }
    for (size_t i = 0; i < itemSlots.size(); ++i) {
        auto it = itemIds.find(static_cast<int>(i));
        if (it == itemIds.end()) {
            std::cerr << "[rooms] " << name << ": item slot " << i << " has no id in meta\n";
            continue;
        }
        room->addEntity(std::make_unique<Entity>(it->second, EntityKind::Item,
                                                 itemSlots[i].tx, itemSlots[i].ty,
                                                 it->second));
    }
    return room;
}
