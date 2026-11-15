#include "sim/ExhaustTrail.h"

namespace ee {

void ExhaustTrail::emit(glm::vec2 pos) {
    if (!points_.empty() && glm::distance(points_.front().pos, pos) < kSpacing) return;
    points_.push_front(TrailPoint{pos, 0.0f});
    if (points_.size() > kMaxPoints) points_.pop_back();
}

void ExhaustTrail::update(float dt) {
    for (TrailPoint& p : points_) p.age += dt;
    while (!points_.empty() && points_.back().age >= kLife) points_.pop_back();
}

} // namespace ee
