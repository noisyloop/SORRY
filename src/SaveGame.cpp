#include "SaveGame.h"

#include <SDL.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace {

std::string saveDirectory() {
#ifdef __APPLE__
    if (const char* home = std::getenv("HOME"))
        return std::string(home) + "/Library/Application Support/sorry";
#endif
    // Elsewhere let SDL pick the platform-appropriate pref dir.
    if (char* pref = SDL_GetPrefPath("", "sorry")) {
        std::string dir(pref);
        SDL_free(pref);
        if (!dir.empty() && (dir.back() == '/' || dir.back() == '\\'))
            dir.pop_back();
        return dir;
    }
    return ".";
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\t': out += "\\t"; break;
        default: out += c;
        }
    }
    return out;
}

// Minimal JSON reader — just enough for the subset save files use
// (flat object with strings, numbers, and arrays of strings).
class JsonReader {
public:
    explicit JsonReader(std::string text) : s_(std::move(text)) {}

    bool parseSave(SaveData& out) {
        skipWs();
        if (!consume('{')) return false;
        if (peek() == '}') return true;
        while (true) {
            std::string key;
            if (!parseString(key) || !consumeWs(':')) return false;
            if (key == "room") {
                if (!parseString(out.room)) return false;
            } else if (key == "player_x") {
                if (!parseNumber(out.playerX)) return false;
            } else if (key == "player_y") {
                if (!parseNumber(out.playerY)) return false;
            } else if (key == "inventory") {
                std::vector<std::string> v;
                if (!parseStringArray(v)) return false;
                out.inventory = std::move(v);
            } else if (key == "flags") {
                std::vector<std::string> v;
                if (!parseStringArray(v)) return false;
                out.flags = {v.begin(), v.end()};
            } else {
                if (!skipValue()) return false;  // forward compatibility
            }
            skipWs();
            if (consume(',')) continue;
            return consume('}');
        }
    }

private:
    char peek() { skipWs(); return pos_ < s_.size() ? s_[pos_] : '\0'; }
    void skipWs() { while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_; }
    bool consume(char c) { skipWs(); if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; } return false; }
    bool consumeWs(char c) { return consume(c); }

    bool parseString(std::string& out) {
        if (!consume('"')) return false;
        out.clear();
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return true;
            if (c == '\\' && pos_ < s_.size()) {
                char e = s_[pos_++];
                switch (e) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                default: out += e;
                }
            } else {
                out += c;
            }
        }
        return false;
    }

    bool parseNumber(double& out) {
        skipWs();
        size_t start = pos_;
        while (pos_ < s_.size() && (std::isdigit(static_cast<unsigned char>(s_[pos_]))
                                    || s_[pos_] == '-' || s_[pos_] == '+'
                                    || s_[pos_] == '.' || s_[pos_] == 'e' || s_[pos_] == 'E'))
            ++pos_;
        if (start == pos_) return false;
        try {
            out = std::stod(s_.substr(start, pos_ - start));
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    bool parseStringArray(std::vector<std::string>& out) {
        if (!consume('[')) return false;
        if (consume(']')) return true;
        while (true) {
            std::string item;
            if (!parseString(item)) return false;
            out.push_back(std::move(item));
            if (consume(',')) continue;
            return consume(']');
        }
    }

    bool skipValue() {
        char c = peek();
        if (c == '"') { std::string tmp; return parseString(tmp); }
        if (c == '[') { std::vector<std::string> tmp; return parseStringArray(tmp); }
        double n;
        return parseNumber(n);
    }

    std::string s_;
    size_t pos_ = 0;
};

}  // namespace

SaveGame::SaveGame() {
    std::string dir = saveDirectory();
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec)
        std::cerr << "[save] could not create " << dir << ": " << ec.message() << "\n";
    path_ = dir + "/save.json";
}

bool SaveGame::write(const SaveData& data) const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"room\": \"" << jsonEscape(data.room) << "\",\n";
    json << "  \"player_x\": " << data.playerX << ",\n";
    json << "  \"player_y\": " << data.playerY << ",\n";
    json << "  \"inventory\": [";
    for (size_t i = 0; i < data.inventory.size(); ++i)
        json << (i ? ", " : "") << '"' << jsonEscape(data.inventory[i]) << '"';
    json << "],\n";
    json << "  \"flags\": [";
    size_t i = 0;
    for (const auto& f : data.flags)
        json << (i++ ? ", " : "") << '"' << jsonEscape(f) << '"';
    json << "]\n}\n";

    std::ofstream out(path_, std::ios::trunc);
    if (!out) {
        std::cerr << "[save] cannot write " << path_ << "\n";
        return false;
    }
    out << json.str();
    return static_cast<bool>(out);
}

std::optional<SaveData> SaveGame::read() const {
    std::ifstream in(path_);
    if (!in)
        return std::nullopt;
    std::stringstream buf;
    buf << in.rdbuf();
    SaveData data;
    JsonReader reader(buf.str());
    if (!reader.parseSave(data)) {
        std::cerr << "[save] " << path_ << " is corrupt; starting fresh\n";
        return std::nullopt;
    }
    return data;
}
