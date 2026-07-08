#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Entity.h"

enum class Tile : char {
    Wall = '#',
    Floor = '.',
    Grass = '~',
    Door = '>',
};

enum class Direction { North, South, East, West };
Direction oppositeOf(Direction d);
const char* directionName(Direction d);

// A room: tile grid + entities + music mood + transitions.
class Room {
public:
    static constexpr int kTile = 32;

    Room(std::string name, int width, int height, std::vector<Tile> tiles);

    const std::string& name() const { return name_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int pixelWidth() const { return width_ * kTile; }
    int pixelHeight() const { return height_ * kTile; }

    Tile tileAt(int tx, int ty) const;
    bool blocksMovement(int tx, int ty) const;  // walls, bounds, solid entities

    // Doors map to a compass direction from their position on the map edge.
    struct DoorTile { int tx, ty; Direction dir; };
    const std::vector<DoorTile>& doors() const { return doors_; }
    const DoorTile* doorFacing(Direction d) const;
    const DoorTile* doorAt(int tx, int ty) const;

    void setMood(std::string m) { mood_ = std::move(m); }
    const std::string& mood() const { return mood_; }

    void setOnEnterFlag(std::string f) { onEnterFlag_ = std::move(f); }
    const std::string& onEnterFlag() const { return onEnterFlag_; }

    void setTransition(Direction d, std::string room) { transitions_[d] = std::move(room); }
    // Empty string if there is no transition that way.
    std::string transitionTo(Direction d) const;

    void addEntity(std::unique_ptr<Entity> e) { entities_.push_back(std::move(e)); }
    void removeEntity(const std::string& id);
    Entity* entityAt(int tx, int ty) const;
    const std::vector<std::unique_ptr<Entity>>& entities() const { return entities_; }

    void update(double dt);

    // First walkable tile, used as a spawn fallback.
    void defaultSpawn(int& tx, int& ty) const;

private:
    std::string name_;
    int width_;
    int height_;
    std::vector<Tile> tiles_;
    std::vector<DoorTile> doors_;
    std::string mood_ = "calm_pentatonic";
    std::string onEnterFlag_;
    std::unordered_map<Direction, std::string> transitions_;
    std::vector<std::unique_ptr<Entity>> entities_;
};
