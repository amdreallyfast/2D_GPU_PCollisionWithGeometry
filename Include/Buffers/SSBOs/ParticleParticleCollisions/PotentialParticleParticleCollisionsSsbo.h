#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an array of PotentialParticleCollision structures in a GPU 
    buffer.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class PotentialParticleParticleCollisionsSsbo : public SsboBase
{
public:
    PotentialParticleParticleCollisionsSsbo(unsigned int numParticles);
    ~PotentialParticleParticleCollisionsSsbo() = default;
    using SharedPtr = std::shared_ptr<PotentialParticleParticleCollisionsSsbo>;
    using SharedConstPtr = std::shared_ptr<const PotentialParticleParticleCollisionsSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumItems() const;

private:
    unsigned int _numItems;
};

