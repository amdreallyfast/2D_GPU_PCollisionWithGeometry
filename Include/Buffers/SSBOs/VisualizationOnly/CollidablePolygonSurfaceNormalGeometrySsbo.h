#pragma once

#include <string>
#include "Include/Buffers/SSBOs/VertexSsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Uses the same loading mechanism as CollidablePolygonSsbo, but only generates vertices to 
    visually depict the surface normals.  CollidablePolygonSsbo should not do that or else 
    there will be particle collisions with the surface normals.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidablePolygonSurfaceNormalGeometrySsbo : public VertexSsboBase
{
public:
    CollidablePolygonSurfaceNormalGeometrySsbo(const std::string &blenderMeshFilePath);
    ~CollidablePolygonSurfaceNormalGeometrySsbo() = default;
    using SharedPtr = std::shared_ptr<CollidablePolygonSurfaceNormalGeometrySsbo >;
    using SharedConstPtr = std::shared_ptr<const CollidablePolygonSurfaceNormalGeometrySsbo >;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumPolygons() const;

private:
    unsigned int _numPolygons;
};

