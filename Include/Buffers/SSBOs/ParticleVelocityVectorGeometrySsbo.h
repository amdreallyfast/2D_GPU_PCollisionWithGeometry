#pragma once

#include "Include/Buffers/SSBOs/VertexSsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an SSBO of lines for each particle's velocity vectors.  This 
    is used to visualize the result of particle collisions.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class ParticleVelocityVectorGeometrySsbo : public VertexSsboBase
{
public:
    ParticleVelocityVectorGeometrySsbo(unsigned int numParticles);
    ~ParticleVelocityVectorGeometrySsbo() = default;
    using SharedPtr = std::shared_ptr<ParticleVelocityVectorGeometrySsbo>;
    using SharedConstPtr = std::shared_ptr<const ParticleVelocityVectorGeometrySsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
};

