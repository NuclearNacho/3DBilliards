#ifndef GAME_H_
#define GAME_H_

#include <exception>
#include <string>
#include <vector>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "scene_graph.h"
#include "resource_manager.h"
#include "camera.h"
#include "ball.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace game {

    // Exception type for the game
    class GameException : public std::exception
    {
    private:
        std::string message_;
    public:
        GameException(std::string message) : message_(message) {};
        virtual const char* what() const throw() { return message_.c_str(); };
        virtual ~GameException() throw() {};
    };

    // Game application
    class Game {

    public:
        // Constructor and destructor
        Game(void);
        ~Game();
        // Call Init() before calling any other method
        void Init(void);
        // Set up resources for the game
        void SetupResources(void);
        // Set up initial scene
        void SetupScene(void);
        // Run the game: keep the application active
        void MainLoop(void);

    private:
        // GLFW window
        GLFWwindow* window_;

        // Scene graph containing all nodes to render
        SceneGraph scene_;

        // Resources available to the game
        ResourceManager resman_;

        // Camera abstraction
        Camera camera_;

        // Flag to turn animation on/off
        bool animating_;

        // White cue ball (player)
        Ball* white_ball_;

        // First-person camera flag (camera follows white ball)
        bool first_person_;

        // Free-camera flag (toggle with C) - free movement without moving white ball
        bool free_camera_;

        // Stored third-person camera pose (so user can set up third-person, switch to FP to shoot, then restore)
        glm::vec3 stored_third_pos_;
        glm::quat stored_third_ori_;
        bool has_stored_third_;

        // (Optional) store FP pose before switching to third so toggles can restore FP if needed
        glm::vec3 stored_fp_pos_;
        glm::quat stored_fp_ori_;
        bool has_stored_fp_;
        // Stored FP forward vector (so shots taken after switching to 3rd still use FP aim)
        glm::vec3 stored_fp_forward_;
        bool has_stored_fp_forward_;

        // Show the white ball after player pressed a power key (1-9)
        bool show_white_on_shot_;

        // Scene graph camera node (optional)
        SceneNode* camera_node_;

        // All balls (including white_ball_ in the vector or stored separately)
        std::vector<Ball*> balls_;

        // Pocket positions and pocket radius multiplier
        std::vector<glm::vec3> pockets_;
        float pocket_radius_multiplier_;

        // Fixed-step physics
        const float physics_dt_ = 1.0f / 120.0f;
        float physics_accumulator_;

        // Physics activity tracking: true while any ball moves above threshold
        bool physics_active_;
        // Threshold (units/sec) under which velocities are considered stopped
        float physics_stop_threshold_;

        // World bounds (cube half-extent)
        float world_half_extent_;

        // Uniform linear deceleration for balls (units/sec^2)
        float linear_deceleration_;

        // Camera free-move parameters
        float camera_move_speed_;
        float camera_rotate_speed_deg_;

        // Helpers: create ball instances and fields
        Ball* CreateBallInstance(std::string entity_name, std::string object_name, std::string material_name);
        void CreateBallField(int num_balls = 15);
        // Update white-ball visibility according to current camera mode / pocketed state
        void UpdateWhiteVisibility(void);

        // Physics update step
        void UpdatePhysicsStep(float dt);

        // Continuous input handling
        void ProcessContinuousInput(float dt);

        // Apply shot impulse to white ball. If override_dir != nullptr, use that direction (world-space).
        void ShootWhiteBall(float power, const glm::vec3* override_dir = nullptr);

        // Methods to initialize the game
        void InitWindow(void);
        void InitView(void);
        void InitEventHandlers(void);

        // Methods to handle events (single-press)
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void ResizeCallback(GLFWwindow* window, int width, int height);

        // Tracer scene node (single instance reused each frame)
        SceneNode* tracer_node_;

        // Tracer parameters (world units)
        float tracer_length_;      // how far the tracer extends    
        float tracer_thickness_;   // cross-section size
        // Debug: when true the code in MainLoop will force-draw the tracer in front of the camera.
        // Keep false for normal gameplay to avoid overdraw/hangs.
        bool tracer_debug_draw_;
        // Update tracer each frame
        void UpdateTracer();

        // Extracted helpers (new)
        // Physics helpers
        void HandlePocketDetection(Ball* b);
        void HandleBallBallCollisions();
        void RespawnWhiteBallIfPocketed();

        // Tracer helpers: compute target and configure the tracer node
        // Returns true if a tracer should be shown and fills outContact/outResultantDir/outPredictedVelLen.
        bool ComputeTracerTarget(Ball*& outTarget, glm::vec3& outContact, glm::vec3& outResultantDir, float& outPredictedVelLen);
        void ConfigureTracerNode(const glm::vec3& contact, const glm::vec3& resultantDir, float length);

    }; // class Game

} // namespace game

#endif // GAME_H_