#pragma once

#include <string>
#include "Include/Buffers/SSBOs/VertexSsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an SSBO of bounding boxes for the collidable geometry.  
    Geometry consists of PolygonFace objects.  At this time (6-20-2017), all these PolygonFace 
    objects will considered part of the same object.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class CollidableGeometrySsbo : public VertexSsboBase
{
public:
    CollidableGeometrySsbo(const std::string &blenderMeshFilePath);
    ~CollidableGeometrySsbo() = default;
    using SharedPtr = std::shared_ptr<CollidableGeometrySsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidableGeometrySsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumPolygons() const;

private:
    unsigned int _numPolygons;
};

