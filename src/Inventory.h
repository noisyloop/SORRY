#pragma once

#include <string>
#include <vector>

class Renderer;
class Assets;

// Ordered list of item ids with a display-name registry.
class Inventory {
public:
    static std::string displayName(const std::string& itemId);

    bool has(const std::string& itemId) const;
    void add(const std::string& itemId);  // ignores duplicates
    const std::vector<std::string>& items() const { return items_; }
    void setItems(std::vector<std::string> items) { items_ = std::move(items); }

    void render(Renderer& r, Assets& assets) const;

private:
    std::vector<std::string> items_;
};
