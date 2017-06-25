#include "Include/ShaderControllers/ParticleReset.h"

#include <string>

#include "Include/Buffers/PersistentAtomicCounterBuffer.h"
#include "Shaders/ShaderStorage.h"
#include "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "ThirdParty/glm/gtc/type_ptr.hpp"


//#include "Include/Buffers/Particle.h"
//static ParticleSsbo::SharedConstPtr particleSsbo = nullptr;


namespace ShaderControllers
{
    /*----------------------------------------------------------------------------------------
    Description:
        Gives members initial values.
        
        Constructs the ParticleResetPoint and ParticleResetBar compute shaders out of the 
        necessary shader pieces.
        Looks up all uniforms in the resultant shaders.
    Parameters: 
        ssboToReset     ParticleUpdate will tell the SSBO to configure its buffer size 
                        uniforms for the compute shader.
    Returns:    None
    Creator:    John Cox, 4/2017
    ----------------------------------------------------------------------------------------*/
    ParticleReset::ParticleReset(const ParticleSsbo::SharedConstPtr &ssboToReset) :
        _totalParticleCount(0),
        _computeProgramIdBarEmitters(0),
        _computeProgramIdPointEmitters(0),
        _unifLocPointEmitterCenter(-1),
        _unifLocPointMaxParticleEmitCount(-1),
        _unifLocPointMinParticleVelocity(-1),
        _unifLocPointMaxParticleVelocity(-1),
        _unifLocBarEmitterP1(-1),
        _unifLocBarEmitterP2(-1),
        _unifLocBarEmitterEmitDir(-1),
        _unifLocBarMaxParticleEmitCount(-1),
        _unifLocBarMinParticleVelocity(-1),
        _unifLocBarMaxParticleVelocity(-1)
    {
        //particleSsbo = ssboToReset;

        _totalParticleCount = ssboToReset->NumVertices();

        // construct the compute shader
        ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
        std::string shaderKey;

        // first make the particle reset shader for point emitters
        shaderKey = "particle reset point emitter";
        shaderStorageRef.NewCompositeShader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/Version.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/SsboBufferBindings.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/QuickNormalize.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleReset/Random.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleReset/ParticleResetPointEmitter.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _computeProgramIdPointEmitters = shaderStorageRef.GetShaderProgram(shaderKey);
        ssboToReset->ConfigureConstantUniforms(_computeProgramIdPointEmitters);

        // uniform lookups for the point emitter shader
        _unifLocPointEmitterCenter = shaderStorageRef.GetUniformLocation(shaderKey, "uPointEmitterCenter");
        _unifLocPointMaxParticleEmitCount = shaderStorageRef.GetUniformLocation(shaderKey, "uMaxParticleEmitCount");
        _unifLocPointMinParticleVelocity = shaderStorageRef.GetUniformLocation(shaderKey, "uMinParticleVelocity");
        _unifLocPointMaxParticleVelocity = shaderStorageRef.GetUniformLocation(shaderKey, "uMaxParticleVelocity");

        // now for the bar emitters
        shaderKey = "particle reset bar emitter";
        shaderStorageRef.NewCompositeShader(shaderKey);
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/Version.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/ComputeShaderWorkGroupSizes.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/SsboBufferBindings.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleBuffer.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/QuickNormalize.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleReset/Random.comp");
        shaderStorageRef.AddPartialShaderFile(shaderKey, "Shaders/Compute/ParticleReset/ParticleResetBarEmitter.comp");
        shaderStorageRef.CompileCompositeShader(shaderKey, GL_COMPUTE_SHADER);
        shaderStorageRef.LinkShader(shaderKey);
        _computeProgramIdBarEmitters = shaderStorageRef.GetShaderProgram(shaderKey);
        ssboToReset->ConfigureConstantUniforms(_computeProgramIdBarEmitters);

        // uniform lookups for the bar emitter shader
        // Note: This also requires a "min velocity between min and max".
        _unifLocBarEmitterP1 = shaderStorageRef.GetUniformLocation(shaderKey, "uBarEmitterP1");
        _unifLocBarEmitterP2 = shaderStorageRef.GetUniformLocation(shaderKey, "uBarEmitterP2");
        _unifLocBarEmitterEmitDir = shaderStorageRef.GetUniformLocation(shaderKey, "uBarEmitterEmitDir");
        _unifLocBarMaxParticleEmitCount = shaderStorageRef.GetUniformLocation(shaderKey, "uMaxParticleEmitCount");
        _unifLocBarMinParticleVelocity = shaderStorageRef.GetUniformLocation(shaderKey, "uMinParticleVelocity");
        _unifLocBarMaxParticleVelocity = shaderStorageRef.GetUniformLocation(shaderKey, "uMaxParticleVelocity");

        // uniform values are set in ResetParticles(...)
    }
    
    /*--------------------------------------------------------------------------------------------
    Description:
        Cleans up buffers and shader programs that were created for this shader controller.
    Parameters: None
    Returns:    None
    Creator:    John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    ParticleReset::~ParticleReset()
    {
        glDeleteProgram(_computeProgramIdBarEmitters);
        glDeleteProgram(_computeProgramIdPointEmitters);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        An AddEmitter(...) overload that adds a point emitter to internal storage.  
    Parameters:
        pointEmitter    A shared pointer to a const point emitter.
    Returns:    None
    Creator: John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleReset::AddEmitter(ParticleEmitterPoint::CONST_SHARED_PTR pointEmitter)
    {
        // make a copy of it
        _pointEmitters.push_back(pointEmitter);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        An AddEmitter(...) overload that adds a point emitter to internal storage.  
    Parameters:
        barEmitter  A shared pointer to a const bar emitter.
    Returns:    None
    Creator: John Cox, 4/2017
    --------------------------------------------------------------------------------------------*/
    void ParticleReset::AddEmitter(ParticleEmitterBar::CONST_SHARED_PTR barEmitter)
    {
        // make a copy of it
        _barEmitters.push_back(barEmitter);
    }

