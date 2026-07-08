#include "Player.h"

#include <cmath>

#include "Input.h"
#include "Room.h"

namespace {
// Collision box inset within the 32px sprite: forgiving corners, feet-anchored.
constexpr double kInsetX = 5;
constexpr double kInsetTop = 12;
constexpr double kInsetBottom = 2;
}

void Player::setTilePosition(int tx, int ty) {
    x_ = tx * kTile;
    y_ = ty * kTile;
}

int Player::tileX() const { return static_cast<int>((x_ + kTile / 2) / kTile); }
int Player::tileY() const { return static_cast<int>((y_ + kTile / 2) / kTile); }

void Player::facingTile(int& tx, int& ty) const {
    tx = tileX();
    ty = tileY();
    switch (facing_) {
    case Facing::Down:  ++ty; break;
    case Facing::Up:    --ty; break;
    case Facing::Left:  --tx; break;
    case Facing::Right: ++tx; break;
    }
}

bool Player::collides(double nx, double ny, const Room& room) const {
    const double left = nx + kInsetX;
    const double right = nx + kTile - kInsetX - 1;
    const double top = ny + kInsetTop;
    const double bottom = ny + kTile - kInsetBottom - 1;
    for (double px : {left, right})
        for (double py : {top, bottom})
            if (room.blocksMovement(static_cast<int>(px / kTile),
                                    static_cast<int>(py / kTile)))
                return true;
    return false;
}

void Player::update(double dt, const Input& input, const Room& room) {
    double dx = 0, dy = 0;
    if (input.moveLeft())  dx -= 1;
    if (input.moveRight()) dx += 1;
    if (input.moveUp())    dy -= 1;
    if (input.moveDown())  dy += 1;

    moving_ = (dx != 0 || dy != 0);
    if (moving_) {
        if (dx != 0 && dy != 0) {
            dx *= M_SQRT1_2;
            dy *= M_SQRT1_2;
        }
        // Facing follows the dominant axis; horizontal wins ties.
        if (std::abs(dx) >= std::abs(dy) && dx != 0)
            facing_ = dx > 0 ? Facing::Right : Facing::Left;
        else if (dy != 0)
            facing_ = dy > 0 ? Facing::Down : Facing::Up;

        // Move per axis so we slide along walls instead of sticking.
        double nx = x_ + dx * kSpeed * dt;
        if (!collides(nx, y_, room)) x_ = nx;
        double ny = y_ + dy * kSpeed * dt;
        if (!collides(x_, ny, room)) y_ = ny;

        animTime_ += dt;
        walkFrame_ = static_cast<int>(animTime_ / 0.18) % 2;
    } else {
        animTime_ = 0;
        walkFrame_ = 0;
    }
}

std::string Player::spriteName() const {
    const char* dir = "down";
    switch (facing_) {
    case Facing::Down:  dir = "down"; break;
    case Facing::Up:    dir = "up"; break;
    case Facing::Left:  dir = "left"; break;
    case Facing::Right: dir = "right"; break;
    }
    return std::string("player_") + dir + "_" + std::to_string(walkFrame_);
}
