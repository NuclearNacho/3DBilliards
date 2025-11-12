#include <stdexcept>
#include <algorithm> // added for std::remove used in SetParent
#define GLM_FORCE_RADIANS
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <time.h>

#include "scene_node.h"

namespace game {

    SceneNode::SceneNode(const std::string name, const Resource* geometry, const Resource* material) {

        // Set name of scene node
        name_ = name;

        // Set geometry
        if (geometry->GetType() == PointSet) {
            mode_ = GL_POINTS;
        }
        else if (geometry->GetType() == Mesh) {
            mode_ = GL_TRIANGLES;
        }
        else {
            throw(std::invalid_argument(std::string("Invalid type of geometry")));
        }

        array_buffer_ = geometry->GetArrayBuffer();
        element_array_buffer_ = geometry->GetElementArrayBuffer();
        size_ = geometry->GetSize();

        // Set material (shader program)
        if (material->GetType() != Material) {
            throw(std::invalid_argument(std::string("Invalid type of material")));
        }

        material_ = material->GetResource();

        // Other attributes
        scale_ = glm::vec3(1.0, 1.0, 1.0);
        visible_ = true;

        // initialize transform to identity
        position_ = glm::vec3(0.0f);
        orientation_ = glm::quat();

        // Hierarchy
        parent_ = nullptr;
        children_.clear();

        // Initialize override/color-hint flags
        override_color_enabled_ = false;
        override_color_ = glm::vec3(1.0f, 1.0f, 1.0f);
        color_hint_enabled_ = false;
        color_hint_ = glm::vec3(1.0f, 1.0f, 1.0f);
    }


    SceneNode::~SceneNode() {
        // Note: do not delete children here; SceneGraph owns nodes.
    }


    const std::string SceneNode::GetName(void) const {

        return name_;
    }


    glm::vec3 SceneNode::GetPosition(void) const {

        return position_;
    }


    glm::quat SceneNode::GetOrientation(void) const {

        return orientation_;
    }


    glm::vec3 SceneNode::GetScale(void) const {

        return scale_;
    }


    void SceneNode::SetPosition(glm::vec3 position) {

        position_ = position;
    }


    void SceneNode::SetOrientation(glm::quat orientation) {

        // Ensure orientation stays normalized to avoid drift when applying many rotations
        orientation_ = glm::normalize(orientation);
    }


    void SceneNode::SetScale(glm::vec3 scale) {

        scale_ = scale;
    }


    void SceneNode::SetVisible(bool visible) {
        visible_ = visible;
    }


    bool SceneNode::IsVisible(void) const {
        return visible_;
    }


    // Hierarchy methods
    void SceneNode::SetParent(SceneNode* parent) {
        // remove from old parent if present
        if (parent_ == parent) return;
        if (parent_) {
            auto& siblings = parent_->children_;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }
        parent_ = parent;
        if (parent_) parent_->children_.push_back(this);
    }

    SceneNode* SceneNode::GetParent() const {
        return parent_;
    }

    void SceneNode::AddChild(SceneNode* child) {
        if (!child) return;
        child->SetParent(this);
    }

    const std::vector<SceneNode*>& SceneNode::GetChildren() const {
        return children_;
    }


    void SceneNode::Translate(glm::vec3 trans) {

        position_ += trans;
    }


    void SceneNode::Rotate(glm::quat rot) {

        // Use same multiplication convention as Camera (left-multiply) so rotations compose consistently.
        // Also normalize after applying rotation to avoid quaternion drift/scale over many operations.
        orientation_ = rot * orientation_;
        orientation_ = glm::normalize(orientation_);
    }


    void SceneNode::Scale(glm::vec3 scale) {

        scale_ *= scale;
    }


    GLenum SceneNode::GetMode(void) const {

        return mode_;
    }


    GLuint SceneNode::GetArrayBuffer(void) const {

        return array_buffer_;
    }


