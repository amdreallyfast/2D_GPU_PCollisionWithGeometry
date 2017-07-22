#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"

/*------------------------------------------------------------------------------------------------
Description:
    SortingData objects get sorted 1 bit at a time from the read part of the buffer to 
    the write part of the buffer on each loop of the parallel sort routine (there is no "swap" 
    in GPU-land, so have to use a second buffer), so this buffer needs to be big enough to 
    contain a read/write pair of SortingData objects.  Thus it must contain 2x the number 
    of particles.
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
class ParticleSortingDataSsbo: public SsboBase
{
public:
    ParticleSortingDataSsbo(unsigned int numParticles);
    ~ParticleSortingDataSsbo() = default;
    using SharedPtr = std::shared_ptr<ParticleSortingDataSsbo>;
    using SharedConstPtr = std::shared_ptr<const ParticleSortingDataSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumItems() const;

private:
    unsigned int _numItems;
};
