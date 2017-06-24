#pragma once

#include <string>
#include "Include/Buffers/SSBOs/VertexSsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an SSBO of bounding boxes for the collideable geometry.  
    Geometry consists of PolygonFace objects.  At this time (6-20-2017), all these PolygonFace 
    objects will considered part of the same object.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class CollideableGeometrySsbo : public VertexSsboBase
{
public:
    CollideableGeometrySsbo(const std::string &blenderMeshFilePath);
    virtual ~CollideableGeometrySsbo() = default;
    using SharedPtr = std::shared_ptr<CollideableGeometrySsbo>;
    using SharedConstPtr = std::shared_ptr<const CollideableGeometrySsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumPolygons() const;

private:
    unsigned int _numPolygons;
};

