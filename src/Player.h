#pragma once

#include <string>

class Room;
class Input;

enum class Facing { Down, Up, Left, Right };

// Pixel-position player with tile collision, 4-direction facing and a
// 2-frame walk cycle.
class Player {
public:
    static constexpr int kTile = 32;
    static constexpr double kSpeed = 150.0;  // px/sec

    void setTilePosition(int tx, int ty);
    void setPixelPosition(double x, double y) { x_ = x; y_ = y; }

    void update(double dt, const Input& input, const Room& room);

    double x() const { return x_; }
    double y() const { return y_; }
    int tileX() const;
    int tileY() const;
    // The tile the player is facing — where interactions land.
    void facingTile(int& tx, int& ty) const;

    Facing facing() const { return facing_; }
    bool moving() const { return moving_; }
    int walkFrame() const { return walkFrame_; }
    double animTime() const { return animTime_; }

    // "player_down_0", or "player" if directional frames are absent.
    std::string spriteName() const;

private:
    bool collides(double nx, double ny, const Room& room) const;

    double x_ = 0, y_ = 0;  // top-left of the sprite, pixels
    Facing facing_ = Facing::Down;
    bool moving_ = false;
    int walkFrame_ = 0;
    double animTime_ = 0;
};
