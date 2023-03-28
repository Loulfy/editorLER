//
// Created by loulfy on 12/03/2023.
//

#ifndef LER_ARC_H
#define LER_ARC_H

#include "ler_cam.hpp"
#define M_PI 3.141592f

namespace ler
{
    class ArcCamera : public Controller
    {
    public:

        void mouseCallback(int button, int action) override;
        void motionCallback(const glm::vec2& pos) override;
        void scrollCallback(const glm::vec2& offset) override;
        void updateMatrices() override;
        void reset() override { m_active = false; }

    private:

        bool m_active = false;
    };
}

#endif //LER_ARC_H
