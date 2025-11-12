#ifndef BALL_H_
#define BALL_H_

#include <string>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#define GLM_FORCE_RADIANS
#include <glm/gtc/quaternion.hpp>

#include "resource.h"
#include "scene_node.h"

namespace game {

    // Ball class, derived from SceneNode, for billiards balls
    class Ball : public SceneNode {
    public:
        // Declare constructor (definition in ball.cpp)
        Ball(const std::string& name, const Resource *geometry, const Resource *material);

        virtual ~Ball();

        // Update called by scene; override if needed
        virtual void Update(void) override;

        // Velocity
        glm::vec3 GetVelocity() const { return velocity_; }
        void SetVelocity(const glm::vec3& v) { velocity_ = v; }

        // Pocketed state
        bool IsPocketed() const { return pocketed_; }
        void SetPocketed(bool p) { pocketed_ = p; }

        // Ball base radius (before scaling)
        float GetBaseRadius() const { return base_radius_; }
        void SetBaseRadius(float r) { base_radius_ = r; }

    private:
        glm::vec3 velocity_;
        bool pocketed_;
        float base_radius_;
    };

} // namespace game

#endif // BALL_H_