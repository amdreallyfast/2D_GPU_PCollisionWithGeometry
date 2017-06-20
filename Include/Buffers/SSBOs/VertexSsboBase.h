#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This is a base class for geometry SSBOs.  That is, it is for vertex buffers that are 
    modified in compute shader but need to be drawn with Geomertry.vert via the RenderGeometry 
    compute controller.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
class VertexSsboBase : public SsboBase
{
public:
    VertexSsboBase();
    virtual ~VertexSsboBase() = default;
    using SharedPtr = std::shared_ptr<VertexSsboBase>;
    using SharedConstPtr = std::shared_ptr<const VertexSsboBase>;

protected:
    void ConfigureRender() override;

};

