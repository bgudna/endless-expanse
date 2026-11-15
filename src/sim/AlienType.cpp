#include "sim/AlienType.h"

#include <nlohmann/json.hpp>

namespace ee {

namespace {
bool parseRole(const std::string& s, Role& out) {
    if (s == "raider") out = Role::Raider;
    else if (s == "trader") out = Role::Trader;
    else if (s == "passer") out = Role::Passer;
    else if (s == "distress") out = Role::Distress;
    else return false;
    return true;
}

bool parseTint(const std::string& s, Tint& out) {
    if (s == "cyan") out = Tint::Cyan;
    else if (s == "amber") out = Tint::Amber;
    else if (s == "phosphor") out = Tint::Phosphor;
    else if (s == "red") out = Tint::Red;
    else if (s == "white") out = Tint::White;
    else return false;
    return true;
}
} // namespace

const char* roleName(Role r) {
    switch (r) {
    case Role::Raider: return "RAIDER";
    case Role::Trader: return "TRADER";
    case Role::Passer: return "PASSER";
    case Role::Distress: return "DISTRESS";
    }
    return "?";
}

std::vector<AlienType> parseAlienTypes(const std::string& text, std::string* error) {
    using nlohmann::json;
    std::vector<AlienType> out;
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg;
        out.clear();
        return out;
    };
    try {
        const json doc = json::parse(text);
        if (!doc.contains("aliens") || !doc["aliens"].is_array()) return fail("missing \"aliens\" array");
        for (const json& j : doc["aliens"]) {
            AlienType t;
            t.id = j.at("id").get<std::string>();
            t.name = j.value("name", t.id);
            if (!parseRole(j.at("role").get<std::string>(), t.role)) return fail("bad role in " + t.id);
            if (!parseTint(j.value("tint", std::string("red")), t.tint)) return fail("bad tint in " + t.id);
            t.shape = j.value("shape", 0);
            t.radius = j.value("radius", t.radius);
            t.maxSpeed = j.value("max_speed", t.maxSpeed);
            t.thrust = j.value("thrust", t.thrust);
            t.turnRate = j.value("turn_rate", t.turnRate);
            t.hull = j.value("hull", t.hull);
            t.shield = j.value("shield", t.shield);
            t.damage = j.value("damage", t.damage);
            t.cooldown = j.value("cooldown", t.cooldown);
            t.range = j.value("range", t.range);
            t.projectileSpeed = j.value("projectile_speed", t.projectileSpeed);
            t.spread = j.value("spread", t.spread);
            t.sight = j.value("sight", t.sight);
            t.weight = j.value("weight", t.weight);
            t.minDifficulty = j.value("min_difficulty", t.minDifficulty);
            if (t.hull <= 0.0f || t.radius <= 0.0f || t.maxSpeed <= 0.0f || t.thrust <= 0.0f ||
                t.turnRate <= 0.0f || t.cooldown <= 0.0f)
                return fail("non-positive stat in " + t.id);
            out.push_back(std::move(t));
        }
    } catch (const std::exception& e) {
        return fail(std::string("json error: ") + e.what());
    }
    if (out.empty()) return fail("no alien types defined");
    return out;
}

} // namespace ee
