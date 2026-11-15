#pragma once

#include "world/Sector.h"

#include <string>
#include <vector>

namespace ee {

enum class Role { Raider, Trader, Passer, Distress };

// Data-driven alien ship definition (assets/data/aliens.json).
struct AlienType {
    std::string id;
    std::string name;
    Role role = Role::Raider;
    int shape = 0; // index into the procedural alien shapes
    Tint tint = Tint::Red;
    float radius = 20.0f;
    float maxSpeed = 300.0f;
    float thrust = 250.0f;
    float turnRate = 3.0f;
    float hull = 40.0f;
    float shield = 0.0f;
    float damage = 5.0f; // per shot; 0 = unarmed
    float cooldown = 0.6f;
    float range = 500.0f;
    float projectileSpeed = 700.0f;
    float spread = 0.1f; // radians of aim error
    float sight = 1500.0f;
    float weight = 1.0f;
    float minDifficulty = 1.0f;
};

// Parses the JSON text. On failure returns an empty list and fills `error`.
std::vector<AlienType> parseAlienTypes(const std::string& jsonText, std::string* error = nullptr);

const char* roleName(Role r);

} // namespace ee
