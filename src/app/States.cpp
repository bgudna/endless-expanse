#include "app/States.h"

#include "core/Random.h"
#include "vector/Shapes.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace ee {

namespace {
constexpr int kMaxRocks = 3000;
constexpr int kRockStep = 50;
constexpr int kStarCount = 300;
constexpr int kRockVariants = 8;
} // namespace

// ---- Boot ---------------------------------------------------------------
void BootState::update(double dt, StateStack& stack) {
    elapsed_ += dt;
    if (elapsed_ >= 0.5) stack.replace(std::make_unique<MainMenuState>(r_));
}
void BootState::render(double) {}

// ---- Main menu ----------------------------------------------------------
MainMenuState::MainMenuState(Renderer& r) : r_(r), ship_(shapes::ship()) {}

void MainMenuState::handleInput(const InputState& input, StateStack& stack) {
    if (input.pressed[InputState::Left]) stack.replace(std::make_unique<PlayingState>(r_));
}

void MainMenuState::update(double dt, StateStack&) {
    prevTime_ = time_;
    time_ += dt;
}

void MainMenuState::render(double alpha) {
    const double t = prevTime_ + (time_ - prevTime_) * alpha;
    const glm::vec2 size = r_.logicalSize();
    const glm::vec2 center = size * 0.5f;

    r_.lines().setView(r_.screenView());
    r_.lines().shape(ship_, Transform2D{.position = center + glm::vec2(0.0f, 20.0f),
                                        .rotation = static_cast<float>(t) * 0.6f,
                                        .scale = 60.0f},
                     colors::phosphor, 2.0f);

    r_.drawTextCentered("ENDLESS EXPANSE", center - glm::vec2(0.0f, 140.0f), 54.0f, colors::cyan,
                        2.5f);
    if (std::fmod(t, 1.2) < 0.8)
        r_.drawTextCentered("CLICK TO START", center + glm::vec2(0.0f, 140.0f), 20.0f,
                            colors::amber, 1.8f);
}

// ---- Playing ------------------------------------------------------------
PlayingState::PlayingState(Renderer& r)
    : r_(r), ship_(shapes::ship()), planet_(shapes::planet()), star_(shapes::starCross()) {
    for (int i = 0; i < kRockVariants; ++i)
        rockShapes_.push_back(shapes::asteroid(static_cast<std::uint64_t>(i) + 1));

    Random rng(12345);
    for (int i = 0; i < kStarCount; ++i)
        stars_.push_back({{rng.range(-1500.0f, 1500.0f), rng.range(-900.0f, 900.0f)},
                          rng.range(2.0f, 6.0f), rng.range(0.3f, 1.0f)});
    for (int i = 0; i < kMaxRocks; ++i)
        rocks_.push_back({{rng.range(-700.0f, 700.0f), rng.range(-400.0f, 400.0f)},
                          rng.range(8.0f, 28.0f), rng.range(-1.0f, 1.0f),
                          static_cast<int>(rng.nextU64() % kRockVariants)});
}

void PlayingState::handleInput(const InputState& input, StateStack& stack) {
    mouse_ = {input.mouseX, input.mouseY};
    if (input.pressed[InputState::Left]) r_.setBloomQuality(nextQuality(r_.bloomQuality()));
    if (input.pressed[InputState::Right]) stack.push(std::make_unique<PausedState>(r_));
    if (input.wheelY != 0.0f) {
        const int delta = static_cast<int>(std::lround(input.wheelY)) * kRockStep;
        rockCount_ = std::clamp(rockCount_ + delta, 0, kMaxRocks);
    }
}

void PlayingState::update(double dt, StateStack&) {
    prevSimTime_ = simTime_;
    simTime_ += dt;
}

void PlayingState::render(double alpha) {
    const double t = prevSimTime_ + (simTime_ - prevSimTime_) * alpha;
    LineBatch& lines = r_.lines();
    lines.setView(r_.worldView());

    for (const Star& s : stars_)
        lines.shape(star_, Transform2D{.position = s.pos, .scale = s.size},
                    colors::scaled(colors::white, s.brightness * 0.6f), 1.0f);

    lines.shape(planet_, Transform2D{.position = {-250.0f, -120.0f}, .scale = 90.0f}, colors::cyan,
                2.0f);

    for (int i = 0; i < rockCount_; ++i) {
        const Rock& rock = rocks_[static_cast<std::size_t>(i)];
        lines.shape(rockShapes_[static_cast<std::size_t>(rock.variant)],
                    Transform2D{.position = rock.pos,
                                .rotation = static_cast<float>(t) * rock.spin,
                                .scale = rock.radius},
                    colors::amber, 1.5f);
    }

    // The ship sits at the world origin and turns to face the mouse.
    const glm::vec2 toMouse = mouse_ - r_.logicalSize() * 0.5f;
    lines.shape(ship_, Transform2D{.rotation = std::atan2(toMouse.y, toMouse.x), .scale = 28.0f},
                colors::phosphor, 2.5f);

    const std::string hint = "LEFT CLICK: BLOOM " + std::string(toString(r_.bloomQuality())) +
                             "   WHEEL: ROCKS " + std::to_string(rockCount_) +
                             "   RIGHT CLICK: PAUSE";
    r_.drawText(hint, {16.0f, r_.logicalSize().y - 28.0f}, 12.0f, colors::scaled(colors::cyan, 0.8f),
                1.2f);
}

// ---- Paused -------------------------------------------------------------
void PausedState::handleInput(const InputState& input, StateStack& stack) {
    if (input.pressed[InputState::Left]) stack.pop();
    if (input.pressed[InputState::Right]) {
        stack.clear();
        stack.push(std::make_unique<MainMenuState>(r_));
    }
}

void PausedState::render(double) {
    const glm::vec2 center = r_.logicalSize() * 0.5f;
    r_.drawTextCentered("PAUSED", center - glm::vec2(0.0f, 30.0f), 48.0f, colors::red, 2.5f);
    r_.drawTextCentered("LEFT CLICK: RESUME   RIGHT CLICK: MAIN MENU", center + glm::vec2(0.0f, 40.0f),
                        14.0f, colors::white, 1.4f);
}

} // namespace ee
