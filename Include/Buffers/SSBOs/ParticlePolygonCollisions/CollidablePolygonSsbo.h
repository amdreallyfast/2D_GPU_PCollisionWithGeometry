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
class CollidablePolygonSsbo : public VertexSsboBase
{
public:
    CollidablePolygonSsbo(const std::string &blenderMeshFilePath);
    ~CollidablePolygonSsbo() = default;
    using SharedPtr = std::shared_ptr<CollidablePolygonSsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidablePolygonSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumPolygons() const;

private:
    unsigned int _numPolygons;
};

