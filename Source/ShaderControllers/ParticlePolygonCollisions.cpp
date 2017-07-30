
#include "Include/ShaderControllers/ParticlePolygonCollisions.h"

#include "Shaders/ShaderStorage.h"
#include "ThirdParty/glload/include/glload/gl_4_4.h"

#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"

// for profiling and checking results
#include "Include/ShaderControllers/ProfilingWaitToFinish.h"
#include "Include/Buffers/SortingData.h"
#include "Include/Buffers/Particle.h"
#include "Include/Buffers/BvhNode.h"
#include "Include/Buffers/PotentialParticleCollisions.h"
#include "Include/Geometry/Box2D.h"
#include "Include/Geometry/PolygonFace.h"

#include <chrono>
#include <fstream>
#include <iostream>
using std::cout;
using std::endl;

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
    ParticlePolygonCollisions::ParticlePolygonCollisions(
        const std::string &blenderObjFilePath, const ParticleSsbo::SharedConstPtr particleSsbo) :
        _programIdCopyGeometryToCopyBuffer(0),
        _programIdGenerateSortingData(0),
        _programIdPrefixScanStage1(0),
        _programIdPrefixScanStage2(0),
        _programIdPrefixScanStage3(0),
        _programIdSortSortingDataWithPrefixSums(0),
        _programIdSortGeometry(0),
        _programIdGuaranteeSortingDataUniqueness(0),
        _programIdGenerateLeafNodeBoundingBoxes(0),
        _programIdGenerateBinaryRadixTree(0),
        _programIdMergeBoundingVolumes(0),
        _programIdDetectCollisions(0),
        _programIdResolveCollisions(0),
        _programIdGeneratePolygonBoundingBoxGeometry(0),

        _collideablePolygonSsbo(blenderObjFilePath),
        _sortingDataSsbo(_collideablePolygonSsbo.NumPolygons()),
        _prefixSumSsbo(_collideablePolygonSsbo.NumPolygons()),
        _bvhNodeSsbo(_collideablePolygonSsbo.NumPolygons()),
        _potentialCollisionsSsbo(particleSsbo->NumParticles()),
        _boundingBoxGeometrySsbo(_collideablePolygonSsbo.NumPolygons()),
        _surfaceNormalGeometrySsbo(blenderObjFilePath),
        _originalParticleSsbo(particleSsbo)
    {
        AssembleSortingShaders();
        AssembleBvhShaders();
        AssembleCollisionShaders();
        AssembleGeometryCreationShaders();

        // load the buffer size uniforms where the SSBOs will be used
        _collideablePolygonSsbo.ConfigureConstantUniforms(_programIdCopyGeometryToCopyBuffer);
        _collideablePolygonSsbo.ConfigureConstantUniforms(_programIdGenerateSortingData);
        _collideablePolygonSsbo.ConfigureConstantUniforms(_programIdSortGeometry);
        _collideablePolygonSsbo.ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        _collideablePolygonSsbo.ConfigureConstantUniforms(_programIdDetectCollisions);

        _sortingDataSsbo.ConfigureConstantUniforms(_programIdGenerateSortingData);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage1);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdSortGeometry);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdGuaranteeSortingDataUniqueness);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdGenerateBinaryRadixTree);

        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage1);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage2);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage3);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);

        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdGenerateBinaryRadixTree);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdMergeBoundingVolumes);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdGeneratePolygonBoundingBoxGeometry);

        _potentialCollisionsSsbo.ConfigureConstantUniforms(_programIdDetectCollisions);

        _boundingBoxGeometrySsbo.ConfigureConstantUniforms(_programIdGeneratePolygonBoundingBoxGeometry);

        particleSsbo->ConfigureConstantUniforms(_programIdDetectCollisions);


        // geometry doesn't move, so its BVH will be static through the life of the program
        GenerateCollidablePolygonBvh();
        GenerateBoundingBoxGeometry();


        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Cleans up shader programs that were created for this shader controller.  The SSBOs clean 
        themselves up.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    ParticlePolygonCollisions::~ParticlePolygonCollisions()
    {
        glDeleteProgram(_programIdCopyGeometryToCopyBuffer);
        glDeleteProgram(_programIdGenerateSortingData);
        glDeleteProgram(_programIdPrefixScanStage1);
        glDeleteProgram(_programIdPrefixScanStage2);
        glDeleteProgram(_programIdPrefixScanStage3);
        glDeleteProgram(_programIdSortSortingDataWithPrefixSums);
        glDeleteProgram(_programIdSortGeometry);

        glDeleteProgram(_programIdGuaranteeSortingDataUniqueness);
        glDeleteProgram(_programIdGenerateLeafNodeBoundingBoxes);
        glDeleteProgram(_programIdGenerateBinaryRadixTree);
        glDeleteProgram(_programIdMergeBoundingVolumes);

        glDeleteProgram(_programIdDetectCollisions);
        glDeleteProgram(_programIdResolveCollisions);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in particles that collide
        with the collidable polygons receiving new position, previous position, and velocity
        vectors.
    
        Note: Particle-particle collisions may happen before or after this.  Whichever happens
        prior will set new "prev particle position" and "new particle position", and whichever 
        happens later will analyze them and set them again.
    Parameters: 
        withProfiling   If true, performs the collision detection and resolution with 
                        std::chrono calls, forced waiting for each shader to finish, and 
                        reporting of durations to stdout and to files. 
                        
                        If false, then everything is performed as fast as possible (no timing or
                        forced waiting or writing to stdout)
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::DetectAndResolve(bool withProfiling) const
    {
        // Note: unlike the rest of the shaders in this shader controller, particle-polygon 
        // collision detection is governed by the number of threads (one thread per particle), 
        // NOT by the number of collidable polygons.
        unsigned int numParticles = _originalParticleSsbo->NumParticles();
        int numWorkGroupsX = numParticles / WORK_GROUP_SIZE_X;
        int remainder = numParticles % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        if (withProfiling)
        {
            cout << "detecting collisions for up to " << numParticles << " particles with " << _collideablePolygonSsbo.NumPolygons() << " polygons" << endl;

            // for profiling
            using namespace std::chrono;
            steady_clock::time_point start;
            steady_clock::time_point end;
            long long durationDetectCollisions = 0;
            long long durationResolveCollisions = 0;

            start = high_resolution_clock::now();
            DetectCollisions(numWorkGroupsX);
            WaitForComputeToFinish();
            end = high_resolution_clock::now();
            durationDetectCollisions = duration_cast<microseconds>(end - start).count();

            start = high_resolution_clock::now();
            //ResolveCollisions(numWorkGroupsX);
            WaitForComputeToFinish();
            end = high_resolution_clock::now();
            durationResolveCollisions = duration_cast<microseconds>(end - start).count();

            // report the results to file
            // Note: Write the results to a tab-delimited text file so that I can dump them into an 
            // Excel spreadsheet.
            std::ofstream outFile("ProfilingDurations/DetectAndResolveParticlePolygonCollisionsDuration.txt");
            if (outFile.is_open())
            {
                long long totalSortingTime = durationDetectCollisions + durationResolveCollisions;

                cout << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;
                outFile << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;

                cout << "detect collisions: " << durationDetectCollisions << "\tmicroseconds" << endl;
                outFile << "detect collisions: " << durationDetectCollisions << "\tmicroseconds" << endl;

                cout << "resolve collisions: " << durationResolveCollisions << "\tmicroseconds" << endl;
                outFile << "resolve collisions: " << durationResolveCollisions << "\tmicroseconds" << endl;
            }
            outFile.close();
        }
        else
        {
            DetectCollisions(numWorkGroupsX);
            ResolveCollisions(numWorkGroupsX);
        }
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Used so that the RenderGeometry shader controller can draw the geometry that the 
        particles are colliding with.
    Parameters: None
    Returns:    
        The SSBO that contains the collideable geometry's vertices.
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    const VertexSsboBase &ParticlePolygonCollisions::GetCollidableGeometrySsbo() const
    {
        return _collideablePolygonSsbo;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Used by the RenderGeometry shader controller to draw the collidable geometry's surface 
        normals to see if particle collisions look right.
    Parameters: None
    Returns:    
        The SSBO that contains the collideable geometry's surface normals.
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    const VertexSsboBase &ParticlePolygonCollisions::GetCollidableGeometryNormals() const
    {
        return _surfaceNormalGeometrySsbo;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Used by the RenderGeometry shader controller to draw the collidable geometry's bounding 
        boxes.  It is little more than a nice visualization.
    Parameters: None
    Returns:    
        The SSBO that contains the collideable geometry's bounding box geometry.
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    const VertexSsboBase &ParticlePolygonCollisions::GetCollidableGeometryBoundingBoxesSsbo() const
    {
        return _boundingBoxGeometrySsbo;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Primarily serves to clean up the constructor.

        Assembles headers, buffers, and functional .comp files for the shaders that sort the 
        collidable geometry by their position-generated Morton Codes.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::AssembleSortingShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "copy geometry to copy buffer";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/Sorting/CopyGeometryToCopyBuffer.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdCopyGeometryToCopyBuffer = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate geometry sorting data";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/Sorting/GenerateGeometrySortingData.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateSortingData = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "geometry prefix scan stage 1";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/Sorting/PrefixScanStage1.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage1 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "geometry prefix scan stage 2";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/Sorting/PrefixScanStage2.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage2 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "geometry prefix scan stage 3";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/Sorting/PrefixScanStage3.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage3 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "sort geometry sorting data with prefix sums";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/Sorting/SortSortingDataWithPrefixSums.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdSortSortingDataWithPrefixSums = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "sort geometry";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/Sorting/SortCollidablePolygons.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdSortGeometry = shaderStorageRef.GetShaderProgram(shaderKey);

        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Primarily serves to clean up the constructor.

        Assembles headers, buffers, and functional .comp files for the shaders that generate the 
        bounding volume hierarchy. 
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::AssembleBvhShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "guarantee collidable geometry sorting data uniqueness";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/BvhGeneration/GuaranteeSortingDataUniqueness.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGuaranteeSortingDataUniqueness = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate collidable geometry bounding boxes";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/BvhGeneration/GenerateLeafNodeBoundingBoxes.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateLeafNodeBoundingBoxes = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate collidable geometry binary radix tree";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/BvhGeneration/GenerateBinaryRadixTree.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateBinaryRadixTree = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "merge collidable geometry bounding volumes";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/BvhGeneration/MergeBoundingVolumes.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdMergeBoundingVolumes = shaderStorageRef.GetShaderProgram(shaderKey);

        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Primarily serves to clean up the constructor.

        Assembles headers, buffers, and functional .comp files for the shaders that detect and 
        resolve particle-polygon collisions.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::AssembleCollisionShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "detect particle-polygon collisions";
        filePath = "Shaders/Compute/Collisions/ParticlePolygon/DetectParticlePolygonCollisions.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdDetectCollisions = shaderStorageRef.GetShaderProgram(shaderKey);

        //shaderKey = "resolve particle-polygon collisions";
        //filePath = "Shaders/Compute/Collisions/ParticlePolygon/ResolveParticlePolygonCollisions.comp";
        //shaderStorageRef.NewShader(shaderKey);
        //shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        //shaderStorageRef.LinkShader(shaderKey);
        //_programIdResolveCollisions = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Primarily serves to clean up the constructor.

        Assembles headers, buffers, and functional .comp files for the shaders that generate 
        renderable geometry out of non-renderable stuff.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::AssembleGeometryCreationShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "generate collidable polygon BVH geometry";
        filePath = "Shaders/Compute/Visualization/GenerateCollidablePolygonBoundingBoxGeometry.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGeneratePolygonBoundingBoxGeometry = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the dispatches of many compute shaders that will eventually result 
        in a bounding volume hierarchy (BVH).  Geometry doesn't move in htis program, so this 
        BVH will not change after its first creation.

        Note: Unlike ParticleParticleCollisions::DetectAndResolve(...), there are no sorting 
        "with profiling" and "without profiling" options because collidable geometry is static 
        after creation and therefore doesn't need to be run every frame and therefore doesn't 
        require high performance and therefore does not require profiling to determine where the 
        performance bottlenecks are.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::GenerateCollidablePolygonBvh() const
    {
        int numWorkGroupsX = _collideablePolygonSsbo.NumPolygons() / WORK_GROUP_SIZE_X;
        int remainder = _collideablePolygonSsbo.NumPolygons() % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        // the prefix scan works on 2 items per thread
        // Note: See description of the ParticlePrefixScanSsbo constructor for why the prefix 
        // scan algorithm needs its own work group size calculation.
        unsigned int numItemsInPrefixScanBuffer = _prefixSumSsbo.NumDataEntries();
        int numWorkGroupsXForPrefixSum = numItemsInPrefixScanBuffer / (WORK_GROUP_SIZE_X * 2);
        remainder = numItemsInPrefixScanBuffer % (WORK_GROUP_SIZE_X * 2);
        numWorkGroupsXForPrefixSum += (remainder == 0) ? 0 : 1;

        SortCollidablePolygons(numWorkGroupsX, numWorkGroupsXForPrefixSum);
        GenerateBvh(numWorkGroupsX);
        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in sorting the 
        CollidablePolygonBuffer and CollidablePolygonSortingDataBuffer.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
        numWorkGroupsXPrefixScan    See comment where this value was calculated.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::SortCollidablePolygons(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const
    {
        PrepareToSortGeometry(numWorkGroupsX);
        
        // Morton Codes are 30bits
        unsigned int totalBitCount = 32;

        bool writeToSecondBuffer = true;
        unsigned int sortingDataReadBufferOffset = 0;
        unsigned int sortingDataWriteBufferOffset = 0;
        for (unsigned int bitNumber = 0; bitNumber < totalBitCount; bitNumber++)
        {
            sortingDataReadBufferOffset = static_cast<unsigned int>(!writeToSecondBuffer) * _collideablePolygonSsbo.NumPolygons();
            sortingDataWriteBufferOffset = static_cast<unsigned int>(writeToSecondBuffer) * _collideablePolygonSsbo.NumPolygons();

            PrefixScan(numWorkGroupsXPrefixScan, bitNumber, sortingDataReadBufferOffset);
            SortSortingDataWithPrefixScan(numWorkGroupsX, bitNumber, sortingDataReadBufferOffset, sortingDataWriteBufferOffset);

            // swap read/write buffers and do it again
            writeToSecondBuffer = !writeToSecondBuffer;
        }

        // the sorting data's final location is in the "write" half of the sorting data buffer
        SortCollidablePolygonsUsingSortingData(numWorkGroupsX, sortingDataWriteBufferOffset);
        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in a balanced binary tree of 
        bounding boxes from the leaves (collidable polygons) up to the root of the tree.
    Parameters: 
        numWorkGroupsX  Expected to be the total polygon count divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::GenerateBvh(unsigned int numWorkGroupsX) const
    {
        PrepareForBinaryTree(numWorkGroupsX);
        GenerateBinaryRadixTree(numWorkGroupsX);
        MergeNodesIntoBvh(numWorkGroupsX);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that fills in the 
        PotentialParticlePolygonCollisionsBuffer.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.  No, 
                        that is not a typo.  Particle-polygon collision detection uses one 
                        thread per particle, NOT per polygon.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::DetectCollisions(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdDetectCollisions);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //{
        //    unsigned int startingIndexBytes = 0;
        //    std::vector<BvhNode> checkParticleBvhNodes(_originalParticleSsbo->NumParticles());
        //    unsigned int bufferSizeBytes = checkParticleBvhNodes.size() * sizeof(BvhNode);
        //    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 5);
        //    void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //    memcpy(checkParticleBvhNodes.data(), bufferPtr, bufferSizeBytes);
        //    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        //}


        // for verifying/debugging
        unsigned int startingIndexBytes = 0;
        std::vector<PotentialParticleCollisions> checkPotentialCollisions(_potentialCollisionsSsbo.NumItems());
        unsigned int bufferSizeBytes = checkPotentialCollisions.size() * sizeof(SortingData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _potentialCollisionsSsbo.BufferId());
        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkPotentialCollisions.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        for (size_t i = 0; i < checkPotentialCollisions.size(); i++)
        {
            const PotentialParticleCollisions &potentialCollisions = checkPotentialCollisions[i];
            if (potentialCollisions._numPotentialCollisions != 0)
            {
                printf("");
            }
        }
        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in particles that collide
        with the collidable polygons receiving new position, previous position, and velocity
        vectors.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.  No, 
                        that is not a typo.  Particle-polygon collision detection uses one 
                        thread per particle, NOT per polygon.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::ResolveCollisions(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdResolveCollisions);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that generates geometry out of the polygon bounding boxes
    Parameters: 
        numWorkGroupsX  Expected to be the total polygon count divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::GenerateBoundingBoxGeometry() const
    {
        int numWorkGroupsX = _collideablePolygonSsbo.NumPolygons() / WORK_GROUP_SIZE_X;
        int remainder = _collideablePolygonSsbo.NumPolygons() % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        glUseProgram(_programIdGeneratePolygonBoundingBoxGeometry);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //// for verifying/debugging
        //unsigned int startingIndexBytes = 0;
        //std::vector<Box2D> checkGeometryData(_boundingBoxGeometrySsbo.NumBoxes());
        //unsigned int bufferSizeBytes = checkGeometryData.size() * sizeof(Box2D);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _boundingBoxGeometrySsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkGeometryData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of geometry sorting.
    Parameters: 
        numWorkGroupsX      Expected to be number of polygons divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::PrepareToSortGeometry(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdCopyGeometryToCopyBuffer);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateSortingData);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on different buffers, so only need one memory barrier 
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //// for verifying/debugging
        //unsigned int startingIndexBytes = 0;
        //std::vector<SortingData> checkSortingData(_sortingDataSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of sorting.  This is where the main magic of sorting takes place through a parallel 
        prefix sum (usually called a prefix "scan").  

        Note: The work group size is special here.  The algorithm calls for each thread to work 
        on two items, so the expected work group count is the number of polygons divided by 2x 
        the work group size.  Sufficiant buffer size was allocated for this algorithm in 
        GeometryPrefixSumSsbo.
    Parameters: 
        numWorkGroupsX          See Description
        bitNumber               0 - 32
        sortingDataReadOffset   The sorting data is read from this half of the buffer.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::PrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset) const
    {
        //unsigned int startingIndexBytes = 0;
        //std::vector<unsigned int> checkPrefixScan(_prefixSumSsbo.TotalBufferEntries());
        //unsigned int bufferSizeBytes = checkPrefixScan.size() * sizeof(unsigned int);
        //void *bufferPtr = nullptr;

        glUseProgram(_programIdPrefixScanStage1);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_POLYGON_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _prefixSumSsbo.BufferId());
        //bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkPrefixScan.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glUseProgram(_programIdPrefixScanStage2);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _prefixSumSsbo.BufferId());
        //bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkPrefixScan.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glUseProgram(_programIdPrefixScanStage3);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _prefixSumSsbo.BufferId());
        //bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkPrefixScan.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //// verify the sum
        //// Note: Start +1 so the first pass can do "i-1", but index 0 is "total number of ones", 
        //// so start at 2.
        //for (size_t i = 2; i < checkPrefixScan.size(); i++)
        //{
        //    // each successive value must be greater than or equal to the sum before it
        //    if (checkPrefixScan[i] < checkPrefixScan[i - 1])
        //    {
        //        printf("");
        //    }
        //    //else if (checkPrefixScan[i] != 0)
        //    //{
        //    //    printf("");
        //    //}
        //}
        //printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.  
    Parameters: 
        numWorkGroupsX          Expected to be number of polygons divided by work group size.
        bitNumber               0 - 32
        sortingDataReadOffset   The sorting data is read from this half of the buffer...
        sortingDataWriteOffset  And sorted according to the prefix sums into this half.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::SortSortingDataWithPrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const
    {
        ////unsigned int startingIndexBytes = sortingDataWriteOffset * sizeof(SortingData);
        //unsigned int startingIndexBytes = 0;
        //std::vector<SortingData> checkSortingData(_sortingDataSsbo.NumItems() * 2);
        //unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //void *bufferPtr = nullptr;

        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        //bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        glUseProgram(_programIdSortSortingDataWithPrefixSums);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_POLYGON_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_POLYGON_SORTING_DATA_BUFFER_WRITE_OFFSET, sortingDataWriteOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        //bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //std::vector<unsigned int> binaryBuffer(checkSortingData.size());
        //for (size_t i = 0; i < checkSortingData.size(); i++)
        //{
        //    unsigned int bitVal = (checkSortingData[i]._sortingData >> bitNumber) & 1;
        //    binaryBuffer[i] = bitVal;
        //    if (bitVal == 1)
        //    {
        //        printf("");
        //    }
        //}
        //printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        The end of collidable geometry sorting.
    Parameters: 
        numWorkGroupsX          Expected to be number of polygons divided by work group size.
        sortingDataReadOffset   Tells the shader which half of the 
                                CollidablePolygonSortingDataBuffer has the latest sort values.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::SortCollidablePolygonsUsingSortingData(unsigned int numWorkGroupsX, unsigned int sortingDataReadOffset) const
    {
        //// verify sorted data
        //// Note: Only need to copy the first half of the buffer.  This is where the last loop of 
        //// the radix sorting algorithm put the sorting data.
        ////start = high_resolution_clock::now();
        //unsigned int startingIndexBytes = sortingDataReadOffset;
        //std::vector<SortingData> checkSortingData(_sortingDataSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        //for (unsigned int i = 1; i < checkSortingData.size(); i++)
        //{
        //    // start at 1 so that prevIndex isn't out of bounds
        //    unsigned int thisIndex = i;
        //    unsigned int prevIndex = i - 1;
        //    unsigned int val = checkSortingData[thisIndex]._sortingData;
        //    unsigned int prevVal = checkSortingData[prevIndex]._sortingData;
        //    if (val < prevVal)
        //    {
        //        printf("value %u at index %u is >= previous value %u and index %u\n", val, i, prevVal, i - 1);
        //    }
        //}
        //printf("");

        glUseProgram(_programIdSortGeometry);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_POLYGON_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Modifies the CollidablePolygonSortingDataBuffer so that the resulting tree won't have 
        depth spikes due to duplicate entries, then gives each leaf node in the 
        CollidablePolygonBvhNodeBuffer a bounding box based on the polygon that it is 
        associated with.
    Parameters: 
        numWorkGroupsX      Expected to be number of polygons divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::PrepareForBinaryTree(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGuaranteeSortingDataUniqueness);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateLeafNodeBoundingBoxes);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on independent data, so only need one memory barrier at the end
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //unsigned int startingIndexBytes = 0;
        //unsigned int bufferSizeBytes = 0;
        //void *bufferPtr = nullptr;

        //startingIndexBytes = 0;
        //std::vector<BvhNode> checkBinaryTree(_bvhNodeSsbo.NumTotalNodes());
        //bufferSizeBytes = checkBinaryTree.size() * sizeof(BvhNode);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bvhNodeSsbo.BufferId());
        //bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkBinaryTree.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //startingIndexBytes = 0;
        //std::vector<SortingData> checkSortingData(_sortingDataSsbo.NumItems());
        //bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        //bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //// check for duplicate elements
        //// Note: This is an array of sorted elements, but the elements themselves are not on the 
        //// range [0,n-1], so I can't do the fanciest and most efficient checks.  I also don't 
        //// want to bother using a hash approach (std::map<...>), and this is just a debugging 
        //// thing anyway, so go brute force.
        //for (size_t i = 0; i < checkSortingData.size(); i++)
        //{
        //    for (size_t j = i + 1; j < checkSortingData.size(); j++)
        //    {
        //        if (checkSortingData[i]._sortingData == checkSortingData[j]._sortingData)
        //        {
        //            printf("");
        //        }
        //    }
        //}
        //printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        All that sorting to get to here.
    Parameters: 
        numWorkGroupsX      Expected to be the number of polygons divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::GenerateBinaryRadixTree(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGenerateBinaryRadixTree);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //// verify that the binary tree is valid by checking that all parent-child relationships 
        //// are reciprocated 
        //// Note: By virtue of being a binary tree, every node except the root has a parent, and 
        //// that parent also specifies that node as a child exactly once.
        //unsigned int startingIndexBytes = 0;
        //std::vector<BvhNode> checkBinaryTree(_bvhNodeSsbo.NumTotalNodes());
        //unsigned int bufferSizeBytes = checkBinaryTree.size() * sizeof(BvhNode);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bvhNodeSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkBinaryTree.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //// check the root node (no parent, only children)
        //int rootnodeindex = _bvhNodeSsbo.NumLeafNodes();
        //const BvhNode &rootnode = checkBinaryTree[rootnodeindex];
        //if ((rootnodeindex != checkBinaryTree[rootnode._leftChildIndex]._parentIndex) &&
        //    (rootnodeindex != checkBinaryTree[rootnode._rightChildIndex]._parentIndex))
        //{
        //    // root-child relationship not reciprocated
        //    printf("");
        //}

        //// check all the other nodes (have parents, leaves don't have children)
        //for (size_t thisNodeIndex = 0; thisNodeIndex < checkBinaryTree.size(); thisNodeIndex++)
        //{
        //    const BvhNode &thisNode = checkBinaryTree[thisNodeIndex];

        //    if (thisNode._parentIndex == -1)
        //    {
        //        // skip if it is the root; everyone else should have a parent
        //        if (thisNodeIndex != _bvhNodeSsbo.NumLeafNodes())
        //        {
        //            // bad: non-root node has a -1 parent
        //            printf("");
        //        }
        //    }
        //    else
        //    {
        //        // this node is only one of the parent's childre, so the parent-child 
        //        // relationship is only a problem if neither of the parent's child are this one
        //        if ((thisNodeIndex != checkBinaryTree[thisNode._parentIndex]._leftChildIndex) &&
        //            (thisNodeIndex != checkBinaryTree[thisNode._parentIndex]._rightChildIndex))
        //        {
        //            // parent-child relationship not reciprocated
        //            printf("");
        //        }
        //    }
        //}

        //printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        And finally the binary radix tree blooms with beautiful bounding boxes into a Bounding 
        Volume Hierarchy.  I'm tired and am thinking of nice "tree in spring" analogy.  The 
        analogy starts to fall apart when I think of creating the tree anew ~60x/sec.  
    Parameters: 
        numWorkGroupsX      Expected to be number of polygons divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticlePolygonCollisions::MergeNodesIntoBvh(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdMergeBoundingVolumes);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //// for debugging
        //unsigned int startingIndexBytes = 0;
        //std::vector<BvhNode> checkBinaryTree(_bvhNodeSsbo.NumTotalNodes());
        //unsigned int bufferSizeBytes = checkBinaryTree.size() * sizeof(BvhNode);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bvhNodeSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkBinaryTree.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}
