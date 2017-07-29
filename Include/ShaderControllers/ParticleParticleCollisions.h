#pragma once

#include <memory>
#include <string>

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Buffers/SSBOs/ParticleParticleCollisions/ParticleBvhNodeSsbo.h"
#include "Include/Buffers/SSBOs/ParticleParticleCollisions/ParticlePropertiesSsbo.h"
#include "Include/Buffers/SSBOs/ParticleParticleCollisions/ParticleSortingDataSsbo.h"
#include "Include/Buffers/SSBOs/ParticleParticleCollisions/ParticlePrefixSumSsbo.h"
#include "Include/Buffers/SSBOs/ParticleParticleCollisions/ParticlePotentialCollisionsSsbo.h"
#include "Include/Buffers/SSBOs/VisualizationOnly/ParticleVelocityVectorGeometrySsbo.h"
#include "Include/Buffers/SSBOs/VisualizationOnly/ParticleBoundingBoxGeometrySsbo.h"


namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        This compute controller is responsible for generating a BVH from a sorted Particle SSBO, 
        then having each particle check for possible collisions and resolve as necessary.
    Creator:    John Cox, 3/2017
    --------------------------------------------------------------------------------------------*/
    class ParticleParticleCollisions
    {
    public:
        ParticleParticleCollisions(const ParticleSsbo::SharedConstPtr particleSsbo, const ParticlePropertiesSsbo::SharedConstPtr particlePropertiesSsbo);
        ~ParticleParticleCollisions();

        void DetectAndResolve(bool withProfiling, bool generateGeometry) const;
        const VertexSsboBase &ParticleVelocityVectorSsbo() const;
        const VertexSsboBase &ParticleBoundingBoxSsbo() const;

    private:
        unsigned int _numParticles;

        // sorting
        void AssembleSortingShaders();
        unsigned int _programIdCopyParticlesToCopyBuffer;
        unsigned int _programIdGenerateSortingData;
        unsigned int _programIdPrefixScanStage1;
        unsigned int _programIdPrefixScanStage2;
        unsigned int _programIdPrefixScanStage3;
        unsigned int _programIdSortSortingDataWithPrefixSums;
        unsigned int _programIdSortParticles;

        // organization
        void AssembleBvhShaders();
        unsigned int _programIdGuaranteeSortingDataUniqueness;
        unsigned int _programIdGenerateLeafNodeBoundingBoxes;
        unsigned int _programIdGenerateBinaryRadixTree;
        unsigned int _programIdMergeBoundingVolumes;

        // all that for the coup de grace
        void AssembleCollisionShaders();
        unsigned int _programIdDetectCollisions;
        unsigned int _programIdResolveCollisions;

        // for drawing pretty things
        unsigned int _programIdGenerateVerticesParticleVelocityVectors;
        unsigned int _programIdGenerateVerticesParticleBoundingBoxes;

        void SortParticlesWithoutProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const;
        void SortParticlesWithProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const;

        void GenerateBvhWithoutProfiling(unsigned int numWorkGroupsX) const;
        void GenerateBvhWithProfiling(unsigned int numWorkGroupsX) const;

        void DetectAndResolveCollisionsWithoutProfiling(unsigned int numWorkGroupsX) const;
        void DetectAndResolveCollisionsWithProfiling(unsigned int numWorkGroupsX) const;

        // the "without profiling" and "with profiling" go through these same steps
        void PrepareToSortParticles(unsigned int numWorkGroupsX) const;
        void PrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset) const;
        void SortSortingDataWithPrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const;
        void SortParticlesUsingSortingData(unsigned int numWorkGroupsX, unsigned int sortingDataReadOffset) const;

        void PrepareForBinaryTree(unsigned int numWorkGroupsX) const;
        void GenerateBinaryRadixTree(unsigned int numWorkGroupsX) const;
        void MergeNodesIntoBvh(unsigned int numWorkGroupsX) const;
        void DetectCollisions(unsigned int numWorkGroupsX) const;
        void ResolveCollisions(unsigned int numWorkGroupsX) const;

        //// for drawing pretty things
        //void GenerateGeometry(unsigned int numWorkGroupsX) const;

        // buffers for sorting, BVH generation, and anything else that's necessary
        ParticleSortingDataSsbo _sortingDataSsbo;
        ParticlePrefixSumSsbo _prefixSumSsbo;
        ParticleBvhNodeSsbo _bvhNodeSsbo;
        ParticlePotentialCollisionsSsbo _potentialCollisionsSsbo;
        ParticleVelocityVectorGeometrySsbo _velocityVectorGeometrySsbo;
        ParticleBoundingBoxGeometrySsbo _boundingBoxGeometrySsbo;
        //BvhGeometrySsbo _bvhGeometrySsbo;

        // used for verifying that particle sorting is working
        const ParticleSsbo::SharedConstPtr _originalParticleSsbo; 
    };
}
