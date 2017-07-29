#pragma once

#include "Include/Buffers/SSBOs/VertexSsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticleBoundingBoxGeometrySsbo, but for the collidable geometry.

    Note: I am aware that this class name has "geometry" twice in it, but I couldn't think of a 
    better name.  It's the vertices (geometry) for the collidable geometry, so it is the 
    geometry's bounding box geometry.  It bugs me, but I can't think of anything better.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidableGeometryBoundingBoxGeometrySsbo : public VertexSsboBase
{
public:
    CollidableGeometryBoundingBoxGeometrySsbo(unsigned int numPolygons);
    ~CollidableGeometryBoundingBoxGeometrySsbo() = default;
    using SharedPtr = std::shared_ptr<CollidableGeometryBoundingBoxGeometrySsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidableGeometryBoundingBoxGeometrySsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
};

