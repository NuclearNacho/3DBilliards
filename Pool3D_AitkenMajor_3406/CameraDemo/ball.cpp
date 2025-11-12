#include "ball.h"

namespace game {

Ball::Ball(const std::string& name, const Resource *geometry, const Resource *material)
    : SceneNode(name, geometry, material), velocity_(0.0f), pocketed_(false), base_radius_(1.0f) {
}

Ball::~Ball() {
}

void Ball::Update(void) {
    // No per-frame update here; physics and transforms are driven by the Game's physics step.
    // Keep as a no-op to avoid duplicating state changes.
}

} // namespace game
