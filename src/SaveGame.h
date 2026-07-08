#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// Snapshot of persistent state. Written as JSON to
// ~/Library/Application Support/sorry/save.json (macOS).
struct SaveData {
    std::string room = "bedroom";
    double playerX = 0;
    double playerY = 0;
    std::vector<std::string> inventory;
    std::unordered_set<std::string> flags;
};

class SaveGame {
public:
    SaveGame();  // resolves + creates the save directory

    const std::string& path() const { return path_; }

    bool write(const SaveData& data) const;
    std::optional<SaveData> read() const;  // nullopt if missing or unparsable

private:
    std::string path_;
};
