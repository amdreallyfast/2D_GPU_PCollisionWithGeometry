#include "Include/ShaderControllers/ParticleUpdate.h"

#include <string>

#include "Include/Buffers/PersistentAtomicCounterBuffer.h"
#include "Shaders/ShaderStorage.h"
#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "ThirdParty/glm/gtc/type_ptr.hpp"


namespace ShaderControllers
{
    /*--------------------------------------------------------------------------------------------
    Description:
        Gives members initial values.
        
        Constructs the ParticleUpdate compute shader out of the necessary shader pieces.
        Looks up all uniforms in the resultant ParticleUpdate shader.
    Parameters: 
        ssboToUpdate    ParticleUpdate will tell the SSBO to configure its buffer size uniforms 
                        for the compute shader.
        particleRegionCenter    Used in conjunction with radius to tell when a particle goes 
        particleRegionRedius    out of bounds.
    Returns:    None
    Creator:    John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    ParticleUpdate::ParticleUpdate(const ParticleSsbo::SharedConstPtr &ssboToUpdate) :
        _totalParticleCount(0),
        _activeParticleCount(0),
        _computeProgramId(0),
        _unifLocDeltaTimeSec(-1)
    {
        _totalParticleCount = ssboToUpdate->NumVertices();

        // construct the compute shader
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();

        std::string shaderKey = "particle update";
        shaderStorageRef.NewCompositeShader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/Version.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/SsboBufferBindings.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleRegionBoundaries.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleUpdate.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _computeProgramId = shaderStorageRef.GetShaderProgram(shaderKey);
        ssboToUpdate->ConfigureConstantUniforms(_computeProgramId);

        _unifLocDeltaTimeSec = shaderStorageRef.GetUniformLocation(shaderKey, "uDeltaTimeSec");

        // delta time set in Update(...)
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Cleans up buffers and shader programs that were created for this shader controller.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    ParticleUpdate::~ParticleUpdate()
    {
        glDeleteProgram(_computeProgramId);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Resets the "num active particles" atomic counter, dispatches the shader, and reads the 
        number of active particles after the shader finished.
    
        The number of work groups is based on the maximum number of particles.
    Parameters:    
        deltaTimeSec    Self-explanatory
    Returns:    None
    Creator:    John Cox (10-10-2016)
    --------------------------------------------------------------------------------------------*/
    void ParticleUpdate::Update(float deltaTimeSec)
    {
        // spread out the particles between lots of work items, but keep it 1-dimensional 
        // because the particle buffer is a 1-dimensional array
        // Note: +1 because integer division drops the remainder, and I want all the particles 
        // to have a shot.
        GLuint numWorkGroupsX = (_totalParticleCount / WORK_GROUP_SIZE_X) + 1;
        GLuint numWorkGroupsY = 1;
        GLuint numWorkGroupsZ = 1;

        glUseProgram(_computeProgramId);

        glUniform1f(_unifLocDeltaTimeSec, deltaTimeSec);

        // the atomic counter is used to count the total number of active particles after this 
        // update
        PersistentAtomicCounterBuffer::GetInstance().ResetCounter();
        glDispatchCompute(numWorkGroupsX, numWorkGroupsY, numWorkGroupsZ);

        // the results of the moved particles need to be visible to the next compute shader that 
        // accesses the buffer, vertex data sourced from the particle buffer need to reflect the 
        // updated movements, and reads from atomic counters must be visible as well (for number 
        // of active particles)
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

        // cleanup
        glUseProgram(0);

        // now that all active particles have updated, check how many active particles exist 
        _activeParticleCount = PersistentAtomicCounterBuffer::GetInstance().GetCounterValue();
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        A simple getter for the number of particles that were active on the last Update(...) 
        call.
        
        Useful for performance comparison with CPU version.
    Parameters: None
    Returns:    None
    Creator:    John Cox (1-7-2017)
    --------------------------------------------------------------------------------------------*/
    unsigned int ParticleUpdate::NumActiveParticles() const
    {
        return _activeParticleCount;
    }

}
