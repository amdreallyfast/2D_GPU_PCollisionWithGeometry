#pragma once

#include <string>

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Buffers/SSBOs/ParticleGeometryCollisions/CollidableGeometrySsbo.h"
#include "Include/Buffers/SSBOs/ParticleGeometryCollisions/CollidableGeometryBvhNodeSsbo.h"
#include "Include/Buffers/SSBOs/ParticleGeometryCollisions/CollidableGeometrySortingDataSsbo.h"
#include "Include/Buffers/SSBOs/ParticleGeometryCollisions/CollidableGeometryPrefixSumSsbo.h"
#include "Include/Buffers/SSBOs/ParticleCollisions/ParticlePotentialCollisionsSsbo.h"
#include "Include/Buffers/SSBOs/VisualizationOnly/CollidableGeometryBoundingBoxGeometrySsbo.h"
#include "Include/Buffers/SSBOs/VisualizationOnly/CollidableGeometrySurfaceNormalsSsbo.h"


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
        const VertexSsboBase &GetCollidableGeometrySsbo() const;
        const VertexSsboBase &GetCollidableGeometryNormals() const;
        const VertexSsboBase &GetCollidableGeometryBoundingBoxesSsbo() const;

    private:
        // sorting
        void AssembleSortingShaders();
        unsigned int _programIdCopyGeometryToCopyBuffer;
        unsigned int _programIdGenerateSortingData;
        unsigned int _programIdPrefixScanStage1;
        unsigned int _programIdPrefixScanStage2;
        unsigned int _programIdPrefixScanStage3;
        unsigned int _programIdSortSortingDataWithPrefixSums;
        unsigned int _programIdSortGeometry;

        void SortGeometryWithoutProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const;
        void SortGeometryWithProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const;

        //void ResolveCollisionsWithoutProfiling(unsigned int numWorkGroupsX) const;
        //void ResolveCollisionsWithProfiling(unsigned int numWorkGroupsX) const;

        // the "without profiling" and "with profiling" go through these same steps
        void PrepareToSortGeometry(unsigned int numWorkGroupsX) const;
        void PrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset) const;
        void SortSortingDataWithPrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const;
        void SortGeometry(unsigned int numWorkGroupsX, unsigned int sortingDataReadOffset) const;

        //// TODO: split into "detect" and "resolve", with "detect" checking for bounding box overlaps and putting PolygonFace indexes into the PotentialParticleCollisionsBuffer (move buffer out of the ParticleCollisions/Buffers/ folder and up to a folder that both particle collisions and particle-geometry collisions can access) and with "resolve" checking for boundary crossings and resolving per-particle
        //void DetectAndResolveCollisions(unsigned int numWorkGroupsX) const;

        // buffers for all that jazz
        CollidableGeometrySsbo _collideableGeometrySsbo;
        CollidableGeometrySortingDataSsbo _sortingDataSsbo;
        CollidableGeometryPrefixSumSsbo _prefixSumSsbo;
        CollidableGeometryBvhNodeSsbo _bvhNodeSsbo;
        ParticlePotentialCollisionsSsbo _potentialCollisionsSsbo;

        // for visualization only
        CollidableGeometryBoundingBoxGeometrySsbo _boundingBoxGeometrySsbo;
        CollidableGeometrySurfaceNormalsSsbo _surfaceNormalGeometrySsbo;

        // for debugging
        ParticleSsbo::SharedConstPtr _originalParticleSsbo;
    };
}