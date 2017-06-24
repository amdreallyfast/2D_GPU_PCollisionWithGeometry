
#include "Include/ShaderControllers/ParticleGeometryCollisions.h"

namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Gives members initial values.  Generates compute shaders for the different stages of the 
        BVH generation.  
    Parameters:
        blenderObjFilePath      Used to load the geometry.
        particleSsbo        Need the buffer size uniform set for these compute shaders.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    ParticleGeometryCollisions::ParticleGeometryCollisions(
        const std::string &blenderObjFilePath, const ParticleSsbo::SharedConstPtr particleSsbo) :
        _numParticles(particleSsbo->NumParticles()),
        _programIdResolveCollisions(0),

        _collideableGeometrySsbo(blenderObjFilePath)
    {
        AssembleProgramResolveCollisions();
    }

    ParticleGeometryCollisions::~ParticleGeometryCollisions()
    {

    }

    void ParticleGeometryCollisions::DetectAndResolve(bool withProfiling) const
    {

    }

    const VertexSsboBase &ParticleGeometryCollisions::GeometrySsbo() const
    {

    }

    void ParticleGeometryCollisions::AssembleProgramHeader(const std::string &shaderKey) const
    {

    }

    void ParticleGeometryCollisions::AssembleProgramResolveCollisions()
    {

    }

    void ParticleGeometryCollisions::ResolveCollisionsWithoutProfiling(unsigned int numWorkGroupsX) const
    {

    }
    void ParticleGeometryCollisions::ResolveCollisionsWithProfiling(unsigned int numWorkGroupsX) const
    {

    }
}
