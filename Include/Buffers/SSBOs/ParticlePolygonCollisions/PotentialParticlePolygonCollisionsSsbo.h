#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an array of PotentialParticleCollision structures in a GPU 
    buffer.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class PotentialParticlePolygonCollisionsSsbo : public SsboBase
{
public:
    PotentialParticlePolygonCollisionsSsbo(unsigned int numParticles);
    ~PotentialParticlePolygonCollisionsSsbo() = default;
    using SharedPtr = std::shared_ptr<PotentialParticlePolygonCollisionsSsbo>;
    using SharedConstPtr = std::shared_ptr<const PotentialParticlePolygonCollisionsSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumItems() const;

private:
    unsigned int _numItems;
};

