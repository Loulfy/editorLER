//
// Created by loulfy on 24/03/2023.
//

#ifndef LER_CAM_H
#define LER_CAM_H

#include <memory>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace ler
{
    enum class Action
    {
        Release = 0,
        Press = 1,
        Repeat = 2
    };

    class Controller
    {
    public:

        virtual void keyboardCallback(int key, int action, float delta) {}
        virtual void mouseCallback(int button, int action) {}
        virtual void motionCallback(const glm::vec2& pos) {}
        virtual void scrollCallback(const glm::vec2& offset) {}

        virtual void updateMatrices() {};
        virtual void reset() {};

        void setViewportSize(int width, int height) { m_viewport = glm::vec2(width, height); }
        virtual void setPointOfView(const glm::vec3& center) { m_target = center; }
        void setFieldOfView(float fov) { m_fov = fov; }
        void setEyeDistance(float z) { m_position.z = z; }

        glm::vec3 rayCast(const glm::vec2& pos);
        glm::vec3 unProject(const glm::vec3& pos);

        [[nodiscard]] glm::mat4 getViewMatrix() const { return m_view; }
        [[nodiscard]] glm::mat4 getProjMatrix() const { return m_proj; }
        [[nodiscard]] glm::vec3 getViewDirection() const;
        [[nodiscard]] glm::vec3 getRightVector() const;

    protected:

        glm::mat4 m_proj = glm::mat4(1.f);
        glm::mat4 m_view = glm::mat4(1.f);
        float m_fov = 55.f;
        glm::vec3 m_position = glm::vec3(0, 0, 5);
        glm::vec2 m_viewport = glm::vec2(1280, 720);
        glm::vec3 m_target = glm::vec3(0);
        glm::vec2 m_last = glm::vec2(0,0);

        static constexpr glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    };

    using ControllerPtr = std::shared_ptr<Controller>;

    class FpsCamera : public Controller
    {
    public:

        void keyboardCallback(int key, int action, float delta) override;
        void motionCallback(const glm::vec2& pos) override;
        void updateMatrices() override;
        void reset() override { firstMouse = true; }

        glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = glm::vec3(1);
        glm::vec3 right = glm::vec3(1);

        // euler angles
        float yaw = -90.f;
        float pitch = 0.f;

        // camera options
        float movementSpeed = 80.f;
        float mouseSensitivity = 0.1f;

        bool firstMouse = true;
        bool lockMouse = false;
    };
}

#endif //LER_CAM_H
