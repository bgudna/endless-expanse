#pragma once

#include "core/Random.h"
#include "sim/Alien.h"
#include "sim/Director.h"
#include "sim/Ship.h"
#include "sim/Spatial.h"
#include "sim/Vitals.h"
#include "world/Galaxy.h"
#include "world/SectorStreamer.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace ee {

struct Projectile {
    glm::vec2 pos{0.0f};
    glm::vec2 vel{0.0f};
    float life = 1.0f;
    float damage = 5.0f;
    int owner = 0; // 0 = player, otherwise the alien id
};

enum class SimEventType { Explosion, Hit, AlienDestroyed, PlayerDestroyed, Message, EncounterStarted };

struct SimEvent {
    SimEventType type = SimEventType::Message;
    glm::vec2 pos{0.0f}; // origin-relative at the time of the event
    float value = 0.0f;  // damage / blast radius
    std::string text;
};

struct PlayerWeapon {
    float damage = 12.0f;
    float cooldown = 0.35f;
    float range = 750.0f;
    float projectileSpeed = 950.0f;
};

// The complete headless game simulation: world streaming, the player ship, aliens and
// their AI, projectiles, hazards, collisions, and the encounter director. No SDL/GL.
// Positions are origin-relative (see D14).
class Simulation {
public:
    static constexpr float kPlayerRadius = 14.0f;

    Simulation(std::uint64_t galaxySeed, std::vector<AlienType> types, DirectorConfig cfg = {});
    Simulation(const Simulation&) = delete;
    Simulation& operator=(const Simulation&) = delete;

    void update(double dt);
    // Absolute sector box (inclusive) the caller wants loaded (the view); the 3x3 around the
    // player is always loaded.
    void setStreamBox(SectorCoord lo, SectorCoord hi) {
        lo_ = lo;
        hi_ = hi;
    }

    // ---- player ----
    Ship& player() { return player_; }
    const Ship& player() const { return player_; }
    Vitals& vitals() { return vitals_; }
    const Vitals& vitals() const { return vitals_; }
    bool playerAlive() const { return playerAlive_; }
    bool playerInNebula() const { return inNebula_; }
    bool playerInRadiation() const { return inRadiation_; }
    void setGodMode(bool on) { godMode_ = on; }
    const PlayerWeapon& weapon() const { return weapon_; }

    // ---- world ----
    const Galaxy& galaxy() const { return galaxy_; }
    SectorCoord origin() const { return origin_; }
    const SectorStreamer& streamer() const { return streamer_; }
    bool isDestroyed(SectorCoord c, std::size_t index) const {
        return destroyed_.count({c.x, c.y, static_cast<int>(index)}) != 0;
    }
    glm::dvec2 absolutePosition(glm::vec2 local) const {
        return glm::dvec2(origin_.x, origin_.y) * static_cast<double>(kSectorSize) + glm::dvec2(local);
    }
    // Origin-relative shift applied since the last call (non-zero right after a rebase), so the
    // caller can shift its own world-space state (camera, particles, ...).
    glm::vec2 takeOriginShift() {
        const glm::vec2 s = pendingShift_;
        pendingShift_ = glm::vec2(0.0f);
        return s;
    }

    // ---- aliens and combat ----
    const std::vector<AlienShip>& aliens() const { return aliens_; }
    const AlienShip* findAlien(int id) const;
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    void setTarget(int alienId) { target_ = alienId; }
    int target() const { return target_; }
    void setWeaponsFree(bool on) { weaponsFree_ = on; }
    bool weaponsFree() const { return weaponsFree_; }

    // The alien currently waiting for an answer to its hail, or null.
    const AlienShip* hailingAlien() const { return findAlien(hailingId_); }
    void answerHail(bool accept);

    // ---- encounters / stats ----
    const EncounterDirector& director() const { return director_; }
    // Starts an encounter immediately (used by the director and by tests). Returns false if
    // nothing could be spawned.
    bool startEncounter(EncounterKind kind);
    int encountersStarted() const { return encountersStarted_; }
    int encountersOfKind(EncounterKind k) const { return perKind_[static_cast<std::size_t>(k)]; }
    int kills() const { return kills_; }
    double time() const { return time_; }

    std::vector<SimEvent> takeEvents() {
        std::vector<SimEvent> e;
        e.swap(events_);
        return e;
    }

private:
    struct ZoneResult {
        bool nebula = false;
        bool radiation = false;
    };

    SectorCoord sectorOf(glm::vec2 p) const;
    const SectorGrid& gridFor(SectorCoord c, const Sector& s);
    void resolveStatic(Ship& ship, float radius, Vitals& vitals);
    ZoneResult applyZones(Ship& ship, Vitals& vitals, float radius, double dt);
    void detonate(SectorCoord c, std::size_t index, glm::vec2 center);
    void updateProjectiles(double dt);
    bool projectileHitsStatic(const Projectile& p);
    void firePlayer(const AlienShip& target);
    void spawnProjectile(glm::vec2 pos, glm::vec2 dir, float speed, float range, float damage, int owner);
    void updateAliensAndCleanup(double dt);
    void rebaseOrigin();
    const AlienType* pickType(Role role, float difficulty);
    AlienShip& spawnAlien(const AlienType& type, glm::vec2 pos, int encounterId);
    float difficulty() const;
    void emit(SimEventType type, glm::vec2 pos, float value, std::string text = {});
    void message(std::string text) { emit(SimEventType::Message, player_.position(), 0.0f, std::move(text)); }

    Galaxy galaxy_;
    SectorStreamer streamer_;
    SectorCoord origin_;
    SectorCoord lo_, hi_;
    std::unordered_map<SectorCoord, SectorGrid, SectorCoordHash> grids_;
    std::set<std::array<int, 3>> destroyed_;
    std::vector<std::uint16_t> scratch_;

    std::vector<AlienType> types_;
    EncounterDirector director_;
    Random rng_;

    Ship player_;
    Vitals vitals_;
    PlayerWeapon weapon_;
    float playerCooldown_ = 0.0f;
    bool playerAlive_ = true;
    bool godMode_ = false;
    bool inNebula_ = false;
    bool inRadiation_ = false;

    std::vector<AlienShip> aliens_;
    std::vector<Projectile> projectiles_;
    int nextAlienId_ = 1;
    int nextEncounterId_ = 1;
    int target_ = -1;
    int hailingId_ = -1;
    bool weaponsFree_ = false;

    glm::vec2 pendingShift_{0.0f};
    std::vector<SimEvent> events_;
    int encountersStarted_ = 0;
    std::array<int, 5> perKind_{};
    int kills_ = 0;
    double time_ = 0.0;
    std::uint64_t stepCount_ = 0;
};

} // namespace ee
