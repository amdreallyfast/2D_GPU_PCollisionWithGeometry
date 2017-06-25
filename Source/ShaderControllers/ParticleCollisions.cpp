#include "Include/ShaderControllers/ParticleCollisions.h"

#include "Shaders/ShaderStorage.h"
#include "ThirdParty/glload/include/glload/gl_4_4.h"

// for profiling and checking results
#include "Include/ShaderControllers/ProfilingWaitToFinish.h"
#include "Include/Buffers/ParticleSortingData.h"
#include "Include/Buffers/BvhNode.h"
#include "Include/Buffers/ParticlePotentialCollisions.h"
#include "Include/Buffers/Particle.h"
#include "Include/Geometry/MyVertex.h"
#include "Include/Buffers/ParticleProperties.h"

#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"

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

        Note: Two SSBOs are provided as constructor arguments because they are both externally 
        generated SSBOs and both of their corresponding compute header files have buffer size 
        uniforms that need to set their values with any shader programs that this shader 
        controller generates.
    Parameters:
        leafData    Passed in so that it can have its uniforms set for the shaders.
        bvhSsbo     Contains info on the number of leaves.  
    Returns:    None
    Creator:    John Cox, 3/2017
    --------------------------------------------------------------------------------------------*/
    ParticleCollisions::ParticleCollisions(const ParticleSsbo::SharedConstPtr particleSsbo,
        const ParticlePropertiesSsbo::SharedConstPtr particlePropertiesSsbo) :
        _numParticles(particleSsbo->NumParticles()),

        _programIdCopyParticlesToCopyBuffer(0),
        _programIdGenerateSortingData(0),
        _programIdClearWorkGroupSums(0),
        _programIdGetBitForPrefixScan(0),
        _programIdPrefixScanOverAllData(0),
        _programIdPrefixScanOverWorkGroupSums(0),
        _programIdSortSortingDataWithPrefixSums(0),
        _programIdSortParticles(0),
        _programIdGuaranteeSortingDataUniqueness(0),
        _programIdGenerateLeafNodeBoundingBoxes(0),
        _programIdGenerateBinaryRadixTree(0),
        _programIdMergeBoundingVolumes(0),
        _programIdDetectCollisions(0),
        _programIdResolveCollisions(0),
        _programIdGenerateVerticesParticleVelocityVectors(0),
        _programIdGenerateVerticesParticleBoundingBoxes(0),

        // generate buffers
        _particleSortingDataSsbo(particleSsbo->NumParticles()),
        _prefixSumSsbo(particleSsbo->NumParticles()),
        _bvhNodeSsbo(particleSsbo->NumParticles()),
        
        //// Note: For N particles there are N leaves and N-1 internal nodes in the tree, and each 
        //// node's bounding box has 4 faces.  
        //_bvhGeometrySsbo(((particleSsbo->NumParticles() * 2) - 1) * 4),

        _particlePotentialCollisionsSsbo(particleSsbo->NumParticles()),

        _velocityVectorGeometrySsbo(particleSsbo->NumParticles()),
        _boundingBoxGeometrySsbo(particleSsbo->NumParticles()),
        
        // kept around for debugging purposes
        _originalParticleSsbo(particleSsbo)
    {
        // the programs used during the parallel sort
        AssembleProgramCopyParticlesToCopyBuffer();
        AssembleProgramGenerateSortingData();
        AssembleProgramClearWorkGroupSums();
        AssembleProgramGetBitForPrefixScan();
        AssembleProgramPrefixScanOverAllData();
        AssembleProgramPrefixScanOverWorkGroupSums();
        AssembleProgramSortSortingDataWithPrefixSums();
        AssembleProgramSortParticles();

        // the programs used during BVH construction
        AssembleProgramGuaranteeSortingDataUniqueness();
        AssembleProgramGenerateLeafNodeBoundingBoxes();
        AssembleProgramGenerateBinaryRadixTree();
        AssembleProgramMergeBoundingVolumes();

        // the programs used for the collisions themselves
        AssembleProgramDetectCollisions();
        AssembleProgramResolveCollisions();

        // and for the geometry generation to visualize the results 
        AssembleProgramGenerateVerticesParticleVelocityVectors();
        AssembleProgramGenerateVerticesParticleBoundingBoxes();

        // load the buffer size uniforms where the SSBOs will be used
        particleSsbo->ConfigureConstantUniforms(_programIdCopyParticlesToCopyBuffer);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateSortingData);
        particleSsbo->ConfigureConstantUniforms(_programIdSortParticles);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        particleSsbo->ConfigureConstantUniforms(_programIdDetectCollisions);
        particleSsbo->ConfigureConstantUniforms(_programIdResolveCollisions);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateVerticesParticleVelocityVectors);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateVerticesParticleBoundingBoxes);

        particlePropertiesSsbo->ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        particlePropertiesSsbo->ConfigureConstantUniforms(_programIdResolveCollisions);
        particlePropertiesSsbo->ConfigureConstantUniforms(_programIdGenerateVerticesParticleBoundingBoxes);

        _particleSortingDataSsbo.ConfigureConstantUniforms(_programIdGenerateSortingData);
        _particleSortingDataSsbo.ConfigureConstantUniforms(_programIdGetBitForPrefixScan);
        _particleSortingDataSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);
        _particleSortingDataSsbo.ConfigureConstantUniforms(_programIdSortParticles);
        _particleSortingDataSsbo.ConfigureConstantUniforms(_programIdGuaranteeSortingDataUniqueness);
        _particleSortingDataSsbo.ConfigureConstantUniforms(_programIdGenerateBinaryRadixTree);

        _prefixSumSsbo.ConfigureConstantUniforms(_programIdGetBitForPrefixScan);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanOverAllData);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanOverWorkGroupSums);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);

        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdGenerateBinaryRadixTree);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdMergeBoundingVolumes);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdDetectCollisions);

        _particlePotentialCollisionsSsbo.ConfigureConstantUniforms(_programIdDetectCollisions);
        _particlePotentialCollisionsSsbo.ConfigureConstantUniforms(_programIdResolveCollisions);

        _velocityVectorGeometrySsbo.ConfigureConstantUniforms(_programIdGenerateVerticesParticleVelocityVectors);

        _boundingBoxGeometrySsbo.ConfigureConstantUniforms(_programIdGenerateVerticesParticleBoundingBoxes);


        //unsigned int startingIndex = 0;
        //std::vector<ParticleProperties> checkParticlePropertiesBuffer(particlePropertiesSsbo->NumProperties());
        //unsigned int bufferSizeBytes = checkParticlePropertiesBuffer.size() * sizeof(ParticleProperties);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, particlePropertiesSsbo->BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkParticlePropertiesBuffer.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);


        //unsigned int startingIndex = 0;
        //std::vector<Particle> checkParticleBuffer(particleSsbo->NumParticles());
        //unsigned int bufferSizeBytes = checkParticleBuffer.size() * sizeof(Particle);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSsbo->BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkParticleBuffer.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Cleans up shader programs that were created for this shader controller.  The SSBOs clean 
        themselves up.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    ParticleCollisions::~ParticleCollisions()
    {
        glDeleteProgram(_programIdCopyParticlesToCopyBuffer);
        glDeleteProgram(_programIdGenerateSortingData);
        glDeleteProgram(_programIdClearWorkGroupSums);
        glDeleteProgram(_programIdGetBitForPrefixScan);
        glDeleteProgram(_programIdPrefixScanOverAllData);
        glDeleteProgram(_programIdPrefixScanOverWorkGroupSums);
        glDeleteProgram(_programIdSortSortingDataWithPrefixSums);
        glDeleteProgram(_programIdSortParticles);
        glDeleteProgram(_programIdGuaranteeSortingDataUniqueness);
        glDeleteProgram(_programIdGenerateLeafNodeBoundingBoxes);
        glDeleteProgram(_programIdGenerateBinaryRadixTree);
        glDeleteProgram(_programIdMergeBoundingVolumes);
        glDeleteProgram(_programIdDetectCollisions);
        glDeleteProgram(_programIdResolveCollisions);
        glDeleteProgram(_programIdGenerateVerticesParticleVelocityVectors);
        glDeleteProgram(_programIdGenerateVerticesParticleBoundingBoxes);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the dispatches of many compute shaders that will eventually result 
        in new particle velocities for colliding particles.

        To understand the need for all the programs, examine the problem in reverse:
        (3) Need an O(logN) tree traversal for collision detection.  N^2 is no good.
        (2) Quad trees cannot be generated on the GPU, but a binary radix tree can.  A binary 
            radix tree with each node containing ever-larger bounding boxes as they approach the 
            root will work nicely.
        (1) A binary radix tree can only be constructed over sorted data, and each leaf node in 
            the tree (one for each particle) needs to be next to other leaf nodes that are very 
            close by in 2D space.  A Z-Order curve (a space-filling curve) will work nicely to 
            place nearby particles close together.  Perform the sort using a parallel radix 
            sorting algorithm (the only parallel sorting algorithm that I know).

        The stages of collision detection and resolution are as follows:
        (1) sort the particles along a Z-order curve
            (a) prepare to sort particles
                (i)  copy particles to 2nd half of the particle buffer
                (ii) generate the Morton Codes (value along the Z-Order curve) for each particle
            (b) loop bits 0-31
                (i)   prepare for prefix scan
                    1. clear work group sums to 0
                    2. get next bit for prefix scan
                (ii)  prefix scan over all sorting data
                (iii) prefix scan over work group sums
                (iv)  sort sorting data with prefix sums
            (c) sort particles using the final sorted data
        (2) generate a bounding volume hierarchy (BVH) from the sorted data
            (a) prepare for binary tree
                (i)  guarantee sorting data uniqueness (see GuaranteeSortingDataUniqueness.comp)
                (ii) generate bounding boxes for each leaf node
            (b) generate the binary radix tree out of the particle sorting data
            (c) merge bounding boxes from the leaves up to the root of the tree
        (3) detect and resolve collisions
            (a) traverse the BVH and detect overlaps with leaves (other particles)
            (b) resolve any overlaps collisions

        I want to profile each step, so all the most-indented steps are in their own 
        shader-dispatching functions.  The "profiling" version of each stage ((1), (2), and (3)) 
        will surround the calls to these functions with profiling stuff and the resulting times 
        will be tallied and recorded.  The non-profiling versions will simply call each 
        shader-dispatching function.
    Parameters: 
        withProfiling   If true, performs the sorting, BVH generation, and collision detection 
                        and resolution with std::chrono calls, forced waiting for each shader to 
                        finish, and reporting of the durations to stdout and to files.  
                        
                        If false, then everything is performed as fast as possible (no timing or 
                        forced waiting or writing to stdout)
        generateGeometry    Self-explanatory.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::DetectAndResolve(bool withProfiling, bool generateGeometry) const
    {
        // most shaders work on 1 item per thread
        int numWorkGroupsX = _numParticles / WORK_GROUP_SIZE_X;
        int remainder = _numParticles % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        // the prefix scan works on 2 items per thread
        // Note: See description of PrefixScanSsbo for why the prefix scan algorithm needs its 
        // own work group size calculation.
        unsigned int numItemsInPrefixScanBuffer = _prefixSumSsbo.NumDataEntries();
        int numWorkGroupsXForPrefixSum = numItemsInPrefixScanBuffer / PREFIX_SCAN_ITEMS_PER_WORK_GROUP;
        remainder = numItemsInPrefixScanBuffer % PREFIX_SCAN_ITEMS_PER_WORK_GROUP;
        numWorkGroupsXForPrefixSum += (remainder == 0) ? 0 : 1;

        if (withProfiling)
        {
            //SortParticlesWithProfiling(numWorkGroupsX, numWorkGroupsXForPrefixSum);
            //GenerateBvhWithProfiling(numWorkGroupsX);
            SortParticlesWithoutProfiling(numWorkGroupsX, numWorkGroupsXForPrefixSum);
            GenerateBvhWithoutProfiling(numWorkGroupsX);
            DetectAndResolveCollisionsWithProfiling(numWorkGroupsX);
        }
        else
        {
            SortParticlesWithoutProfiling(numWorkGroupsX, numWorkGroupsXForPrefixSum);
            GenerateBvhWithoutProfiling(numWorkGroupsX);
            DetectAndResolveCollisionsWithoutProfiling(numWorkGroupsX);
        }

        if (generateGeometry)
        {
            // visualize the results
            GenerateGeometry(numWorkGroupsX);
        }
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Used so that the RenderGeometry shader controller can draw the lines that indicate where 
        the particles are going.  This will help with debugging aside from being pretty.
    Parameters: None
    Returns:    
        A const reference to the particle velocity vector geometry SSBO.
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    const VertexSsboBase &ParticleCollisions::ParticleVelocityVectorSsbo() const
    {
        return _velocityVectorGeometrySsbo;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Used so that the RenderGeometry shader controller can draw the lines that indicate what 
        space each particle considers collidable with itself.  This will help with debugging 
        aside from being pretty.
    Parameters: None
    Returns:    
        A const reference to the particle bounding box geometry SSBO.
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    const VertexSsboBase &ParticleCollisions::ParticleBoundingBoxSsbo() const
    {
        return _boundingBoxGeometrySsbo;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        The GLSL version declaration, compute shader work group sizes, 
        cross-shader uniform locations, and SSBO buffer bindings are used in very compute 
        shader.  This function puts their assembly into one place.
    Parameters: 
        The key to the composite shader that is under construction.
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramHeader(const std::string &shaderKey) const
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/Version.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/SsboBufferBindings.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that prepares the 
        ParticleBuffer for the final stage of particle sorting in SortParticles.comp.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramCopyParticlesToCopyBuffer()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "copy particles to copy buffer";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/CopyParticlesToCopyBuffer.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdCopyParticlesToCopyBuffer = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that prepares the 
        data over which particles will be sorted.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramGenerateSortingData()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "generate sorting data";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleRegionBoundaries.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/PositionToMortonCode.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleSortingDataBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/GenerateSortingData.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateSortingData = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that resets the 
        prefix sum for each work group's result to 0.

        Part off the radix sort loop.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramClearWorkGroupSums()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "clear work group sums";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/PrefixScanBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/ClearWorkGroupSums.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdClearWorkGroupSums = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that extracts a 
        single bit from the sorting data for use during the prefix scan.

        Part of the radix sort loop.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramGetBitForPrefixScan()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "get bit for prefix scan";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleSortingDataBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/PrefixScanBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/GetBitForPrefixScan.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGetBitForPrefixScan = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that runs the 
        first part of the prefix scan. 

        Part of the radix sort loop.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramPrefixScanOverAllData()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "prefix scan over all data";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/PrefixScanBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/PrefixScanOverAllData.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanOverAllData = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that runs the
        second part of the prefix scan.

        Part of the radix sort loop.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramPrefixScanOverWorkGroupSums()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "prefix scan over work group sums";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/PrefixScanBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/PrefixScanOverWorkGroupSums.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanOverWorkGroupSums = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that sorts the 
        sorting data given the results of the prefix scan.

        Part of the radix sort loop.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramSortSortingDataWithPrefixSums()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "sort sorting data with prefix sums";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleSortingDataBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/PrefixScanBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/SortSortingDataWithPrefixSums.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdSortSortingDataWithPrefixSums = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that copies 
        particles from the second half of the ParticleBuffer to their sorted position in the 
        first half.  This program is the complement to CopyParticlesToCopyBuffer.comp.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramSortParticles()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "sort particles";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleSortingDataBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/SortParticles.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdSortParticles = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that modifies the 
        data over which particles were sorted so that every entry is unique.  This will prevent 
        depth spikes in the binary radix tree.

        Note: As mentioned in GuaranteeSortingDataUniqueness.comp, this shader CANNOT be 
        summoned before the sorting is done.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramGuaranteeSortingDataUniqueness() 
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "guarantee sorting data uniqueness";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleSortingDataBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/GuaranteeSortingDataUniqueness.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGuaranteeSortingDataUniqueness = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that generates the 
        borders for each particle's bounding box.

        Note: Can be run at the same time as GenerateBinaryRadixTree.comp.  They write different 
        data, so they can be dispatched without a glMemoryBarrier(...) in between them. 
        
        (??verify??)

    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramGenerateLeafNodeBoundingBoxes()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "generate leaf node bounding boxes";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticlePropertiesBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/BvhNodeBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/GenerateLeafNodeBoundingBoxes.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateLeafNodeBoundingBoxes = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that generates the 
        binary radix tree.

        Note: Can be run at the same time as GenerateLeafNodeBoundingBoxes.comp.  Both are 
        writing to leaf nodes, but they are writing different data, so one can be dispatched 
        right after the other without the need for a memory barrier.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramGenerateBinaryRadixTree()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "generate binary radix tree";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleSortingDataBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/BvhNodeBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/GenerateBinaryRadixTree.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateBinaryRadixTree = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that takes the 
        bounding boxes the were made in GenerateLeafNodeBoundingBoxes.comp and merges them up 
        the tree to the root.  This takes a tree with bounding volumes at the leaves and 
        generates a bounding volume hierarchy.  All the previous shaders' sorting and tree 
        generation and supporting duties were to get to this point.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 5/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramMergeBoundingVolumes()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "merge bounding volumes";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/BvhNodeBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/MergeBoundingVolumes.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdMergeBoundingVolumes = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that takes leaf 
        node bounding boxes and figures out which other, if any, leaf node bounding boxes it 
        overlaps with.  Fills out the ParticlePotentialCollisionsBuffer.

        The whole purpose of creating a BVH was to use this shader for particle-particle 
        collision detection.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramDetectCollisions()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "detect particle-particle collisions";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/BvhNodeBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/MaxNumPotentialCollisions.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticlePotentialCollisionsBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/DetectParticleCollisions.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdDetectCollisions = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that performs 
        collisions between particles and that gives them new velocity vectors.  
    Parameters: None
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramResolveCollisions()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "resolve particle-particle collisions";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/MaxNumPotentialCollisions.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticlePotentialCollisionsBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticlePropertiesBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/ResolveParticleCollisions.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdResolveCollisions = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that analyzes each 
        active particle and generates a start and end vertex out of it.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramGenerateVerticesParticleVelocityVectors()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "generate particle velocity vector geometry";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/GeometryStuff/MyVertex.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleVelocityVectorGeometryBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/QuickNormalize.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/GenerateVerticesParticleVelocityVectors.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateVerticesParticleVelocityVectors = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that analyzes each 
        active particle and generates a box of MyVertex objects out of it for rendering.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::AssembleProgramGenerateVerticesParticleBoundingBoxes()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "generate particle bounding box geometry";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/GeometryStuff/MyVertex.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/GeometryStuff/PolygonFace.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/GeometryStuff/BoundingBox.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticleBoundingBoxGeometryBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/Buffers/ParticlePropertiesBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleCollisions/GenerateVerticesParticleBoundingBoxes.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateVerticesParticleBoundingBoxes = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in sorting the ParticleBuffer 
        and ParticleSortingDataBuffer.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
        numWorkGroupsXPrefixScan    See comment where this value was calculated.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::SortParticlesWithoutProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const
    {
        PrepareToSortParticles(numWorkGroupsX);

        // parallel radix sorting algorithm over each bit of the Morton Codes 
        // Note: MUST sort over all 32 bits in GLSL's uint.  See GenerateSortingData.comp for 
        // more detail, but the gist is that the sorting data for inactive particles is 
        // 0xC0000000 for generating the BVH tree.  That is 2x 1s in the 30th and 31st bit, and 
        // the least significant 30bits are 0s.  Sorting these inactive particles to the back 
        // therefore requires sorting over all 32 bits (actually, I think that I could get away 
        // with sorting 31 bits..??should I??)
        unsigned int totalBitCount = 32;

        bool writeToSecondBuffer = true;
        unsigned int sortingDataReadBufferOffset = 0;
        unsigned int sortingDataWriteBufferOffset = 0;
        for (unsigned int bitNumber = 0; bitNumber < totalBitCount; bitNumber++)
        {
            sortingDataReadBufferOffset = static_cast<unsigned int>(!writeToSecondBuffer) * _numParticles;
            sortingDataWriteBufferOffset = static_cast<unsigned int>(writeToSecondBuffer) * _numParticles;

            PrepareForPrefixScan(bitNumber, sortingDataReadBufferOffset);
            PrefixScanOverParticleSortingData(numWorkGroupsXPrefixScan);
            SortSortingDataWithPrefixScan(numWorkGroupsX, bitNumber, sortingDataReadBufferOffset, sortingDataWriteBufferOffset);

            // swap read/write buffers and do it again
            writeToSecondBuffer = !writeToSecondBuffer;
        }

        // the sorting data's final location is in the "write" half of the sorting data buffer
        SortParticlesWithSortedData(numWorkGroupsX, sortingDataWriteBufferOffset);

        // all done
        glUseProgram(0);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Like SortParticlesWithoutProfiling(...), but with 
        (1) std::chrono calls 
        (2) forced wait for shader to finish so that the std::chrono calls get an accurate 
            reading for how long the shader takes 
        (3) verification that the sort resulted in smallest->largest values in 
            ParticleSortingDataBuffer (can't check the ParticleBuffer itself for sorting because 
            particles don't carry the sorting data with them)
        (4) writing the output to a file (if desired)
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
        numWorkGroupsXPrefixScan    See comment where this value was calculated.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::SortParticlesWithProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const
    {
        cout << "sorting " << _numParticles << " particles" << endl;
        unsigned int totalBitCount = 32;

        // for profiling
        using namespace std::chrono;
        steady_clock::time_point start;
        steady_clock::time_point end;
        long long durationPrepareToSort = 0;
        long long durationParticleSort = 0;
        long long durationSortVerification = 0;
        std::vector<long long> durationsPrepareForPrefixScan(totalBitCount);
        std::vector<long long> durationsPrefixScan(totalBitCount);
        std::vector<long long> durationsSortSortingData(totalBitCount);

        start = high_resolution_clock::now();
        PrepareToSortParticles(numWorkGroupsX);
        WaitForComputeToFinish();
        end = high_resolution_clock::now();
        durationPrepareToSort = duration_cast<microseconds>(end - start).count();

        bool writeToSecondBuffer = true;
        unsigned int sortingDataReadBufferOffset = 0;
        unsigned int sortingDataWriteBufferOffset = 0;
        for (unsigned int bitNumber = 0; bitNumber < totalBitCount; bitNumber++)
        {
            sortingDataReadBufferOffset = static_cast<unsigned int>(!writeToSecondBuffer) * _numParticles;
            sortingDataWriteBufferOffset = static_cast<unsigned int>(writeToSecondBuffer) * _numParticles;

            start = high_resolution_clock::now();
            PrepareForPrefixScan(bitNumber, sortingDataReadBufferOffset);
            WaitForComputeToFinish();
            end = high_resolution_clock::now();
            durationsPrepareForPrefixScan[bitNumber] = duration_cast<microseconds>(end - start).count();

            start = high_resolution_clock::now();
            PrefixScanOverParticleSortingData(numWorkGroupsXPrefixScan);
            WaitForComputeToFinish();
            end = high_resolution_clock::now();
            durationsPrefixScan[bitNumber] = duration_cast<microseconds>(end - start).count();

            start = high_resolution_clock::now();
            SortSortingDataWithPrefixScan(numWorkGroupsX, bitNumber, sortingDataReadBufferOffset, sortingDataWriteBufferOffset);
            WaitForComputeToFinish();
            end = high_resolution_clock::now();
            durationsSortSortingData[bitNumber] = duration_cast<microseconds>(end - start).count();

            // swap read/write buffers and do it again
            writeToSecondBuffer = !writeToSecondBuffer;
        }

        // wherever the sorting data ended up, that is where the shader should read from
        start = high_resolution_clock::now();
        SortParticlesWithSortedData(numWorkGroupsX, sortingDataWriteBufferOffset);
        WaitForComputeToFinish();
        end = high_resolution_clock::now();
        durationParticleSort = duration_cast<microseconds>(end - start).count();

        // verify sorted data
        // Note: Only need to copy the first half of the buffer.  This is where the last loop of 
        // the radix sorting algorithm put the sorting data.
        start = high_resolution_clock::now();
        unsigned int startingIndex = sortingDataWriteBufferOffset;
        std::vector<ParticleSortingData> checkSortingData(_particleSortingDataSsbo.NumItems());
        unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(ParticleSortingData);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particleSortingDataSsbo.BufferId());
        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
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

        end = high_resolution_clock::now();
        durationSortVerification = duration_cast<microseconds>(end - start).count();

        // report results
        // Note: Write the results to a tab-delimited text file so that I can dump them into an 
        // Excel spreadsheet.
        std::ofstream outFile("ParallelSortDurations.txt");
        if (outFile.is_open())
        {
            long long totalSortingTime = durationPrepareToSort + durationParticleSort;
            for (unsigned int bitCounter = 0; bitCounter < totalBitCount; bitCounter++)
            {
                totalSortingTime += durationsPrepareForPrefixScan[bitCounter];
                totalSortingTime += durationsPrefixScan[bitCounter];
                totalSortingTime += durationsSortSortingData[bitCounter];
            }

            cout << "total sorting time: " << totalSortingTime << "\tmicroseconds" << endl;
            outFile << "total sorting time: " << totalSortingTime << "\tmicroseconds" << endl;

            cout << "sort verification: " << durationSortVerification << "\tmicroseconds" << endl;
            outFile << "sort verification: " << durationSortVerification << "\tmicroseconds" << endl;

            cout << "preparation: " << durationPrepareToSort << "\tmicroseconds" << endl;
            outFile << "preparation: " << durationPrepareToSort << "\tmicroseconds" << endl;

            cout << "move particles to sorted positions: " << durationParticleSort << "\tmicroseconds" << endl;
            outFile << "move particles to sorted positions: " << durationParticleSort << "\tmicroseconds" << endl;

            cout << endl << "prepare for prefix scan:" << endl;
            outFile << endl << "prepare for prefix scan:" << endl;
            for (size_t i = 0; i < durationsPrepareForPrefixScan.size(); i++)
            {
                cout << "\t" << durationsPrepareForPrefixScan[i] << "\tmicroseconds" << endl;
                outFile << "\t" << durationsPrepareForPrefixScan[i] << "\tmicroseconds" << endl;
            }

            cout << endl << "prefix scan:" << endl;
            outFile << endl << "prefix scan:" << endl;
            for (size_t i = 0; i < durationsPrefixScan.size(); i++)
            {
                cout << "\t" << durationsPrefixScan[i] << "\tmicroseconds" << endl;
                outFile << "\t" << durationsPrefixScan[i] << "\tmicroseconds" << endl;
            }

            cout << endl << "sort sorting data:" << endl;
            outFile << endl << "sort sorting data:" << endl;
            for (size_t i = 0; i < durationsSortSortingData.size(); i++)
            {
                cout << "\t" << durationsSortSortingData[i] << "\tmicroseconds" << endl;
                outFile << "\t" << durationsSortSortingData[i] << "\tmicroseconds" << endl;
            }
        }
        outFile.close();

        // all done
        glUseProgram(0);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in a balanced binary tree of 
        bounding boxes from the leaves (particles) up to the root of the tree.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::GenerateBvhWithoutProfiling(unsigned int numWorkGroupsX) const
    {
        PrepareForBinaryTree(numWorkGroupsX);
        GenerateBinaryRadixTree(numWorkGroupsX);
        MergeNodesIntoBvh(numWorkGroupsX);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Like GenerateBvhWithoutProfiling(...), but with 
        (1) std::chrono calls 
        (2) forced wait for shader to finish so that the std::chrono calls get an accurate 
            reading for how long the shader takes 
        (3) verification of a valid tree (all nodes' parent-child relationships are reciprocated)
        (4) writing the output to a file (if desired)
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::GenerateBvhWithProfiling(unsigned int numWorkGroupsX) const
    {
        cout << "generating BVH for " << _numParticles << " particles" << endl;

        // for profiling
        using namespace std::chrono;
        steady_clock::time_point start;
        steady_clock::time_point end;
        long long durationPrepData = 0;
        long long durationGenerateTree = 0;
        long long durationMergeBoundingBoxes = 0;
        long long durationCheckForValidTree = 0;

        // prep data
        start = high_resolution_clock::now();
        PrepareForBinaryTree(numWorkGroupsX);
        WaitForComputeToFinish();
        end = high_resolution_clock::now();
        durationPrepData = duration_cast<microseconds>(end - start).count();

        // generate the tree
        start = high_resolution_clock::now();
        GenerateBinaryRadixTree(numWorkGroupsX);
        WaitForComputeToFinish();
        end = high_resolution_clock::now();
        durationGenerateTree = duration_cast<microseconds>(end - start).count();

        // populate the tree with bounding volumes to finish the BVH
        start = high_resolution_clock::now();
        MergeNodesIntoBvh(numWorkGroupsX);
        WaitForComputeToFinish();
        end = high_resolution_clock::now();
        durationMergeBoundingBoxes = duration_cast<microseconds>(end - start).count();

        // verify that the binary tree is valid by checking that all parent-child relationships 
        // are reciprocated 
        // Note: By virtue of being a binary tree, every node except the root has a parent, and 
        // that parent also specifies that node as a child exactly once.
        start = high_resolution_clock::now();
        unsigned int startingIndex = 0;
        std::vector<BvhNode> checkBinaryTree(_bvhNodeSsbo.NumTotalNodes());
        unsigned int bufferSizeBytes = checkBinaryTree.size() * sizeof(BvhNode);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bvhNodeSsbo.BufferId());
        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkBinaryTree.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        
        // check the root node (no parent, only children)
        int rootnodeindex = _bvhNodeSsbo.NumLeafNodes();
        const BvhNode &rootnode = checkBinaryTree[rootnodeindex];
        if ((rootnodeindex != checkBinaryTree[rootnode._leftChildIndex]._parentIndex) &&
            (rootnodeindex != checkBinaryTree[rootnode._rightChildIndex]._parentIndex))
        {
            // root-child relationship not reciprocated
            printf("");
        }

        // check all the other nodes (have parents, leaves don't have children)
        for (size_t thisNodeIndex = 0; thisNodeIndex < checkBinaryTree.size(); thisNodeIndex++)
        {
            const BvhNode &thisNode = checkBinaryTree[thisNodeIndex];

            if (thisNode._parentIndex == -1)
            {
                // skip if it is the root; everyone else should have a parent
                if (thisNodeIndex != _bvhNodeSsbo.NumLeafNodes())
                {
                    // bad: non-root node has a -1 parent
                    printf("");
                }
            }
            else
            {
                if ((thisNodeIndex != checkBinaryTree[thisNode._parentIndex]._leftChildIndex) &&
                    (thisNodeIndex != checkBinaryTree[thisNode._parentIndex]._rightChildIndex))
                {
                    // parent-child relationship not reciprocated
                    printf("");
                }
            }
        }

        end = high_resolution_clock::now();
        durationCheckForValidTree = duration_cast<microseconds>(end - start).count();

        // report results
        // Note: Write the results to a tab-delimited text file so that I can dump them into an 
        // Excel spreadsheet.
        std::ofstream outFile("GenerateBvhDurations.txt");
        if (outFile.is_open())
        {
            long long totalSortingTime = durationPrepData + durationGenerateTree + durationMergeBoundingBoxes;

            cout << "total BVH generation time: " << totalSortingTime << "\tmicroseconds" << endl;
            outFile << "total BVH generation time: " << totalSortingTime << "\tmicroseconds" << endl;

            cout << "prep data: " << durationPrepData << "\tmicroseconds" << endl;
            outFile << "prep data: " << durationPrepData << "\tmicroseconds" << endl;

            cout << "generate tree: " << durationGenerateTree << "\tmicroseconds" << endl;
            outFile << "generate tree: " << durationGenerateTree << "\tmicroseconds" << endl;

            cout << "merge bounding boxes: " << durationMergeBoundingBoxes << "\tmicroseconds" << endl;
            outFile << "merge bounding boxes: " << durationMergeBoundingBoxes << "\tmicroseconds" << endl;

            cout << "check for valid tree: " << durationCheckForValidTree << "\tmicroseconds" << endl;
            outFile << "check for valid tree: " << durationCheckForValidTree << "\tmicroseconds" << endl;
        }
        outFile.close();
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in colliding particles 
        receiving new velocity vectors.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::DetectAndResolveCollisionsWithoutProfiling(
        unsigned int numWorkGroupsX) const
    {
        DetectCollisions(numWorkGroupsX);
        ResolveCollisions(numWorkGroupsX);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Like DetectAndResolveCollisionsWithoutProfiling(...), but with 
        (1) std::chrono calls 
        (2) forced wait for shader to finish so that the std::chrono calls get an accurate 
            reading for how long the shader takes 
        (3) writing the output to a file (if desired)

        Note: There is no structure to verify as there was for particle sorting and BVH 
        generation.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::DetectAndResolveCollisionsWithProfiling(
        unsigned int numWorkGroupsX) const
    {
        cout << "detecting collisions for up to " << _numParticles << " particles" << endl;

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
        ResolveCollisions(numWorkGroupsX);
        WaitForComputeToFinish();
        end = high_resolution_clock::now();
        durationResolveCollisions = duration_cast<microseconds>(end - start).count();

        //unsigned int startingIndex = 0;
        //std::vector<ParticlePotentialCollisions> checkPotentialCollisions(_particlePotentialCollisionsSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkPotentialCollisions.size() * sizeof(ParticlePotentialCollisions);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particlePotentialCollisionsSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkPotentialCollisions.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        unsigned int startingIndex = 0;
        std::vector<Particle> checkPostCollisionParticles(_originalParticleSsbo->NumParticles());
        unsigned int bufferSizeBytes = checkPostCollisionParticles.size() * sizeof(Particle);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _originalParticleSsbo->BufferId());
        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkPostCollisionParticles.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        for (size_t i = 0; i < checkPostCollisionParticles.size(); i++)
        {
            if (isnan(checkPostCollisionParticles[i]._vel.x))
            {
                printf("");
            }
        }

        //// nothing to verify, so just report the results
        //// Note: Write the results to a tab-delimited text file so that I can dump them into an 
        //// Excel spreadsheet.
        //std::ofstream outFile("DetectAndResolveCollisionsDurations.txt");
        //if (outFile.is_open())
        //{
        //    long long totalSortingTime = durationDetectCollisions + durationResolveCollisions;

        //    cout << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;
        //    outFile << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;

        //    cout << "detect collisions: " << durationDetectCollisions << "\tmicroseconds" << endl;
        //    outFile << "detect collisions: " << durationDetectCollisions << "\tmicroseconds" << endl;

        //    cout << "resolve collisions: " << durationResolveCollisions << "\tmicroseconds" << endl;
        //    outFile << "resolve collisions: " << durationResolveCollisions << "\tmicroseconds" << endl;
        //}
        //outFile.close();
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::PrepareToSortParticles(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdCopyParticlesToCopyBuffer);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateSortingData);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on independent data, so only need one memory barrier at the end
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.  Number of work groups calculated internally.

        Note: The number of work groups for this sorting stage need special handling.  Most 
        shaders operate on 1 item per thread and will require 
        ("num particles" / "work group size X") + 1 work groups to give every data entry 
        (particle, particle sorting, BVH nodes, etc.) a thread.  The prefix sum algorithm 
        operates on 2 items per thread and needs special handling (read description block of 
        PrefixSumSsbo for details).  
        
        This stage is neither.  Clearing out the work group sums operates on 2 items per thread 
        and requires 1, and exactly 1, work groups.  Getting bits for the prefix sum operates on 
        1 item per thread and is a function of the size of the prefix scan array.  Deal with 
        these special cases manually.
    Parameters:
        bitNumber               0 - 31
        sortingDataReadOffset   Tells the shader which half of the ParticleSortingDataBuffer has 
                                the latest sort values.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::PrepareForPrefixScan(unsigned int bitNumber, 
        unsigned int sortingDataReadOffset) const
    {
        // Note: The "work group sums" array is the size of a single work group * 2.  This shader
        // should only be dispatched with a single work group.
        glUseProgram(_programIdClearWorkGroupSums);
        glDispatchCompute(1, 1, 1);

        int numWorkGroupsX = _prefixSumSsbo.NumDataEntries() / WORK_GROUP_SIZE_X;
        int remainder = _prefixSumSsbo.NumDataEntries() % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;
        glUseProgram(_programIdGetBitForPrefixScan);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on independent data, so only need one memory barrier at the end
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.
    Parameters: 
        numWorkGroupsX          Expected to be the size of the prefix scan buffer divided by 
                                work group size.
        sortingDataReadOffset   Tells the shader which half of the ParticleSortingDataBuffer has 
                                the latest sort values.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::PrefixScanOverParticleSortingData(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdPrefixScanOverAllData);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // this stage is designed to work with 1 work group, and exactly 1 work group
        // Note: See PrefixScanBuffer.comp description for details on this second stage.
        glUseProgram(_programIdPrefixScanOverWorkGroupSums);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.
    Parameters: 
        numWorkGroupsX          Expected to be number of particles divided by work group size.
        bitNumber               0 - 32
        sortingDataReadOffset   The sortng data is read from this half of the buffer...
        sortingDataWriteOffset  And sorted according to the prefix sums into this half.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::SortSortingDataWithPrefixScan(
        unsigned int numWorkGroupsX, unsigned int bitNumber, 
        unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const
    {
        glUseProgram(_programIdSortSortingDataWithPrefixSums);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_WRITE_OFFSET, sortingDataWriteOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.
    Parameters: 
        numWorkGroupsX          Expected to be number of particles divided by work group size.
        sortingDataReadOffset   Tells the shader which half of the ParticleSortingDataBuffer has 
                                the latest sort values.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::SortParticlesWithSortedData(unsigned int numWorkGroupsX, 
        unsigned int sortingDataReadOffset) const
    {
        glUseProgram(_programIdSortParticles);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Modifies the ParticleSortingDataBuffer so that the resulting tree won't have depth 
        spikes due to duplicate entries, then gives each leaf node in the BvhNodeBuffer a 
        bounding box based on the particle that it is associated with.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::PrepareForBinaryTree(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGuaranteeSortingDataUniqueness);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateLeafNodeBoundingBoxes);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on independent data, so only need one memory barrier at the end
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        All that sorting to get to here.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::GenerateBinaryRadixTree(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGenerateBinaryRadixTree);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        And finally the binary radix tree blooms with beautiful bounding boxes into a Bounding 
        Volume Hierarchy.  I'm tired and am thinking of nice "tree in spring" analogy.  The 
        analogy starts to fall apart when I think of creating the tree anew ~60x/sec.  
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::MergeNodesIntoBvh(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdMergeBoundingVolumes);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Populates the ParticlePotentialCollisionsBuffer.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::DetectCollisions(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdDetectCollisions);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Reads the ParticlePotentialCollisionsBuffer and gives particles new velocity vectors if 
        they collide.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::ResolveCollisions(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdResolveCollisions);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Generates 2x vertices per particle for a start-end pair.  
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleCollisions::GenerateGeometry(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGenerateVerticesParticleVelocityVectors);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateVerticesParticleBoundingBoxes);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // this geometry buffer will not be used in any shader storage, but it will be used for 
        // rendering
        //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_SHADER_BIT);
        glMemoryBarrier(GL_VERTEX_SHADER_BIT);

        //// in case of debugging
        //WaitForComputeToFinish();
        //unsigned int startingIndex = 0;
        //std::vector<MyVertex> checkResultantGeometry(_velocityVectorGeometrySsbo.NumVertices());
        //unsigned int bufferSizeBytes = checkResultantGeometry.size() * sizeof(MyVertex);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _velocityVectorGeometrySsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkResultantGeometry.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

}