    GLuint SceneNode::GetElementArrayBuffer(void) const {

        return element_array_buffer_;
    }


    GLsizei SceneNode::GetSize(void) const {

        return size_;
    }


    GLuint SceneNode::GetMaterial(void) const {

        return material_;
    }


    void SceneNode::SetOverrideColor(const glm::vec3& color) {
        override_color_enabled_ = true;
        override_color_ = color;
    }

    void SceneNode::ClearOverrideColor(void) {
        override_color_enabled_ = false;
    }

    bool SceneNode::HasOverrideColor(void) const {
        return override_color_enabled_;
    }

    void SceneNode::SetColorHint(const glm::vec3& color) {
        color_hint_enabled_ = true;
        color_hint_ = color;
    }

    bool SceneNode::HasColorHint(void) const {
        return color_hint_enabled_;
    }

    glm::vec3 SceneNode::GetColorHint(void) const {
        return color_hint_;
    }


    void SceneNode::Draw(Camera* camera, const glm::mat4& parentTransform) {

        // Skip rendering if invisible
        if (!visible_) {
            // still traverse children (they may be independently visible)
            for (auto child : children_) {
                child->Draw(camera, parentTransform);
            }
            return;
        }

        // Compute local->world transform using parentTransform
        glm::mat4 scaling = glm::scale(glm::mat4(1.0f), scale_);
        glm::mat4 rotation = glm::mat4_cast(orientation_);
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position_);
        glm::mat4 localWorld = parentTransform * (translation * rotation * scaling);

        // Select proper material (shader program)
        glUseProgram(material_);

        // Set geometry to draw
        glBindBuffer(GL_ARRAY_BUFFER, array_buffer_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_array_buffer_);

        // Set globals for camera
        camera->SetupShader(material_);

        // Set world matrix and other shader input variables
        SetupShader(material_, localWorld);

        // Draw geometry
        if (mode_ == GL_POINTS) {
            glDrawArrays(mode_, 0, size_);
        }
        else {
            glDrawElements(mode_, size_, GL_UNSIGNED_INT, 0);
        }

        // Draw children with this node's world transform as parent
        for (auto child : children_) {
            child->Draw(camera, localWorld);
        }
    }


    void SceneNode::Update(void) {

        // Do nothing for this generic type of scene node
    }


    void SceneNode::SetupShader(GLuint program, const glm::mat4& world) {

        // Set attributes for shaders
        GLint vertex_att = glGetAttribLocation(program, "vertex");
        glVertexAttribPointer(vertex_att, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), 0);
        glEnableVertexAttribArray(vertex_att);

        GLint normal_att = glGetAttribLocation(program, "normal");
        glVertexAttribPointer(normal_att, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
        glEnableVertexAttribArray(normal_att);

        GLint color_att = glGetAttribLocation(program, "color");
        if (color_att >= 0) {
            if (override_color_enabled_) {
                // Use a constant attribute value instead of the VBO color array
                glDisableVertexAttribArray(color_att);
                glVertexAttrib3f(color_att, override_color_.x, override_color_.y, override_color_.z);
            }
            else {
                // Use per-vertex colors from the VBO
                glVertexAttribPointer(color_att, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (void*)(6 * sizeof(GLfloat)));
                glEnableVertexAttribArray(color_att);
            }
        }

        GLint tex_att = glGetAttribLocation(program, "uv");
        glVertexAttribPointer(tex_att, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (void*)(9 * sizeof(GLfloat)));
        glEnableVertexAttribArray(tex_att);

        // World transformation
        GLint world_mat = glGetUniformLocation(program, "world_mat");
        glUniformMatrix4fv(world_mat, 1, GL_FALSE, glm::value_ptr(world));

        // Timer
        GLint timer_var = glGetUniformLocation(program, "timer");
        double current_time = glfwGetTime();
        glUniform1f(timer_var, (float)current_time);
    }

} // namespace game;
