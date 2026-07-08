#pragma once

#include <string>

enum class EntityKind { Npc, Item, Trigger };

// Anything that occupies a tile: NPCs, items, triggers.
class Entity {
public:
    Entity(std::string id, EntityKind kind, int tileX, int tileY, std::string sprite)
        : id_(std::move(id)), kind_(kind), tileX_(tileX), tileY_(tileY),
          sprite_(std::move(sprite)) {}
    virtual ~Entity() = default;

    const std::string& id() const { return id_; }
    EntityKind kind() const { return kind_; }
    int tileX() const { return tileX_; }
    int tileY() const { return tileY_; }
    const std::string& sprite() const { return sprite_; }

    // Solid entities block movement; items are walk-over.
    virtual bool solid() const { return kind_ == EntityKind::Npc; }
    virtual void update(double dt) { (void)dt; }

private:
    std::string id_;
    EntityKind kind_;
    int tileX_;
    int tileY_;
    std::string sprite_;
};
