#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticleBvhNodeSsbo, but for the collidabel geometry.  Used in conjunction with 
    ParticleBvhNodeSsbo for particle-geometry collision detection.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidablePolygonBvhNodeSsbo : public SsboBase
{
public:
    CollidablePolygonBvhNodeSsbo(unsigned int numPolygons);
    ~CollidablePolygonBvhNodeSsbo() = default;
    using SharedPtr = std::shared_ptr<CollidablePolygonBvhNodeSsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidablePolygonBvhNodeSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumLeafNodes() const;
    //unsigned int NumInternalNodes() const;    // add if ever needed
    unsigned int NumTotalNodes() const;

private:
    unsigned int _numLeaves;
    unsigned int _numInternalNodes;
    unsigned int _numTotalNodes;
};


