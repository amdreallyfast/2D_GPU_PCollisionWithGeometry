#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"

/*------------------------------------------------------------------------------------------------
Description:
    Encapsulates the SSBO that is used for calculating prefix sums as part of the parallel radix 
    sorting algorithm.

    Note: "Prefix scan", "prefix sum", same thing.
Creator:    John Cox, 3/2017
------------------------------------------------------------------------------------------------*/
class PrefixSumSsbo : public SsboBase
{
public:
    PrefixSumSsbo(unsigned int numDataEntries);
    virtual ~PrefixSumSsbo() = default;
    using SharedPtr = std::shared_ptr<PrefixSumSsbo>;
    using SharedConstPtr = std::shared_ptr<const PrefixSumSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumDataEntries() const;
    unsigned int TotalBufferEntries() const;

private:
    unsigned int _numPerGroupPrefixSums;
    unsigned int _numDataEntries;
};
