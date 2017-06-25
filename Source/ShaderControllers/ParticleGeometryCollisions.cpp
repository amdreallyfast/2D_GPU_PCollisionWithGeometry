
#include "Include/ShaderControllers/ParticleGeometryCollisions.h"

#include "Shaders/ShaderStorage.h"
#include "ThirdParty/glload/include/glload/gl_4_4.h"

#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"

// for profiling and checking results
#include "Include/ShaderControllers/ProfilingWaitToFinish.h"
#include "Include/Buffers/Particle.h"
#include "Include/Geometry/PolygonFace.h"
#include <algorithm>

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
        _numParticles(particleSsbo->NumParticles()),
        _programIdResolveCollisions(0),
        _collideableGeometrySsbo(blenderObjFilePath),
        _particleSsbo(particleSsbo)
    {
        AssembleProgramResolveCollisions();

        // load the buffer size uniforms where the SSBOs will be used
        particleSsbo->ConfigureConstantUniforms(_programIdResolveCollisions);

        _collideableGeometrySsbo.ConfigureConstantUniforms(_programIdResolveCollisions);

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
        glDeleteProgram(_programIdResolveCollisions);
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
        int numWorkGroupsX = _numParticles / WORK_GROUP_SIZE_X;
        int remainder = _numParticles % WORK_GROUP_SIZE_X;
        numWorkGroupsX += (remainder == 0) ? 0 : 1;

        if (withProfiling)
        {
            ResolveCollisionsWithProfiling(numWorkGroupsX);
        }
        else
        {
            ResolveCollisionsWithoutProfiling(numWorkGroupsX);
        }
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Used so that the RenderGeometry shader controller can draw the geometry that the 
        particles are colliding with.
    Parameters: None
    Returns:    
        A const reference to the SSBO that contains the collideable geometry's vertices.
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    const VertexSsboBase &ParticleGeometryCollisions::GeometrySsbo() const
    {
        return _collideableGeometrySsbo;
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        The GLSL version declaration, compute shader work group sizes, 
        cross-shader uniform locations, and SSBO buffer bindings are used in multiple compute 
        shaders.  This function puts their assembly into one place.
    Parameters: 
        The key to the composite shader that is under construction.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::AssembleProgramHeader(const std::string &shaderKey) const
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/Version.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/SsboBufferBindings.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp");
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Assembles headers, buffers, and functional .comp files for the shader that has each 
        particle checking if it crossed a 2D polygon face, and if it did, to redirect the 
        particle around the surface's normal and set the new velocity in the bounce direction.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::AssembleProgramResolveCollisions()
    {
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "resolve particle-geometry collisions";
        shaderStorageRef.NewCompositeShader(shaderKey);
        AssembleProgramHeader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/GeometryStuff/MyVertex.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/GeometryStuff/PolygonFace.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleGeometryCollisions/Buffers/CollideableGeometryBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleGeometryCollisions/ResolveParticleGeometryCollisions.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _programIdResolveCollisions = shaderStorageRef.GetShaderProgram(shaderKey);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method governs the shader dispatches that will result in particles bouncing off any 
        geometry that they collide with.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by the work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::ResolveCollisionsWithoutProfiling(unsigned int numWorkGroupsX) const
    {
        DetectAndResolveCollisions(numWorkGroupsX);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Like ResolveCollisionsWithoutProfiling(...), but with 
        (1) std::chrono calls 
        (2) forced wait for shader to finish so that the std::chrono calls get an accurate 
            reading for how long the shader takes 
        (3) writing the output to a file (if desired)
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by the work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::ResolveCollisionsWithProfiling(unsigned int numWorkGroupsX) const
    {
        cout << "detecting collisions for up to " << _numParticles << " particles with " << 
            _collideableGeometrySsbo.NumPolygons() << " 2D polygon faces" << endl;

        // for profiling
        using namespace std::chrono;
        steady_clock::time_point start;
        steady_clock::time_point end;
        long long durationDetectAndResolveCollisions = 0;

        start = high_resolution_clock::now();
        DetectAndResolveCollisions(numWorkGroupsX);
        WaitForComputeToFinish();
        end = high_resolution_clock::now();
        durationDetectAndResolveCollisions = duration_cast<microseconds>(end - start).count();

        // report the results to file
        // Note: Write the results to a tab-delimited text file so that I can dump them into an 
        // Excel spreadsheet.
        std::ofstream outFile("DetectAndResolveCollisionsDurations.txt");
        if (outFile.is_open())
        {
            long long totalSortingTime = durationDetectAndResolveCollisions;

            cout << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;
            outFile << "total collision handling time: " << totalSortingTime << "\tmicroseconds" << endl;

            cout << "detect and resolve collisions: " << durationDetectAndResolveCollisions << "\tmicroseconds" << endl;
            outFile << "detect and resolve collisions: " << durationDetectAndResolveCollisions << "\tmicroseconds" << endl;
        }
        outFile.close();
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        This method performs the actual work of dispatching the shader that will detect and 
        resolve the particle collisions with geometry.
    Parameters: 
        numWorkGroupsX  Expected to be the total particle count divided by the work group size.
    Returns:    None
    Creator:    John Cox, 6/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleGeometryCollisions::DetectAndResolveCollisions(unsigned int numWorkGroupsX) const
    {
        glUseProgram(_programIdResolveCollisions);
        glDispatchCompute(numWorkGroupsX, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_SHADER_BIT);


        std::vector<PolygonFace> checkPolygons(_collideableGeometrySsbo.NumPolygons());
        {
            unsigned int startingIndex = 0;
            unsigned int bufferSizeBytes = checkPolygons.size() * sizeof(PolygonFace);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, _collideableGeometrySsbo.BufferId());
            void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
            memcpy(checkPolygons.data(), bufferPtr, bufferSizeBytes);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        std::vector<Particle> checkParticles(_particleSsbo->NumParticles());
        {
            unsigned int startingIndex = 0;
            unsigned int bufferSizeBytes = checkParticles.size() * sizeof(Particle);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, _particleSsbo->BufferId());
            void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
            memcpy(checkParticles.data(), bufferPtr, bufferSizeBytes);
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }

        int maxVal = 0;
        for (size_t i = 0; i < checkParticles.size(); i++)
        {
            maxVal = std::max(maxVal, checkParticles[i]._numNearbyParticles);
            if (checkParticles[i]._numNearbyParticles == 37)
            {
                printf("");
            }
            else if (checkParticles[i]._isActive == 37)
            {
                printf("");
            }
        }
        printf("");
    }
}
