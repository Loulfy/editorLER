//
// Created by loulfy on 24/03/2023.
//

#include "ler_cam.hpp"

namespace ler
{
    glm::vec3 Controller::getViewDirection() const
    {
        auto m = glm::inverse(m_view);
        return {m[2]};
    }

    glm::vec3 Controller::getRightVector() const
    {
        auto m = glm::inverse(m_view);
        return {m[0]};
    }

    glm::vec3 Controller::rayCast(const glm::vec2& pos)
    {
        float x = (2.0f * pos.x) / m_viewport[2] - 1.0f;
        float y = 1.0f - (2.0f * pos.y) / m_viewport[3];
        float z = 1.0f;
        glm::vec3 ray_nds = glm::vec3(x, y, z);
        glm::vec4 ray_clip = glm::vec4(ray_nds.x, ray_nds.y, -1.0f, 1.0f);
        // eye space to clip we would multiply by projection so
        // clip space to eye space is the inverse projection
        glm::vec4 ray_eye = inverse(m_proj) * ray_clip;
        // convert point to forwards
        ray_eye = glm::vec4(ray_eye.x, ray_eye.y, -1.0f, 0.0f);
        // world space to eye space is usually multiply by view so
        // eye space to world space is inverse view
        glm::vec4 inv_ray_wor = (inverse(m_view) * ray_eye);
        glm::vec3 ray_wor = glm::vec3(inv_ray_wor.x, inv_ray_wor.y, inv_ray_wor.z);
        ray_wor = normalize(ray_wor);
        return ray_wor;
    }

    glm::vec3 Controller::unProject(const glm::vec3& pos)
    {
        return glm::unProject(pos, m_view, m_proj, glm::vec4(0, 0, m_viewport));
    }

    void FpsCamera::keyboardCallback(int key, int action, float delta)
    {
        if(action == 0)
            return;
        float velocity = movementSpeed * delta;
        if (key == 87) // Forward
            m_position += front * velocity;
        if (key == 83) // Backward
            m_position -= front * velocity;
        if (key == 65) // Left
            m_position -= right * velocity;
        if (key == 68) // Right
            m_position += right * velocity;
        if (key == 69) // Down
            m_position.y += 1 * velocity;
        if (key == 340) // Up
            m_position.y -= 1 * velocity;
    }

    void FpsCamera::motionCallback(const glm::vec2& pos)
    {
        if(lockMouse)
            return;

        if (firstMouse)
        {
            m_last = pos;
            firstMouse = false;
        }

        float xoffset = pos.x - m_last.x;
        float yoffset = m_last.y - pos.y; // reversed since y-coordinates go from bottom to top

        m_last = pos;

        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        // update Front, Right and Up Vectors using the updated Euler angles
        updateMatrices();
    }

    void FpsCamera::updateMatrices()
    {
        // calculate the new Front vector
        glm::vec3 tmp;
        tmp.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        tmp.y = sin(glm::radians(pitch));
        tmp.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(tmp);
        // also re-calculate the Right and Up vector
        right = glm::normalize(glm::cross(front, worldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        up = glm::normalize(glm::cross(right, front));

        m_view = glm::lookAt(m_position, m_position + front, up);
        m_proj = glm::perspective(glm::radians(m_fov), m_viewport.x/m_viewport.y, 0.01f, 10000.f);
    }
}