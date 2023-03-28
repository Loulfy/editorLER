//
// Created by loulfy on 12/03/2023.
//

#include "ler_arc.hpp"
#include "ler_log.hpp"

namespace ler
{
    void ArcCamera::updateMatrices()
    {
        m_proj = glm::perspective(glm::radians(m_fov), m_viewport.x/m_viewport.y, 0.01f, 10000.f);
        m_view = glm::lookAt(m_position, m_target, worldUp);
    }

    float sgn(double v)
    {
        return (v < 0.f) ? -1.f : ((v > 0.f) ? 1.f : 0.f);
    }

    void ArcCamera::mouseCallback(int button, int action)
    {
        m_active = action == 1;
    }

    void ArcCamera::motionCallback(const glm::vec2& pos)
    {
        if(!m_active)
        {
            m_last = pos;
            return;
        }

        glm::vec4 position(m_position, 1);
        glm::vec4 pivot(m_target, 1);

        float deltaAngleX = (2 * M_PI / m_viewport.x); // a movement from left to right = 2*PI = 360 deg
        float deltaAngleY = (M_PI / m_viewport.y);  // a movement from top to bottom = PI = 180 deg

        float cosAngle = glm::dot(glm::normalize(getViewDirection()), getRightVector());
        if (cosAngle * sgn(deltaAngleY) > 0.99f)
            deltaAngleY = 0;
        /*if (cosAngle < -0.99f)
            deltaAngleY = M_PI;*/

        float xAngle = (m_last.x - pos.x) * deltaAngleX;
        float yAngle = (m_last.y - pos.y) * deltaAngleY;

        glm::mat4x4 rotationMatrixX(1.0f);
        rotationMatrixX = glm::rotate(rotationMatrixX, xAngle, worldUp);
        position = (rotationMatrixX * (position - pivot)) + pivot;

        glm::mat4x4 rotationMatrixY(1.0f);
        rotationMatrixY = glm::rotate(rotationMatrixY, yAngle, getRightVector());
        m_position = (rotationMatrixY * (position - pivot)) + pivot;

        m_last = pos;
        updateMatrices();
    }

    void ArcCamera::scrollCallback(const glm::vec2& offset)
    {
        m_fov = std::max(0.001f, m_fov * ((offset.y > 0) ? 1.1f : 0.9f));
        if (m_fov < 1.0f)
            m_fov = 1.0f;
        if (m_fov > 70.0f)
            m_fov = 70.0f;
        updateMatrices();
    }
}