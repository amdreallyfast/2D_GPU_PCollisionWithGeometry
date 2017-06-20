#pragma once

#include "Include/Buffers/SSBOs/ParticleSsbo.h"

namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Encapsulates the rendering of particles.

    Creator:    John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    class RenderParticles
    {
    public:
        RenderParticles();
        ~RenderParticles();

        void Render(const ParticleSsbo::SharedPtr &particleSsboToRender) const;

    private:
        unsigned int _renderProgramId;
    };
}
