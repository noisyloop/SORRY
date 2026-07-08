#pragma once

#include <memory>
#include <string>

#include "Room.h"

// Loads assets/maps/<name>.txt (tile grid) + assets/maps/<name>.meta
// (key=value: music_mood, transitions, npcs, items, on_enter_flag).
//
// Map characters:
//   # wall   . floor   ~ tall grass   > door   N npc-slot   I item-slot
// N and I slots are numbered 0,1,2... in reading order and bound to ids via
// the meta file, e.g.  npcs = 0:npc_mother 1:npc_shadow
class RoomLoader {
public:
    explicit RoomLoader(std::string assetRoot) : root_(std::move(assetRoot)) {}

    // Returns null (with a std::cerr explanation) if the map is missing/bad.
    std::unique_ptr<Room> load(const std::string& name) const;

private:
    std::string root_;
};
