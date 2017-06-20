#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Used during particle collision detection to generate the bounding volume hierarchy (BVH).

    Contains enough nodes for
    - 1 leaf node for each particle (n leaves)
    - the branches all the way to the root (n-1 internal nodes)

    Why have both leaves and internal nodes instead of just internal nodes and rely on the 
    ParticleBuffer as the leaves?  Because generating the bounding boxes for the tree requires 
    traversing from the leaves up to the root.  If the Particle objects themselves were the 
    leaves, then they would have to carry around a "parent node index" member, and that member 
    is only relevant when constructing the BVH, so I decided that Particle objects will not 
    carry it.  Also, leaf nodes that share the same structure as the internal nodes makes 
    bounding box generation and comparison easier.
Creator:    John Cox, 3/2017
------------------------------------------------------------------------------------------------*/
class BvhNodeSsbo : public SsboBase
{
public:
    BvhNodeSsbo(unsigned int numParticles);
    virtual ~BvhNodeSsbo() = default;
    using SharedPtr = std::shared_ptr<BvhNodeSsbo>;
    using SharedConstPtr = std::shared_ptr<const BvhNodeSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumLeafNodes() const;
    //unsigned int NumInternalNodes() const;    // add if ever needed
    unsigned int NumTotalNodes() const;

private:
    unsigned int _numLeaves;
    unsigned int _numInternalNodes;
    unsigned int _numTotalNodes;
};


