#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_RADIANS

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <time.h>
#include <sstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <random>

#include "ball.h"
#include "game.h"
#include "bin/path_config.h"


namespace game {
    // Some configuration constants 
    const std::string window_title_g = "Billiards in Space";
    const unsigned int window_width_g = 800;
    const unsigned int window_height_g = 600;
    const bool window_full_screen_g = false;

    // Viewport and camera settings
    float camera_near_clip_distance_g = 0.01;
    float camera_far_clip_distance_g = 1000.0;
    float camera_fov_g = 50.0; // Field-of-view of camera
    const glm::vec3 viewport_background_color_g(0.4f, 0.4f, 0.4f); // grey background (walls/skybox)
    glm::vec3 camera_position_g(0.0f, 0.0f, 800.0f);
    glm::vec3 camera_look_at_g(0.0f, 0.0f, 0.0f);
    glm::vec3 camera_up_g(0.0f, 1.0f, 0.0f);

    // Materials 
    const std::string material_directory_g = MATERIAL_DIRECTORY;


    Game::Game(void) : window_(nullptr), animating_(true),
        white_ball_(nullptr), first_person_(true), free_camera_(false), show_white_on_shot_(false), camera_node_(nullptr),
        physics_accumulator_(0.0f), world_half_extent_(300.0f),
        linear_deceleration_(50.0f), // default decel, tuneable
        camera_move_speed_(200.0f), camera_rotate_speed_deg_(10.0f),
        pocket_radius_multiplier_(1.5f),
        has_stored_third_(false), has_stored_fp_(false), has_stored_fp_forward_(false),
        physics_active_(false), physics_stop_threshold_(0.01f),
        tracer_node_(nullptr), tracer_length_(400.0f), tracer_thickness_(5.0f),
        tracer_debug_draw_(false)
    {
        // Don't do heavy work in constructor; Init() will do it.
    }

    Game::~Game() {
        // Terminate GLFW and allow other destructors to run
        glfwTerminate();
    }

    Ball* Game::CreateBallInstance(std::string entity_name, std::string object_name, std::string material_name) {

        Resource* geom = resman_.GetResource(object_name);
        if (!geom) {
            throw(GameException(std::string("Could not find resource \"") + object_name + std::string("\"")));
        }

        Resource* mat = resman_.GetResource(material_name);
        if (!mat) {
            throw(GameException(std::string("Could not find resource \"") + material_name + std::string("\"")));
        }

        Ball* ball = new Ball(entity_name, geom, mat);
        scene_.AddNode(ball);
        balls_.push_back(ball);

        // Explicit base radius matching the sphere mesh (CreateColoredSphere used radius = 1.0f)
        ball->SetBaseRadius(1.0f);

        // Set a color hint on the ball so other systems can query a representative color.
        // We map known mesh name substrings to the colors used when creating colored spheres.
        glm::vec3 hintColor(1.0f, 1.0f, 1.0f);
        bool haveHint = true;
        if (object_name.find("Yellow") != std::string::npos) {
            hintColor = glm::vec3(1.0f, 1.0f, 0.0f);
        } else if (object_name.find("Blue") != std::string::npos) {
            hintColor = glm::vec3(0.0f, 0.0f, 1.0f);
        } else if (object_name.find("RedA") != std::string::npos) {
            hintColor = glm::vec3(1.0f, 0.0f, 0.0f);
        } else if (object_name.find("RedB") != std::string::npos) {
            hintColor = glm::vec3(0.6f, 0.0f, 0.0f);
        } else if (object_name.find("Purple") != std::string::npos) {
            hintColor = glm::vec3(0.5f, 0.0f, 0.5f);
        } else if (object_name.find("Orange") != std::string::npos) {
            hintColor = glm::vec3(1.0f, 0.5f, 0.0f);
        } else if (object_name.find("Green") != std::string::npos) {
            hintColor = glm::vec3(0.0f, 1.0f, 0.0f);
        } else if (object_name.find("Black") != std::string::npos) {
            hintColor = glm::vec3(0.0f, 0.0f, 0.0f);
        } else if (object_name.find("White") != std::string::npos) {
            hintColor = glm::vec3(1.0f, 1.0f, 1.0f);
        } else {
            haveHint = false;
        }
        if (haveHint) {
            // SceneNode::SetColorHint is inherited by Ball
            ball->SetColorHint(hintColor);
        }

        return ball;
    }


    void Game::Init(void) {

        // Run all initialization steps
        InitWindow();
        InitView();
        InitEventHandlers();

        // Set variables
        animating_ = true;
    }


