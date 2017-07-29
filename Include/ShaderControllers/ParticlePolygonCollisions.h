#pragma once

#include <string>

#include "Include/Buffers/SSBOs/ParticleSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonBvhNodeSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonSortingDataSsbo.h"
#include "Include/Buffers/SSBOs/ParticlePolygonCollisions/CollidablePolygonPrefixSumSsbo.h"
#include "Include/Buffers/SSBOs/ParticleParticleCollisions/ParticlePotentialCollisionsSsbo.h"
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

        void GenerateCollidableGeometryBvh() const;
        void SortCollidableGeometry(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const;
        void GenerateBvh(unsigned int numWorkGroupsX);

        //void ResolveCollisionsWithoutProfiling(unsigned int numWorkGroupsX) const;
        //void ResolveCollisionsWithProfiling(unsigned int numWorkGroupsX) const;

        void PrepareToSortGeometry(unsigned int numWorkGroupsX) const;
        void PrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset) const;
        void SortSortingDataWithPrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const;
        void SortGeometryWithSortingData(unsigned int numWorkGroupsX, unsigned int sortingDataReadOffset) const;
        
        void PrepareForBinaryTree(unsigned int numWorkGroupsX) const;
        void GenerateBinaryRadixTree(unsigned int numWorkGroupsX) const;
        void MergeNodesIntoBvh(unsigned int numWorkGroupsX) const;


        //// TODO: split into "detect" and "resolve", with "detect" checking for bounding box overlaps and putting PolygonFace indexes into the PotentialParticleCollisionsBuffer (move buffer out of the ParticleParticleCollisions/Buffers/ folder and up to a folder that both particle collisions and particle-geometry collisions can access) and with "resolve" checking for boundary crossings and resolving per-particle
        //void DetectAndResolveCollisions(unsigned int numWorkGroupsX) const;

        // buffers for all that jazz
        CollidablePolygonSsbo _collideableGeometrySsbo;
        CollidablePolygonSortingDataSsbo _sortingDataSsbo;
        CollidablePolygonPrefixSumSsbo _prefixSumSsbo;
        CollidablePolygonBvhNodeSsbo _bvhNodeSsbo;
        ParticlePotentialCollisionsSsbo _potentialCollisionsSsbo;

        // for visualization only
        CollidablePolygonBoundingBoxGeometrySsbo _boundingBoxGeometrySsbo;
        CollidablePolygonSurfaceNormalGeometrySsbo _surfaceNormalGeometrySsbo;

        // for debugging
        ParticleSsbo::SharedConstPtr _originalParticleSsbo;
    };
}