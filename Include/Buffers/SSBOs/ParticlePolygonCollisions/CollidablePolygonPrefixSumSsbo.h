#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticlePrefixSumSsbo, but for the collidable geometry.

    Note: "Prefix scan", "prefix sum", same thing.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidablePolygonPrefixSumSsbo : public SsboBase
{
public:
    CollidablePolygonPrefixSumSsbo(unsigned int numDataEntries);
    ~CollidablePolygonPrefixSumSsbo() = default;
    using SharedPtr = std::shared_ptr<CollidablePolygonPrefixSumSsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidablePolygonPrefixSumSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumDataEntries() const;
    unsigned int TotalBufferEntries() const;

private:
    unsigned int _numDataEntries;
};
