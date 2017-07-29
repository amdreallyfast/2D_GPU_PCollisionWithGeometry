#pragma once

#include <string>
#include "Include/Buffers/SSBOs/VertexSsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Uses the same loading mechanism as CollidableGeometrySsbo, but only generates vertices to 
    visually depict the surface normals.  CollidableGeometrySsbo should not do that or else 
    there will be particle collisions with the surface normals.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidableGeometrySurfaceNormalsSsbo : public VertexSsboBase
{
public:
    CollidableGeometrySurfaceNormalsSsbo(const std::string &blenderMeshFilePath);
    ~CollidableGeometrySurfaceNormalsSsbo() = default;
    using SharedPtr = std::shared_ptr<CollidableGeometrySurfaceNormalsSsbo >;
    using SharedConstPtr = std::shared_ptr<const CollidableGeometrySurfaceNormalsSsbo >;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumPolygons() const;

private:
    unsigned int _numPolygons;
};

