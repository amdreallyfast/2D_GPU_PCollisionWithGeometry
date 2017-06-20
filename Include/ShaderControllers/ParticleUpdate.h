#pragma once

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Buffers/PersistentAtomicCounterBuffer.h"

#include "ThirdParty/glm/vec4.hpp"

namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Encapsulates the following particle updates via compute shader:
        (1) Updates particle positions based on their velocity in the previous frame.
        (2) If any particles have gone out of bounds, flag them as inactive.
        (3) Emit as many particles for this frame as each emitter allows.

        There is one compute shader that does this, and this class is built to communicate with 
        and summon that particular shader.

        Note: This class is not concerned with the particle SSBO.  It is concerned with uniforms 
        and summoning the shader.  SSBO setup is performed in the appropriate SSBO object.
    Creator:    John Cox, 11-24-2016 (restructured to use composite shaders in 4/2017)
    --------------------------------------------------------------------------------------------*/
    class ParticleUpdate
    {
    public:
        ParticleUpdate(const ParticleSsbo::SharedConstPtr &ssboToUpdate);
        ~ParticleUpdate();

        void Update(float deltaTimeSec);
        unsigned int NumActiveParticles() const;

    private:
        unsigned int _totalParticleCount;
        unsigned int _activeParticleCount;
        unsigned int _computeProgramId;
        
        // these uniforms are specific to this shader
        int _unifLocDeltaTimeSec;
    };
}
