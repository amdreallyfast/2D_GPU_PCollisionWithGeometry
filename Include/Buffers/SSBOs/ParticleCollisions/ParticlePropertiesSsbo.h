#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"


/*------------------------------------------------------------------------------------------------
Description:
    This generates and maintains an array of ParticleProperties structures in a GPU buffer.
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
class ParticlePropertiesSsbo : public SsboBase
{
public:
    ParticlePropertiesSsbo();
    ~ParticlePropertiesSsbo() = default;
    using SharedPtr = std::shared_ptr<ParticlePropertiesSsbo>;
    using SharedConstPtr = std::shared_ptr<const ParticlePropertiesSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;

    unsigned int NumProperties() const;

private:
    unsigned int _numProperties;
};


