#pragma once

#include "Entity.h"

// An NPC's dialogue tree is looked up by its id in the dialogue registry
// (see Dialogue.h / dialogueForNpc). Sprite name matches the id so art can
// be dropped in as assets/sprites/<id>.png.
class NPC : public Entity {
public:
    NPC(std::string id, int tileX, int tileY)
        : Entity(id, EntityKind::Npc, tileX, tileY, id) {}

    void update(double dt) override;
    double idleTime() const { return idleTime_; }  // drives the idle bob

private:
    double idleTime_ = 0;
};
