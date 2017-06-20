#pragma once

#include "Include/Buffers/SSBOs/SsboBase.h"

/*------------------------------------------------------------------------------------------------
Description:
    Encapsulates the SSBO that stores Particles.  It generates a chunk of space on the GPU that 
    is big enough to store the requested number of particles, and since this buffer will be used 
    in a drawing shader as well as compute shader, this class will also set up the VAO and the 
    vertex attributes.

    Allocates enough space for 2x the number of requested particles.  The second half is used 
    during particle sorting.  There is no "swap" function in GPU programming, so the particles 
    need to be copied from the first half to the second half, then copied back to their sorted 
    position.
Creator:    John Cox, 3/2017
------------------------------------------------------------------------------------------------*/
class ParticleSsbo : public SsboBase
{
public:
    ParticleSsbo(unsigned int numParticles);
    virtual ~ParticleSsbo() = default;
    using SharedPtr = std::shared_ptr<ParticleSsbo>;
    using SharedConstPtr = std::shared_ptr<const ParticleSsbo>;

    void ConfigureConstantUniforms(unsigned int computeProgramId) const override;
    unsigned int NumParticles() const;

private:
    void ConfigureRender() override;

    unsigned int _numParticles;
};