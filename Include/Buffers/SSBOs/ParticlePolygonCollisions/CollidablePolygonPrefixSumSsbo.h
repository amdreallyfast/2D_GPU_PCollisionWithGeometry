#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticlePrefixSumSsbo, but for the collidable geometry.

    Note: "Prefix scan", "prefix sum", same thing.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class CollidableGeometryPrefixSumSsbo : public SsboBase
{
public:
    CollidableGeometryPrefixSumSsbo(unsigned int numDataEntries);
    ~CollidableGeometryPrefixSumSsbo() = default;
    using SharedPtr = std::shared_ptr<CollidableGeometryPrefixSumSsbo>;
    using SharedConstPtr = std::shared_ptr<const CollidableGeometryPrefixSumSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumDataEntries() const;
    unsigned int TotalBufferEntries() const;

private:
    unsigned int _numDataEntries;
};