    void Game::InitWindow(void) {

        if (!glfwInit()) {
            throw(GameException(std::string("Could not initialize the GLFW library")));
        }

        if (window_full_screen_g) {
            window_ = glfwCreateWindow(window_width_g, window_height_g, window_title_g.c_str(), glfwGetPrimaryMonitor(), NULL);
        }
        else {
            window_ = glfwCreateWindow(window_width_g, window_height_g, window_title_g.c_str(), NULL, NULL);
        }
        if (!window_) {
            glfwTerminate();
            throw(GameException(std::string("Could not create window")));
        }

        glfwMakeContextCurrent(window_);

        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            throw(GameException(std::string("Could not initialize the GLEW library: ") + std::string((const char*)glewGetErrorString(err))));
        }
    }


    void Game::InitView(void) {

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Note: tracer is rendered opaque for now — do not enable alpha blending here.
        // If you later want semi-transparent tracer, re-enable blending:
        // glEnable(GL_BLEND);
        // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        int width, height;
        glfwGetFramebufferSize(window_, &width, &height);
        glViewport(0, 0, width, height);

        camera_.SetView(camera_position_g, camera_look_at_g, camera_up_g);
        camera_.SetProjection(camera_fov_g, camera_near_clip_distance_g, camera_far_clip_distance_g, width, height);
    }


    void Game::InitEventHandlers(void) {

        glfwSetWindowUserPointer(window_, (void*)this);
        glfwSetKeyCallback(window_, KeyCallback);
        glfwSetFramebufferSizeCallback(window_, ResizeCallback);
    }


    void Game::SetupResources(void) {

        // Base sphere geometry is still useful but we'll create color-specific sphere meshes
        // Create colored sphere meshes for cue and other balls
        // Ball radius in game instances will be controlled by SetScale on the node (we use scale 10.0f in SetupScene)
        resman_.CreateColoredSphere("Sphere_White", glm::vec3(1.0f, 1.0f, 1.0f), false, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Black", glm::vec3(0.0f, 0.0f, 0.0f), false, 1.0f, 24, 24);

        // Solid colored balls
        resman_.CreateColoredSphere("Sphere_Yellow", glm::vec3(1.0f, 1.0f, 0.0f), false, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Blue", glm::vec3(0.0f, 0.0f, 1.0f), false, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_RedA", glm::vec3(1.0f, 0.0f, 0.0f), false, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Purple", glm::vec3(0.5f, 0.0f, 0.5f), false, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Orange", glm::vec3(1.0f, 0.5f, 0.0f), false, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Green", glm::vec3(0.0f, 1.0f, 0.0f), false, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_RedB", glm::vec3(0.6f, 0.0f, 0.0f), false, 1.0f, 24, 24);

        // Gradient (stripe-like) versions: same colors, interpolated toward white
        resman_.CreateColoredSphere("Sphere_Yellow_Stripe", glm::vec3(1.0f, 1.0f, 0.0f), true, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Blue_Stripe", glm::vec3(0.0f, 0.0f, 1.0f), true, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_RedA_Stripe", glm::vec3(1.0f, 0.0f, 0.0f), true, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Purple_Stripe", glm::vec3(0.5f, 0.0f, 0.5f), true, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Orange_Stripe", glm::vec3(1.0f, 0.5f, 0.0f), true, 1.0f, 24, 24);
        resman_.CreateColoredSphere("Sphere_Green_Stripe", glm::vec3(0.0f, 1.0f, 0.0f), true, 1.0f, 24, 24);
        // duplicate red stripe
        resman_.CreateColoredSphere("Sphere_RedB_Stripe", glm::vec3(0.6f, 0.0f, 0.0f), true, 1.0f, 24, 24);

        // Create pocket guide sphere geometry (darker grey)
        // We'll create a unit sphere (radius = 1.0) and scale instances to the world pocket radius in SetupScene.
        resman_.CreateColoredSphere("Pocket_Sphere", glm::vec3(0.3f, 0.3f, 0.3f), false, 1.0f, 16, 16);

        // Create a torus geometry resource is no longer needed here (was moved previously).
        // Load basic material (reused). NOTE: CreateColoredSphere baked vertex colors, so single material is fine.
        std::string filename = std::string(MATERIAL_DIRECTORY) + std::string("/material");
        resman_.LoadResource(Material, "ObjectMaterial", filename.c_str());

        // Create tracer box (unit depth = 1.0). We'll scale per-instance to desired length/thickness.
        resman_.CreateBox("Tracer", 0.05f, 0.05f, 1.0f); // thin tall box aligned along +Z
    }


    void Game::MainLoop(void) {

        double last_frame = glfwGetTime();
        while (!glfwWindowShouldClose(window_)) {
            double current_time = glfwGetTime();
            float dt = (float)(current_time - last_frame);
            if (dt <= 0.0f) dt = 0.0001f;
            last_frame = current_time;

            // Handle continuous input (camera movement, free-camera controls)
            ProcessContinuousInput(dt);

            // Fixed-step physics updates
            physics_accumulator_ += dt;
            while (physics_accumulator_ >= physics_dt_) {
                UpdatePhysicsStep(physics_dt_);
                physics_accumulator_ -= physics_dt_;
            }

            // Update scene at a lower rate if animating_ (keeps existing behaviour for other node updates)
            if (animating_) {
                static double last_time = 0;
                double current_time_anim = glfwGetTime();
                if ((current_time_anim - last_time) > 0.05) {
                    scene_.Update();
                    last_time = current_time_anim;
                }
            }

            // If in first-person mode, position camera at white ball
            if (first_person_ && white_ball_) {
                // camera follows white ball only in first-person mode
                glm::vec3 wbpos = white_ball_->GetPosition();
                camera_.SetPosition(wbpos);
                // keep camera orientation as-is (so player aims by rotating camera)
            }

            // Update tracer (visual aiming helper)
            UpdateTracer();

            // Draw the scene
            scene_.Draw(&camera_);

            // Debug-forward draw for tracer (only colored tracer now)
            if (tracer_node_ && tracer_node_->IsVisible() && tracer_debug_draw_) {
                // Save original transform so we can restore it after the debug draw
                glm::vec3 origPos = tracer_node_->GetPosition();
                glm::quat origOri = tracer_node_->GetOrientation();
                glm::vec3 origScale = tracer_node_->GetScale();

                // Force tracer to a visible place in front of camera for debugging:
                glm::vec3 camPos = camera_.GetPosition();
                glm::vec3 camFwd = glm::normalize(camera_.GetForward());
                glm::vec3 debugPos = camPos + camFwd * 50.0f; // 50 units in front of camera
                tracer_node_->SetPosition(debugPos);

                // Orient so +Z of box matches camera forward
                glm::vec3 localZ(0.0f, 0.0f, 1.0f);
                glm::vec3 axis = glm::cross(localZ, camFwd);
                float axisLen = glm::length(axis);
                glm::quat debugOri;
                float dot = glm::dot(localZ, camFwd);
                if (axisLen < 1e-6f) {
                    if (dot > 0.999999f) debugOri = glm::quat();
                    else debugOri = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
                }
                else {
                    axis = axis / axisLen;
                    float angle = acos(glm::clamp(dot, -1.0f, 1.0f));
                    debugOri = glm::angleAxis(angle, axis);
                }
                tracer_node_->SetOrientation(debugOri);

                // Make the tracer clearly visible: thin but long rectangle
                tracer_node_->SetScale(glm::vec3(10.0f, 10.0f, 200.0f));

                // Render tracer last so it's always visible (temporarily relax GL state)
                GLboolean depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
                GLboolean cull_was_enabled = glIsEnabled(GL_CULL_FACE);

                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                glm::mat4 identity(1.0f);
                tracer_node_->Draw(&camera_, identity);

                // Restore GL state
                if (cull_was_enabled) glEnable(GL_CULL_FACE);
                if (depth_was_enabled) glEnable(GL_DEPTH_TEST);

                // Restore original transform so normal tracer logic remains unchanged
                tracer_node_->SetPosition(origPos);
                tracer_node_->SetOrientation(origOri);
                tracer_node_->SetScale(origScale);
            }

            glfwSwapBuffers(window_);
            glfwPollEvents();
        }
    }


    void Game::CreateBallField(int num_balls) {

        // Prepare the ordered list of mesh names to create exactly 15 balls matching your specification:
        // 1 black solid + 7 solids (yellow, blue, red, purple, orange, green, red) + 7 gradient-to-white variants
        std::vector<std::string> meshNames;
        meshNames.push_back("Sphere_Black");
        meshNames.push_back("Sphere_Yellow");
        meshNames.push_back("Sphere_Blue");
        meshNames.push_back("Sphere_RedA");
        meshNames.push_back("Sphere_Purple");
        meshNames.push_back("Sphere_Orange");
        meshNames.push_back("Sphere_Green");
        meshNames.push_back("Sphere_RedB");
        // gradient counterparts (same order except black has no stripe)
        meshNames.push_back("Sphere_Yellow_Stripe");
        meshNames.push_back("Sphere_Blue_Stripe");
        meshNames.push_back("Sphere_RedA_Stripe");
        meshNames.push_back("Sphere_Purple_Stripe");
        meshNames.push_back("Sphere_Orange_Stripe");
        meshNames.push_back("Sphere_Green_Stripe");
        meshNames.push_back("Sphere_RedB_Stripe");

        // limit to requested number if necessary
        int createCount = std::min(num_balls, (int)meshNames.size());

        float cluster_radius = 60.0f;
        for (int i = 0; i < createCount; ++i) {
            std::stringstream ss;
            ss << i;
            std::string name = "BallInstance" + ss.str();

            // Pick the mesh name for this ball
            std::string meshName = meshNames[i];

            // Create ball instance using the color-specific mesh
            // CreateBallInstance expects object_name and material name; object_name must be a Mesh resource
            Ball* b = CreateBallInstance(name, meshName, "ObjectMaterial");

            // Place roughly in a spherical cluster
            float theta = 2.0f * glm::pi<float>() * ((float)rand() / RAND_MAX);
            float phi = acos(2.0f * ((float)rand() / RAND_MAX) - 1.0f);
            float r = cluster_radius * ((float)rand() / RAND_MAX);
            glm::vec3 pos(r * sin(phi) * cos(theta), r * sin(phi) * sin(theta), r * cos(phi));
            b->SetPosition(pos);
            b->SetScale(glm::vec3(10.0f)); // match white ball scale
            b->SetVelocity(glm::vec3(0.0f));
            b->SetPocketed(false);
        }
    }


    void Game::UpdatePhysicsStep(float dt) {

        // Utility lambdas
        auto apply_deceleration = [this](glm::vec3& v, float dt) {
            float speed = glm::length(v);
            if (speed <= 0.0f) return;
            float dec = linear_deceleration_ * dt;
            if (speed <= dec) {
                v = glm::vec3(0.0f);
            }
            else {
                v -= (v / speed) * dec;
            }
            };

        // Move balls according to velocity, handle wall collisions, pockets
        for (Ball* b : balls_) {
            if (!b || b->IsPocketed()) continue;
            glm::vec3 pos = b->GetPosition();
            glm::vec3 vel = b->GetVelocity();
            float radius = b->GetBaseRadius() * b->GetScale().x; // world radius

            // Integrate position
            pos += vel * dt;
            b->SetPosition(pos);

            // Wall collisions (cube bounds): reflect velocity if colliding the walls
            for (int axis = 0; axis < 3; ++axis) {
                if (pos[axis] - radius < -world_half_extent_) {
                    pos[axis] = -world_half_extent_ + radius;
                    vel[axis] = -vel[axis];
                }
                else if (pos[axis] + radius > world_half_extent_) {
                    pos[axis] = world_half_extent_ - radius;
                    vel[axis] = -vel[axis];
                }
            }
            b->SetPosition(pos);
            b->SetVelocity(vel);

            // Pocket-sphere collision moved to helper
            HandlePocketDetection(b);

            if (b->IsPocketed()) continue;

            // (Removed previous separate "torus" collision logic and the old per-ball pocket check;
            // pocket-sphere test above now handles removal for all balls including white.)


        }

        // If the white ball has been pocketed, try to respawn it somewhere non-colliding.
        RespawnWhiteBallIfPocketed();

        // Ball-ball collisions (pairwise) moved to helper
        HandleBallBallCollisions();

        // Apply uniform deceleration to all balls (simulate friction in space table)
        for (Ball* b : balls_) {
            if (!b || b->IsPocketed()) continue;
            glm::vec3 v = b->GetVelocity();
            apply_deceleration(v, dt);
            b->SetVelocity(v);
        }

        // Determine whether any balls are still moving above the stop threshold and update physics_active_
        {
            bool anyMoving = false;
            const float eps = physics_stop_threshold_;
            for (Ball* b : balls_) {
                if (!b || b->IsPocketed()) continue;
                if (glm::length(b->GetVelocity()) > eps) {
                    anyMoving = true;
                    break;
                }
            }
            physics_active_ = anyMoving;
        }
    }


    void Game::HandlePocketDetection(Ball* b) {
        if (!b || !white_ball_) return;

        glm::vec3 pos = b->GetPosition();
        float radius = b->GetBaseRadius() * b->GetScale().x;
        float white_ball_world_radius = white_ball_->GetScale().x * white_ball_->GetBaseRadius();
        // NOTE: pocket spheres are scaled 3x larger than the computed pocket radius in SetupScene
        float pocket_sphere_world = white_ball_world_radius * pocket_radius_multiplier_ * 3.0f; // matches SetupScene scale

        for (const auto& p : pockets_) {
            float d = glm::length(pos - p);
            if (d <= (pocket_sphere_world + radius)) {
                // collided with pocket guide sphere => remove from world
                b->SetVisible(false);
                b->SetPocketed(true);
                b->SetVelocity(glm::vec3(0.0f));
                break;
            }
        }
    }


    void Game::RespawnWhiteBallIfPocketed() {
        if (!white_ball_ || !white_ball_->IsPocketed()) return;

        // Parameters for respawn attempts
        const int maxAttempts = 64;
        const float clearance_margin = 0.1f; // extra gap to avoid grazes
        float white_r = white_ball_->GetBaseRadius() * white_ball_->GetScale().x;

        // Compute pocket avoidance radius similar to SetupScene (pocket spheres are 3x scaled)
        float ball_world_radius = white_ball_->GetScale().x * white_ball_->GetBaseRadius();
        float pocket_radius = ball_world_radius * pocket_radius_multiplier_ * 3.0f;

        // spawn region: keep within world bounds with a small margin
        float spawnLimit = world_half_extent_ - white_r - 1.0f;
        if (spawnLimit < 1.0f) spawnLimit = world_half_extent_; // fallback

        static thread_local std::mt19937 rng((unsigned)std::time(nullptr));
        std::uniform_real_distribution<float> dist(-spawnLimit, spawnLimit);

        bool placed = false;
        for (int attempt = 0; attempt < maxAttempts; ++attempt) {
            glm::vec3 candidate(dist(rng), dist(rng), dist(rng));

            // avoid pockets
            bool bad = false;
            for (const auto& p : pockets_) {
                if (glm::length(candidate - p) <= (pocket_radius + white_r + clearance_margin)) {
                    bad = true;
                    break;
                }
            }
            if (bad) continue;

            // ensure not colliding with any existing (non-pocketed) ball
            for (Ball* other : balls_) {
                if (!other || other == white_ball_ || other->IsPocketed()) continue;
                glm::vec3 op = other->GetPosition();
                float orad = other->GetBaseRadius() * other->GetScale().x;
                float minDist = white_r + orad + clearance_margin;
                if (glm::length(candidate - op) < minDist) {
                    bad = true;
                    break;
                }
            }
            if (bad) continue;

            // Found a free spot: place white ball here
            white_ball_->SetPosition(candidate);
            white_ball_->SetVelocity(glm::vec3(0.0f));
            white_ball_->SetPocketed(false);
            // Restore visibility according to current FP/TP state
            UpdateWhiteVisibility();
            placed = true;
            break;
        }

        if (!placed) {
            // Could not find a non-colliding spot after attempts: leave white ball pocketed/hidden.
            // Optionally log debug:
            std::cout << "[Game] Warning: could not find free spawn for white ball after " << maxAttempts << " attempts. Leaving pocketed.\n";
        }
    }


    void Game::HandleBallBallCollisions() {
        for (size_t i = 0; i < balls_.size(); ++i) {
            Ball* A = balls_[i];
            if (!A || A->IsPocketed()) continue;
            for (size_t j = i + 1; j < balls_.size(); ++j) {
                Ball* B = balls_[j];
                if (!B || B->IsPocketed()) continue;

                glm::vec3 pA = A->GetPosition();
                glm::vec3 pB = B->GetPosition();
                glm::vec3 d = pA - pB;
                float dist = glm::length(d);
                float rA = A->GetBaseRadius() * A->GetScale().x;
                float rB = B->GetBaseRadius() * B->GetScale().x;
                float minDist = rA + rB;
                if (dist <= 0.0f) continue;

                if (dist < minDist) {
                    // push apart to avoid sticking
                    glm::vec3 n = d / dist;
                    float penetration = minDist - dist;
                    pA += n * (penetration * 0.5f);
                    pB -= n * (penetration * 0.5f);
                    A->SetPosition(pA);
                    B->SetPosition(pB);

                    // compute relative velocity along normal
                    glm::vec3 vA = A->GetVelocity();
                    glm::vec3 vB = B->GetVelocity();
                    float rel = glm::dot(vA - vB, n);
                    if (rel > 0.0f) continue; // already separating

                    // Equal masses, elastic collision: exchange normal components
                    float p = rel;
                    vA = vA - n * p;
                    vB = vB + n * p;
                    A->SetVelocity(vA);
                    B->SetVelocity(vB);
                }
            }
        }
    }


    bool Game::ComputeTracerTarget(Ball*& outTarget, glm::vec3& outContact, glm::vec3& outResultantDir, float& outPredictedVelLen) {
        outTarget = nullptr;
        outPredictedVelLen = 0.0f;

        if (!tracer_node_ || !white_ball_) return false;

        // Use the player's current aiming direction.
        glm::vec3 origin = white_ball_->GetPosition();
        glm::vec3 aimDir;
        if (first_person_) {
            aimDir = camera_.GetForward();
        }
        else if (has_stored_fp_forward_) {
            aimDir = stored_fp_forward_;
        }
        else {
            aimDir = camera_.GetForward();
        }

        float aimLen = glm::length(aimDir);
        if (aimLen < 1e-6f) return false;
        aimDir = aimDir / aimLen;

        // Precompute white ball radius and velocity
        float white_r = white_ball_->GetBaseRadius() * white_ball_->GetScale().x;
        glm::vec3 white_v = white_ball_->GetVelocity();

        // If white is nearly stationary, use a preview incoming velocity for aiming display
        const float white_stationary_eps = 1e-3f;
        const float preview_speed = 300.0f; // tune: how "fast" preview shot looks (world units/sec)
        if (glm::length(white_v) < white_stationary_eps) {
            // Use aimDir so preview represents the direction you'd shoot from camera/aim
            white_v = aimDir * preview_speed;
        }

        float bestT = FLT_MAX;
        glm::vec3 bestContact(0.0f);
        Ball* bestTargetLocal = nullptr;

        // Ray-sphere intersection against an inflated target sphere (r_target + r_white)
        for (Ball* b : balls_) {
            if (!b || b->IsPocketed() || b == white_ball_) continue;

            glm::vec3 C = b->GetPosition();
            float r_target = b->GetBaseRadius() * b->GetScale().x;
            float inflated_r = r_target + white_r;

            // Solve |origin + t*aimDir - C|^2 = inflated_r^2
            glm::vec3 oc = origin - C;
            float a = glm::dot(aimDir, aimDir); // should be 1.0 since normalized, but keep general
            float bcoef = 2.0f * glm::dot(aimDir, oc);
            float ccoef = glm::dot(oc, oc) - inflated_r * inflated_r;
            float disc = bcoef * bcoef - 4.0f * a * ccoef;
            if (disc < 0.0f) continue;

            float sqrtD = std::sqrt(disc);
            float t1 = (-bcoef - sqrtD) / (2.0f * a);
            float t2 = (-bcoef + sqrtD) / (2.0f * a);

            float t = FLT_MAX;
            if (t1 > 1e-5f) t = t1;
            else if (t2 > 1e-5f) t = t2;
            else continue;

            if (t < bestT) {
                bestT = t;
                bestTargetLocal = b;
                bestContact = origin + aimDir * t; // collision point between surfaces (white center path)
            }
        }

        if (!bestTargetLocal) return false;

        // Compute collision normal at contact point (from target center toward contact)
        glm::vec3 C = bestTargetLocal->GetPosition();
        glm::vec3 contact = bestContact;
        glm::vec3 normal = contact - C;
        float nlen = glm::length(normal);
        if (nlen < 1e-6f) {
            // Degenerate: fallback to center-to-center
            normal = glm::normalize(C - origin);
        }
        else {
            normal = normal / nlen;
        }

        // For your physics (equal masses, elastic), the target's outgoing normal component equals
        // the white-target relative velocity projected onto the collision normal.
        // Compute relative velocity (white - target)
        glm::vec3 v_rel = white_v - bestTargetLocal->GetVelocity();
        float v_rel_along = glm::dot(v_rel, -normal); // negative because normal points from target->contact
        if (v_rel_along <= 1e-6f) return false;

        // target surface contact point (on target sphere) = center + normal * r_target
        float r_target = bestTargetLocal->GetBaseRadius() * bestTargetLocal->GetScale().x;
        glm::vec3 surfaceContact = C + normal * r_target;

        // Predicted target velocity (post-collision) = direction * magnitude
        glm::vec3 predicted_target_vel = (-normal) * v_rel_along; // outgoing direction points from white->target

        glm::vec3 resultantDir = glm::normalize(predicted_target_vel);

        outTarget = bestTargetLocal;
        outContact = surfaceContact;                 // <-- return surface contact, not white-center
        outResultantDir = resultantDir;
        outPredictedVelLen = glm::length(predicted_target_vel);
        return true;
    }


    void Game::ConfigureTracerNode(const glm::vec3& contact, const glm::vec3& resultantDir, float length) {
        if (!tracer_node_) return;

        // Place the tracer a bit in front of the contact along resultant direction (so it only extends forward)
        const float startOffset = 0.0f; // tunable offset so tracer begins clear of target
        glm::vec3 startPoint = contact + resultantDir * startOffset;

        // Camera safety (still hide if camera nearly at startPoint to avoid visual issues)
        glm::vec3 camPos = camera_.GetPosition();
        float distCameraToStart = glm::length(startPoint - camPos);
        const float cameraSafety = 0.5f; // hide tracer when camera nearly at startPoint
        if (distCameraToStart < cameraSafety) {
            tracer_node_->SetVisible(false);
            return;
        }

        // Cap length to space diagonal
        float maxSpaceDiagonal = 1500;
        float finalLength = std::min(length, maxSpaceDiagonal);
        if (finalLength < 1e-3f) {
            tracer_node_->SetVisible(false);
            return;
        }

        // Center tracer box so its back face sits at startPoint and it extends along resultantDir
        glm::vec3 nodePos = startPoint + resultantDir * (finalLength * 0.5f);

        // Orientation: rotate local +Z to align with resultantDir
        glm::vec3 localZ(0.0f, 0.0f, 1.0f);
        glm::vec3 axis = glm::cross(localZ, resultantDir);
        float axisLen = glm::length(axis);
        glm::quat ori;
        float dot = glm::dot(localZ, resultantDir);
        if (axisLen < 1e-6f) {
            if (dot > 0.999999f) ori = glm::quat();
            else ori = glm::angleAxis(glm::pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
        }
        else {
            axis = axis / axisLen;
            float angle = acos(glm::clamp(dot, -1.0f, 1.0f));
            ori = glm::angleAxis(angle, axis);
        }

        // Scale: base box is 0.05 x 0.05 x 1.0
        const float base_thickness = 0.05f;
        const float base_depth = 1.0f;
        float thickness = tracer_thickness_;
        float sx = (thickness / base_thickness);
        float sy = (thickness / base_thickness);
        float sz = (finalLength / base_depth);

        // Clamp scales to safe maxima
        const float maxScale = 2000.0f;
        sx = glm::clamp(sx, 0.001f, maxScale);
        sy = glm::clamp(sy, 0.001f, maxScale);
        sz = glm::clamp(sz, 0.001f, maxScale);

        tracer_node_->SetPosition(nodePos);
        tracer_node_->SetOrientation(ori);
        tracer_node_->SetScale(glm::vec3(sx, sy, sz));
        tracer_node_->SetVisible(true);
    }


    void Game::UpdateTracer() {
        if (!tracer_node_ || !white_ball_) {
            if (tracer_node_) {
                tracer_node_->SetVisible(false);
                tracer_node_->ClearOverrideColor();
            }
            return;
        }

        // Don't draw tracer while physics are active (balls moving)
        if (physics_active_) {
            tracer_node_->SetVisible(false);
            tracer_node_->ClearOverrideColor();
            return;
        }

        Ball* target = nullptr;
        glm::vec3 contact;
        glm::vec3 resultantDir;
        float predictedVelLen = 0.0f;

        if (!ComputeTracerTarget(target, contact, resultantDir, predictedVelLen)) {
            tracer_node_->SetVisible(false);
            tracer_node_->ClearOverrideColor();
            return;
        }

        // If we have a target and it exposes a color hint, apply it to the tracer node
        if (target && target->HasColorHint()) {
            glm::vec3 c = target->GetColorHint();
            tracer_node_->SetOverrideColor(c);
        }
        else {
            tracer_node_->ClearOverrideColor();
        }

        // Choose length: either based on predicted speed or configured tracer_length_
        float speedScale = 10.0f; // world units per (velocity unit) — tune this
        float length = glm::min(tracer_length_, predictedVelLen * speedScale);
        if (length < 1e-3f) length = tracer_length_;

        ConfigureTracerNode(contact, resultantDir, length);
    }


    void Game::UpdateWhiteVisibility(void) {
        if (!white_ball_) return;
        // White ball visible only when not in first-person and not pocketed
        bool visible = !white_ball_->IsPocketed() && !first_person_;
        white_ball_->SetVisible(visible);
    }


    void Game::ProcessContinuousInput(float dt) {

        if (!window_) return;

        // If right Alt is held, temporarily increase rotation speed
        bool alt_left = (glfwGetKey(window_, GLFW_KEY_LEFT_ALT) == GLFW_PRESS);
        float effective_rotate_speed_deg = camera_rotate_speed_deg_;
        if (alt_left) effective_rotate_speed_deg *= 5.0f;

        // Continuous movement applies only when in third-person (camera free)
        if (!first_person_) {
            // Movement (WASD + up/down)
            glm::vec3 move(0.0f);
            if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS) move += camera_.GetForward();
            if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS) move -= camera_.GetForward();
            if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS) move -= camera_.GetSide();
            if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS) move += camera_.GetSide();
            if (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS) move += camera_.GetUp();
            if (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) move -= camera_.GetUp();

            if (glm::length(move) > 0.0f) {
                move = glm::normalize(move) * camera_move_speed_ * dt;
                camera_.Translate(move);
            }

            // Rotation J/L yaw left/right
            bool j = (glfwGetKey(window_, GLFW_KEY_J) == GLFW_PRESS);
            bool l = (glfwGetKey(window_, GLFW_KEY_L) == GLFW_PRESS);
            if (j && !l) camera_.Yaw(glm::radians(effective_rotate_speed_deg * dt));
            else if (l && !j) camera_.Yaw(glm::radians(-effective_rotate_speed_deg * dt));

            // Look up/down I/K
            bool i = (glfwGetKey(window_, GLFW_KEY_I) == GLFW_PRESS);
            bool k = (glfwGetKey(window_, GLFW_KEY_K) == GLFW_PRESS);
            if (i && !k) camera_.Pitch(glm::radians(effective_rotate_speed_deg * dt));
            else if (k && !i) camera_.Pitch(glm::radians(-effective_rotate_speed_deg * dt));

            // Roll Q/E (Q = left-roll, E = right-roll)
            bool q = (glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS);
            bool e = (glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS);
            if (q && !e) camera_.Roll(glm::radians(effective_rotate_speed_deg * dt));
            else if (e && !q) camera_.Roll(glm::radians(-effective_rotate_speed_deg * dt));
        }
        else {
            // In first-person attached to white ball, allow yaw via J/L and look up/down via I/K (player aiming)
            bool j = (glfwGetKey(window_, GLFW_KEY_J) == GLFW_PRESS);
            bool l = (glfwGetKey(window_, GLFW_KEY_L) == GLFW_PRESS);
            if (j && !l) camera_.Yaw(glm::radians(effective_rotate_speed_deg * dt));
            else if (l && !j) camera_.Yaw(glm::radians(-effective_rotate_speed_deg * dt));

            bool i = (glfwGetKey(window_, GLFW_KEY_I) == GLFW_PRESS);
            bool k = (glfwGetKey(window_, GLFW_KEY_K) == GLFW_PRESS);
            if (i && !k) camera_.Pitch(glm::radians(effective_rotate_speed_deg * dt));
            else if (k && !i) camera_.Pitch(glm::radians(-effective_rotate_speed_deg * dt));

            // Roll Q/E in first-person as well (keeps controls consistent)
            bool q = (glfwGetKey(window_, GLFW_KEY_Q) == GLFW_PRESS);
            bool e = (glfwGetKey(window_, GLFW_KEY_E) == GLFW_PRESS);
            if (q && !e) camera_.Roll(glm::radians(effective_rotate_speed_deg * dt));
            else if (e && !q) camera_.Roll(glm::radians(-effective_rotate_speed_deg * dt));
        }
    }


    void Game::ShootWhiteBall(float power, const glm::vec3* override_dir) {
        if (!white_ball_ || white_ball_->IsPocketed()) return;

        // Choose direction: override_dir (preferred) or camera forward.
        glm::vec3 dir;
        if (override_dir && glm::length(*override_dir) > 1e-6f) {
            dir = glm::normalize(*override_dir);
        }
        else {
            dir = glm::normalize(camera_.GetForward());
        }
        if (glm::length(dir) <= 0.0f) return;

        // Map power (1..9) to impulse magnitude (tuneable)
        const float base_impulse = 1000.0f;
        float impulse = base_impulse * (power / 9.0f);

        // Apply immediate velocity change (mass assumed uniform)
        glm::vec3 newv = white_ball_->GetVelocity() + dir * impulse;
        white_ball_->SetVelocity(newv);

        // Mark physics active so view toggling can be blocked until motion settles
        physics_active_ = true;
    }


    void Game::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

        void* ptr = glfwGetWindowUserPointer(window);
        Game* game = (Game*)ptr;
        if (!game) return;

        // Quit game if 'Backspace' is pressed
        if (key == GLFW_KEY_BACKSPACE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
            return;
        }

        // Helper: compute a stable horizontal forward (zero Y) from a candidate forward vector
        auto horizontal_forward = [](const glm::vec3& f) {
            glm::vec3 fh(f.x, 0.0f, f.z);
            float len = glm::length(fh);
            if (len < 1e-6f) {
                // fallback to world-forward if forward is nearly vertical
                return glm::vec3(0.0f, 0.0f, -1.0f);
            }
            return fh / len;
            };

        // Toggle third-person on 'C' (single-press)
        if (key == GLFW_KEY_C && action == GLFW_PRESS) {

            // If a shot is in progress (physics active), block toggling until balls stop
            if (game->physics_active_) {
                return;
            }

            if (game->first_person_) {
                // leaving FIRST-PERSON -> enter THIRD-PERSON
                game->stored_fp_pos_ = game->camera_.GetPosition();
                game->stored_fp_ori_ = game->camera_.GetOrientation();
                game->stored_fp_forward_ = game->camera_.GetForward();
                game->has_stored_fp_ = true;
                game->has_stored_fp_forward_ = true;

                if (game->has_stored_third_) {
                    game->camera_.SetPosition(game->stored_third_pos_);
                    game->camera_.SetOrientation(game->stored_third_ori_);
                }
                else if (game->white_ball_) {
                    // prefer stored FP forward to compute sensible behind-ball third-person placement
                    glm::vec3 wbpos = game->white_ball_->GetPosition();
                    glm::vec3 forward = game->has_stored_fp_forward_ ? game->stored_fp_forward_ : game->camera_.GetForward();
                    glm::vec3 fh = horizontal_forward(forward);
                    const float back_distance = 60.0f;
                    const float up_offset = 15.0f;
                    glm::vec3 new_cam_pos = wbpos - fh * back_distance + camera_up_g * up_offset;
                    // Look in the same horizontal direction as FP to avoid introducing unintended pitch.
                    game->camera_.SetView(new_cam_pos, new_cam_pos + fh, camera_up_g);
                }

                game->stored_third_pos_ = game->camera_.GetPosition();
                game->stored_third_ori_ = game->camera_.GetOrientation();
                game->has_stored_third_ = true;

                game->first_person_ = false;
                game->UpdateWhiteVisibility();
            }
            else {
                // leaving THIRD-PERSON -> enter FIRST-PERSON
                game->stored_third_pos_ = game->camera_.GetPosition();
                game->stored_third_ori_ = game->camera_.GetOrientation();
                game->has_stored_third_ = true;

                game->first_person_ = true;
                if (game->white_ball_) {
                    game->camera_.SetPosition(game->white_ball_->GetPosition());
                    if (game->has_stored_fp_) {
                        game->camera_.SetOrientation(game->stored_fp_ori_);
                    }
                }
                game->UpdateWhiteVisibility();
            }

            return;
        }

        // Number keys 1..9 used to shoot white ball with that power (single press)
        if ((key >= GLFW_KEY_1 && key <= GLFW_KEY_9) && action == GLFW_PRESS) {
            int p = key - GLFW_KEY_0;
            float power = static_cast<float>(p);

            // If shot originates from first-person, save the FP pose/orientation/forward
            if (game->first_person_) {
                game->stored_fp_pos_ = game->camera_.GetPosition();
                game->stored_fp_ori_ = game->camera_.GetOrientation();
                game->stored_fp_forward_ = game->camera_.GetForward();
                game->has_stored_fp_ = true;
                game->has_stored_fp_forward_ = true;
            }

            // Record intent to show after shot, but DO NOT make visible while still in first-person.
            game->show_white_on_shot_ = true;

            // Determine shot direction: prefer stored FP forward if available (shot originated from FP),
            // otherwise use current camera forward.
            glm::vec3 shot_dir;
            const glm::vec3* shot_dir_ptr = nullptr;
            if (game->has_stored_fp_forward_) {
                shot_dir = game->stored_fp_forward_;
                shot_dir_ptr = &shot_dir;
            }

            // 1) Apply the shot first so white ball velocity is set (use override when available)
            game->ShootWhiteBall(power, shot_dir_ptr);

            // 2) Then switch to third-person camera if the shot originated in FP.
            if (game->first_person_ && game->white_ball_) {

                if (game->has_stored_third_) {
                    // restore the stored third-person pose
                    game->camera_.SetPosition(game->stored_third_pos_);
                    game->camera_.SetOrientation(game->stored_third_ori_);
                }
                else {
                    // no stored third-person pose: compute default behind-ball placement using stored FP forward when possible
                    glm::vec3 wbpos = game->white_ball_->GetPosition();
                    glm::vec3 vel = game->white_ball_->GetVelocity();

                    glm::vec3 forward;
                    if (glm::length(vel) > 1e-5f) {
                        forward = glm::normalize(vel);
                    }
                    else {
                        forward = game->has_stored_fp_forward_ ? game->stored_fp_forward_ : game->camera_.GetForward();
                    }

                    glm::vec3 fh = horizontal_forward(forward);
                    const float back_distance = 60.0f;
                    const float up_offset = 15.0f;
                    glm::vec3 new_cam_pos = wbpos - fh * back_distance + camera_up_g * up_offset;
                    // Look along the horizontal forward to avoid a downward pitch introduced by looking directly at the ball.
                    game->camera_.SetView(new_cam_pos, new_cam_pos + fh, camera_up_g);
                }

                // Store current third-person pose so future toggles can restore it
                game->stored_third_pos_ = game->camera_.GetPosition();
                game->stored_third_ori_ = game->camera_.GetOrientation();
                game->has_stored_third_ = true;

                // mark as third-person so MainLoop stops snapping camera to the white ball
                game->first_person_ = false;

                // Update visibility (now in 3rd-person, white ball should be visible unless pocketed)
                game->UpdateWhiteVisibility();
            }

            return;
        }
    }


    void Game::ResizeCallback(GLFWwindow* window, int width, int height) {
        // Update GL viewport and forward the new size to the Game's camera projection
        glViewport(0, 0, width, height);
        void* ptr = glfwGetWindowUserPointer(window);
        Game* game = static_cast<Game*>(ptr);
        if (game) {
            game->camera_.SetProjection(camera_fov_g, camera_near_clip_distance_g, camera_far_clip_distance_g, width, height);
        }
    }


    void Game::SetupScene(void) {

        scene_.SetBackgroundColor(viewport_background_color_g);

        // Create pocket positions (cube centered at origin; use world_half_extent_)
        pockets_.clear();
        float h = world_half_extent_;
        // corners (8)
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    pockets_.push_back(glm::vec3(sx * h, sy * h, sz * h));
        // edge midpoints (12): one coordinate 0, others +/- h
        for (int axis = 0; axis < 3; ++axis) {
            for (int s1 = -1; s1 <= 1; s1 += 2)
                for (int s2 = -1; s2 <= 1; s2 += 2) {
                    glm::vec3 p(0.0f);
                    int a1 = (axis + 1) % 3;
                    int a2 = (axis + 2) % 3;
                    p[axis] = 0.0f;
                    p[a1] = s1 * h;
                    p[a2] = s2 * h;
                    pockets_.push_back(p);
                }
        }

        // Create white ball (player)
        {
            Resource* geom = resman_.GetResource("Sphere_White");
            Resource* mat = resman_.GetResource("ObjectMaterial");
            if (geom && mat) {
                // Place the cue ball further back from the origin.
                glm::vec3 cue_pos(-300.0f, 0.0f, 0.0f);
                white_ball_ = CreateBallInstance("WhiteBall", "Sphere_White", "ObjectMaterial");
                white_ball_->SetPosition(cue_pos);
                white_ball_->SetScale(glm::vec3(10.0f)); // ball radius = base * 10 units
                white_ball_->SetVelocity(glm::vec3(0.0f));
                white_ball_->SetPocketed(false);

                // Ensure the first-person camera (which follows the white ball) is looking at the origin on startup.
                camera_.SetView(cue_pos, glm::vec3(0.0f, 0.0f, 0.0f), camera_up_g);

                // Capture this initial FP orientation so switching back to FP restores correct look direction.
                stored_fp_pos_ = camera_.GetPosition();
                stored_fp_ori_ = camera_.GetOrientation();
                stored_fp_forward_ = camera_.GetForward();
                has_stored_fp_ = true;
                has_stored_fp_forward_ = true;

                // Enforce visibility invariant via helper so the white ball is hidden in FP on startup.
                UpdateWhiteVisibility();
            }
        }

        // Create pocket guide spheres at each pocket (instances of the Pocket_Sphere mesh)
        {
            Resource* sphereGeom = resman_.GetResource("Pocket_Sphere");
            Resource* mat = resman_.GetResource("ObjectMaterial");
            if (sphereGeom && mat && white_ball_) {
                // Compute world pocket guide scale from white ball size and configured pocket multiplier
                float ball_world_radius = white_ball_->GetScale().x * white_ball_->GetBaseRadius(); // usually 10 * base
                float pocket_radius = ball_world_radius * pocket_radius_multiplier_;
                // The pocket sphere geometry has radius = 1.0; scale instances so sphere world radius ~= pocket_radius
                // Make pocket spheres 3x larger (matches other code that checks against 3x scale)
                float scale_factor = pocket_radius * 3.0f; // because base radius == 1

                for (size_t i = 0; i < pockets_.size(); ++i) {
                    glm::vec3 p = pockets_[i];
                    std::string name = std::string("PocketGuide") + std::to_string(i);
                    SceneNode* sn = scene_.CreateNode(name, sphereGeom, mat);
                    // Place centered at pocket
                    sn->SetPosition(p);
                    // Uniform scale to make sphere radius ~ pocket_radius * 3
                    sn->SetScale(glm::vec3(scale_factor));
                    // Ensure visible
                    sn->SetVisible(true);
                }
            }
        }

        // Create tracer SceneNode from the "Tracer" geometry so UpdateTracer() has a node to update.
        {
            Resource* tracerGeom = resman_.GetResource("Tracer");
            Resource* mat = resman_.GetResource("ObjectMaterial");
            if (tracerGeom && mat) {
                tracer_node_ = scene_.CreateNode("TracerNode", tracerGeom, mat);
                // Start hidden; UpdateTracer will SetVisible(true) when it has a target
                tracer_node_->SetVisible(false);
                // Give a neutral scale (UpdateTracer overwrites it)
                tracer_node_->SetScale(glm::vec3(1.0f));
            }
            else {
                std::cerr << "Warning: tracer resource or material not found\n";
            }
        }

        // Create other balls (cluster)
        CreateBallField(15);
    }
} // namespace game