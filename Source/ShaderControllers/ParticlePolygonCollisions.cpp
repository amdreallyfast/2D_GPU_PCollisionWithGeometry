
#include "Include/ShaderControllers/ParticleGeometryCollisions.h"

#include "Shaders/ShaderStorage.h"
#include "ThirdParty/glload/include/glload/gl_4_4.h"

#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"

// for profiling and checking results
#include "Include/ShaderControllers/ProfilingWaitToFinish.h"
#include "Include/Buffers/SortingData.h"
#include "Include/Buffers/Particle.h"
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
    ParticleGeometryCollisions::ParticleGeometryCollisions(
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

        _collideableGeometrySsbo(blenderObjFilePath),
        _sortingDataSsbo(_collideableGeometrySsbo.NumPolygons()),
        _prefixSumSsbo(_collideableGeometrySsbo.NumPolygons()),
        _bvhNodeSsbo(_collideableGeometrySsbo.NumPolygons()),
        _potentialCollisionsSsbo(particleSsbo->NumParticles()),
        _boundingBoxGeometrySsbo(_collideableGeometrySsbo.NumPolygons()),
        _surfaceNormalGeometrySsbo(blenderObjFilePath),
        _originalParticleSsbo(particleSsbo)
    {
        AssembleSortingShaders();

        // load the buffer size uniforms where the SSBOs will be used
        _collideableGeometrySsbo.ConfigureConstantUniforms(_programIdCopyGeometryToCopyBuffer);
        _collideableGeometrySsbo.ConfigureConstantUniforms(_programIdGenerateSortingData);
        _collideableGeometrySsbo.ConfigureConstantUniforms(_programIdSortGeometry);

        _sortingDataSsbo.ConfigureConstantUniforms(_programIdGenerateSortingData);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage1);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdSortGeometry);

        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage1);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage2);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage3);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);

        // geometry doesn't move, so its BVH will be static through the life of the program
        GenerateCollidableGeometryBvh();


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
    ParticleGeometryCollisions::~ParticleGeometryCollisions()
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
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the dispatches of many compute shaders that will eventually result 
        in new positions and particle velocities for any particles that collided with geometry 
        this frame.
        
        Note: Particle-particle collisions may happen before or after this.  Whichever happens 
        prior will set new "prev particle position" and "new particle position", and whichever 
        happens later will analyze them and set them again.
    Parameters: 
        withProfiling   If true, performs the sorting, BVH generation, and collision detection 
                        and resolution with std::chrono calls, forced waiting for each shader to 
                        finish, and reporting of the durations to stdout and to files.  
                        
                        If false, then everything is performed as fast as possible (no timing or 
                        forced waiting or writing to stdout)
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::DetectAndResolve(bool withProfiling) const
    {
        int numWorkGroupsX = _collideableGeometrySsbo.NumPolygons() / WORK_GROUP_SIZE_X;
        int remainder = _collideableGeometrySsbo.NumPolygons() % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        if (withProfiling)
        {
            //ResolveCollisionsWithProfiling(numWorkGroupsX);
        }
        else
        {
            //ResolveCollisionsWithoutProfiling(numWorkGroupsX);
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
    const VertexSsboBase &ParticleGeometryCollisions::GetCollidableGeometrySsbo() const
    {
        return _collideableGeometrySsbo;
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
    const VertexSsboBase &ParticleGeometryCollisions::GetCollidableGeometryNormals() const
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
    const VertexSsboBase &ParticleGeometryCollisions::GetCollidableGeometryBoundingBoxesSsbo() const
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
    void ParticleGeometryCollisions::AssembleSortingShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "copy geometry to copy buffer";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometrySort/CopyGeometryToCopyBuffer.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdCopyGeometryToCopyBuffer = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate geometry sorting data";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometrySort/GenerateGeometrySortingData.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateSortingData = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "geometry prefix scan stage 1";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometrySort/PrefixScanStage1.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage1 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "geometry prefix scan stage 2";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometrySort/PrefixScanStage2.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage2 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "geometry prefix scan stage 3";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometrySort/PrefixScanStage3.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage3 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "sort geometry sorting data with prefix sums";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometrySort/SortSortingDataWithPrefixSums.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdSortSortingDataWithPrefixSums = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "sort geometry";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometrySort/SortCollidableGeometry.comp";
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
    void ParticleGeometryCollisions::AssembleBvhShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "guarantee collidable geometry sorting data uniqueness";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometryBvh/GuaranteeSortingDataUniqueness.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGuaranteeSortingDataUniqueness = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate collidable geometry bounding boxes";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometryBvh/GenerateLeafNodeBoundingBoxes.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateLeafNodeBoundingBoxes = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate collidable geometry binary radix tree";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometryBvh/GenerateBinaryRadixTree.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateBinaryRadixTree = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "merge collidable geometry bounding volumes";
        filePath = "Shaders/Compute/Collisions/ParticleGeometryCollisions/GeometryBvh/MergeBoundingVolumes.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdMergeBoundingVolumes = shaderStorageRef.GetShaderProgram(shaderKey);

    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the dispatches of many compute shaders that will eventually result 
        in a bounding volume hierarchy (BVH).  Geometry doesn't move in htis program, so this 
        BVH will not change after its first creation.

        Note: Unlike ParticleCollisions::DetectAndResolve(...), there are no sorting 
        "with profiling" and "without profiling" options because collidable geometry is static 
        after creation and therefore doesn't need to be run every frame and therefore doesn't 
        require high performance and therefore does not require profiling to determine where the 
        performance bottlenecks are.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::GenerateCollidableGeometryBvh() const
    {
        int numWorkGroupsX = _collideableGeometrySsbo.NumPolygons() / WORK_GROUP_SIZE_X;
        int remainder = _collideableGeometrySsbo.NumPolygons() % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        // the prefix scan works on 2 items per thread
        // Note: See description of the ParticlePrefixScanSsbo constructor for why the prefix 
        // scan algorithm needs its own work group size calculation.
        unsigned int numItemsInPrefixScanBuffer = _prefixSumSsbo.NumDataEntries();
        int numWorkGroupsXForPrefixSum = numItemsInPrefixScanBuffer / (WORK_GROUP_SIZE_X * 2);
        remainder = numItemsInPrefixScanBuffer % (WORK_GROUP_SIZE_X * 2);
        numWorkGroupsXForPrefixSum += (remainder == 0) ? 0 : 1;

        SortCollidableGeometry(numWorkGroupsX, numWorkGroupsXForPrefixSum);
        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in sorting the 
        CollidableGeometryBuffer and CollidableGeometrySortingDataBuffer.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
        numWorkGroupsXPrefixScan    See comment where this value was calculated.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::SortCollidableGeometry(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const
    {
        PrepareToSortGeometry(numWorkGroupsX);
        
        // Morton Codes are 30bits
        unsigned int totalBitCount = 32;

        bool writeToSecondBuffer = true;
        unsigned int sortingDataReadBufferOffset = 0;
        unsigned int sortingDataWriteBufferOffset = 0;
        for (unsigned int bitNumber = 0; bitNumber < totalBitCount; bitNumber++)
        {
            sortingDataReadBufferOffset = static_cast<unsigned int>(!writeToSecondBuffer) * _collideableGeometrySsbo.NumPolygons();
            sortingDataWriteBufferOffset = static_cast<unsigned int>(writeToSecondBuffer) * _collideableGeometrySsbo.NumPolygons();

            PrefixScan(numWorkGroupsXPrefixScan, bitNumber, sortingDataReadBufferOffset);
            SortSortingDataWithPrefixScan(numWorkGroupsX, bitNumber, sortingDataReadBufferOffset, sortingDataWriteBufferOffset);

            // swap read/write buffers and do it again
            writeToSecondBuffer = !writeToSecondBuffer;
        }

        // the sorting data's final location is in the "write" half of the sorting data buffer
        SortGeometryWithSortingData(numWorkGroupsX, sortingDataWriteBufferOffset);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of geometry sorting.
    Parameters: 
        numWorkGroupsX      Expected to be number of polygons divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::PrepareToSortGeometry(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdCopyGeometryToCopyBuffer);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateSortingData);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on different buffers, so only need one memory barrier 
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // for verifying/debugging
        unsigned int startingIndexBytes = 0;
        std::vector<SortingData> checkSortingData(_sortingDataSsbo.NumItems());
        unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
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
    void ParticleGeometryCollisions::PrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset) const
    {
        unsigned int startingIndexBytes = 0;
        std::vector<unsigned int> checkPrefixScan(_prefixSumSsbo.TotalBufferEntries());
        unsigned int bufferSizeBytes = checkPrefixScan.size() * sizeof(unsigned int);
        void *bufferPtr = nullptr;

        glUseProgram(_programIdPrefixScanStage1);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_GEOMETRY_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _prefixSumSsbo.BufferId());
        bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkPrefixScan.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glUseProgram(_programIdPrefixScanStage2);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _prefixSumSsbo.BufferId());
        bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkPrefixScan.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glUseProgram(_programIdPrefixScanStage3);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _prefixSumSsbo.BufferId());
        bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkPrefixScan.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        // verify the sum
        // Note: Start +1 so the first pass can do "i-1", but index 0 is "total number of ones", 
        // so start at 2.
        for (size_t i = 2; i < checkPrefixScan.size(); i++)
        {
            // each successive value must be greater than or equal to the sum before it
            if (checkPrefixScan[i] < checkPrefixScan[i - 1])
            {
                printf("");
            }
            //else if (checkPrefixScan[i] != 0)
            //{
            //    printf("");
            //}
        }
        printf("");
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
    void ParticleGeometryCollisions::SortSortingDataWithPrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const
    {
        //unsigned int startingIndexBytes = sortingDataWriteOffset * sizeof(SortingData);
        unsigned int startingIndexBytes = 0;
        std::vector<SortingData> checkSortingData(_sortingDataSsbo.NumItems() * 2);
        unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        void *bufferPtr = nullptr;

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        glUseProgram(_programIdSortSortingDataWithPrefixSums);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_GEOMETRY_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_GEOMETRY_SORTING_DATA_BUFFER_WRITE_OFFSET, sortingDataWriteOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

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
        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        The end of collidable geometry sorting.
    Parameters: 
        numWorkGroupsX          Expected to be number of polygons divided by work group size.
        sortingDataReadOffset   Tells the shader which half of the 
                                CollidableGeometrySortingDataBuffer has the latest sort values.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::SortGeometryWithSortingData(unsigned int numWorkGroupsX, unsigned int sortingDataReadOffset) const
    {
        glUseProgram(_programIdSortGeometry);
        glUniform1ui(UNIFORM_LOCATION_COLLIDABLE_GEOMETRY_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // verify sorted data
        // Note: Only need to copy the first half of the buffer.  This is where the last loop of 
        // the radix sorting algorithm put the sorting data.
        //start = high_resolution_clock::now();
        unsigned int startingIndexBytes = sortingDataReadOffset;
        std::vector<SortingData> checkSortingData(_sortingDataSsbo.NumItems());
        unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _sortingDataSsbo.BufferId());
        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        for (unsigned int i = 1; i < checkSortingData.size(); i++)
        {
            // start at 1 so that prevIndex isn't out of bounds
            unsigned int thisIndex = i;
            unsigned int prevIndex = i - 1;
            unsigned int val = checkSortingData[thisIndex]._sortingData;
            unsigned int prevVal = checkSortingData[prevIndex]._sortingData;
            if (val < prevVal)
            {
                printf("value %u at index %u is >= previous value %u and index %u\n", val, i, prevVal, i - 1);
            }
        }
        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Modifies the CollidableGeometrySortingDataBuffer so that the resulting tree won't have 
        depth spikes due to duplicate entries, then gives each leaf node in the 
        CollidableGeometryBvhNodeBuffer a bounding box based on the polygon that it is 
        associated with.
    Parameters: 
        numWorkGroupsX      Expected to be number of polygons divided by work group size.
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::PrepareForBinaryTree(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGuaranteeSortingDataUniqueness);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateLeafNodeBoundingBoxes);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on independent data, so only need one memory barrier at the end
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //unsigned int startingIndexBytes = 0;
        //std::vector<SortingData> checkSortingData(_particleSortingDataSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particleSortingDataSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

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

    void ParticleGeometryCollisions::GenerateBinaryRadixTree(unsigned int numWorkGroupsX) const
    {

    }

    void ParticleGeometryCollisions::MergeNodesIntoBvh(unsigned int numWorkGroupsX) const
    {

    }


    ///*--------------------------------------------------------------------------------------------
    //Description:
    //    This method governs the shader dispatches that will result in particles bouncing off any 
    //    geometry that they collide with.
    //Parameters: 
    //    numWorkGroupsX  Expected to be the total particle count divided by the work group size.
    //Returns:    None
    //Creator:    John Cox, 6/2017
    //--------------------------------------------------------------------------------------------*/
    //void ParticleGeometryCollisions::ResolveCollisionsWithoutProfiling(unsigned int numWorkGroupsX) const
    //{
    //    DetectAndResolveCollisions(numWorkGroupsX);
    //}

    ///*--------------------------------------------------------------------------------------------
    //Description:
    //    Like ResolveCollisionsWithoutProfiling(...), but with 
    //    (1) std::chrono calls 
    //    (2) forced wait for shader to finish so that the std::chrono calls get an accurate 
    //        reading for how long the shader takes 
    //    (3) writing the output to a file (if desired)
    //Parameters: 
    //    numWorkGroupsX  Expected to be the total particle count divided by the work group size.
    //Returns:    None
    //Creator:    John Cox, 6/2017
    //--------------------------------------------------------------------------------------------*/
    //void ParticleGeometryCollisions::ResolveCollisionsWithProfiling(unsigned int numWorkGroupsX) const
    //{
    //    cout << "detecting collisions for up to " << _numParticles << " particles with " << 
    //        _collideableGeometrySsbo.NumPolygons() << " 2D polygon faces" << endl;

    //    // for profiling
    //    using namespace std::chrono;
    //    steady_clock::time_point start;
    //    steady_clock::time_point end;
    //    long long durationDetectAndResolveCollisions = 0;

    //    start = high_resolution_clock::now();
    //    DetectAndResolveCollisions(numWorkGroupsX);
    //    WaitForComputeToFinish();
    //    end = high_resolution_clock::now();
    //    durationDetectAndResolveCollisions = duration_cast<microseconds>(end - start).count();

    //    // report the results to file
    //    // Note: Write the results to a tab-delimited text file so that I can dump them into an 
    //    // Excel spreadsheet.
    //    std::ofstream outFile("DetectAndResolveCollisionsDurations.txt");
    //    if (outFile.is_open())
    //    {
    //        long long totalSortingTime = durationDetectAndResolveCollisions;

    //        cout << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;
    //        outFile << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;

    //        cout << "detect and resolve collisions: " << durationDetectAndResolveCollisions << "\tmicroseconds" << endl;
    //        outFile << "detect and resolve collisions: " << durationDetectAndResolveCollisions << "\tmicroseconds" << endl;
    //    }
    //    outFile.close();
    //}

    ///*--------------------------------------------------------------------------------------------
    //Description:
    //    This method performs the actual work of dispatching the shader that will detect and 
    //    resolve the particle collisions with geometry.
    //Parameters: 
    //    numWorkGroupsX  Expected to be the total particle count divided by the work group size.
    //Returns:    None
    //Creator:    John Cox, 6/2017
    //--------------------------------------------------------------------------------------------*/
    //void ParticleGeometryCollisions::DetectAndResolveCollisions(unsigned int numWorkGroupsX) const
    //{
    //    glUseProgram(_programIdResolveCollisions);
    //    glDispatchCompute(numWorkGroupsX, 1, 1);
    //    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_SHADER_BIT);


    //    std::vector<PolygonFace> checkPolygons(_collideableGeometrySsbo.NumPolygons());
    //    {
    //        unsigned int startingIndexBytes = 0;
    //        unsigned int bufferSizeBytes = checkPolygons.size() * sizeof(PolygonFace);
    //        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _collideableGeometrySsbo.BufferId());
    //        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
    //        memcpy(checkPolygons.data(), bufferPtr, bufferSizeBytes);
    //        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    //    }

    //    std::vector<Particle> checkParticles(_particleSsbo->NumParticles());
    //    {
    //        unsigned int startingIndexBytes = 0;
    //        unsigned int bufferSizeBytes = checkParticles.size() * sizeof(Particle);
    //        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particleSsbo->BufferId());
    //        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
    //        memcpy(checkParticles.data(), bufferPtr, bufferSizeBytes);
    //        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    //    }

    //    int maxVal = 0;
    //    for (size_t i = 0; i < checkParticles.size(); i++)
    //    {
    //        maxVal = std::max(maxVal, checkParticles[i]._numNearbyParticles);
    //        if (checkParticles[i]._numNearbyParticles == 37)
    //        {
    //            printf("");
    //        }
    //        else if (checkParticles[i]._isActive == 37)
    //        {
    //            printf("");
    //        }
    //    }
    //    printf("");
    //}
}
