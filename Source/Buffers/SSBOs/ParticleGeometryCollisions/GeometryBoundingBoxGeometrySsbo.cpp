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
class GeometryBoundingBoxGeometrySsbo : public VertexSsboBase
{
public:
    GeometryBoundingBoxGeometrySsbo(unsigned int numPolygons);
    ~GeometryBoundingBoxGeometrySsbo() = default;
    using SharedPtr = std::shared_ptr<GeometryBoundingBoxGeometrySsbo>;
    using SharedConstPtr = std::shared_ptr<const GeometryBoundingBoxGeometrySsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
};

