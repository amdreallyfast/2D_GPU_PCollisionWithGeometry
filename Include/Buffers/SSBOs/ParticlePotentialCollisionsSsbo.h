#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an array of ParticlePotentialCollisionsSsbo structures in a GPU 
    buffer.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class ParticlePotentialCollisionsSsbo : public SsboBase
{
public:
    ParticlePotentialCollisionsSsbo(unsigned int numParticles);
    ~ParticlePotentialCollisionsSsbo() = default;
    using SharedPtr = std::shared_ptr<ParticlePotentialCollisionsSsbo>;
    using SharedConstPtr = std::shared_ptr<const ParticlePotentialCollisionsSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumItems() const;

private:
    unsigned int _numItems;
};

