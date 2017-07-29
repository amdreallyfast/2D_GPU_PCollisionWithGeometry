#pragma once

#include "Include/Buffers/SSBOs/VertexSsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an SSBO of bounding boxes for each particle's collision region.
    Each particle has a collision radius, but a square is easier to draw.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class ParticleBoundingBoxGeometrySsbo : public VertexSsboBase
{
public:
    ParticleBoundingBoxGeometrySsbo(unsigned int numParticles);
    ~ParticleBoundingBoxGeometrySsbo() = default;
    using SharedPtr = std::shared_ptr<ParticleBoundingBoxGeometrySsbo>;
    using SharedConstPtr = std::shared_ptr<const ParticleBoundingBoxGeometrySsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
};

