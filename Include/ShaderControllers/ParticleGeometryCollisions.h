#pragma once

#include <string>

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Buffers/SSBOs/CollideableGeometrySsbo.h"


namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        This compute controller is responsible for loading geometry from the provided Blender3D 
        .obj file and setting up and calling the appropriate compute shaders to get particles to 
        bounce off the geometry.
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
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

        // the "without profiling" and "with profiling" go through these same steps
        
        // TODO: split into "detect" and "resolve", with "detect" checking for bounding box overlaps and putting PolygonFace indexes into the ParticlePotentialCollisionsBuffer (move buffer out of the ParticleCollisions/Buffers/ folder and up to a folder that both particle collisions and particle-geometry collisions can access) and with "resolve" checking for boundary crossings and resolving per-particle
        void DetectAndResolveCollisions(unsigned int numWorkGroupsX) const;

        // buffers for all that jazz
        CollideableGeometrySsbo _collideableGeometrySsbo;

        // for debugging
        ParticleSsbo::SharedConstPtr _particleSsbo;
    };
}