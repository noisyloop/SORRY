#include "Room.h"

Direction oppositeOf(Direction d) {
    switch (d) {
    case Direction::North: return Direction::South;
    case Direction::South: return Direction::North;
    case Direction::East:  return Direction::West;
    case Direction::West:  return Direction::East;
    }
    return Direction::South;
}

const char* directionName(Direction d) {
    switch (d) {
    case Direction::North: return "north";
    case Direction::South: return "south";
    case Direction::East:  return "east";
    case Direction::West:  return "west";
    }
    return "?";
}

Room::Room(std::string name, int width, int height, std::vector<Tile> tiles)
    : name_(std::move(name)), width_(width), height_(height), tiles_(std::move(tiles)) {
    // Classify each door tile by which map edge it sits closest to.
    for (int ty = 0; ty < height_; ++ty) {
        for (int tx = 0; tx < width_; ++tx) {
            if (tileAt(tx, ty) != Tile::Door) continue;
            Direction dir;
            if (ty == 0) dir = Direction::North;
            else if (ty == height_ - 1) dir = Direction::South;
            else if (tx == 0) dir = Direction::West;
            else if (tx == width_ - 1) dir = Direction::East;
            else dir = (ty < height_ / 2) ? Direction::North : Direction::South;
            doors_.push_back({tx, ty, dir});
        }
    }
}

Tile Room::tileAt(int tx, int ty) const {
    if (tx < 0 || ty < 0 || tx >= width_ || ty >= height_)
        return Tile::Wall;
    return tiles_[static_cast<size_t>(ty) * width_ + tx];
}

bool Room::blocksMovement(int tx, int ty) const {
    if (tileAt(tx, ty) == Tile::Wall)
        return true;
    if (const Entity* e = entityAt(tx, ty))
        return e->solid();
    return false;
}

const Room::DoorTile* Room::doorFacing(Direction d) const {
    for (const auto& door : doors_)
        if (door.dir == d) return &door;
    return nullptr;
}

const Room::DoorTile* Room::doorAt(int tx, int ty) const {
    for (const auto& door : doors_)
        if (door.tx == tx && door.ty == ty) return &door;
    return nullptr;
}

std::string Room::transitionTo(Direction d) const {
    auto it = transitions_.find(d);
    return it != transitions_.end() ? it->second : std::string{};
}

void Room::removeEntity(const std::string& id) {
    std::erase_if(entities_, [&](const auto& e) { return e->id() == id; });
}

Entity* Room::entityAt(int tx, int ty) const {
    for (const auto& e : entities_)
        if (e->tileX() == tx && e->tileY() == ty)
            return e.get();
    return nullptr;
}

void Room::update(double dt) {
    for (auto& e : entities_)
        e->update(dt);
}

void Room::defaultSpawn(int& tx, int& ty) const {
    for (int ty2 = 1; ty2 < height_ - 1; ++ty2)
        for (int tx2 = 1; tx2 < width_ - 1; ++tx2)
            if (tileAt(tx2, ty2) == Tile::Floor && !entityAt(tx2, ty2)) {
                tx = tx2; ty = ty2;
                return;
            }
    tx = width_ / 2;
    ty = height_ / 2;
}
