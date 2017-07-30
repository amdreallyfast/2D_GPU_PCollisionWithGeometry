#pragma once

#include <string>

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonBvhNodeSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonSortingDataSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonPrefixSumSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/PotentialParticlePolygonCollisionsSsbo.h"
#include "Include/Buffers/SSBOs/VisualizationOnly/CollidablePolygonBoundingBoxGeometrySsbo.h"
#include "Include/Buffers/SSBOs/VisualizationOnly/CollidablePolygonSurfaceNormalGeometrySsbo.h"


namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        This compute controller is responsible for loading geometry from the provided Blender3D 
        .obj file and setting up and calling the appropriate compute shaders to get particles to 
        bounce off the geometry.
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    class ParticlePolygonCollisions
    {
    public:
        ParticlePolygonCollisions(const std::string &blenderObjFilePath, const ParticleSsbo::SharedConstPtr particleSsbo);
        ~ParticlePolygonCollisions();

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
        void AssembleGeometryCreationShaders();
        unsigned int _programIdGeneratePolygonBoundingBoxGeometry;

        void GenerateCollidablePolygonBvh() const;
        void SortCollidablePolygons(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const;
        void GenerateBvh(unsigned int numWorkGroupsX) const;
        void DetectCollisions(unsigned int numWorkGroupsX) const;
        void ResolveCollisions(unsigned int numWorkGroupsX) const;
        void GenerateBoundingBoxGeometry() const;

        void PrepareToSortGeometry(unsigned int numWorkGroupsX) const;
        void PrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset) const;
        void SortSortingDataWithPrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const;
        void SortCollidablePolygonsUsingSortingData(unsigned int numWorkGroupsX, unsigned int sortingDataReadOffset) const;
        
        void PrepareForBinaryTree(unsigned int numWorkGroupsX) const;
        void GenerateBinaryRadixTree(unsigned int numWorkGroupsX) const;
        void MergeNodesIntoBvh(unsigned int numWorkGroupsX) const;

        // buffers for all that jazz
        CollidablePolygonSsbo _collideablePolygonSsbo;
        CollidablePolygonSortingDataSsbo _sortingDataSsbo;
        CollidablePolygonPrefixSumSsbo _prefixSumSsbo;
        CollidablePolygonBvhNodeSsbo _bvhNodeSsbo;
        PotentialParticlePolygonCollisionsSsbo _potentialCollisionsSsbo;

        // for visualization only
        CollidablePolygonBoundingBoxGeometrySsbo _boundingBoxGeometrySsbo;
        CollidablePolygonSurfaceNormalGeometrySsbo _surfaceNormalGeometrySsbo;

        // for debugging
        ParticleSsbo::SharedConstPtr _originalParticleSsbo;
    };
}