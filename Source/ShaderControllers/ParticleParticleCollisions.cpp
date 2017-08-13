#include "Include/ShaderControllers/ParticleParticleCollisions.h"

#include "Shaders/ShaderStorage.h"
#include "ThirdParty/glload/include/glload/gl_4_4.h"

#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"

// for profiling and checking results
#include "Include/ShaderControllers/ProfilingWaitToFinish.h"
#include "Include/Buffers/SortingData.h"
#include "Include/Buffers/BvhNode.h"
#include "Include/Buffers/PotentialParticleCollisions.h"
#include "Include/Buffers/Particle.h"
#include "Include/Geometry/MyVertex.h"
#include "Include/Buffers/ParticleProperties.h"

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
    ParticleParticleCollisions::ParticleParticleCollisions(const ParticleSsbo::SharedConstPtr particleSsbo,
        const ParticlePropertiesSsbo::SharedConstPtr particlePropertiesSsbo) :
        _numParticles(particleSsbo->NumParticles()),

        _programIdCopyParticlesToCopyBuffer(0),
        _programIdGenerateSortingData(0),
        _programIdPrefixScanStage1(0),
        _programIdPrefixScanStage2(0),
        _programIdPrefixScanStage3(0),
        _programIdSortSortingDataWithPrefixSums(0),
        _programIdSortParticles(0),
        _programIdGuaranteeSortingDataUniqueness(0),
        _programIdGenerateLeafNodeBoundingBoxes(0),
        _programIdGenerateBinaryRadixTree(0),
        _programIdMergeBoundingVolumes(0),
        _programIdDetectCollisions(0),
        _programIdResolveCollisions(0),
        _programIdGenerateParticleVelocityVectorGeometry(0),
        _programIdGenerateParticleBoundingBoxGeometry(0),

        // generate buffers
        _sortingDataSsbo(particleSsbo->NumParticles()),
        _prefixSumSsbo(particleSsbo->NumParticles()),
        _bvhNodeSsbo(particleSsbo->NumParticles()),
        
        //// Note: For N particles there are N leaves and N-1 internal nodes in the tree, and each 
        //// node's bounding box has 4 faces.  
        //_bvhGeometrySsbo(((particleSsbo->NumParticles() * 2) - 1) * 4),

        _potentialCollisionsSsbo(particleSsbo->NumParticles()),

        _velocityVectorGeometrySsbo(particleSsbo->NumParticles()),
        _boundingBoxGeometrySsbo(particleSsbo->NumParticles()),
        
        // kept around for debugging purposes
        _originalParticleSsbo(particleSsbo)
    {
        AssembleSortingShaders();
        AssembleBvhShaders();
        AssembleCollisionShaders();
        AssembleGeometryCreationShaders();

        // load the buffer size uniforms where the SSBOs will be used
        particleSsbo->ConfigureConstantUniforms(_programIdCopyParticlesToCopyBuffer);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateSortingData);
        particleSsbo->ConfigureConstantUniforms(_programIdSortParticles);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        particleSsbo->ConfigureConstantUniforms(_programIdDetectCollisions);
        particleSsbo->ConfigureConstantUniforms(_programIdResolveCollisions);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateParticleBoundingBoxGeometry);
        particleSsbo->ConfigureConstantUniforms(_programIdGenerateParticleVelocityVectorGeometry);

        particlePropertiesSsbo->ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        particlePropertiesSsbo->ConfigureConstantUniforms(_programIdResolveCollisions);
        particlePropertiesSsbo->ConfigureConstantUniforms(_programIdGenerateParticleBoundingBoxGeometry);

        _sortingDataSsbo.ConfigureConstantUniforms(_programIdGenerateSortingData);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage1);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdSortParticles);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdGuaranteeSortingDataUniqueness);
        _sortingDataSsbo.ConfigureConstantUniforms(_programIdGenerateBinaryRadixTree);

        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage1);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage2);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdPrefixScanStage3);
        _prefixSumSsbo.ConfigureConstantUniforms(_programIdSortSortingDataWithPrefixSums);

        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdGenerateLeafNodeBoundingBoxes);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdGenerateBinaryRadixTree);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdMergeBoundingVolumes);
        _bvhNodeSsbo.ConfigureConstantUniforms(_programIdDetectCollisions);

        _potentialCollisionsSsbo.ConfigureConstantUniforms(_programIdDetectCollisions);
        _potentialCollisionsSsbo.ConfigureConstantUniforms(_programIdResolveCollisions);

        _velocityVectorGeometrySsbo.ConfigureConstantUniforms(_programIdGenerateParticleVelocityVectorGeometry);

        _boundingBoxGeometrySsbo.ConfigureConstantUniforms(_programIdGenerateParticleBoundingBoxGeometry);


        //unsigned int startingIndexBytes = 0;
        //std::vector<ParticleProperties> checkParticlePropertiesBuffer(particlePropertiesSsbo->NumProperties());
        //unsigned int bufferSizeBytes = checkParticlePropertiesBuffer.size() * sizeof(ParticleProperties);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, particlePropertiesSsbo->BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkParticlePropertiesBuffer.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);


        //unsigned int startingIndexBytes = 0;
        //std::vector<Particle> checkParticleBuffer(particleSsbo->NumParticles());
        //unsigned int bufferSizeBytes = checkParticleBuffer.size() * sizeof(Particle);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSsbo->BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkParticleBuffer.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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
    ParticleParticleCollisions::~ParticleParticleCollisions()
    {
        glDeleteProgram(_programIdCopyParticlesToCopyBuffer);
        glDeleteProgram(_programIdGenerateSortingData);
        glDeleteProgram(_programIdPrefixScanStage1);
        glDeleteProgram(_programIdPrefixScanStage2);
        glDeleteProgram(_programIdPrefixScanStage3);
        glDeleteProgram(_programIdSortSortingDataWithPrefixSums);
        glDeleteProgram(_programIdSortParticles);
        glDeleteProgram(_programIdGuaranteeSortingDataUniqueness);
        glDeleteProgram(_programIdGenerateLeafNodeBoundingBoxes);
        glDeleteProgram(_programIdGenerateBinaryRadixTree);
        glDeleteProgram(_programIdMergeBoundingVolumes);
        glDeleteProgram(_programIdDetectCollisions);
        glDeleteProgram(_programIdResolveCollisions);
        glDeleteProgram(_programIdGenerateParticleVelocityVectorGeometry);
        glDeleteProgram(_programIdGenerateParticleBoundingBoxGeometry);
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
    void ParticleParticleCollisions::DetectAndResolve(bool withProfiling, bool generateGeometry) const
    {
        // most shaders work on 1 item per thread
        int numWorkGroupsX = _numParticles / WORK_GROUP_SIZE_X;
        int remainder = _numParticles % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        // the prefix scan works on 2 items per thread
        // Note: See description of the ParticlePrefixScanSsbo constructor for why the prefix 
        // scan algorithm needs its own work group size calculation.
        unsigned int numItemsInPrefixScanBuffer = _prefixSumSsbo.NumDataEntries();
        int numWorkGroupsXForPrefixSum = numItemsInPrefixScanBuffer / (WORK_GROUP_SIZE_X * 2);
        remainder = numItemsInPrefixScanBuffer % (WORK_GROUP_SIZE_X * 2);
        numWorkGroupsXForPrefixSum += (remainder == 0) ? 0 : 1;

        if (withProfiling)
        {
            SortParticlesWithProfiling(numWorkGroupsX, numWorkGroupsXForPrefixSum);
            GenerateBvhWithProfiling(numWorkGroupsX);
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
    const VertexSsboBase &ParticleParticleCollisions::GetParticleVelocityVectorSsbo() const
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
    const VertexSsboBase &ParticleParticleCollisions::GetParticleBoundingBoxSsbo() const
    {
        return _boundingBoxGeometrySsbo;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Primarily serves to clean up the constructor.

        Assembles headers, buffers, and functional .comp files for the shaders that sort 
        particles by their position-generated Morton Codes.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::AssembleSortingShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "copy particles to copy buffer";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/Sorting/CopyParticlesToCopyBuffer.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdCopyParticlesToCopyBuffer = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate particle sorting data";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/Sorting/GenerateParticleSortingData.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateSortingData = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "particle prefix scan stage 1";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/Sorting/PrefixScanStage1.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage1 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "particle prefix scan stage 2";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/Sorting/PrefixScanStage2.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage2 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "particle prefix scan stage 3";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/Sorting/PrefixScanStage3.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdPrefixScanStage3 = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "sort particle sorting data with prefix sums";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/Sorting/SortSortingDataWithPrefixSums.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdSortSortingDataWithPrefixSums = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "sort particles";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/Sorting/SortParticles.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdSortParticles = shaderStorageRef.GetShaderProgram(shaderKey);
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
    void ParticleParticleCollisions::AssembleBvhShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "guarantee particle sorting data uniqueness";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/BvhGeneration/GuaranteeSortingDataUniqueness.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGuaranteeSortingDataUniqueness= shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate particle bounding boxes";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/BvhGeneration/GenerateLeafNodeBoundingBoxes.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateLeafNodeBoundingBoxes = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate particle binary radix tree";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/BvhGeneration/GenerateBinaryRadixTree.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateBinaryRadixTree = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "merge particle bounding volumes";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/BvhGeneration/MergeBoundingVolumes.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdMergeBoundingVolumes = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Primarily serves to clean up the constructor.

        Assembles headers, buffers, and functional .comp files for the shaders that detect and 
        resolve particle-particle collisions.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 7/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::AssembleCollisionShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "detect particle-particle collisions";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/DetectParticleParticleCollisions.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdDetectCollisions = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "resolve particle-particle collisions";
        filePath = "Shaders/Compute/Collisions/ParticleParticle/ResolveParticleParticleCollisions.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdResolveCollisions = shaderStorageRef.GetShaderProgram(shaderKey);
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
    void ParticleParticleCollisions::AssembleGeometryCreationShaders()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;
        std::string filePath;

        shaderKey = "generate particle BVH geometry";
        filePath = "Shaders/Compute/Visualization/GenerateParticleBoundingBoxGeometry.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateParticleBoundingBoxGeometry = shaderStorageRef.GetShaderProgram(shaderKey);

        shaderKey = "generate particle velocity vector geometry";
        filePath = "Shaders/Compute/Visualization/GenerateParticleVelocityVectorGeometry.comp";
        shaderStorageRef.NewShader(shaderKey);
        shaderStorageRef.AddAndCompileShaderFile(shaderKey, filePath, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdGenerateParticleVelocityVectorGeometry = shaderStorageRef.GetShaderProgram(shaderKey);
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
    void ParticleParticleCollisions::SortParticlesWithoutProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const
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

            PrefixScan(numWorkGroupsXPrefixScan, bitNumber, sortingDataReadBufferOffset);
            SortSortingDataWithPrefixScan(numWorkGroupsX, bitNumber, sortingDataReadBufferOffset, sortingDataWriteBufferOffset);

            // swap read/write buffers and do it again
            writeToSecondBuffer = !writeToSecondBuffer;
        }

        // the sorting data's final location is in the "write" half of the sorting data buffer
        SortParticlesUsingSortingData(numWorkGroupsX, sortingDataWriteBufferOffset);

        // all done
        glUseProgram(0);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Like SortParticlesWithoutProfiling(...), but with 
        (1) std::chrono calls 
        (2) forced wait for shader to finish so that the std::chrono calls get an accurate 
            reading for how long the shader takes 
        (3) writing the output to a file (if desired)
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by work group size.
        numWorkGroupsXPrefixScan    See comment where this value was calculated.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::SortParticlesWithProfiling(unsigned int numWorkGroupsX, unsigned int numWorkGroupsXPrefixScan) const
    {
        cout << "sorting " << _numParticles << " particles" << endl;
        unsigned int totalBitCount = 32;

        // for profiling
        using namespace std::chrono;
        steady_clock::time_point start;
        steady_clock::time_point end;
        long long totalSortingTime = 0;

        start = high_resolution_clock::now();
        PrepareToSortParticles(numWorkGroupsX);

        bool writeToSecondBuffer = true;
        unsigned int sortingDataReadBufferOffset = 0;
        unsigned int sortingDataWriteBufferOffset = 0;
        for (unsigned int bitNumber = 0; bitNumber < totalBitCount; bitNumber++)
        {
            sortingDataReadBufferOffset = static_cast<unsigned int>(!writeToSecondBuffer) * _numParticles;
            sortingDataWriteBufferOffset = static_cast<unsigned int>(writeToSecondBuffer) * _numParticles;

            PrefixScan(numWorkGroupsXPrefixScan, bitNumber, sortingDataReadBufferOffset);
            SortSortingDataWithPrefixScan(numWorkGroupsX, bitNumber, sortingDataReadBufferOffset, sortingDataWriteBufferOffset);

            // swap read/write buffers and do it again
            writeToSecondBuffer = !writeToSecondBuffer;
        }

        // wherever the sorting data ended up, that is where the shader should read from
        SortParticlesUsingSortingData(numWorkGroupsX, sortingDataWriteBufferOffset);

        end = high_resolution_clock::now();
        totalSortingTime = duration_cast<microseconds>(end - start).count();

        // report results
        // Note: Write the results to a tab-delimited text file so that I can dump them into an 
        // Excel spreadsheet.
        std::ofstream outFile("ProfilingDurations/ParticleParallelSort.txt");
        if (outFile.is_open())
        {
            cout << "total particle sorting time: " << totalSortingTime << "\tmicroseconds" << endl;
            outFile << "total particle sorting time: " << totalSortingTime << "\tmicroseconds" << endl;
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
    void ParticleParticleCollisions::GenerateBvhWithoutProfiling(unsigned int numWorkGroupsX) const
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
    void ParticleParticleCollisions::GenerateBvhWithProfiling(unsigned int numWorkGroupsX) const
    {
        cout << "generating BVH for " << _numParticles << " particles" << endl;

        // for profiling
        using namespace std::chrono;
        steady_clock::time_point start;
        steady_clock::time_point end;
        long long durationPrepData = 0;
        long long durationGenerateTree = 0;
        long long durationMergeBoundingBoxes = 0;

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

        // report results
        // Note: Write the results to a tab-delimited text file so that I can dump them into an 
        // Excel spreadsheet.
        std::ofstream outFile("ProfilingDurations/GenerateParticleBvh.txt");
        if (outFile.is_open())
        {
            long long totalBvhGenerationTime = durationPrepData + durationGenerateTree + durationMergeBoundingBoxes;

            cout << "particle BVH generation: " << endl <<
                "\ttotal: " << totalBvhGenerationTime << "ms" << endl <<
                "\tprep data: " << durationPrepData << "ms" << endl <<
                "\tgenerate tree: " << durationGenerateTree << "ms" << endl <<
                "\tmerge bounding boxes: " << durationMergeBoundingBoxes << "ms" << endl;
            outFile << "particle BVH generation: " << endl <<
                "\ttotal: " << totalBvhGenerationTime << "ms" << endl <<
                "\tprep data: " << durationPrepData << "ms" << endl <<
                "\tgenerate tree: " << durationGenerateTree << "ms" << endl <<
                "\tmerge bounding boxes: " << durationMergeBoundingBoxes << "ms" << endl;
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
    void ParticleParticleCollisions::DetectAndResolveCollisionsWithoutProfiling(
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
    void ParticleParticleCollisions::DetectAndResolveCollisionsWithProfiling(
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

        //unsigned int startingIndexBytes = 0;
        //std::vector<PotentialParticleCollisions> checkPotentialCollisions(_particlePotentialCollisionsSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkPotentialCollisions.size() * sizeof(PotentialParticleCollisions);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particlePotentialCollisionsSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkPotentialCollisions.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //unsigned int startingIndexBytes = 0;
        //std::vector<Particle> checkPostCollisionParticles(_originalParticleSsbo->NumParticles());
        //unsigned int bufferSizeBytes = checkPostCollisionParticles.size() * sizeof(Particle);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _originalParticleSsbo->BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkPostCollisionParticles.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //for (size_t i = 0; i < checkPostCollisionParticles.size(); i++)
        //{
        //    if (isnan(checkPostCollisionParticles[i]._vel.x))
        //    {
        //        printf("");
        //    }
        //}

        // nothing to verify, so just report the results
        // Note: Write the results to a tab-delimited text file so that I can dump them into an 
        // Excel spreadsheet.
        std::ofstream outFile("ProfilingDurations/DetectAndResolveParticleParticleCollisions.txt");
        if (outFile.is_open())
        {
            long long totalSortingTime = durationDetectCollisions + durationResolveCollisions;

            cout << "particle-particle collision handling time:" << endl <<
                "\ttotal: " << totalSortingTime << endl <<
                "\tdetect collisions: " << durationDetectCollisions << endl <<
                "\tresolve collisions: " << durationResolveCollisions << endl;
            outFile << "particle-particle collision handling time:" << endl <<
                "\ttotal: " << totalSortingTime << endl <<
                "\tdetect collisions: " << durationDetectCollisions << endl <<
                "\tresolve collisions: " << durationResolveCollisions << endl;
        }
        outFile.close();
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::PrepareToSortParticles(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdCopyParticlesToCopyBuffer);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateSortingData);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // the two shaders worked on different buffers, so only need one memory barrier 
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //unsigned int startingIndexBytes = 0;
        //std::vector<SortingData> checkSortingData(_particleSortingDataSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particleSortingDataSsbo.BufferId());
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
        on two items, so the expected work group count is the number of particles divided by 2x 
        the work group size.  Sufficiant buffer size was allocated for this algorithm in 
        ParticlePrefixSumSsbo.
    Parameters: 
        numWorkGroupsX          See Description
        bitNumber               0 - 32
        sortingDataReadOffset   The sorting data is read from this half of the buffer.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::PrefixScan(unsigned int numWorkGroupsX, unsigned int bitNumber, unsigned int sortingDataReadOffset) const
    {
        glUseProgram(_programIdPrefixScanStage1);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //unsigned int startingIndexBytes = 0;
        //std::vector<unsigned int> checkPrefixScan(_prefixSumSsbo.TotalBufferEntries());
        //unsigned int bufferSizeBytes = checkPrefixScan.size() * sizeof(unsigned int);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _prefixSumSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
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
        //    else if (checkPrefixScan[i] != 0)
        //    {
        //        printf("");
        //    }
        //}
        //printf("");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Part of particle sorting.  
    Parameters: 
        numWorkGroupsX          Expected to be number of particles divided by work group size.
        bitNumber               0 - 32
        sortingDataReadOffset   The sorting data is read from this half of the buffer...
        sortingDataWriteOffset  And sorted according to the prefix sums into this half.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::SortSortingDataWithPrefixScan(
        unsigned int numWorkGroupsX, unsigned int bitNumber, 
        unsigned int sortingDataReadOffset, unsigned int sortingDataWriteOffset) const
    {
        glUseProgram(_programIdSortSortingDataWithPrefixSums);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_WRITE_OFFSET, sortingDataWriteOffset);
        glUniform1ui(UNIFORM_LOCATION_BIT_NUMBER, bitNumber);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        //unsigned int startingIndexBytes = sortingDataWriteOffset * sizeof(SortingData);
        //std::vector<SortingData> checkSortingData(_particleSortingDataSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particleSortingDataSsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkSortingData.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        //std::vector<unsigned int> binaryBuffer(_particleSortingDataSsbo.NumItems());
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
        The end of particle sorting.
    Parameters: 
        numWorkGroupsX          Expected to be number of particles divided by work group size.
        sortingDataReadOffset   Tells the shader which half of the ParticleSortingDataBuffer has 
                                the latest sort values.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::SortParticlesUsingSortingData(unsigned int numWorkGroupsX,
        unsigned int sortingDataReadOffset) const
    {
        //// verify sorted data
        //// Note: Only need to copy the first half of the buffer.  This is where the last loop of 
        //// the radix sorting algorithm put the sorting data.
        ////start = high_resolution_clock::now();
        //unsigned int startingIndexBytes = sortingDataReadOffset;
        //std::vector<SortingData> checkSortingData(_particleSortingDataSsbo.NumItems());
        //unsigned int bufferSizeBytes = checkSortingData.size() * sizeof(SortingData);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particleSortingDataSsbo.BufferId());
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

        glUseProgram(_programIdSortParticles);
        glUniform1ui(UNIFORM_LOCATION_PARTICLE_SORTING_DATA_BUFFER_READ_OFFSET, sortingDataReadOffset);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Modifies the ParticleSortingDataBuffer so that the resulting tree won't have depth 
        spikes due to duplicate entries, then gives each leaf node in the ParticleBvhNodeBuffer a 
        bounding box based on the particle that it is associated with.
    Parameters: 
        numWorkGroupsX      Expected to be the number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::PrepareForBinaryTree(unsigned int numWorkGroupsX) const
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
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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
        numWorkGroupsX      Expected to be the number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::GenerateBinaryRadixTree(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGenerateBinaryRadixTree);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // verify that the binary tree is valid by checking that all parent-child relationships 
        // are reciprocated 
        // Note: By virtue of being a binary tree, every node except the root has a parent, and 
        // that parent also specifies that node as a child exactly once.
        unsigned int startingIndexBytes = 0;
        std::vector<BvhNode> checkBinaryTree(_bvhNodeSsbo.NumTotalNodes());
        unsigned int bufferSizeBytes = checkBinaryTree.size() * sizeof(BvhNode);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bvhNodeSsbo.BufferId());
        void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        memcpy(checkBinaryTree.data(), bufferPtr, bufferSizeBytes);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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
                // this node is only one of the parent's childre, so the parent-child 
                // relationship is only a problem if neither of the parent's child are this one
                if ((thisNodeIndex != checkBinaryTree[thisNode._parentIndex]._leftChildIndex) &&
                    (thisNodeIndex != checkBinaryTree[thisNode._parentIndex]._rightChildIndex))
                {
                    // parent-child relationship not reciprocated
                    printf("");
                }
            }
        }

        printf("");
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
    void ParticleParticleCollisions::MergeNodesIntoBvh(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdMergeBoundingVolumes);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Populates the PotentialParticleParticleCollisionsBuffer.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::DetectCollisions(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdDetectCollisions);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Reads the PotentialParticleParticleCollisionsBuffer and gives particles new velocity vectors if 
        they collide.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::ResolveCollisions(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdResolveCollisions);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Generates vertices for each particle's velocity vector and for its BVH node bounding box.
    Parameters: 
        numWorkGroupsX      Expected to be number of particles divided by work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleParticleCollisions::GenerateGeometry(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdGenerateParticleVelocityVectorGeometry);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glUseProgram(_programIdGenerateParticleBoundingBoxGeometry);
        glDispatchCompute(numWorkGroupsX, 1, 1);

        // this geometry buffer will not be used in any shader storage, but it will be used for 
        // rendering
        //glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_SHADER_BIT);
        glMemoryBarrier(GL_VERTEX_SHADER_BIT);

        //// in case of debugging
        //WaitForComputeToFinish();
        //unsigned int startingIndexBytes = 0;
        //std::vector<MyVertex> checkResultantGeometry(_velocityVectorGeometrySsbo.NumVertices());
        //unsigned int bufferSizeBytes = checkResultantGeometry.size() * sizeof(MyVertex);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, _velocityVectorGeometrySsbo.BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndexBytes, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkResultantGeometry.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

}
