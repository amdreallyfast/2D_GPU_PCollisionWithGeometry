#pragma once

#include <string>

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Buffers/SSBOs/CollideableGeometrySsbo.h"


namespace ShaderControllers
{
    class ParticleGeometryCollisions
    {
    public:
        ParticleGeometryCollisions(const std::string &blenderObjFilePath, const ParticleSsbo::SharedConstPtr particleSsbo);
        ~ParticleGeometryCollisions();

        void DetectAndResolve(bool withProfiling) const;
        const VertexSsboBase &GeometrySsbo() const;

    private:
        // the number of work groups will be based on the number of particles (one thread for each particle)
        unsigned int _numParticles;

        unsigned int _programIdResolveCollisions;

        void AssembleProgramHeader(const std::string &shaderKey) const;
        void AssembleProgramResolveCollisions();

        void ResolveCollisionsWithoutProfiling(unsigned int numWorkGroupsX) const;
        void ResolveCollisionsWithProfiling(unsigned int numWorkGroupsX) const;

        // buffers for all that jazz
        CollideableGeometrySsbo _collideableGeometrySsbo;
    };
}