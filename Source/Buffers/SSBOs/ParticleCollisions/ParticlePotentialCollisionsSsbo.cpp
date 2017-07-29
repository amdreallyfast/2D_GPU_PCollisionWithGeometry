#include "Include/Buffers/SSBOs/ParticlePotentialCollisionsSsbo.h"

#include "ThirdParty/glload/include/glload/gl_4_4.h"
#include "Shaders/ShaderHeaders/SsboBufferBindings.comp"
#include "Shaders/ShaderHeaders/CrossShaderUniformLocations.comp"
#include "Shaders/ShaderStorage.h"

#include "Include/Buffers/ParticlePotentialCollisions.h"

#include <vector>

/*------------------------------------------------------------------------------------------------
Description:
    Initializes base class, then gives derived class members initial values and allocates space 
    for the SSBO.
Parameters: 
    numParticles    Self-explanatory.
Returns:    None
Creator:    John Cox, 5/2017
------------------------------------------------------------------------------------------------*/
ParticlePotentialCollisionsSsbo::ParticlePotentialCollisionsSsbo(unsigned int numParticles) :
    SsboBase(),  // generate buffers
    _numItems(numParticles)
{
    std::vector<ParticlePotentialCollisions> v(numParticles);

    // now bind this new buffer to the dedicated buffer binding location
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, PARTICLE_POTENTIAL_COLLISIONS_BUFFER_BINDING, _bufferId);

    // and fill it with new data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _bufferId);
    glBufferData(GL_SHADER_STORAGE_BUFFER, v.size() * sizeof(ParticlePotentialCollisions), v.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

/*------------------------------------------------------------------------------------------------
Description:
    Defines the buffer's size uniform in the specified shader.  It uses the #define'd uniform 
    location found in CrossShaderUniformLocations.comp.
Parameters: 
    computeProgramId    Self-explanatory.
Returns:    None
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
void ParticlePotentialCollisionsSsbo::ConfigureConstantUniforms(unsigned int computeProgramId) const
{
    ShaderStorage &shaderStorageRef = ShaderStorage::GetInstance();
    unsigned int bufferSizeUnifLoc = shaderStorageRef.GetUniformLocation(computeProgramId, "uPotentialParticleCollisionsBufferSize");

    // the uniform should remain constant after this 
    glUseProgram(computeProgramId);
    glUniform1ui(bufferSizeUnifLoc, _numItems);
    glUseProgram(0);
}

/*------------------------------------------------------------------------------------------------
Description:
    A simple getter for the value that was passed in on creation.
Parameters: None
Returns:    
    See Description.
Creator:    John Cox, 6/2017
------------------------------------------------------------------------------------------------*/
unsigned int ParticlePotentialCollisionsSsbo::NumItems() const
{
    return _numItems;
}
