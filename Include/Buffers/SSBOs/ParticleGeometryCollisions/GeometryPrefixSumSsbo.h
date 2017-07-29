#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    Like ParticlePrefixSumSsbo, but for the collidable geometry.

    Note: "Prefix scan", "prefix sum", same thing.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
class GeometryPrefixSumSsbo : public SsboBase
{
public:
    GeometryPrefixSumSsbo(unsigned int numDataEntries);
    ~GeometryPrefixSumSsbo() = default;
    using SharedPtr = std::shared_ptr<GeometryPrefixSumSsbo>;
    using SharedConstPtr = std::shared_ptr<const GeometryPrefixSumSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumDataEntries() const;
    unsigned int TotalBufferEntries() const;

private:
    unsigned int _numDataEntries;
};
