#ifndef SCENE_NODE_H_
#define SCENE_NODE_H_

#include <string>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#define GLM_FORCE_RADIANS
#include <glm/gtc/quaternion.hpp>
#include <vector>

#include "resource.h"
#include "camera.h"

namespace game {

    // Class that manages one object in a scene 
    class SceneNode {

    public:
        // Create scene node from given resources
        SceneNode(const std::string name, const Resource* geometry, const Resource* material);

        // Destructor
        virtual ~SceneNode();

        // Get name of node
        const std::string GetName(void) const;

        // Get node attributes
        glm::vec3 GetPosition(void) const;
        glm::quat GetOrientation(void) const;
        glm::vec3 GetScale(void) const;

        // Set node attributes
        void SetPosition(glm::vec3 position);
        void SetOrientation(glm::quat orientation);
        void SetScale(glm::vec3 scale);

        // Visibility control
        void SetVisible(bool visible);
        bool IsVisible(void) const;

        // Parent / child hierarchy
        void SetParent(SceneNode* parent);
        SceneNode* GetParent() const;
        void AddChild(SceneNode* child);
        const std::vector<SceneNode*>& GetChildren() const;

        // Perform transformations on node
        void Translate(glm::vec3 trans);
        void Rotate(glm::quat rot);
        void Scale(glm::vec3 scale);

        // Draw the node according to scene parameters in 'camera'
        // parentTransform: matrix transform accumulated from parents
        virtual void Draw(Camera* camera, const glm::mat4& parentTransform);
        // Update the node
        virtual void Update(void);

        // OpenGL variables
        GLenum GetMode(void) const;
        GLuint GetArrayBuffer(void) const;
        GLuint GetElementArrayBuffer(void) const;
        GLsizei GetSize(void) const;
        GLuint GetMaterial(void) const;

        // Per-node override color (forces shader color attribute to this constant)
        void SetOverrideColor(const glm::vec3& color);
        void ClearOverrideColor(void);
        bool HasOverrideColor(void) const;

        // Color hint metadata (used by game logic to query the representative color for the node)
        void SetColorHint(const glm::vec3& color);
        bool HasColorHint(void) const;
        glm::vec3 GetColorHint(void) const;

    private:
        std::string name_; // Name of the scene node
        GLuint array_buffer_; // References to geometry: vertex and array buffers
        GLuint element_array_buffer_;
        GLenum mode_; // Type of geometry
        GLsizei size_; // Number of primitives in geometry
        GLuint material_; // Reference to shader program
        glm::vec3 position_; // Position of node (local)
        glm::quat orientation_; // Orientation of node (local)
        glm::vec3 scale_; // Scale of node (local)
        bool visible_; // Visibility flag

        // Hierarchy
        SceneNode* parent_;
        std::vector<SceneNode*> children_;

        // Per-node override color (if enabled)
        bool override_color_enabled_;
        glm::vec3 override_color_;

        // Per-node color hint (metadata)
        bool color_hint_enabled_;
        glm::vec3 color_hint_;

        // Set matrices that transform the node in a shader program (uses world transform)
        void SetupShader(GLuint program, const glm::mat4& world);

    }; // class SceneNode

} // namespace game

#endif // SCENE_NODE_H_