    /*--------------------------------------------------------------------------------------------
    Description:
        Dispatches a shader for each emitter, resetting up to particlesPerEmitterPerFrame for 
        each emitter.

        Particles are spread out evenly between all the emitters (or at least as best as 
        possible; technically the first emitter gets first dibs at the inactive particles, then 
        the second emitter, etc.).
    Parameters:    
        particlesPerEmitterPerFrame     Limits the number of particles that are reset per frame 
                                        so that they don't all spawn at once.
    Returns:    None
    Creator:    John Cox (10-10-2016)
                Originally from an even earlier class.  The "particle reset" and 
                "particle update" are the two oldest compute shaders in my demos.
    --------------------------------------------------------------------------------------------*/
    void ParticleReset::ResetParticles(unsigned int particlesPerEmitterPerFrame)
    {
        if (_pointEmitters.empty() && _barEmitters.empty())
        {
            // nothing to do
            return;
        }

        // spreading the particles evenly between multiple emitters is done by letting all the 
        // particle emitters have a go at all the inactive particles
        // Note: Yes, this algorithm is such that emitters resetting particles have to travers 
        // through the entire particle collection, but there isn't a way of telling the CPU 
        // where they were when the last particle was reset.  Also, after the "particles per 
        // emitter per frame" limit is reached, the vast majority of the threads will simply 
        // return, so it's actually pretty fast.
        GLuint numWorkGroupsX = (_totalParticleCount / WORK_GROUP_SIZE_X) + 1;
        GLuint numWorkGroupsY = 1;
        GLuint numWorkGroupsZ = 1;

        // give all point emitters a chance to reactivate inactive particles at their positions
        glUseProgram(_computeProgramIdPointEmitters);
        glUniform1ui(_unifLocPointMaxParticleEmitCount, particlesPerEmitterPerFrame);
        for (size_t pointEmitterCount = 0; pointEmitterCount < _pointEmitters.size(); pointEmitterCount++)
        {
            // reset everything necessary to control the emission parameters for this emitter
            // Note: This atomic counter is used to enforce the number of emitted particles per 
            // emitter per frame 
            PersistentAtomicCounterBuffer::GetInstance().ResetCounter();
            ParticleEmitterPoint::CONST_SHARED_PTR &emitter = _pointEmitters[pointEmitterCount];

            glUniform1f(_unifLocPointMinParticleVelocity, emitter->GetMinVelocity());
            glUniform1f(_unifLocPointMaxParticleVelocity, emitter->GetMaxVelocity());
            glUniform4fv(_unifLocPointEmitterCenter, 1, glm::value_ptr(emitter->GetPos()));

            // compute ALL the resets! (then make the results visible to the next use of the 
            // SSBO and to vertext buffer)
            glDispatchCompute(numWorkGroupsX, numWorkGroupsY, numWorkGroupsZ);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
        }

        // and now for any bar emitters
        glUseProgram(_computeProgramIdBarEmitters);
        glUniform1ui(_unifLocBarMaxParticleEmitCount, particlesPerEmitterPerFrame);
        for (size_t barEmitterCount = 0; barEmitterCount < _barEmitters.size(); barEmitterCount++)
        {
            PersistentAtomicCounterBuffer::GetInstance().ResetCounter();
            ParticleEmitterBar::CONST_SHARED_PTR &emitter = _barEmitters[barEmitterCount];

            glUniform1f(_unifLocBarMinParticleVelocity, emitter->GetMinVelocity());
            glUniform1f(_unifLocBarMaxParticleVelocity, emitter->GetMaxVelocity());

            // each bar needs to upload three position vectors (p1, p2, and emit direction)
            glUniform4fv(_unifLocBarEmitterP1, 1, glm::value_ptr(emitter->GetBarStart()));
            glUniform4fv(_unifLocBarEmitterP2, 1, glm::value_ptr(emitter->GetBarEnd()));
            glUniform4fv(_unifLocBarEmitterEmitDir, 1, glm::value_ptr(emitter->GetEmitDir()));

            // MOAR resets!
            glDispatchCompute(numWorkGroupsX, numWorkGroupsY, numWorkGroupsZ);
            glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
        }

        //unsigned int startingIndex = 0;
        //std::vector<Particle> checkResetParticles(particleSsbo->NumParticles());
        //unsigned int bufferSizeBytes = checkResetParticles.size() * sizeof(Particle);
        //glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleSsbo->BufferId());
        //void *bufferPtr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, startingIndex, bufferSizeBytes, GL_MAP_READ_BIT);
        //memcpy(checkResetParticles.data(), bufferPtr, bufferSizeBytes);
        //glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

        //for (size_t i = 0; i < checkResetParticles.size(); i++)
        //{
        //    if (checkResetParticles[i]._isActive == 1)
        //    {
        //        printf("");
        //    }
        //}

        // cleanup
        glUseProgram(0);
    }


}
