// NEW: Utils/Interner.h
#pragma once
#include <string>
#include <unordered_set>
#include <string_view>

class StringInterner {
public:
    // Retourne un std::string_view stable tant que l'interner vit
    std::string_view intern(std::string_view s) {
        auto it = pool_.find(std::string(s));
        if (it != pool_.end()) return *it;
        auto [ins, ok] = pool_.insert(std::string(s));
        return *ins;
    }
private:
    std::unordered_set<std::string> pool_;
};
