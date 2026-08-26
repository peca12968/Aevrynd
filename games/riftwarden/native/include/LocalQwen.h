#pragma once
#include <optional>
#include <string>

struct BattleState { int wave{},lives{},gold{},towers{},kills{}; };
struct GeneratedEvent { std::string title,description,type; float magnitude{.2f}; int duration{10}; };
class LocalQwen {
public:
    std::optional<GeneratedEvent> Generate(const BattleState&) const;
};
